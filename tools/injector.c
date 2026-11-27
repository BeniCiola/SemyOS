#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <dirent.h>

#define MIK_MAGIC               0x314B494D
#define MIK_BLOCK_SIZE          4096
#define MIK_CLUSTER_DATA_SIZE   4092
#define MIK_CLUSTER_EOF         0xFFFFFFFF
#define MIK_PARTITION_LBA_START 128
#define MIK_SUPERBLOCK_BLOCK    0
#define MIK_ROOT_INODE          0

#define MIK_TYPE_FREE           0x00
#define MIK_TYPE_FILE           0x01
#define MIK_PERM_ALL            0x07

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

void read_block(FILE *img, uint32_t block, void *buf) {
    long offset = (MIK_PARTITION_LBA_START * 512L) + ((long)block * MIK_BLOCK_SIZE);
    fseek(img, offset, SEEK_SET);
    fread(buf, MIK_BLOCK_SIZE, 1, img);
}

void write_block(FILE *img, uint32_t block, const void *buf) {
    long offset = (MIK_PARTITION_LBA_START * 512L) + ((long)block * MIK_BLOCK_SIZE);
    fseek(img, offset, SEEK_SET);
    fwrite(buf, MIK_BLOCK_SIZE, 1, img);
}

void read_inode(FILE *img, const mik_superblock_t *sb, uint32_t id, mik_inode_t *out) {
	mik_inode_t block_buf[128];
	uint32_t block = sb->inode_table_start + (id / 128);
	read_block(img, block, block_buf);
	*out = block_buf[id % 128];
}

void write_inode(FILE *img, const mik_superblock_t *sb, uint32_t id, const mik_inode_t *in) {
	mik_inode_t block_buf[128];
	uint32_t block = sb->inode_table_start + (id / 128);
    read_block(img, block, block_buf);
	block_buf[id % 128] = *in;
	write_block(img, block, block_buf);
}


void inject_file(FILE *img, const mik_superblock_t *sb, const char *filepath, const char *filename){
	if(strlen(filename) > 28){
		printf("The file %s has a too long name.\n", filename);
		return;
	}

	FILE *f = fopen(filepath, "rb");
	if(!f){
		printf("The file path of %s doesn't exist.\n", filename);
		return;
	}

	fseek(f, 0, SEEK_END);
	uint32_t file_size = ftell(f);
	fseek(f, 0, SEEK_SET);

	uint8_t *file_data = malloc(file_size > 0 ? file_size : 1);
	if (file_size > 0) fread(file_data, 1, file_size, f);
	fclose(f);

	int free_inode = -1;
	mik_inode_t inode;

	for(uint32_t i = 1; i < sb->inode_count; i++){
		read_inode(img, sb, i, &inode);
		if(inode.type == MIK_TYPE_FREE){
			free_inode = i;
			break;
		}
	}
	if(free_inode == -1){
		printf("We don't have the enough inodes for the file %s.\n", filename);
		free(file_data);
		return;
	}

	uint32_t needed = (file_size + MIK_CLUSTER_DATA_SIZE - 1) / MIK_CLUSTER_DATA_SIZE;
	if(needed == 0) needed = 1;

	uint32_t *clusters = malloc(needed * sizeof(uint32_t));
	uint8_t bitmap[MIK_BLOCK_SIZE];
	read_block(img, sb->bitmap_start, bitmap);

	uint32_t found = 0;
	for(uint32_t b = 0; b < sb->total_blocks && found < needed; b++){
		if((bitmap[b / 8] & (1 << (b % 8))) == 0){
			bitmap[b / 8] |= (1 << (b % 8));
			clusters[found++] = b;
		}
	}
	if(found < needed){
		printf("We don't have the enough space for the file %s.\n", filename);
		free(file_data);
		free(clusters);
		return;
	}
	write_block(img, sb->bitmap_start, bitmap);

	for(uint32_t c = 0; c < needed; c++){
		mik_cluster_t cluster;
		memset(&cluster, 0, sizeof(mik_cluster_t));

		uint32_t offset = c * MIK_CLUSTER_DATA_SIZE;
		uint32_t chunk = (file_size > offset) ? (file_size - offset) : 0;
		if(chunk > MIK_CLUSTER_DATA_SIZE) chunk = MIK_CLUSTER_DATA_SIZE;

		if(chunk > 0) memcpy(cluster.data, file_data + offset, chunk);
		cluster.next_cluster = (c + 1 < needed) ? clusters[c + 1] : MIK_CLUSTER_EOF;
		write_block(img, clusters[c], &cluster);
	}

	memset(&inode, 0, sizeof(mik_inode_t));
	inode.type = MIK_TYPE_FILE;
	inode.perms = MIK_PERM_ALL;
	inode.links = 1;
	inode.size = file_size;
	inode.first_cluster = clusters[0];
	inode.parent_inode = MIK_ROOT_INODE;
	write_inode(img, sb, free_inode, &inode);

	mik_inode_t root_in;
	read_inode(img, sb, MIK_ROOT_INODE, &root_in);

	mik_cluster_t root_cluster;
	read_block(img, root_in.first_cluster, &root_cluster);

	mik_dirent_t *entries = (mik_dirent_t *)root_cluster.data;

	for(uint32_t i = 0; i < 127; i++){
		if(entries[i].inode_id == 0){
			entries[i].inode_id = free_inode;
			memset(entries[i].name, 0, 28);
			strncpy(entries[i].name, filename, 28);
			write_block(img, root_in.first_cluster, &root_cluster);

			root_in.size += sizeof(mik_dirent_t);
			write_inode(img, sb, MIK_ROOT_INODE, &root_in);
			break;
		}
	}

	free(file_data);
	free(clusters);
}

int main(int argc, char **argv){
	if(argc < 3) return 1;

	FILE *img = fopen(argv[1], "rb+");
	if(!img) return 1;

	mik_superblock_t sb;
	read_block(img, MIK_SUPERBLOCK_BLOCK, &sb);
	if(sb.magic != MIK_MAGIC){
		printf("Error: The disk hasn't a compatible MIK version (or hasn't MIK) in %s.\n", argv[1]);
		fclose(img);
		return 1;
	}

	DIR *dir = opendir(argv[2]);
	if(!dir){
		fclose(img);
		return 1;
	}

	struct dirent *entry;
	while((entry = readdir(dir)) != NULL){
		size_t len = strlen(entry->d_name);
		if(len <= 5 || strcmp(entry->d_name + len - 5, ".sexy") != 0) continue;

		char fullpath[512];
		snprintf(fullpath, sizeof(fullpath), "%s/%s", argv[2], entry->d_name);
		inject_file(img, &sb, fullpath, entry->d_name);
		printf("Injected: %s\n", entry->d_name);
	}

	closedir(dir);
	fclose(img);
	return 0;
}
