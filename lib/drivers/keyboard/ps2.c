static inline void ps2_wait_write(void) {
    while (inb(0x64) & 0x02);
}

static inline void ps2_wait_read(void) {
    while (!(inb(0x64) & 0x01));
}

void init_ps2_controller(void) {
    ps2_wait_write();
    outb(0x64, 0xAD);
    ps2_wait_write();
    outb(0x64, 0xA7);

    while (inb(0x64) & 0x01) {
        inb(0x60);
    }

    ps2_wait_write();
    outb(0x64, 0x20);

    int timeout = 100000;
    while (!(inb(0x64) & 0x01) && timeout-- > 0);

    if (timeout > 0) {
        uint8_t config = inb(0x60);

        config |= (1 << 0);
        config &= ~(1 << 4);
        config |= (1 << 6);

        ps2_wait_write();
        outb(0x64, 0x60);
        ps2_wait_write();
        outb(0x60, config);
    }

    ps2_wait_write();
    outb(0x64, 0xAE);

    ps2_wait_write();
    outb(0x60, 0xF4);
}
