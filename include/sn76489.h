#ifndef __SN76489_H__
#define __SN76489_H__

#include "SDL2/SDL.h"

#include <stdint.h>
#include <stdbool.h>

#define SAMPLES_PER_CALL 512
#define SAMPLE_BUFFER_SIZE 4096

#define SN76489_SMS_TAPPED_BITS 0x09
#define SN76489_GENERIC_TAPPED_BITS 0x03

#define SN76489_SMS_LFSR_LEN 15
#define SN76489_GENERIC_LFSR_LEN 14

#define SN76489_SET_TYPE(sn, mode) \
sn->lfsr.feedback_bit = SN76489_ ## mode ## _LFSR_LEN; \
sn->lfsr.tapped_bits = SN76489_ ## mode ## _TAPPED_BITS; \
sn->lfsr.reg = (1 << sn->lfsr.feedback_bit)

typedef uint16_t sample_t;

typedef struct lfsr_t {
    uint16_t reg;
    uint8_t feedback_bit;
    uint16_t tapped_bits;
    bool white_noise;
} lfsr_t;

typedef struct sn76489_t {
    SDL_AudioDeviceID audioDev;
    SDL_AudioSpec audioSpec;
    int push_rate_reload;
    int push_rate_counter;
    int queue_check_counter;
    int queue_check_reload;
    int queued_samples;
    sample_t buffer[SAMPLE_BUFFER_SIZE];
    int buffer_idx;

    int counter[4];
    int freq[4];
    int attenuation[4];
    bool sample[4];
    
    lfsr_t lfsr;
    bool lfsr_freq2_counter;

    int latched_idx;
} sn76489_t;

void sn76489_push_sample(sn76489_t* sn, int cycles);
sample_t sn76489_get_sample(sn76489_t* sn);
void sn76489_update(sn76489_t* sn, int cycles);
void sn76489_write(sn76489_t* sn, uint8_t byte);


#endif