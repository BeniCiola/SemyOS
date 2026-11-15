#define PIC1_CMD 0x20
#define PIC1_DATA 0x21
#define PIC2_CMD 0xA0
#define PIC2_DATA 0xA1


inline void outb(uint16_t port, uint8_t val){
	__asm__ volatile ( "outb %b0, %w1" : : "a"(val), "Nd"(port) );
}

inline uint8_t inb(uint16_t port){
	uint8_t ret;
	__asm__ volatile ( "inb %w1, %b0" : "=a"(ret) : "Nd"(port) );
	return ret;
}

inline void io_wait(void){
	outb(0x80, 0);
}

void pic_remap(void){
	outb(PIC1_CMD, 0x11); io_wait();
	outb(PIC2_CMD, 0x11); io_wait();

	outb(PIC1_DATA, 0x20); io_wait();
	outb(PIC2_DATA, 0x28); io_wait();

	outb(PIC1_DATA, 0x04); io_wait();
	outb(PIC2_DATA, 0x02); io_wait();

	outb(PIC1_DATA, 0x01); io_wait();
	outb(PIC2_DATA, 0x01); io_wait();

	outb(PIC1_DATA, 0xFC); io_wait();
	outb(PIC2_DATA, 0xFF); io_wait();
}


static inline uint32_t pci_read_config(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset) {
	uint32_t address = (uint32_t)(
		(1 << 31) |
		(bus << 16) |
		(slot << 11) |
		(func << 8) |
		(offset & 0xFC)
	);

	uint32_t data;

	__asm__ volatile (
		"outl %0, %%dx\n\t"
		"movw $0xCFC, %%dx\n\t"
		"inl %%dx, %0"
		: "=a" (data)
		: "a" (address), "d" (0xCF8)
	);

	return data;
}

static inline void pci_write_config(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset, uint32_t value) {
	uint32_t address = (uint32_t)(
		(1u << 31) |
		((uint32_t)bus << 16) |
		((uint32_t)slot << 11) |
		((uint32_t)func << 8) |
		(offset & 0xFC)
	);

	__asm__ volatile (
		"outl %0, %%dx"
		:
		: "a" (address), "d" ((uint16_t)0xCF8)
	);

	__asm__ volatile (
		"outl %0, %%dx"
		:
		: "a" (value), "d" ((uint16_t)0xCFC)
	);
}
