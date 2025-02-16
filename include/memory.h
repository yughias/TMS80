#ifndef __MEMORY_H__
#define __MEMORY_H__

#include "z80.h"

uint8_t sg_sc_readMemory(z80_t* z80, uint16_t addr);
void sg_sc_writeMemory(z80_t* z80, uint16_t addr, uint8_t byte);
uint8_t sg_sc_mirrored_readMemory(z80_t* z80, uint16_t addr);
void sg_sc_ram_adapter_writeMemory(z80_t* z80, uint16_t addr, uint8_t byte);

uint8_t sms_readMemory(z80_t* z80, uint16_t addr);
void sms_writeMemory(z80_t* z80, uint16_t addr, uint8_t byte);
void sms_no_mapper_writeMemory(z80_t* z80, uint16_t addr, uint8_t byte);

uint8_t console_readIO(z80_t* z80, uint16_t addr);
void console_writeIO(z80_t* z80, uint16_t addr, uint8_t byte);

#endif