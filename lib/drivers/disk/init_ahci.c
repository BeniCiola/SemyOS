#define AHCI_DEV_NULL 0
#define AHCI_DEV_SATA 1
#define AHCI_DEV_SEMB 2
#define AHCI_DEV_PM   3
#define AHCI_DEV_SATAPI 4

typedef volatile struct {
	uint32_t clb;
	uint32_t clbu;
	uint32_t fb;
	uint32_t fbu;
	uint32_t is;
	uint32_t ie;
	uint32_t cmd;
	uint32_t rsv0;
	uint32_t tfd;
	uint32_t sig;
	uint32_t ssts;
	uint32_t sctl;
	uint32_t serr;
	uint32_t sact;
	uint32_t ci;
	uint32_t sntf;
	uint32_t fbs;
	uint32_t rsv1[11];
	uint32_t vendor[4];
} ahci_port_t;

typedef volatile struct {
	uint32_t cap;
	uint32_t ghc;
	uint32_t is;
	uint32_t pi;
	uint32_t vs;
	uint32_t ccc_ctl;
	uint32_t ccc_pts;
	uint32_t em_loc;
	uint32_t em_ctl;
	uint32_t cap2;
	uint32_t bohc;
	uint8_t rsv[0xA0 - 0x2C];
	uint8_t vendor[0x100 - 0xA0];
	ahci_port_t ports[32];
} ahci_abar_t;

typedef struct __attribute__((packed)) {
	uint8_t  cfl:5;
	uint8_t  a:1;
	uint8_t  w:1;
	uint8_t  p:1;
	uint8_t  r:1;
	uint8_t  b:1;
	uint8_t  c:1;
	uint8_t  rsv0:1;
	uint8_t  pmp:4;
	uint16_t prdtl;
	volatile uint32_t prdbc;
	uint32_t ctba;
	uint32_t ctbau;
	uint32_t rsv1[4];
} ahci_cmd_header_t;

typedef struct {
	uint8_t bus;
	uint8_t slot;
	uint8_t func;
	uint32_t abar_physical;
} ahci_device_t;

ahci_device_t ahci_controller;
uint8_t driver_ahci = 0;
ahci_port_t *boot_sata_disk = NULL;


void ahci_stop_cmd(ahci_port_t *port){
	port->cmd &= ~(1 << 0);
	port->cmd &= ~(1 << 4);

	int timeout = 1000000;
	while(port->cmd & ((1 << 14) | (1 << 15))) {
		if(--timeout <= 0) break;
	}
}

void ahci_start_cmd(ahci_port_t *port){
	while(port->cmd & (1 << 14));

	port->cmd |= (1 << 4);
	port->cmd |= (1 << 0);
}

int ahci_check_device_type(ahci_port_t *port){
	uint32_t ssts = port->ssts;
	uint8_t det = ssts & 0x0F;

	if(det != 3) return AHCI_DEV_NULL;

	uint32_t sig = port->sig;
	switch(sig){
		case 0x00000101: return AHCI_DEV_SATA;
		case 0xEB140101: return AHCI_DEV_SATAPI;
		case 0xC33C0101: return AHCI_DEV_SEMB;
		case 0x96690101: return AHCI_DEV_PM;
		default:		 return AHCI_DEV_NULL;
	}
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

					if (bar5 & 0x1) continue;

					uint32_t clear_address = bar5 & 0xFFFFF000;
					if (clear_address == 0) continue;

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

	uint32_t check_reg = pci_read_config(bus, slot, func, 0x04);
	if (!(check_reg & (1 << 2))) {
		print_str((uint8_t *)"Error: PCI Bus Master could not be enabled!", 255, 55, 55, 255);
		crlf_cursor();
	} else {
		print_str((uint8_t *)"PCI Bus Master successfully enabled.", 55, 255, 55, 255);
		crlf_cursor();
	}

	vmm_map_page(ahci_controller.abar_physical, ahci_controller.abar_physical, PTE_PRESENT | PTE_RW | PTE_CACHE_DISABLE);

	ahci_abar_t *abar = (ahci_abar_t *)ahci_controller.abar_physical;

	if(abar->bohc & (1 << 0)){
		abar->bohc |= (1 << 1);

		uint8_t timeout = 50;
		while((abar->bohc & (1 << 0)) && (timeout > 0)){
			sleep_millis(1);
			timeout--;
		}

		timeout = 50;
		while((abar->bohc & (1 << 4)) && (timeout > 0)){
			sleep_millis(1);
			timeout--;
		}
	}

	abar->ghc |= (1u << 31);
	sleep_millis(10);

	uint32_t ports_implemented = abar->pi;
	(void)ports_implemented;
}


