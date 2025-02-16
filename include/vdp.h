#ifndef __VDP_H__
#define __VDP_H__

#include "z80.h"

#include <stdint.h>

#define SCREEN_WIDTH 256
#define SCREEN_HEIGHT 192

#define VBLANK_CYCLES (SCREEN_HEIGHT*CYCLES_PER_LINE)

#define VRAM_SIZE 0x4000
#define CRAM_SIZE 32

typedef enum VDP_REGION {REGION_NTSC, REGION_PAL} VDP_REGION;

typedef struct vdp_t {
    VDP_REGION region;
    uint8_t VRAM[VRAM_SIZE];
    uint8_t CRAM[CRAM_SIZE];
    uint8_t buffer;
    uint16_t control_port;
    uint16_t vram_address;
    bool control_port_flag;
    bool cram_dst;
    int v_counter;
    uint8_t scroll_y;
    uint8_t status_reg;
    uint8_t line_reg;
    uint8_t regs[16];
} vdp_t;

void vdp_write_to_control_port(vdp_t* vdp, uint8_t byte);
uint8_t vdp_read_from_data_port(vdp_t* vdp);
void vdp_write_to_data_port(vdp_t* vdp, uint8_t byte);
uint8_t vdp_read_status_register(vdp_t* vdp, z80_t* z80);
void vdp_fire_interrupt(vdp_t* vdp, z80_t* z80, bool is_vblank);
uint8_t vdp_get_v_counter(vdp_t* vdp);

void vdp_render_line(vdp_t* vdp, int line);

#endif