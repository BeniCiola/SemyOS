// External Libreries
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

// Before libraries
	#define BUFFER_SIZE	256
	#define BUFFER_MASK	(BUFFER_SIZE - 1)

	typedef struct {
		uint8_t data[BUFFER_SIZE];
		volatile uint32_t head;
		volatile uint32_t tail;
	}KbdRingBuffer;
	KbdRingBuffer kbd_buffer;
	void kbd_ring_buffer_init(KbdRingBuffer *rb);
	bool kbd_ring_buffer_is_full(KbdRingBuffer *rb);
	bool kbd_ring_buffer_is_empty(KbdRingBuffer *rb);
	bool kbd_ring_buffer_push(KbdRingBuffer *rb, uint8_t val);
	uint8_t kbd_ring_buffer_pop(KbdRingBuffer *rb);

// Kernel Libraries
#include "../lib/PIC/pic.c"
#include "../lib/IDT/idt.h"
#include "../lib/IDT/idt.c"

// Memory
#include "../lib/memory/heap.c"
#include "../lib/memory/mfunc.c"

// Drivers
#include "../lib/drivers/timer.c"
#include "../lib/drivers/keyboard.c"

// Controllers for my incredible programs
#include "../lib/process/task.c"
#include "../lib/process/korolev.c"

// Video
#define FONT8x16_IMPLEMENTATION
#include "../lib/hubenchang0515/font8x16.h"
#include "../lib/klib/video.c"
#include "../lib/klib/video24.c"
#include "../lib/klib/video32.c"

// Programs
#include "../programs/shell/list.c"
#include "../programs/shell/shell.c"


extern uint8_t __bss_start;
extern uint8_t __bss_end;

static void bss_init(void){
	uint8_t *p = &__bss_start;

	while(p < &__bss_end){
		*p++ = 0;
	}
}


#define VBE_INFO_ADDR 0x7E00

struct framebuffer_info {
	uint32_t address;
	uint16_t pitch;
	uint16_t width;
	uint16_t height;
	uint8_t  bpp;
};

struct framebuffer_info get_framebuffer_info(void){
	volatile uint8_t *vbe = (volatile uint8_t *)VBE_INFO_ADDR;

	struct framebuffer_info fb;

	fb.pitch = *(volatile uint16_t *)(vbe + 0x10);
	fb.width = *(volatile uint16_t *)(vbe + 0x12);
	fb.height = *(volatile uint16_t *)(vbe + 0x14);
	fb.bpp = *(volatile uint8_t *)(vbe + 0x19);
	fb.address = *(volatile uint32_t *)(vbe + 0x28);

	framebuffer = (volatile uint8_t *)(uintptr_t)fb.address;

	return fb;
}

__attribute__((section(".init"))) void _zazhiganiye(void){
	bss_init();
	init_idt();

	struct framebuffer_info fb = get_framebuffer_info();

	if(fb.address == 0 || fb.pitch == 0 || fb.width == 0 || fb.height == 0) {
		while(1) {
			__asm__ volatile("hlt");
		}
	}

	video_init(fb.bpp);

	korolev_init();

	task_create(shell);

	while(1){
		__asm__ volatile("hlt");
	}
}
