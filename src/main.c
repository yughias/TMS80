#include "SDL_MAINLOOP.h"
#include "console.h"

#include <string.h>

char rom_path[FILENAME_MAX];
char bios_path[FILENAME_MAX];
console_t console;

void parse_input();

void setup(){
    char* path = SDL_GetBasePath();

    size(SCREEN_WIDTH, SCREEN_HEIGHT);
    setScaleMode(NEAREST);
    setTitle("TMS80");

    parse_input();

    console_init(&console, rom_path, bios_path);

    sn76489_t* apu = &console.apu;
    apu->audioSpec.freq = 44100;
    apu->audioSpec.channels = 1;
    apu->audioSpec.format = AUDIO_S16;
    apu->audioSpec.samples = SAMPLES_PER_CALL;
    apu->audioSpec.callback = NULL;
    apu->audioDev = SDL_OpenAudioDevice(0, 0, &apu->audioSpec, &apu->audioSpec, 0);

    apu->push_rate_reload = console.refresh_rate * console.cycles_per_frame / apu->audioSpec.freq;
    apu->queue_check_counter = apu->push_rate_reload * SAMPLES_PER_CALL;

    SDL_PauseAudioDevice(apu->audioDev, 0);
    frameRate(console.refresh_rate);
}

void loop(){
    if(isKeyReleased){
        switch(keyPressed){
            case SDLK_F1:
            z80_nmi(&(console.z80));
            break;

            case SDLK_F2:
            if(console.type == SMS)
            console.force_paddle_controller ^= 1;
            break;
        }
    }

    background(0);
    console_run_frame(&console);
    if(console.vdp.regs[1] & 1)
        printf("big sprites!\n");
    if((console.vdp.regs[0] & (1 << 2)) && (console.vdp.regs[1] & (0b11 << 3)))
        printf("extra height!\n");
}

void parse_input(){
    enum {SEARCH_STAGE, ROM_STAGE, BIOS_STAGE} stage = SEARCH_STAGE;
    for(int i = 1; i < getArgc(); i++){
        const char* arg = getArgv(i);
        switch(stage){
            case SEARCH_STAGE:
            if(!strcmp("-b", arg) || !strcmp("--bios", arg)){
                stage = BIOS_STAGE;
            } else if(!strcmp("-r", arg) || !strcmp("--rom", arg))
                stage = ROM_STAGE;
            else
                strncpy(rom_path, arg, FILENAME_MAX - 1);
            break;

            case ROM_STAGE:
            strncpy(rom_path, arg, FILENAME_MAX - 1);
            stage = SEARCH_STAGE;
            break;
            
            case BIOS_STAGE:
            strncpy(bios_path, arg, FILENAME_MAX - 1);
            stage = SEARCH_STAGE;
            break;
        }
    }
    if(!strcmp(rom_path, ""))
        printf("NO INPUT ROM!\n");
    if(!strcmp(bios_path, ""))
        printf("NO INPUT BIOS!\n");
}