// External Libreries
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

// Before things
	#define BUFFER_SIZE	256
	#define BUFFER_MASK	(BUFFER_SIZE - 1)

	#define PTE_CACHE_DISABLE 0x10
	#define PTE_PRESENT 0x01
	#define PTE_RW 0x02

	void vmm_map_page(uint32_t virt, uint32_t phys, uint32_t flags);

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


// Video
#define FONT8x16_IMPLEMENTATION
#include "../lib/hubenchang0515/font8x16.h"
#include "../lib/klib/video.c"
#include "../lib/klib/video24.c"
#include "../lib/klib/video32.c"

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

#include "../lib/drivers/keyboard/keyboard.c"
#include "../lib/drivers/keyboard/ps2.c"

#include "../lib/drivers/disk/init_ahci.c"
#include "../lib/drivers/disk/comm.c"

#include "../lib/drivers/usb/xhci.c"

#include "../lib/drivers/audio/audio_structure.c"
#include "../lib/drivers/audio/audio_init.c"
//#include "../lib/drivers/audio/audio_comm.c"

// Multitasking, Mr. White
#include "../lib/process/task.c"
#include "../lib/process/korolev.c"

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


	print_str((uint8_t *)"Initializing virtual memory...", 255, 255, 255, 255);
	crlf_cursor();

	vmm_init();

	print_str((uint8_t *)"Virtual memory initialized.", 255, 255, 255, 255);
	crlf_cursor();


	print_str((uint8_t *)"Initializing SATA AHCI driver...", 255, 255, 255, 255);
	crlf_cursor();

	search_ahci_driver();

	if(driver_ahci){
		init_ahci();

		ahci_abar_t *abar = (ahci_abar_t *)ahci_controller.abar_physical;
		ahci_detect_ports(abar);
	}

	if(boot_sata_disk != NULL){
		print_str((uint8_t *)"SATA AHCI driver initialized.", 255, 255, 255, 255);
		crlf_cursor();
	} else {
		print_str((uint8_t *)"Direct SATA not found. Trying USB xHCI...", 255, 255, 0, 255);
		crlf_cursor();

		search_xhci_driver();
		init_xhci();

		if(!driver_xhci){
			print_str((uint8_t *)"No SATA or USB xHCI found. Please boot with a valid disk next time.", 255, 55, 55, 255);
			crlf_cursor();
			print_str((uint8_t *)"Entering in sleep mode...", 255, 55, 55, 255);
			while (1) {
				__asm__ volatile("hlt");
			}
		}
	}

	print_str((uint8_t *)"Initializing PS/2 in the madafaka BIOS...", 255, 255, 255, 255);
	crlf_cursor();

	init_ps2_controller();


	print_str((uint8_t *)"Initializing Intel HDA driver...", 255, 255, 255, 255);
	crlf_cursor();

	search_audio_driver();

	if(driver_hda){
		init_audio();
		print_str((uint8_t *)"Intel HDA driver initialized.", 255, 255, 255, 255);
	} else {
		print_str((uint8_t *)"Intel HDA not found.", 255, 100, 0, 255);
	}

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
