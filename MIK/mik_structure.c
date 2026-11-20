#define MIK_MAGIC               0x314B494D
#define MIK_BLOCK_SIZE          4096
#define MIK_CLUSTER_DATA_SIZE   4092
#define MIK_CLUSTER_EOF         0xFFFFFFFF
#define MIK_PARTITION_LBA_START 128
#define MIK_SECTORS_PER_BLOCK   8

#define MIK_TYPE_FREE           0x00
#define MIK_TYPE_FILE           0x01
#define MIK_TYPE_DIR            0x02
#define MIK_TYPE_DEV            0x03

#define MIK_PERM_EXEC           0x01
#define MIK_PERM_WRITE          0x02
#define MIK_PERM_READ           0x04


uint32_t current_dir_inode = 0;
char current_path[512] = "~/";


typedef struct __attribute__((packed)) {
	uint32_t magic;
	uint32_t total_blocks;
	uint32_t bitmap_start;
	uint32_t bitmap_blocks;
	uint32_t inode_table_start;
	uint32_t inode_count;
	uint32_t data_start;
	uint8_t  reserved[4068];
} mik_superblock_t;

typedef struct __attribute__((packed)) {
	uint8_t  data[MIK_CLUSTER_DATA_SIZE];
	uint32_t next_cluster;
} mik_cluster_t;

typedef struct __attribute__((packed)) {
	uint8_t  type;
	uint8_t  perms;
	uint16_t links;
	uint32_t size;
	uint32_t first_cluster;
	uint32_t parent_inode;
	uint8_t  reserved[16];
} mik_inode_t;

typedef struct __attribute__((packed)) {
	uint32_t inode_id;
	char     name[28];
} mik_dirent_t;

static mik_superblock_t *mik_sb = NULL;
static uint8_t mik_mounted = 0;

int mik_read_block(uint32_t block_idx, void *buffer) {
	uint64_t lba = MIK_PARTITION_LBA_START + ((uint64_t)block_idx * MIK_SECTORS_PER_BLOCK);
	if (boot_sata_disk != NULL) {
		return ahci_read(boot_sata_disk, lba, MIK_SECTORS_PER_BLOCK, buffer);
	}
	return 0;
}

int mik_write_block(uint32_t block_idx, void *buffer) {
	uint64_t lba = MIK_PARTITION_LBA_START + ((uint64_t)block_idx * MIK_SECTORS_PER_BLOCK);
	if (boot_sata_disk != NULL) {
		return ahci_write(boot_sata_disk, lba, MIK_SECTORS_PER_BLOCK, buffer);
	}
	return 0;
}
