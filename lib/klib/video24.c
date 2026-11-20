volatile uint8_t *framebuffer;

void put_pixel24(uint8_t r, uint8_t g, uint8_t b, uint8_t a){
	(void)a;

	uint16_t pitch = *(volatile uint16_t *)0x7E32;

	uint32_t offset = (print_posy * pitch) + (print_posx * 3);

	framebuffer[offset + 0] = b;
	framebuffer[offset + 1] = g;
	framebuffer[offset + 2] = r;
}

void clear_char24(uint8_t r, uint8_t g, uint8_t b, uint8_t a){
	for(uint8_t y = 0; y < 16; y++){
		for(uint8_t x = 0; x < 8; x++){
			put_pixel24(r, g, b, a);
			print_posx++;
		}
		print_posx -= 8;
		print_posy++;
	}
	print_posx -= 8;
	print_posy -= 16;
}

void put_char24(uint8_t l, uint8_t r, uint8_t g, uint8_t b, uint8_t a){
	for(uint8_t byt = 0; byt < 16; byt++){
		uint8_t actual_byte = font8x16[l][byt];

		for(int bit = 7; bit >= 0; bit--){
			uint8_t actual_bit = (actual_byte >> bit) & 1;

			if(actual_bit){
				put_pixel(r, g, b, a);
			}
			print_posx++;
		}
		print_posx-=8;
		print_posy++;
	}
	print_posy -= 16;
}

void put_hex24(uint8_t hex, uint8_t r, uint8_t g, uint8_t b, uint8_t a){
    const char hex_chars[] = "0123456789ABCDEF";

    uint8_t alto = (hex >> 4) & 0x0F;
    uint8_t bajo = hex & 0x0F;

    put_char24(hex_chars[alto], r, g, b, a);
    print_posx += 8;

    put_char24(hex_chars[bajo], r, g, b, a);
    print_posx += 8;
}

void print_str24(uint8_t *str, uint8_t r, uint8_t g, uint8_t b, uint8_t a){
	while(*str != 0x00){
		put_char(*str, r, g, b, a);

		if(print_posx < 1024){
			print_posx += 8;
		} else {
			print_posx = 0;
			print_posy += 16;
		}

		str++;
	}
}

void clear_screen24(uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
	(void)a;

	volatile uint8_t *fb = (volatile uint8_t *)framebuffer;
	uint16_t pitch = *(volatile uint16_t *)0x7E32;

	for (uint32_t y = 0; y < 768; y++) {
		for (uint32_t x = 0; x < 1024; x++) {
			uint32_t offset = y * pitch + x * 3;

			fb[offset + 0] = b;
			fb[offset + 1] = g;
			fb[offset + 2] = r;
		}
	}
}
