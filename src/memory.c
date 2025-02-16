#include "memory.h"
#include "console.h"

#include <stdio.h>
#include <stdlib.h>

uint8_t sg_sc_readMemory(z80_t* z80, uint16_t addr){
    console_t* console = z80->master;

    if(addr < 0xC000 && addr < console->cartridge_size)
        return console->cartridge[addr];

    return console->RAM[addr];
}

uint8_t sg_sc_mirrored_readMemory(z80_t* z80, uint16_t addr){
    console_t* console = z80->master;

    if(addr < 0xC000)
        return console->cartridge[addr % console->cartridge_size];

    return console->RAM[addr];
}

void sg_sc_writeMemory(z80_t* z80, uint16_t addr, uint8_t byte){
    console_t* console = z80->master;

    console->RAM[addr] = byte;
}

void sg_sc_ram_adapter_writeMemory(z80_t* z80, uint16_t addr, uint8_t byte){
    console_t* console = z80->master;

    if(addr >= 0x2000 && addr < 0x4000){
        console->cartridge[addr] = byte;
        return;
    }

    console->RAM[addr] = byte;
}

uint8_t sms_bios_readMemory(z80_t* z80, uint16_t addr){
    console_t* console = z80->master;

    if(addr < 0xC000){
        uint8_t bank_idx = addr >> 14;
        uint32_t banked_addr = ((addr & 0x3FFF) + console->banks[bank_idx]*0x4000);
        return console->bios[banked_addr % console->bios_size];
    }

    return console->RAM[addr & ((1 << 13) - 1)];
}

void sms_bios_writeMemory(z80_t* z80, uint16_t addr, uint8_t byte){
    console_t* console = z80->master;

    if(addr == 0xFFFC){
        console->ram_bank = byte;
    }
    if(addr >= 0xFFFD && addr <= 0xFFFF)
        console->banks[addr - 0xFFFD] = byte;

    if(addr >= 0xC000)
        console->RAM[addr & ((1 << 13) - 1)] = byte;
}

uint8_t sms_readMemory(z80_t* z80, uint16_t addr){
    console_t* console = z80->master;
    
    if(addr < 0xC000){
        uint8_t bank_idx = addr >> 14;
        if((console->ram_bank & (1 << 3)) && addr >= 0x8000)
            return console->RAM[(1 << 13) + (addr - 0x8000)];
        if(addr < 1024)
            return console->cartridge[addr];
        uint32_t banked_addr = ((addr & 0x3FFF) + console->banks[bank_idx]*0x4000);
        return console->cartridge[banked_addr % console->cartridge_size];
    }

    return console->RAM[addr & ((1 << 13) - 1)];
}

void sms_writeMemory(z80_t* z80, uint16_t addr, uint8_t byte){
    console_t* console = z80->master;

    if(addr == 0xFFFC){
        console->ram_bank = byte;
    }
    if(addr >= 0xFFFD && addr <= 0xFFFF)
        console->banks[addr - 0xFFFD] = byte;

    if((console->ram_bank & (1 << 3)) && addr >= 0x8000 && addr < 0xC000){
        console->RAM[(1 << 13) + (addr - 0x8000)] = byte;
    }

    if(addr >= 0xC000)
        console->RAM[addr & ((1 << 13) - 1)] = byte;
}

uint8_t console_readIO(z80_t* z80, uint16_t addr){
    console_t* console = z80->master;
    vdp_t* vdp = &console->vdp;
    addr &= 0xFF;

    if(addr >= 0x40 && addr <= 0x7F){
        if(!(addr & 1))
            return vdp_get_v_counter(vdp);
    }

    if(addr >= 0xC0 && addr <= 0xFF){
        if(addr & 1)
            return console_get_keypad_b(console);
        else
            return console_get_keypad_a(console);
    }

    if(addr >= 0x80 && addr <= 0xBF){
        if(addr & 1){
            return vdp_read_status_register(vdp, z80);
        } else
            return vdp_read_from_data_port(vdp);
    }

    return 0xFF;
}

void console_writeIO(z80_t* z80, uint16_t addr, uint8_t byte){
    console_t* console = z80->master;
    vdp_t* vdp = &console->vdp;
    sn76489_t* apu = &console->apu;
    addr &= 0xFF;

    if(console->type == SMS && addr == 0x3E){
        if((byte & (1 << 3))){
            console->z80.readMemory = sms_readMemory;
            console->z80.writeMemory = sms_writeMemory;
        } else {
            console->z80.readMemory = sms_bios_readMemory;
            console->z80.writeMemory = sms_bios_writeMemory;
        }
    }

    if(console->has_keyboard && addr == 0xDE)
        console->keypad_reg = byte;

    if(addr >= 0x40 && addr <= 0x7F)
        sn76489_write(apu, byte);

    if(addr >= 0x80 && addr <= 0xBF){
        if(addr & 1){
            vdp_write_to_control_port(vdp, byte);
        } else
            vdp_write_to_data_port(vdp, byte);
    }
}