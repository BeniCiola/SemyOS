#define PIT_COMMAND 0x43
#define PIT_CHANNEL0 0x40

void korolev_tick(void);

void timer_init(uint16_t frecuency){
	uint32_t divisor = 1193182 / frecuency;

	outb(PIT_COMMAND, 0x36);

	uint8_t l = (uint8_t)(divisor & 0xFF);
	uint8_t h = (uint8_t)((divisor >> 8) & 0xFF);

	outb(PIT_CHANNEL0, l);
	outb(PIT_CHANNEL0, h);
}

void timer_handler(void){
	outb(0x20, 0x20);

	korolev_tick();
}
