#include "sn76489.h"

#include "SDL_MAINLOOP.h"

#define IS_COUNTER_REGISTER(x) (x < 6 && !(x & 1))
#define IS_ATTENUATION_REGISTER(x) (x & 1)
#define IS_NOISE_CONTROL_REGISTER(x) (x == 6)
static sample_t attenuation_value[16] = {
    8191, 6506, 5167, 4104,
    3259, 2588, 2055, 1632,
    1296, 1029,  817,  648, 
     514,  408,  324,    0 
};

static inline bool parity(int val) {
	val ^= val >> 8;
	val ^= val >> 4;
	val ^= val >> 2;
	val ^= val >> 1;
	return val & 1;
};

void sn76489_push_sample(sn76489_t* sn, int cycles){
    sn->push_rate_counter -= cycles;
    if(sn->push_rate_counter <= 0){
        sn->push_rate_counter += sn->push_rate_reload;
        #ifdef __EMSCRIPTEN__
        if(sn->buffer_idx < SAMPLE_BUFFER_SIZE){
            sample_t sample = sn76489_get_sample(sn);
            sn->buffer[sn->buffer_idx++] = sample;
        }
        if(SDL_GetQueuedAudioSize(sn->audioDev) <= SAMPLE_BUFFER_SIZE * sizeof(uint16_t)){
            SDL_QueueAudio(sn->audioDev, sn->buffer, sn->buffer_idx * sizeof(uint16_t));
            sn->buffer_idx = 0;
        }
        #else
        sample_t sample = sn76489_get_sample(sn);
        sn->buffer[sn->buffer_idx++] = sample;
        if(sn->buffer_idx == SAMPLE_BUFFER_SIZE){
            SDL_QueueAudio(sn->audioDev, sn->buffer, sn->buffer_idx*sizeof(uint16_t));
            sn->buffer_idx = 0;
            while(SDL_GetQueuedAudioSize(sn->audioDev) > SAMPLE_BUFFER_SIZE*sizeof(uint16_t));
        }
        #endif
    }
}

void sn76489_update(sn76489_t* sn, int cycles){
    // update tone channels
    for(int i = 0; i < 3; i++){
        if(sn->freq[i] <= (1 << 4)){
            sn->sample[i] = 1;
            continue;
        }

        sn->counter[i] -= cycles;
        if(sn->counter[i] <= 0){
            sn->sample[i] ^= 1;
            sn->counter[i] += sn->freq[i];
        }
    }

    // update lfsr
    sn->counter[3] -= cycles;
    if(sn->counter[3] <= 0){
        sn->lfsr.reg =
            (sn->lfsr.reg >> 1) |
            ((
                sn->lfsr.white_noise ?
                parity(sn->lfsr.reg & sn->lfsr.tapped_bits) :
                sn->lfsr.reg & 1
            ) << sn->lfsr.feedback_bit);

        sn->sample[3] = sn->lfsr.reg & 1;
        sn->counter[3] += (sn->lfsr_freq2_counter ? sn->freq[2] << 1: sn->freq[3]);
    }
}

void sn76489_write(sn76489_t* sn, uint8_t byte){
    bool is_latch = byte & 0x80;
    
    if(is_latch)
        sn->latched_idx = (byte >> 4) & 0b111;

    if(IS_COUNTER_REGISTER(sn->latched_idx)){
        int value = sn->freq[sn->latched_idx >> 1] >> 4;
        if(is_latch){
            value &= ~0xF;
            value |= byte & 0xF;
        } else {
            value &= 0xF;
            value |= (byte & 0x3F) << 4;
        }

        value <<= 4;

        sn->freq[sn->latched_idx >> 1] = value;
    }

    if(IS_ATTENUATION_REGISTER(sn->latched_idx))
        sn->attenuation[sn->latched_idx >> 1] = attenuation_value[byte & 0xF];

    if(IS_NOISE_CONTROL_REGISTER(sn->latched_idx)){
        uint8_t counter_cmd = byte & 0b11;
        if(counter_cmd != 0b11){
            sn->freq[3] = (0x10 << counter_cmd) << 5;
            sn->lfsr_freq2_counter = false;
        } else {
            sn->lfsr_freq2_counter = true;
        }

        sn->lfsr.white_noise = byte & 0b100;
        sn->lfsr.reg = (1 << sn->lfsr.feedback_bit);
    }
}

sample_t sn76489_get_sample(sn76489_t* sn){
    sample_t sample = 0;

    for(int i = 0; i < 4; i++){
        uint16_t ch_sample = sn->sample[i]*sn->attenuation[i];
        sample += ch_sample;
        if(sn->display_idx[i] != DISPLAY_BUFFER_SIZE){
            sn->display_buffers[i][sn->display_idx[i]++] = ch_sample;
        }
    }

    return sample;
}

static void sn76489_draw_wave(int x0, int y0, uint16_t* buffer, int buffer_len, SDL_Surface* s){
    int* pixels = (int*)s->pixels;
    const int white = color(255, 255, 255);

    int avg = 0;
    for(int i = 0; i < buffer_len; i++)
        avg += buffer[i];
    if(buffer_len)
        avg /= buffer_len;

    int idx = 0;
    int start = -1;
    int end = -1;
    for(int i = s->w/4; i < buffer_len; i++){
        uint16_t s0 = buffer[i-1];
        uint16_t s1 = buffer[i];
        if(s0 < avg && s1 >= avg)
            start = i;
        if(start != -1 && s1 < avg && s0 >= avg){
            end = i;
            break;
        } 
    }

    idx = (start + end) / 2;
    idx -= s->w/4;
    if(idx < 0)
        idx = 0;
    
    int prev;
    for(int i = 0; i < s->w/2; i++){
        int sample_idx = idx;
        if(sample_idx >= buffer_len)
            sample_idx = buffer_len ? buffer_len-1 : 0;
        int sample = y0 - (buffer[sample_idx] / 50);
        if(!i)
            prev = sample;
        if(sample >= prev){
            for(int j = prev; j <= sample; j++)
                pixels[x0 + i + j * s->w] = white;
        } else {
            for(int j = sample; j <= prev; j++)
                pixels[x0 + i + j * s->w] = white;
        }
        idx += 1;
        prev = sample;
    }
}

void sn76489_draw_waves(sn76489_t* sn, SDL_Window** win){
    Uint32 id = SDL_GetWindowID(*win);
    if(!id){
        *win = NULL;
        return;
    }
    SDL_Surface* s = SDL_GetWindowSurface(*win);
    int* pixels = (int*)s->pixels;
    SDL_FillRect(s, NULL, 0);

    for(int y = 0; y < 2; y++){
        for(int x = 0; x < 2; x++){
            int idx =  x + y*2;
            int x0 = x*s->w/2;
            int y0 = y*s->h/2 + 200;
            uint16_t* buf = sn->display_buffers[idx];
            int buf_len = sn->display_idx[idx];
            sn76489_draw_wave(x0, y0, buf, buf_len, s);
        }   
    }

    const int grey = color(100, 100, 100);
    for(int i = 0; i < s->w; i++){
        pixels[i + s->h / 2 * s->w] = grey;
    }
    for(int i = 0; i < s->h; i++)
        pixels[s->w / 2 + i * s->w] = grey;
    
    memset(sn->display_buffers, 0, sizeof(sn->display_buffers));
    memset(sn->display_idx, 0, sizeof(sn->display_idx));

    SDL_UpdateWindowSurface(*win);
}