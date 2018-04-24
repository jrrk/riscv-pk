// See LICENSE for license details.

#include <stdint.h>
#include <string.h>
#include "mtrap.h"
#include "hid.h"

volatile uint32_t *const sd_base = (uint32_t *)0x41010000;
volatile uint8_t *hid_vga_ptr;
static int addr_int;

void hid_console_putchar(unsigned char ch)
{
  switch(ch)
    {
    case 8: case 127: if (addr_int & 127) hid_vga_ptr[--addr_int] = ' '; break;
    case 13: addr_int = addr_int & -128; break;
    case 10:
      {
        int lmt = (addr_int|127)+1;
#ifdef SCROLL      
        while (addr_int < lmt) hid_vga_ptr[(addr_int++)] = ' ';
#else
        // For simulation sanity
        hid_vga_ptr[lmt] = ch;
        addr_int = lmt;
#endif
        break;
      }
    default: hid_vga_ptr[addr_int++] = ch;
    }
  if (addr_int >= 4096-128)
    {
#ifdef SCROLL      
      // this is where we scroll
      for (addr_int = 0; addr_int < 4096; addr_int++)
        if (addr_int < 4096-128)
          hid_vga_ptr[addr_int] = hid_vga_ptr[addr_int+128];
        else
          hid_vga_ptr[addr_int] = ' ';
      addr_int = 4096-256;
#else
      addr_int &= 127;
#endif      
    }
}

void hid_init(void *base) {
  //  extern volatile uint32_t *sd_base;
  //  sd_base = (uint32_t *)(0x00010000 + (char *)base);
  hid_vga_ptr = (uint8_t *)base;
  addr_int = 4096-256;
}

void hid_send_irq(uint8_t data)
{

}

void hid_send(uint8_t data)
{
  hid_console_putchar(data);
}

void hid_send_string(const char *str) {
  while (*str) hid_send(*str++);
}

void hid_send_buf(const char *buf, const int32_t len)
{
  int32_t i;
  for (i=0; i<len; i++) hid_send(buf[i]);
}

static inline uint8_t cr2lf(uint8_t ch)
{
  if (ch == '\r') ch = '\n'; /* translate CR to LF, because nobody else will */
  return ch;
}

uint8_t hid_recv()
{
  volatile uint32_t * const keyb_base = (volatile uint32_t*)0x41000000;
  uint32_t key = keyb_base[0];
  if ((1<<16) & ~key) /* FIFO not empty */
    {
      int ch;
      *keyb_base = 0;
      ch = (*keyb_base >> 8) & 127; /* strip off the scan code (default ascii code is UK) */
      return cr2lf(ch);
    }
  
  return 0;
}

// IRQ triggered read
uint8_t hid_read_irq() {
  return 0;
}

// check hid IRQ for read
uint8_t hid_check_read_irq() {
  return 0;
}

// enable hid read IRQ
void hid_enable_read_irq() {

}

size_t err = 0, eth = 0, ddr = 0, rom = 0, bram = 0, intc = 0, clin = 0, hid = 0;

void query_hid(uintptr_t fdt)
{
  uint64_t unknown = 0;
  ddr = 0x80000000;
  bram = 0x40000000;
  eth = 0x41020000;
  hid = 0x41008000;
  if (hid)
    {
      hid_init((void *)hid);
      printm("Video memory start %p\n", hid_vga_ptr);
      if (ddr) printm("DDR memory start %p\n", (void *)ddr);
    }
}
