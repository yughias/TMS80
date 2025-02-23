#include "SDL_MAINLOOP.h"
#include "console.h"

#include <string.h>

char rom_path[FILENAME_MAX];
char bios_path[FILENAME_MAX];
console_t console;

void parse_input();

void setup(){
    setScaleMode(NEAREST);
    setTitle("TMS80");

    parse_input();

    console_init(&console, rom_path, bios_path);
    if(console.vdp.cram_size == CRAM_SIZE_GG){
        size(SCREEN_WIDTH_GG, SCREEN_HEIGHT_GG);
        setWindowSize(width*1.2, height);
    } else
        size(SCREEN_WIDTH_SMS, SCREEN_HEIGHT_SMS);
        
    setAspectRatio(ASPECT_RATIO);

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

    console_run_frame(&console);

    background(0);
    if(console.vdp.cram_size == CRAM_SIZE_GG){
        for(int y = 0; y < height; y++)
            for(int x = 0; x < width; x++)
                pixels[x+y*width] = console.vdp.framebuffer[(x+GG_START_X) + (y+GG_START_Y) * SCREEN_WIDTH_SMS];
    } else {
        for(int i = 0; i < width*height; i++)
            pixels[i] = console.vdp.framebuffer[i];
    }
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