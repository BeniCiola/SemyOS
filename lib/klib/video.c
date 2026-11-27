void (*put_pixel)(uint8_t, uint8_t, uint8_t, uint8_t) = NULL;
void (*clear_char)(uint8_t, uint8_t, uint8_t, uint8_t) = NULL;
void (*put_char)(uint8_t, uint8_t, uint8_t, uint8_t, uint8_t) = NULL;
void (*put_hex)(uint8_t, uint8_t, uint8_t, uint8_t, uint8_t) = NULL;
void (*print_str)(uint8_t*, uint8_t, uint8_t, uint8_t, uint8_t) = NULL;
void (*clear_screen)(uint8_t, uint8_t, uint8_t, uint8_t) = NULL;

void put_pixel24(uint8_t r, uint8_t g, uint8_t b, uint8_t a);
void clear_char24(uint8_t r, uint8_t g, uint8_t b, uint8_t a);
void put_char24(uint8_t c, uint8_t r, uint8_t g, uint8_t b, uint8_t a);
void put_hex24(uint8_t hex, uint8_t r, uint8_t g, uint8_t b, uint8_t a);
void print_str24(uint8_t *str, uint8_t r, uint8_t g, uint8_t b, uint8_t a);
void clear_screen24(uint8_t r, uint8_t g, uint8_t b, uint8_t a);

void put_pixel32(uint8_t r, uint8_t g, uint8_t b, uint8_t a);
void clear_char32(uint8_t r, uint8_t g, uint8_t b, uint8_t a);
void put_char32(uint8_t c, uint8_t r, uint8_t g, uint8_t b, uint8_t a);
void put_hex32(uint8_t hex, uint8_t r, uint8_t g, uint8_t b, uint8_t a);
void print_str32(uint8_t *str, uint8_t r, uint8_t g, uint8_t b, uint8_t a);
void clear_screen32(uint8_t r, uint8_t g, uint8_t b, uint8_t a);


int16_t print_posx = 0;
int16_t print_posy = 0;


void scroll_screen(void){
	uint32_t row_bytes = 16 * fb.pitch;
	uint32_t move_bytes = (fb.height - 16) * fb.pitch;

	memcpy((void *)framebuffer, (uint8_t *)framebuffer + row_bytes, move_bytes);

	uint32_t *last_row = (uint32_t *)((uint8_t *)framebuffer + move_bytes);
	uint32_t count = row_bytes / 4;

	for(uint32_t i = 0; i < count; i++){
		last_row[i] = 0;
	}
}

void crlf_cursor(void){
	print_posx = 0;
	print_posy += 16;

	if((print_posy + 16) > fb.height){
		scroll_screen();
		print_posy -= 16;
	}
}

void video_init(uint8_t bpp){
	if (bpp == 24) {
		put_pixel = put_pixel24;
		clear_char = clear_char24;
		put_char = put_char24;
		put_hex = put_hex24;
		print_str = print_str24;
		clear_screen = clear_screen24;
	}
	else if (bpp == 32) {
		put_pixel = put_pixel32;
		clear_char = clear_char32;
		put_char = put_char32;
		put_hex = put_hex32;
		print_str = print_str32;
		clear_screen = clear_screen32;
	}
}
