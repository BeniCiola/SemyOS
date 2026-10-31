void (*put_pixel)(uint8_t, uint8_t, uint8_t, uint8_t) = NULL;
void (*clear_char)(uint8_t, uint8_t, uint8_t, uint8_t) = NULL;
void (*put_char)(uint8_t, uint8_t, uint8_t, uint8_t, uint8_t) = NULL;
void (*print_str)(uint8_t*, uint8_t, uint8_t, uint8_t, uint8_t) = NULL;
void (*clear_screen)(uint8_t, uint8_t, uint8_t, uint8_t) = NULL;

void put_pixel24(uint8_t r, uint8_t g, uint8_t b, uint8_t a);
void clear_char24(uint8_t r, uint8_t g, uint8_t b, uint8_t a);
void put_char24(uint8_t c, uint8_t r, uint8_t g, uint8_t b, uint8_t a);
void print_str24(uint8_t *str, uint8_t r, uint8_t g, uint8_t b, uint8_t a);
void clear_screen24(uint8_t r, uint8_t g, uint8_t b, uint8_t a);

void put_pixel32(uint8_t r, uint8_t g, uint8_t b, uint8_t a);
void clear_char32(uint8_t r, uint8_t g, uint8_t b, uint8_t a);
void put_char32(uint8_t c, uint8_t r, uint8_t g, uint8_t b, uint8_t a);
void print_str32(uint8_t *str, uint8_t r, uint8_t g, uint8_t b, uint8_t a);
void clear_screen32(uint8_t r, uint8_t g, uint8_t b, uint8_t a);


int16_t print_posx = 0;
int16_t print_posy = 0;


void video_init(uint8_t bpp){
	if (bpp == 24) {
		put_pixel = put_pixel24;
		clear_char = clear_char24;
		put_char = put_char24;
		print_str = print_str24;
		clear_screen = clear_screen24;
	}
	else if (bpp == 32) {
		put_pixel = put_pixel32;
		clear_char = clear_char32;
		put_char = put_char32;
		print_str = print_str32;
		clear_screen = clear_screen32;
	}
}
