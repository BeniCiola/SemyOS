typedef volatile struct {
	uint8_t  caplength;
	uint8_t  rsv0;
	uint16_t hciversion;
	uint32_t hcsparams1;
	uint32_t hcsparams2;
	uint32_t hcsparams3;
	uint32_t hccparams1;
	uint32_t dboff;
	uint32_t rtsoff;
	uint32_t hccparams2;
} __attribute__((packed)) xhci_cap_regs_t;

typedef volatile struct {
	uint32_t usbcmd;
	uint32_t usbsts;
	uint32_t pagesize;
	uint32_t rsv0[2];
	uint32_t dnctrl;
	uint64_t crcr;
	uint32_t rsv1[4];
	uint64_t dcbaap;
	uint32_t config;
} __attribute__((packed)) xhci_op_regs_t;

typedef struct {
	uint8_t bus;
	uint8_t slot;
	uint8_t func;
	uint32_t mmio_phys;
	xhci_cap_regs_t *cap;
	xhci_op_regs_t  *op;
	uint8_t max_ports;
	uint8_t max_slots;
} xhci_controller_t;

xhci_controller_t xhci;
uint8_t driver_xhci = 0;


typedef struct {
	uint64_t parameter;
	uint32_t status;
	uint32_t control;
} __attribute__((packed)) xhci_trb_t;

typedef struct {
	uint64_t ring_segment_base;
	uint32_t ring_segment_size;
	uint32_t rsv0;
} __attribute__((packed)) xhci_erst_entry_t;

typedef volatile struct {
	uint32_t iman;
	uint32_t imod;
	uint32_t erstsz;
	uint32_t rsv0;
	uint64_t erstba;
	uint64_t erdp;
} __attribute__((packed)) xhci_interrupter_regs_t;

typedef volatile struct {
	uint32_t portsc;
	uint32_t portpmsc;
	uint32_t portli;
	uint32_t porthlpmc;
} __attribute__((packed)) xhci_port_regs_t;

uint64_t *dcbaa = NULL;
xhci_trb_t *cmd_ring = NULL;
uint32_t cmd_ring_idx = 0;
uint8_t cmd_ring_cycle = 1;

xhci_trb_t *event_ring = NULL;
uint32_t event_ring_idx = 0;
uint8_t event_ring_cycle = 1;

xhci_interrupter_regs_t *interrupter0 = NULL;
uint32_t *doorbells = NULL;

void xhci_ring_doorbell(uint8_t slot, uint8_t target) {
	doorbells[slot] = (uint32_t)target;
}


void xhci_send_command(uint64_t param, uint32_t status, uint32_t control) {
	cmd_ring[cmd_ring_idx].parameter = param;
	cmd_ring[cmd_ring_idx].status = status;
	cmd_ring[cmd_ring_idx].control = (control & ~1) | cmd_ring_cycle;

	cmd_ring_idx++;
	if (cmd_ring_idx == 255) {
		cmd_ring[255].parameter = (uint64_t)(uintptr_t)cmd_ring;
		cmd_ring[255].status = 0;
		cmd_ring[255].control = (6 << 10) | (1 << 1) | cmd_ring_cycle;
		cmd_ring_idx = 0;
		cmd_ring_cycle ^= 1;
	}

	xhci_ring_doorbell(0, 0);
}

int xhci_wait_event(xhci_trb_t *out_trb) {
	int timeout = 500;
	while (timeout-- > 0) {
		xhci_trb_t *ev = &event_ring[event_ring_idx];
		if ((ev->control & 1) == event_ring_cycle) {
			if (out_trb) *out_trb = *ev;

			event_ring_idx++;
			if (event_ring_idx == 256) {
				event_ring_idx = 0;
				event_ring_cycle ^= 1;
			}

			uint64_t next_erdp = (uint64_t)(uintptr_t)&event_ring[event_ring_idx];
			interrupter0->erdp = next_erdp | (1 << 3);
			return 1;
		}
		sleep_millis(1);
	}
	return 0;
}

void xhci_check_ports(void);

