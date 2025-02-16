#include "sn76489.h"

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
    sn->queue_check_counter -= cycles;
    if(sn->queue_check_counter <= 0){
        sn->queue_check_counter += sn->queue_check_reload;
        sn->queued_samples = SDL_GetQueuedAudioSize(sn->audioDev) / sizeof(sample_t);
    }

    sn->push_rate_counter -= cycles;
    if(sn->push_rate_counter <= 0){
        sn->push_rate_counter += sn->push_rate_reload;
        sample_t sample = sn76489_get_sample(sn);
        if(sn->buffer_idx < SAMPLE_BUFFER_SIZE)
            sn->buffer[sn->buffer_idx++] = sample;

        if(sn->buffer_idx && sn->queued_samples < SAMPLE_BUFFER_SIZE){
            SDL_QueueAudio(sn->audioDev, sn->buffer, sn->buffer_idx*sizeof(sample_t));
            sn->buffer_idx = 0;
        }
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
        sample += sn->sample[i]*sn->attenuation[i];
    }

    return sample;
}