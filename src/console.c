#include "console.h"
#include "memory.h"

#include "SDL_MAINLOOP.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define KEY(x) SDL_SCANCODE_ ## x
#define NO_KEY KEY(UNKNOWN)

#define CONSOLE_SET_REGION(console, zone) \
console->cycles_per_frame = zone ## _CYCLES_PER_FRAME; \
console->refresh_rate = zone ## _REFRESH_RATE; \
console->vdp.region = REGION_ ## zone;

static SDL_Scancode keypad_row_a[8][8] = {
    { KEY(1),  KEY(Q),    KEY(A),    KEY(Z),     KEY(RCTRL),     KEY(COMMA),  KEY(K),            KEY(I)           },
    { KEY(2),  KEY(W),    KEY(S),    KEY(X),     KEY(SPACE),     KEY(PERIOD), KEY(L),            KEY(O)           },
    { KEY(3),  KEY(E),    KEY(D),    KEY(C),     KEY(DELETE),    KEY(SLASH),  KEY(SEMICOLON),    KEY(P)           },
    { KEY(4),  KEY(R),    KEY(F),    KEY(V),     KEY(BACKSPACE), KEY(RALT),   KEY(APOSTROPHE),   KEY(BACKSLASH)   },
    { KEY(5),  KEY(T),    KEY(G),    KEY(B),     NO_KEY,         KEY(DOWN),   KEY(RIGHTBRACKET), KEY(LEFTBRACKET) },
    { KEY(6),  KEY(Y),    KEY(H),    KEY(N),     NO_KEY,         KEY(LEFT),   KEY(RETURN),       NO_KEY           },
    { KEY(7),  KEY(U),    KEY(J),    KEY(M),     NO_KEY,         KEY(RIGHT),  KEY(UP),           NO_KEY           },
    { KEY(UP), KEY(DOWN), KEY(LEFT), KEY(RIGHT), KEY(Z),         KEY(X),      NO_KEY,            NO_KEY           }
};

static SDL_Scancode keypad_row_b[8][4] = {
    { KEY(8),         NO_KEY,    NO_KEY,     NO_KEY      },
    { KEY(9),         NO_KEY,    NO_KEY,     NO_KEY      },
    { KEY(0),         NO_KEY,    NO_KEY,     NO_KEY      },
    { KEY(MINUS),     NO_KEY,    NO_KEY,     NO_KEY      },
    { KEY(EQUALS),    NO_KEY,    NO_KEY,     NO_KEY      },
    { KEY(GRAVE),     NO_KEY,    NO_KEY,     KEY(TAB)    },
    { KEY(RSHIFT),    KEY(LALT), KEY(LCTRL), KEY(LSHIFT) },
    { NO_KEY,         NO_KEY,    NO_KEY,     NO_KEY      }
};

void load_file(const char* filename, uint8_t** buffer, size_t* size){
    FILE* fptr = fopen(filename, "rb");
    if(!fptr){
        printf("can't open file: %s\n", filename);
        exit(EXIT_FAILURE);
    }

    fseek(fptr, 0, SEEK_END);
    *size = ftell(fptr);
    rewind(fptr);

    *buffer = malloc(*size);
    fread(*buffer, 1, *size, fptr);
    fclose(fptr);
}

void console_init(console_t* console, const char* rom_path, const char* bios_path){
    memset(console, 0, sizeof(console_t));
    z80_init(&console->z80);
    console->z80.master = console;
    console->z80.readIO = console_readIO;
    console->z80.writeIO = console_writeIO;

    console->keypad_reg = 0x7;

    if(
        strstr(rom_path, "(Europe)") || strstr(rom_path, "(E)") ||
        strstr(rom_path, "(Brazil)") || strstr(rom_path, "[E]")
    ){
        CONSOLE_SET_REGION(console, PAL);
        printf("PAL!\n");
    } else {
        CONSOLE_SET_REGION(console, NTSC);
        printf("NTSC!\n");
    }

    for(int i = 0; i < 3; i++)
        console->banks[i] = i;

    if(strcmp(rom_path, ""))
        load_file(rom_path, &console->cartridge, &console->cartridge_size);
    else {
        console->cartridge = malloc(1 << 10);
        memset(console->cartridge, 0xFF, 1 << 10);
        console->cartridge_size = 1 << 10;
    }

    if(strcmp(bios_path, "")){
        load_file(bios_path, &console->bios, &console->bios_size);
        console->type = console_detect_type(bios_path);
    } else
        console->type = console_detect_type(rom_path);
    
    console->has_keyboard = console->type == SC3000;

    if(console->type == CONSOLE_UNKNOWN){
        printf("can't detect console!\n");
        exit(EXIT_FAILURE);
    }

    switch(console->type){
        case SG1000:
        case SC3000:
        console->z80.readMemory = console->cartridge_size == (1 << 14) ? sg_sc_mirrored_readMemory : sg_sc_readMemory;
        console->z80.writeMemory = console_detect_ram_adapter(console->cartridge, console->cartridge_size) ? sg_sc_ram_adapter_writeMemory : sg_sc_writeMemory;
        break;

        case SMS:
        if(console->bios){
            console->z80.readMemory = sms_bios_readMemory;
            console->z80.writeMemory = sms_bios_writeMemory;
        } else {
            console->z80.readMemory = sms_readMemory;
            console->z80.writeMemory = sms_writeMemory;
        }
        break;

        default:
        printf("UNKNOWN CONSOLE!\n");
        exit(EXIT_FAILURE);
        break;
    }

    sn76489_t* apu = &console->apu;
    if(console->type == SMS){
        SN76489_SET_TYPE(apu, SMS);
    } else {
        SN76489_SET_TYPE(apu, GENERIC);
    }
}