void xhci_setup_rings(void) {
	xhci.op->config = xhci.max_slots;

	uint32_t dcbaa_phys = pmm_alloc_block();
	for (int i = 0; i < 4096 / 4; i++) ((uint32_t *)dcbaa_phys)[i] = 0;
	dcbaa = (uint64_t *)dcbaa_phys;

	uint32_t hcs2 = xhci.cap->hcsparams2;
	uint32_t max_scratchpad = ((hcs2 >> 21) & 0x1F) | (((hcs2 >> 27) & 0x1F) << 5);
	if (max_scratchpad > 0) {
		uint32_t sp_array_phys = pmm_alloc_block();
		uint64_t *sp_array = (uint64_t *)sp_array_phys;
		for (uint32_t i = 0; i < max_scratchpad; i++) {
			uint32_t buf = pmm_alloc_block();
			for (int j = 0; j < 4096 / 4; j++) ((uint32_t *)buf)[j] = 0;
			sp_array[i] = (uint64_t)buf;
		}
		dcbaa[0] = (uint64_t)sp_array_phys;
	}

	xhci.op->dcbaap = (uint64_t)dcbaa_phys;

	uint32_t cmd_phys = pmm_alloc_block();
	for (int i = 0; i < 4096 / 4; i++) ((uint32_t *)cmd_phys)[i] = 0;
	cmd_ring = (xhci_trb_t *)cmd_phys;
	cmd_ring_idx = 0;
	cmd_ring_cycle = 1;
	xhci.op->crcr = (uint64_t)cmd_phys | 1;

	uint32_t ev_phys = pmm_alloc_block();
	uint32_t erst_phys = pmm_alloc_block();
	for (int i = 0; i < 4096 / 4; i++) {
		((uint32_t *)ev_phys)[i] = 0;
		((uint32_t *)erst_phys)[i] = 0;
	}
	event_ring = (xhci_trb_t *)ev_phys;
	event_ring_idx = 0;
	event_ring_cycle = 1;

	xhci_erst_entry_t *erst = (xhci_erst_entry_t *)erst_phys;
	erst[0].ring_segment_base = (uint64_t)ev_phys;
	erst[0].ring_segment_size = 256;


	uintptr_t runtime_base = xhci.mmio_phys + (xhci.cap->rtsoff & ~0x1F);
	interrupter0 = (xhci_interrupter_regs_t *)(runtime_base + 0x20);
	interrupter0->erstsz = 1;
	interrupter0->erdp = (uint64_t)ev_phys;
	interrupter0->erstba = (uint64_t)erst_phys;

	doorbells = (uint32_t *)(xhci.mmio_phys + (xhci.cap->dboff & ~0x03));

	xhci.op->usbcmd |= (1 << 0);
	int timeout = 100;
	while ((xhci.op->usbsts & (1 << 0)) && timeout-- > 0) {
		sleep_millis(1);
	}

	xhci_send_command(0, 0, (23 << 10));
	xhci_trb_t res;
	if (xhci_wait_event(&res)) {
		uint8_t comp_code = (res.status >> 24) & 0xFF;
		print_str((uint8_t *)"xHCI Ring OK! Status: ", 55, 255, 55, 255);
		put_hex(comp_code, 255, 255, 255, 255);
		crlf_cursor();
	} else {
		print_str((uint8_t *)"xHCI Ring timeout!", 255, 55, 55, 255);
		crlf_cursor();
	}

	xhci_check_ports();
}



void search_xhci_driver(void) {
	driver_xhci = 0;
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

				if (class_code == 0x0C && subclass == 0x03 && prog_if == 0x30) {
					uint32_t bar0 = pci_read_config(bus, slot, func, 0x10);
					if (bar0 & 0x1) continue;

					uint32_t base_addr = bar0 & 0xFFFFFFF0;
					if (base_addr == 0) continue;

					xhci.bus = bus;
					xhci.slot = slot;
					xhci.func = func;
					xhci.mmio_phys = base_addr;
					driver_xhci = 1;
					return;
				}
			}
		}
	}
}

void init_xhci(void) {
	if (!driver_xhci) {
		print_str((uint8_t *)"xHCI controller not found.", 255, 55, 55, 255);
		crlf_cursor();
		return;
	}

	uint32_t cmd = pci_read_config(xhci.bus, xhci.slot, xhci.func, 0x04);
	cmd |= (1 << 1) | (1 << 2);
	pci_write_config(xhci.bus, xhci.slot, xhci.func, 0x04, cmd);

	for (uint32_t offset = 0; offset < 0x4000; offset += 0x1000) {
		vmm_map_page(xhci.mmio_phys + offset, xhci.mmio_phys + offset, PTE_PRESENT | PTE_RW | PTE_CACHE_DISABLE);
	}

	xhci.cap = (xhci_cap_regs_t *)xhci.mmio_phys;
	xhci.op  = (xhci_op_regs_t *)(xhci.mmio_phys + xhci.cap->caplength);

	xhci.max_slots = xhci.cap->hcsparams1 & 0xFF;
	xhci.max_ports = (xhci.cap->hcsparams1 >> 24) & 0xFF;

	xhci.op->usbcmd &= ~(1 << 0);
	int timeout = 100;
	while (!(xhci.op->usbsts & (1 << 0)) && timeout-- > 0) {
		sleep_millis(1);
	}

	xhci.op->usbcmd |= (1 << 1);
	timeout = 100;
	while ((xhci.op->usbcmd & (1 << 1)) && timeout-- > 0) {
		sleep_millis(1);
	}

	timeout = 100;
	while ((xhci.op->usbsts & (1 << 11)) && timeout-- > 0) {
		sleep_millis(1);
	}

	print_str((uint8_t *)"xHCI initialized! Ports: ", 55, 255, 55, 255);
	put_hex(xhci.max_ports, 255, 255, 255, 255);
	crlf_cursor();

	xhci_setup_rings();
}

uint8_t msd_slot_id = 0;


