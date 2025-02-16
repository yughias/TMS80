#include "SDL_MAINLOOP.h"
#include "console.h"

#include <string.h>

console_t console;

void setup(){
    char* path = SDL_GetBasePath();

    size(SCREEN_WIDTH, SCREEN_HEIGHT);
    setScaleMode(NEAREST);
    setTitle("TMS80");

    char rom_path[FILENAME_MAX];
    strncpy(rom_path, getArgv(1), FILENAME_MAX - 1);

    console_init(&console, rom_path);

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