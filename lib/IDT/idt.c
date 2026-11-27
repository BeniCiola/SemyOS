#include "idt.h"

extern void syscall_handler_asm(void);

idt_entry_t idt[256];
idt_ptr_t idt_reg;

extern void idt_flush(uint32_t idt_ptr_address);
extern void timer_init(uint16_t frecuency);

extern void timer_handler_asm(void);
extern void keyboard_handler_asm(void);

void idt_set_gate(uint8_t num, uint32_t base, uint16_t sel, uint8_t flags){
	idt[num].base_low = base & 0xFFFF;
	idt[num].base_high = (base >> 16) & 0xFFFF;

	idt[num].sel = sel;
	idt[num].always0 = 0;
	idt[num].flags = flags;
}

void init_idt(void){
	idt_reg.limit = (sizeof(idt_entry_t) * 256) - 1;
	idt_reg.base = (uint32_t)&idt;

	for (int i = 0; i < 256; i++){
		idt_set_gate(i, 0, 0, 0);
	}

	pic_remap();

	idt_set_gate(32, (uint32_t)timer_handler_asm, 0x08, 0x8E);
	idt_set_gate(33, (uint32_t)keyboard_handler_asm, 0x08, 0x8E);
	idt_set_gate(0x80, (uint32_t)syscall_handler_asm, 0x08, 0x8E);

	idt_flush((uint32_t)&idt_reg);

	timer_init(1000);
	kbd_ring_buffer_init(&kbd_buffer);

	__asm__ volatile("sti");
}
