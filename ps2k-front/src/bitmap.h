#ifndef BITMAP_H_
#define BITMAP_H_

uint8_t bitmap_render_pixel(uint8_t* dispbuf, uint32_t x, uint32_t y);
void bitmap_render_full(void (*out)(const uint8_t*, uint32_t), uint8_t* dispbuf);

#endif /* BITMAP_H_ */