bool console_detect_ram_adapter(uint8_t* cartridge, size_t cartridge_size){
    if(cartridge_size > 0x4000){
        uint8_t* empty_space = malloc(0x2000);
        memset(empty_space, 0xFF, 0x2000);
        
        if(!memcmp(cartridge + 0x2000, empty_space, 0x2000)){
            printf("builtin ram detected!\n");
            return true;
        }

        free(empty_space);
    }

    return false;
}

CONSOLE_TYPE console_detect_type(const char* rom_path){
    const char* dot = strrchr(rom_path, '.');

    if(!dot || dot == rom_path)
        return CONSOLE_UNKNOWN;

    if(!strcmp(dot, ".sg"))
        return SG1000;

    if(!strcmp(dot, ".sc"))
        return SC3000;

    
    if(!strcmp(dot, ".sms"))
        return SMS;

    return CONSOLE_UNKNOWN;
}

uint8_t console_get_keypad_a(console_t* console){
    if(console->force_paddle_controller){
        uint8_t x = 0xF0;
        if(!console->paddle_status)
            x &= ~(1 << 5);
        x |= !console->paddle_status ? (mouseX & 0xF) : (mouseX >> 4);
        if(isMousePressed)
            x &= ~(1 << 4);
        console->paddle_status ^= 1;
        return x;
    }

    const Uint8* keystate = SDL_GetKeyboardState(NULL);
    uint8_t row = console->keypad_reg & 0b111;
    uint8_t out = 0xFF;

    for(int i = 0; i < 8; i++)
        if(keystate[ keypad_row_a[row][i] ])
            out &= ~(1 << i);

    return out;
}

uint8_t console_get_keypad_b(console_t* console){
    const Uint8* keystate = SDL_GetKeyboardState(NULL);
    uint8_t row = console->keypad_reg & 0b111;
    uint8_t out = 0xFF;

    for(int i = 0; i < 4; i++)
        if(keystate[ keypad_row_b[row][i] ])
            out &= ~(1 << i);

    return out;
}

void console_run_frame(console_t* console){
    z80_t* z80 = &console->z80;
    vdp_t* vdp = &console->vdp;
    sn76489_t* apu = &console->apu;

    while(z80->cycles < console->cycles_per_frame){
        uint32_t old_cycles = z80->cycles;
        int old_line = old_cycles / CYCLES_PER_LINE;
        
        z80_step(z80);

        uint32_t elapsed = z80->cycles - old_cycles;
        sn76489_update(apu, elapsed);
        sn76489_push_sample(apu, elapsed);

        int line = z80->cycles / CYCLES_PER_LINE;
        if(line != old_line){
            vdp->v_counter = old_line;

            if(vdp->regs[0] & (1 << 4)){
                if(old_line <= SCREEN_HEIGHT){
                    vdp->line_reg -= 1;
                    if(vdp->line_reg == 0xFF){
                        vdp_fire_interrupt(vdp, z80, false);
                        vdp->line_reg = vdp->regs[0xA];
                    }
                } else {
                    vdp->line_reg = vdp->regs[0xA];
                }
            }

            if(old_line < SCREEN_HEIGHT){
                if(old_line == 0)
                    vdp->scroll_y = vdp->regs[9];
                vdp_render_line(vdp, old_line);
            }

            if(old_line == SCREEN_HEIGHT){
                vdp_fire_interrupt(vdp, z80, true);
            }
        }
    }

    z80->cycles -= console->cycles_per_frame;
}