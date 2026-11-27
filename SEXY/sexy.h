#ifndef SEXY_H
#define SEXY_H

#define SEXY_MAGIC 0x59584553

typedef struct __attribute__((packed)) {
	uint32_t magic;
	uint32_t load_addr;
	uint32_t entry_point;
	uint32_t code_size;
	uint32_t bss_size;
	uint8_t  reserved[12];
} sexy_header_t;

#define SYS_EXIT 0
#define SYS_PRINT 1
#define SYS_SETCURSOR 2

#endif
