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


ahci_port_t *boot_sata_disk = NULL;


void ahci_stop_cmd(ahci_port_t *port){
	port->cmd &= ~(1 << 0);
	port->cmd &= ~(1 << 4);

	while(1){
		if(port->cmd & (1 << 14)) continue;
		if(port->cmd & (1 << 15)) continue;
		break;
	}
}

void ahci_start_cmd(ahci_port_t *port){
	while(port->cmd & (1 << 14));

	port->cmd |= (1 << 4);
	port->cmd |= (1 << 0);
}


int ahci_check_device_type(ahci_port_t *port){
	uint32_t ssts = port->ssts;

	uint8_t ipm = (ssts >> 8) & 0x0F;
	uint8_t det = ssts & 0x0F;

	if(det != 3) return AHCI_DEV_NULL;
	if(ipm != 1) return AHCI_DEV_NULL;

	uint32_t sig = port->sig;
	switch(sig){
		case 0x00000101: return AHCI_DEV_SATA;
		case 0xEB140101: return AHCI_DEV_SATAPI;
		case 0xC33C0101: return AHCI_DEV_SEMB;
		case 0x96690101: return AHCI_DEV_PM;
		default:		 return AHCI_DEV_NULL;
	}
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
			ahci_port_t *port = &abar->ports[i];

			int dev_type = ahci_check_device_type(port);

			if(dev_type == AHCI_DEV_SATA){
				ahci_port_init(port);

				if(boot_sata_disk == NULL){
					boot_sata_disk = port;
				}
			}
		}
	}
}
