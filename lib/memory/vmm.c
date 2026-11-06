#define PTE_PRESENT 0x01
#define PTE_RW 0x02
#define PTE_USER 0x04

uint32_t page_directory[1024] __attribute__((aligned(4096)));

void vmm_init(void){
	for(int i = 0; i < 1024; i++){
		page_directory[i] = 0x00000002;
	}
}

void vmm_map_page(uint32_t virt, uint32_t phys, uint32_t flags){
	uint32_t pd_index = virt >> 22;
	uint32_t pt_index = (virt >> 12) & 0x03FF;

	uint32_t* page_table;

	if(!(page_directory[pd_index] & PTE_PRESENT)){
		uint32_t pt_phys = pmm_alloc_block();

		page_directory[pd_index] = pt_phys | PTE_PRESENT | PTE_RW | flags;

		page_table = (uint32_t*)pt_phys;

		for(int i = 0; i < 1024; i++){
			page_table[i] = 0;
		}
	} else {
		uint32_t pt_phys = page_directory[pd_index] & 0xFFFFF000;
		page_table = (uint32_t*)pt_phys;
	}

	page_table[pt_index] = (phys & 0xFFFFF000) | PTE_PRESENT | PTE_RW | flags;

	__asm__ volatile("invlpg (%0)" : : "r" (virt) : "memory");
}
