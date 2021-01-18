#ifndef DEBUG_H_
#define DEBUG_H_

uint32_t format_hex(char* outbuf, uint32_t bufsize, uint32_t inum, uint32_t mindigits);
void printhex(char* text, uint32_t hex);
void sendbytes_itm(char* text, uint32_t len);
void printhex_itm(char* text, uint32_t hex);
void init_swo(void);

#endif /* DEBUG_H_ */
