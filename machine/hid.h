// See LICENSE for license details.

#ifndef HID_HEADER_H
#define HID_HEADER_H

#include <stdint.h>

enum { clint_base_addr = 0x02000000,
        plic_base_addr = 0x0c000000,
        bram_base_addr = 0x40000000,
          sd_base_addr = 0x40010000,        
          sd_bram_addr = 0x40018000,
         eth_base_addr = 0x40020000,
        keyb_base_addr = 0x40030000,
        uart_base_addr = 0x40034000,
         vga_base_addr = 0x40038000,
        ddr_base_addr  = 0x80000000
      };

extern size_t hid;

extern void query_hid(uintptr_t fdt);
extern void hid_send(uint8_t);
extern void hid_send_irq(uint8_t);
extern void hid_send_string(const char *str);
extern void hid_send_buf(const char *buf, const int32_t len);
extern uint8_t hid_recv();
extern uint8_t hid_read_irq();
extern uint8_t hid_check_read_irq();
extern void hid_enable_read_irq();
extern void hid_disable_read_irq();

#endif