typedef struct {
    uint32_t drop_flags;
    uint32_t add_flags;
    uint32_t rsv0[6];
} __attribute__((packed)) xhci_input_ctrl_ctx_t;

typedef struct {
    uint32_t field[8];
} __attribute__((packed)) xhci_slot_ctx_t;

typedef struct {
    uint32_t field[8];
} __attribute__((packed)) xhci_ep_ctx_t;

xhci_trb_t *ep0_ring = NULL;
uint32_t ep0_ring_idx = 0;
uint8_t ep0_ring_cycle = 1;

void xhci_address_device(uint8_t slot_id, uint8_t port_num, uint8_t speed) {
    uint32_t ctx_size = (xhci.cap->hccparams1 & (1 << 2)) ? 64 : 32;

    uint32_t dev_ctx_phys = pmm_alloc_block();
    for (int i = 0; i < 4096 / 4; i++) ((uint32_t *)dev_ctx_phys)[i] = 0;
    dcbaa[slot_id] = (uint64_t)dev_ctx_phys;

    uint32_t ep0_phys = pmm_alloc_block();
    for (int i = 0; i < 4096 / 4; i++) ((uint32_t *)ep0_phys)[i] = 0;
    ep0_ring = (xhci_trb_t *)ep0_phys;
    ep0_ring_idx = 0;
    ep0_ring_cycle = 1;

    uint32_t in_ctx_phys = pmm_alloc_block();
    for (int i = 0; i < 4096 / 4; i++) ((uint32_t *)in_ctx_phys)[i] = 0;

    xhci_input_ctrl_ctx_t *ctrl = (xhci_input_ctrl_ctx_t *)in_ctx_phys;
    ctrl->add_flags = (1 << 0) | (1 << 1);

    xhci_slot_ctx_t *slot_ctx = (xhci_slot_ctx_t *)(in_ctx_phys + ctx_size);
    slot_ctx->field[0] = ((uint32_t)speed << 20) | (1 << 27);
    slot_ctx->field[1] = ((uint32_t)port_num << 16);

    xhci_ep_ctx_t *ep0_ctx = (xhci_ep_ctx_t *)(in_ctx_phys + (ctx_size * 2));
    uint16_t max_pkt = (speed >= 4) ? 512 : ((speed == 3) ? 64 : 8);

    ep0_ctx->field[1] = (3 << 1) | (4 << 3) | ((uint32_t)max_pkt << 16);
    ep0_ctx->field[2] = ep0_phys | 1;
    ep0_ctx->field[3] = 0;
    ep0_ctx->field[4] = 8;

    uint32_t control = (11 << 10) | ((uint32_t)slot_id << 24);
    xhci_send_command((uint64_t)in_ctx_phys, 0, control);

    xhci_trb_t res;
    if (xhci_wait_event(&res)) {
        uint8_t comp = (res.status >> 24) & 0xFF;
        print_str((uint8_t *)"Address Device status: ", 55, 255, 55, 255);
        put_hex(comp, 255, 255, 255, 255);
        crlf_cursor();
    }
}


void xhci_check_ports(void) {
    xhci_port_regs_t *ports = (xhci_port_regs_t *)((uintptr_t)xhci.op + 0x400);

    for (uint8_t i = 0; i < xhci.max_ports; i++) {
        uint32_t portsc = ports[i].portsc;

        if (portsc & (1 << 0)) { // Dispositivo conectado (CCS = 1)
            if (!(portsc & (1 << 9))) {
                ports[i].portsc = (portsc & 0x0E00C3E0) | (1 << 9);
                sleep_millis(20);
            }

            ports[i].portsc = (ports[i].portsc & 0x0E00C3E0) | (1 << 4);
            int timeout = 100;
            while ((ports[i].portsc & (1 << 4)) && timeout-- > 0) {
                sleep_millis(1);
            }

            if (ports[i].portsc & (1 << 1)) {
                ports[i].portsc = (ports[i].portsc & 0x0E00C200) | (1 << 16);

                xhci_send_command(0, 0, (9 << 10));
                xhci_trb_t res;
                if (xhci_wait_event(&res)) {
                    msd_slot_id = (res.control >> 24) & 0xFF;
                    print_str((uint8_t *)"USB Port U0 & Slot assigned: ", 55, 255, 55, 255);
                    put_hex(msd_slot_id, 255, 255, 255, 255);
                    crlf_cursor();
                }
            }
        }
    }
}

static uint32_t last_usb_ping = 0;

void usb_keepalive_task(void) {
    while (1) {
        if (driver_xhci && msd_slot_id != 0) {
            if ((timer_seconds - last_usb_ping) >= 15) {
                last_usb_ping = timer_seconds;
                xhci_send_command(0, 0, (23 << 10));
                xhci_trb_t dummy;
                xhci_wait_event(&dummy);
            }
        }
        __asm__ volatile("hlt");
    }
}

void xhci_start_daemon(void) {
    if (driver_xhci) {
        task_create(usb_keepalive_task);
    }
}
