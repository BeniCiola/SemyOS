typedef struct BlockHeader {
	size_t size;
	int is_free;
	struct BlockHeader* next;
} BlockHeader;

BlockHeader* heap_start = NULL;