void ahci_port_init(ahci_port_t *port){
	ahci_stop_cmd(port);

	uint32_t cmd_list_phys = pmm_alloc_block();
	if(!cmd_list_phys) return;

	for(int i = 0; i < 1024 / 4; i++){
		((uint32_t*)cmd_list_phys)[i] = 0;
	}

	port->clb = cmd_list_phys;
	port->clbu = 0;

	uint32_t fis_phys = pmm_alloc_block();
	if(!fis_phys) return;

	for(int i = 0; i < 256 / 4; i++){
		((uint32_t*)fis_phys)[i] = 0;
	}

	port->fb = fis_phys;
	port->fbu = 0;

	uint32_t tbl1 = pmm_alloc_block();
	uint32_t tbl2 = pmm_alloc_block();
	for (int i = 0; i < 4096 / 4; i++) {
		((uint32_t*)tbl1)[i] = 0;
		((uint32_t*)tbl2)[i] = 0;
	}

	ahci_cmd_header_t *hdr = (ahci_cmd_header_t*)cmd_list_phys;
	for (int i = 0; i < 32; i++) {
		hdr[i].prdtl = 1;
		hdr[i].ctba = (i < 16) ? (tbl1 + i * 256) : (tbl2 + (i - 16) * 256);
		hdr[i].ctbau = 0;
	}

	ahci_start_cmd(port);
}


void ahci_detect_ports(ahci_abar_t *abar){
	uint32_t ports_implemented = abar->pi;

	for(int i = 0; i < 32; i++){
		if(ports_implemented & (1 << i)){
			ahci_port_t *port = (ahci_port_t*)((uintptr_t)abar + 0x100 + (i * 0x80));

			ahci_stop_cmd(port);

			port->sctl = (port->sctl & ~0x0F) | 1;
			sleep_millis(2);
			port->sctl = (port->sctl & ~0x0F);

			port->serr = 0xFFFFFFFF;

			int timeout = 150;
			while(((port->ssts & 0x0F) != 3) && (timeout > 0)) {
				sleep_millis(1);
				timeout--;
			}

			print_str((uint8_t *)"Port SSTS: ", 255, 255, 0, 255);
			put_hex(port->ssts, 255, 255, 255, 255);
			crlf_cursor();

			if(timeout <= 0) continue;

			port->serr = 0xFFFFFFFF;
			port->is = 0xFFFFFFFF;

			ahci_port_init(port);
			sleep_millis(50);

			int dev_type = ahci_check_device_type(port);

			print_str((uint8_t *)"Port SIG: ", 255, 255, 0, 255);
			put_hex(port->sig, 255, 255, 255, 255);
			crlf_cursor();

			if(dev_type == AHCI_DEV_SATA){
				if(boot_sata_disk == NULL){
					boot_sata_disk = port;
				}
			} else {
				ahci_stop_cmd(port);
			}
		}
	}
}


/*
void ahci_detect_ports(ahci_abar_t *abar){
	uint32_t ports_implemented = abar->pi;

	for(int i = 0; i < 32; i++){
		if(ports_implemented & (1 << i)){
			ahci_port_t *port = &abar->ports[i];

			ahci_stop_cmd(port);

			port->sctl = (port->sctl & ~0x0F) | 1;
			volatile uint32_t dummy = port->sctl;
			(void)dummy;

			sleep_millis(15);

			port->sctl = (port->sctl & ~0x0F);
			dummy = port->sctl;

			int timeout = 1000000;
			while(((port->ssts & 0x0F) != 3) && (--timeout > 0));

			print_str((uint8_t *)"Port SSTS: ", 255, 255, 0, 255);
			put_hex(port->ssts, 255, 255, 255, 255);
			crlf_cursor();

			if(timeout <= 0) continue;

			port->serr = port->serr;
			port->is = port->is;

			sleep_millis(200);

			ahci_port_init(port);
			sleep_millis(10);

			int dev_type = ahci_check_device_type(port);

			print_str((uint8_t *)"Port SIG: ", 255, 255, 0, 255);
			put_hex(port->sig, 255, 255, 255, 255);
			crlf_cursor();

			if(dev_type == AHCI_DEV_SATA){
				if(boot_sata_disk == NULL){
					boot_sata_disk = port;
				}
			} else {
				ahci_stop_cmd(port);
			}
		}
	}
}
*/
