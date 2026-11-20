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
					if (vendor_id == 0x10DE || vendor_id == 0x1002) {
						continue;
					}

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

static void hda_setup_codecs(uint32_t bar0) {
	for (uint8_t codec = 0; codec < 4; codec++) {
		uint32_t vendor = hda_send_verb(bar0, codec, (0x00 << 20) | (0xF00 << 8) | 0x00);
		if (vendor == 0 || vendor == 0xFFFFFFFF) continue;

		uint32_t sub_nodes = hda_send_verb(bar0, codec, (0x00 << 20) | (0xF00 << 8) | 0x04);
		if (sub_nodes == 0xFFFFFFFF) continue;

		uint8_t start_fg = (sub_nodes >> 16) & 0xFF;
		uint8_t num_fg   = sub_nodes & 0xFF;

		for (uint8_t fg = start_fg; fg < start_fg + num_fg; fg++) {
			uint32_t fg_type = hda_send_verb(bar0, codec, (fg << 20) | (0xF00 << 8) | 0x05);
			if ((fg_type & 0xFF) != 0x01) continue;

			hda_send_verb(bar0, codec, (fg << 20) | (0x705 << 8) | 0x00);
			sleep_millis(10);

			hda_send_verb(bar0, codec, (fg << 20) | (0x715 << 8) | 0xFF);
			hda_send_verb(bar0, codec, (fg << 20) | (0x716 << 8) | 0xFF);
			hda_send_verb(bar0, codec, (fg << 20) | (0x717 << 8) | 0xFF);

			uint32_t widgets = hda_send_verb(bar0, codec, (fg << 20) | (0xF00 << 8) | 0x04);
			if (widgets == 0xFFFFFFFF) continue;

			uint8_t start_nid = (widgets >> 16) & 0xFF;
			uint8_t num_nids  = widgets & 0xFF;

			for (uint8_t nid = start_nid; nid < start_nid + num_nids; nid++) {
				uint32_t caps = hda_send_verb(bar0, codec, (nid << 20) | (0xF00 << 8) | 0x09);
				uint8_t type = (caps >> 20) & 0x0F;

				hda_send_verb(bar0, codec, (nid << 20) | (0x705 << 8) | 0x00);

				if (type == 0x0) {
					hda_send_verb(bar0, codec, (nid << 20) | (0x706 << 8) | 0x10);
					hda_send_verb(bar0, codec, (nid << 20) | (0x2 << 16) | 0x0011);
					hda_send_verb(bar0, codec, (nid << 20) | (0x3 << 16) | 0xB07F);
				}
				else if (type == 0x2) {
					for (uint8_t idx = 0; idx < 8; idx++) {
						uint32_t payload = 0x7000 | (idx << 8) | 0x7F;
						hda_send_verb(bar0, codec, (nid << 20) | (0x3 << 16) | payload);
					}
					hda_send_verb(bar0, codec, (nid << 20) | (0x3 << 16) | 0xB07F);
				}
				else if (type == 0x3) {
					hda_send_verb(bar0, codec, (nid << 20) | (0x701 << 8) | 0x00);
					hda_send_verb(bar0, codec, (nid << 20) | (0x3 << 16) | 0x707F);
					hda_send_verb(bar0, codec, (nid << 20) | (0x3 << 16) | 0xB07F);
				}
				else if (type == 0x4) {
					hda_send_verb(bar0, codec, (nid << 20) | (0x707 << 8) | 0xC0);
					hda_send_verb(bar0, codec, (nid << 20) | (0x3 << 16) | 0xB07F);
					hda_send_verb(bar0, codec, (nid << 20) | (0x70C << 8) | 0x02);
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

	uint32_t tcsel = pci_read_config(bus, slot, func, 0x44);
	pci_write_config(bus, slot, func, 0x44, tcsel & ~0x07);

	uint32_t phys_base = audio_controller.bar0_physical;

	vmm_map_page(phys_base, phys_base, PTE_PCD | PTE_PRESENT | PTE_RW);
	vmm_map_page(phys_base + 0x1000, phys_base + 0x1000, PTE_PCD | PTE_PRESENT | PTE_RW);

	audio_driver.regs = (volatile uint8_t *)phys_base;
	volatile uint32_t *hda_regs = (volatile uint32_t *)audio_driver.regs;

	uint16_t gcap = (uint16_t)(hda_regs[0] & 0xFFFF);
	uint8_t input_streams = (gcap >> 8) & 0x0F;

	volatile uint32_t *gctl = &hda_regs[2];

	*gctl &= ~(1u << 0);
	sleep_millis(2);

	*gctl |= (1u << 0);
	while ((*gctl & (1u << 0)) == 0);

	sleep_millis(10);

	uint32_t stream_offset = 0x80 + (input_streams * 0x20);
	audio_driver.out_stream = (hda_stream_t *)(audio_driver.regs + stream_offset);

	hda_comm_init(phys_base, stream_offset);
	hda_init_corb_rirb(phys_base);
	hda_setup_codecs(phys_base);
}
