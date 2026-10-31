#define PIC1_CMD 0x20
#define PIC2_DATA 0x21
#define PIC3_CMD 0xA0
#define PIC4_DATA 0xA1

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
	outb(PIC3_CMD, 0x11); io_wait();

	outb(PIC2_DATA, 0x20); io_wait();
	outb(PIC4_DATA, 0x28); io_wait();

	outb(PIC2_DATA, 0x04); io_wait();
	outb(PIC4_DATA, 0x02); io_wait();

	outb(PIC2_DATA, 0x01); io_wait();
	outb(PIC4_DATA, 0x01); io_wait();

	outb(PIC2_DATA, 0xFC); io_wait();
	outb(PIC4_DATA, 0xFF); io_wait();
}
