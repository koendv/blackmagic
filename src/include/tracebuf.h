#ifndef TRACEBUF_H
#define TRACEBUF_H
extern bool tracebuf_enabled;
extern void tracebuf_set(uint32_t new_buf_addr, uint32_t new_buf_size, uint32_t new_head_addr, uint32_t new_tail_addr);
extern void tracebuf();
#endif
