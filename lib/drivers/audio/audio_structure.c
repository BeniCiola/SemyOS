#define PTE_PCD 0x10

typedef struct __attribute__((packed)) {
	uint32_t address_low;
	uint32_t address_high;
	uint32_t lenght;
	uint32_t flags;
} hda_bdl_entry_t;

typedef struct __attribute__((packed)) {
	uint8_t  ctl0;
	uint8_t  ctl1;
	uint8_t  ctl2;
	uint8_t  sts;
	uint32_t lpip;
	uint32_t cbl;
	uint16_t lvi;
	uint16_t rsv0;
	uint16_t fifos;
	uint16_t fmt;
	uint32_t rsv1;
	uint32_t bdpl;
	uint32_t bdpu;
} hda_stream_t;

typedef struct {
	uint8_t  bus;
	uint8_t  slot;
	uint8_t  func;
	uint32_t bar0_physical;
} hda_device_t;

typedef struct {
	volatile uint8_t *regs;
	hda_stream_t *out_stream;
	uint32_t bdl_phys;
	hda_bdl_entry_t *bdl;
} hda_driver_t;

hda_device_t audio_controller;
hda_driver_t audio_driver;
uint8_t driver_hda = 0;
