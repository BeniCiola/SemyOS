#define PIT_COMMAND 0x43
#define PIT_CHANNEL0 0x40

void korolev_tick(void);

volatile uint16_t timer_ticks = 0;
volatile uint32_t timer_seconds = 0;

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

	timer_ticks++;
	if(timer_ticks >= 1000){
		timer_seconds++;
		timer_ticks = 0;
	}

	korolev_tick();
}


void sleep_millis(uint32_t ms){
	uint32_t target = (timer_seconds * 1000) + timer_ticks + ms;

	while(((timer_seconds * 1000) + timer_ticks) < target){
		__asm__ volatile("hlt");
	}
}
