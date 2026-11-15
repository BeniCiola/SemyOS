void search_audio_driver(void) {
	driver_hda = 0;
	audio_controller.bar0_physical = 0;

	for (uint16_t bus = 0; bus < 256; bus++) {
		for (uint8_t slot = 0; slot < 32; slot++) {
			for (uint8_t func = 0; func < 8; func++) {
				uint32_t reg0 = pci_read_config(bus, slot, func, 0x00);
				uint16_t vendor_id = (uint16_t)(reg0 & 0xFFFF);

				if (vendor_id == 0xFFFF || vendor_id == 0x0000) {
					if (func == 0) break;
					continue;
				}

				uint32_t reg2 = pci_read_config(bus, slot, func, 0x08);
				uint8_t class_code = (uint8_t)(reg2 >> 24);
				uint8_t subclass   = (uint8_t)(reg2 >> 16);

				if (class_code == 0x04 && subclass == 0x03) {
					uint32_t bar0 = pci_read_config(bus, slot, func, 0x10);

					if (bar0 & 0x1) {
						continue;
					}

					uint32_t clear_address = bar0 & 0xFFFFF000;

					if (clear_address == 0) {
						continue;
					}

					audio_controller.bus = bus;
					audio_controller.slot = slot;
					audio_controller.func = func;
					audio_controller.bar0_physical = clear_address;

					driver_hda = 1;
					return;
				}
			}
		}
	}
}


void init_audio(void) {
	if (!driver_hda) return;

	uint8_t bus = audio_controller.bus;
	uint8_t slot = audio_controller.slot;
	uint8_t func = audio_controller.func;

	uint32_t cmd_reg = pci_read_config(bus, slot, func, 0x04);
	cmd_reg |= (1 << 1) | (1 << 2);
	pci_write_config(bus, slot, func, 0x04, cmd_reg);

	uint32_t phys_base = audio_controller.bar0_physical;
	vmm_map_page(phys_base, phys_base, PTE_PCD);
	vmm_map_page(phys_base + 0x1000, phys_base + 0x1000, PTE_PCD);

	audio_driver.regs = (volatile uint8_t *)phys_base;
	volatile uint32_t *hda_regs = (volatile uint32_t *)audio_driver.regs;

	uint16_t gcap = (uint16_t)(hda_regs[0] & 0xFFFF);
	uint8_t input_streams = (gcap >> 8) & 0x0F;

	volatile uint32_t *gctl = &hda_regs[2];

	*gctl &= ~(1u << 0);
	for(volatile int delay = 0; delay < 10000; delay++);

	*gctl |= (1u << 0);
	while ((*gctl & (1u << 0)) == 0);

	uint32_t stream_offset = 0x80 + (input_streams * 0x20);
	audio_driver.out_stream = (hda_stream_t *)(audio_driver.regs + stream_offset);

	(void)input_streams;

	uint32_t bdl_physical = pmm_alloc_block();
	for(uint16_t i = 0; i < 4096 / 4; i++){
		((uint32_t*)bdl_physical)[i] = 0;
	}

	audio_driver.bdl_phys = bdl_physical;
	audio_driver.bdl = (hda_bdl_entry_t *)bdl_physical;

	audio_driver.out_stream->bdpl = bdl_physical;
	audio_driver.out_stream->bdpu = 0;

	audio_driver.out_stream->fmt = 0x4011;
}
