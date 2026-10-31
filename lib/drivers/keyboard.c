static const unsigned char keymap_us[128] = {
	0,  27, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
	'\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',
	0,  'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`',   0,
	'\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/',   0,   '*',   0, ' ',
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	'-', 0, 0, 0, '+'
};

void kbd_ring_buffer_init(KbdRingBuffer *rb){
	rb->head = 0;
	rb->tail = 0;
}

bool kbd_ring_buffer_is_empty(KbdRingBuffer *rb){
	return rb->head == rb->tail;
}

bool kbd_ring_buffer_is_full(KbdRingBuffer *rb){
	return ((rb->head + 1) & BUFFER_MASK) == rb->tail;
}


bool kbd_ring_buffer_push(KbdRingBuffer *rb, uint8_t val){
	if (kbd_ring_buffer_is_full(rb)){
		return false;
	}
	rb->data[rb->head] = val;
	rb->head = (rb->head + 1) & BUFFER_MASK;
	return true;
}

uint8_t kbd_ring_buffer_pop(KbdRingBuffer *rb){
	if(kbd_ring_buffer_is_empty(rb)){
		return 0;
	}
	uint8_t val = rb->data[rb->tail];
	rb->tail = (rb->tail + 1) & BUFFER_MASK;
	return val;
}

void keyboard_handler(void){
	uint8_t scancode = inb(0x60);

	if(!(scancode & 0x80) && scancode < 128){
		unsigned char l = keymap_us[scancode];

		if(l != 0){
			kbd_ring_buffer_push(&kbd_buffer, l);
		}
	}

	outb(0x20, 0x20);
}
