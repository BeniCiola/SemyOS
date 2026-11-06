#define PIC1_CMD 0x20
#define PIC1_DATA 0x21
#define PIC2_CMD 0xA0
#define PIC2_DATA 0xA1


typedef struct {
	uint8_t bus;
	uint8_t slot;
	uint8_t func;
	uint32_t abar_physical;
} ahci_device_t;


extern ahci_device_t ahci_controller;
extern uint8_t driver_ahci;

ahci_device_t ahci_controller;
uint8_t driver_ahci = 0;


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


void search_ahci_driver(void) {
	driver_ahci = 0;
	ahci_controller.abar_physical = 0;

	for (int bus = 0; bus < 256; bus++) {
		for (int slot = 0; slot < 32; slot++) {
			for (int func = 0; func < 8; func++) {
				uint32_t reg0 = pci_read_config(bus, slot, func, 0x00);
				uint16_t vendor_id = (uint16_t)(reg0 & 0xFFFF);

				if (vendor_id == 0xFFFF || vendor_id == 0x0000) {
					if (func == 0) break;
					continue;
				}

				uint32_t reg2 = pci_read_config(bus, slot, func, 0x08);
				uint8_t class_code = (uint8_t)(reg2 >> 24);
				uint8_t subclass   = (uint8_t)(reg2 >> 16);
				uint8_t prog_if    = (uint8_t)(reg2 >> 8);

				if (class_code == 0x01 && subclass == 0x06 && prog_if == 0x01) {
					uint32_t bar5 = pci_read_config(bus, slot, func, 0x24);

					if (bar5 & 0x1) {
						continue;
					}

					uint32_t clear_address = bar5 & 0xFFFFF000;

					if (clear_address == 0) {
						continue;
					}

					ahci_controller.bus = bus;
					ahci_controller.slot = slot;
					ahci_controller.func = func;
					ahci_controller.abar_physical = clear_address;

					driver_ahci = 1;
					return;
				}
			}
		}
	}
}

void init_ahci(void) {
    if (!driver_ahci) return;

    uint8_t bus = ahci_controller.bus;
    uint8_t slot = ahci_controller.slot;
    uint8_t func = ahci_controller.func;

    uint32_t cmd_reg = pci_read_config(bus, slot, func, 0x04);
    cmd_reg |= (1 << 1) | (1 << 2);
    pci_write_config(bus, slot, func, 0x04, cmd_reg);

    volatile uint32_t *abar = (volatile uint32_t *)ahci_controller.abar_physical;

    volatile uint32_t *ghc = &abar[1];
    *ghc |= (1u << 31);

    volatile uint32_t *pi = &abar[3];
    uint32_t ports_implemented = *pi;

    (void)ports_implemented;
}
