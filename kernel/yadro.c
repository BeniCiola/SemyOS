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
#include "../lib/memory/pmm.c"
#include "../lib/memory/vmm.c"
#include "../lib/memory/mfunc.c"

// Drivers
#include "../lib/drivers/timer.c"
#include "../lib/drivers/keyboard.c"
#include "../lib/drivers/disk/init_ahci.c"
#include "../lib/drivers/disk/comm.c"

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

	print_str((uint8_t *)"Video successfully initialized (if you read this).", 255, 255, 255, 255);
	crlf_cursor();

	print_str((uint8_t *)"Initializing physical memory...", 255, 255, 255, 255);
	crlf_cursor();

	pmm_init(32 * 1024 * 1024, 0x00100000);
	pmm_set_region(0x00200000, 30 * 1024 * 1024, 0);

	print_str((uint8_t *)"Physical memory initialized.", 255, 255, 255, 255);
	crlf_cursor();

	print_str((uint8_t *)"Initializing SATA AHCI driver...", 255, 255, 255, 255);
	crlf_cursor();

	search_ahci_driver();

	if(driver_ahci){
		ahci_abar_t *abar = (ahci_abar_t *)ahci_controller.abar_physical;
		uint32_t ports_implemented = abar->pi;

		for(int i = 0; i < 32; i++){
			if(ports_implemented & (1 << i)){
				ahci_port_t *port = &abar->ports[i];

				port->sctl = 1;
				for(volatile int delay = 0; delay < 10000; delay++);
				port->sctl = 0;

				int timeout = 1000000;
				while(((port->ssts & 0x0F) != 3) && (--timeout > 0));

				if(timeout <= 0) continue;

				if(port->sig == 0x00000101){
					ahci_stop_cmd(port)

					uint32_t cmd_list_phys = pmm_alloc_block();
					port->clb = cmd_list_phys;
					port->clbu = 0;

					uint32_t fis_phys = pmm_alloc_block();
					port->fb = fis_phys;
					port->fbu = 0;

					ahci_start_cmd(port);

					break;
				}
			}
		}
	}

	print_str((uint8_t *)"SATA AHCI driver initialized.", 255, 255, 255, 255);
	crlf_cursor();


	print_str((uint8_t *)"Initializing virtual memory...", 255, 255, 255, 255);
	crlf_cursor();

	vmm_init();

	print_str((uint8_t *)"Virtual memory initialized.", 255, 255, 255, 255);
	crlf_cursor();


	print_str((uint8_t *)"Initializing multitasking manager...", 255, 255, 255, 255);
	crlf_cursor();

	korolev_init();

	print_str((uint8_t *)"Multitasking manager initialized.", 255, 255, 255, 255);
	crlf_cursor();


	print_str((uint8_t *)"Triying to run system...", 255, 255, 255, 255);
	crlf_cursor();

	sleep_millis(1500);

	clear_screen(0, 0, 0, 0);
	print_posx = 0;
	print_posy = 0;

	print_str((uint8_t *)"Welcome to SemyOS: A system for engineers.", 255, 255, 255, 255);
	crlf_cursor();

	task_create(shell);

	while(1){
		__asm__ volatile("hlt");
	}
}
