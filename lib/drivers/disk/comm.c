typedef struct __attribute__((packed)) {
    uint32_t dba;
    uint32_t dbau;
    uint32_t rsv0;
    uint32_t dbc:22;
    uint32_t rsv1:9;
    uint32_t i:1;
} ahci_prdt_entry_t;


typedef struct __attribute__((packed)) {
    uint8_t  cfis[64];
    uint8_t  acmd[16];
    uint8_t  rsv[48];
    ahci_prdt_entry_t prdt_entry[1];
} ahci_cmd_tbl_t;


typedef struct __attribute__((packed)) {
    uint8_t  fis_type;
    uint8_t  pmport:4;
    uint8_t  rsv0:3;
    uint8_t  c:1;
    uint8_t  command;
    uint8_t  featurel;
    uint8_t  lba0, lba1, lba2;
    uint8_t  device;
    uint8_t  lba3, lba4, lba5;
    uint8_t  featureh;
    uint8_t  countl, counth;
    uint8_t  icc;
    uint8_t  control;
    uint8_t  rsv1[4];
} fis_reg_h2d_t;


int ahci_find_slot(ahci_port_t *port) {
    uint32_t slots = (port->sact | port->ci);
    for (int i = 0; i < 32; i++) {
        if ((slots & (1 << i)) == 0) return i;
    }
    return -1;
}

int ahci_rw(ahci_port_t *port, uint64_t lba, uint16_t count, void *buf, int write) {
    port->is = 0xFFFFFFFF;

    int slot = ahci_find_slot(port);
    if (slot == -1) return 0;

    ahci_cmd_header_t *hdr = &((ahci_cmd_header_t*)port->clb)[slot];
    hdr->cfl = sizeof(fis_reg_h2d_t) / 4;
    hdr->w = write ? 1 : 0;
    hdr->prdtl = 1;

    ahci_cmd_tbl_t *tbl = (ahci_cmd_tbl_t*)hdr->ctba;
    for (int i = 0; i < sizeof(ahci_cmd_tbl_t) / 4; i++) ((uint32_t*)tbl)[i] = 0;

    tbl->prdt_entry[0].dba = (uint32_t)buf;
    tbl->prdt_entry[0].dbc = (count * 512) - 1;
    tbl->prdt_entry[0].i = 1;

    fis_reg_h2d_t *fis = (fis_reg_h2d_t*)tbl->cfis;
    fis->fis_type = 0x27;
    fis->c = 1;
    fis->command = write ? 0x35 : 0x25;
    fis->device = 1 << 6;

    fis->lba0 = (uint8_t)lba;
    fis->lba1 = (uint8_t)(lba >> 8);
    fis->lba2 = (uint8_t)(lba >> 16);
    fis->lba3 = (uint8_t)(lba >> 24);
    fis->lba4 = (uint8_t)(lba >> 32);
    fis->lba5 = (uint8_t)(lba >> 40);

    fis->countl = count & 0xFF;
    fis->counth = (count >> 8) & 0xFF;

    while (port->tfd & (0x80 | 0x08));
    port->ci = (1 << slot);

    while (1) {
        if ((port->ci & (1 << slot)) == 0) break;
        if (port->is & (1 << 30)) return 0;
    }

    return 1;
}


int ahci_read(ahci_port_t *port, uint64_t lba, uint16_t count, void *buf) {
    return ahci_rw(port, lba, count, buf, 0);
}

int ahci_write(ahci_port_t *port, uint64_t lba, uint16_t count, void *buf) {
    return ahci_rw(port, lba, count, buf, 1);
}
