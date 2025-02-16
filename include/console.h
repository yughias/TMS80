#ifndef __CONSOLE_H__
#define __CONSOLE_H__

#include "z80.h"
#include "sn76489.h"
#include "vdp.h"

#include <stdint.h>
#include <stdbool.h>

#define CYCLES_PER_LINE 228

#define NTSC_REFRESH_RATE 59.9227434033
#define NTSC_CLOCK_RATE 3579545
#define NTSC_CYCLES_PER_FRAME 59736

#define PAL_REFRESH_RATE 49.7014596255682 
#define PAL_CLOCK_RATE 3546895
#define PAL_CYCLES_PER_FRAME 71364

#define RAM_SIZE 0x10000

typedef enum CONSOLE_TYPE {CONSOLE_UNKNOWN, SG1000, SC3000, SMS} CONSOLE_TYPE;

typedef struct console_t
{
    CONSOLE_TYPE type;

    double refresh_rate;
    size_t cycles_per_frame;
    
    z80_t z80;
    vdp_t vdp;
    sn76489_t apu;

    uint8_t* cartridge;
    size_t cartridge_size;

    bool has_keyboard;
    bool force_paddle_controller;
    bool paddle_status;
    uint8_t keypad_reg;

    uint8_t ram_bank;
    uint8_t banks[3];

    uint8_t RAM[RAM_SIZE];
} console_t;

void console_init(console_t* console, const char* filename);
void console_run_frame(console_t* console);

CONSOLE_TYPE console_detect_type(const char* filename);
bool console_detect_ram_adapter(uint8_t* cartridge, size_t cartridge_size);

uint8_t console_get_keypad_a(console_t* console);
uint8_t console_get_keypad_b(console_t* console);


#endif