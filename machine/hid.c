// See LICENSE for license details.

#include <stdint.h>
#include <string.h>
#include "mtrap.h"
#include "hid.h"

#define SCROLL

volatile uint32_t *const sd_base = (uint32_t *)sd_base_addr;

void hid_send_irq(uint8_t data)
{

}

void hid_send(uint8_t data)
{
  volatile uint32_t *const uart_base = (uint32_t *)uart_base_addr;
  *uart_base = data;
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
  volatile uint32_t * const keyb_base = (volatile uint32_t*)keyb_base_addr;
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

size_t err = 0, eth = 0, ddr = 0, rom = 0, bram = 0, intc = 0, clin = 0;

void query_hid(uintptr_t fdt)
{
  ddr = ddr_base_addr;
  bram = bram_base_addr;
  eth = eth_base_addr;
}
