#define HDA_SD_CTL   0x00
#define HDA_SD_STS   0x03
#define HDA_SD_LPIB  0x04
#define HDA_SD_CBL   0x08
#define HDA_SD_LVI   0x0C
#define HDA_SD_FIFOW 0x0E
#define HDA_SD_FMT   0x12
#define HDA_SD_BDPL	 0x18
#define HDA_SD_BDPU  0x1C

typedef struct {
	uint32_t address_l;
	uint32_t address_h;
	uint32_t length;
	uint32_t ioc;
} __attribute__((packed)) hda_comm_bdl_entry_t;

static volatile uint32_t *hda_comm_base = 0;
static uint32_t hda_oss_offset = 0x80;

static volatile uint32_t *corb_buf = 0;
static volatile uint64_t *rirb_buf = 0;
static uint16_t corb_wp = 0;
static uint16_t rirb_rp = 0;

void hda_comm_init(uint32_t bar0_virt_address, uint32_t oss_offset){
	hda_comm_base = (volatile uint32_t *)(bar0_virt_address & ~0xF);
	hda_oss_offset = oss_offset;
}

void hda_init_corb_rirb(uint32_t bar0_virt_address){
	volatile uint8_t *regs = (volatile uint8_t *)(bar0_virt_address & ~0xF);

	*(volatile uint8_t *)(regs + 0x4C) &= ~0x02;
	*(volatile uint8_t *)(regs + 0x5C) &= ~0x02;

	int t = 10000;
	while ((*(volatile uint8_t *)(regs + 0x4C) & 0x02) && --t);
	t = 10000;
	while ((*(volatile uint8_t *)(regs + 0x5C) & 0x02) && --t);

	uint32_t ring_phys = pmm_alloc_block();
	vmm_map_page(ring_phys, ring_phys, PTE_PCD | PTE_PRESENT | PTE_RW);
	for (uint32_t i = 0; i < 4096 / 4; i++) ((uint32_t *)ring_phys)[i] = 0;

	corb_buf = (volatile uint32_t *)ring_phys;
	rirb_buf = (volatile uint64_t *)(ring_phys + 0x800);

	*(volatile uint32_t *)(regs + 0x40) = ring_phys;
	*(volatile uint32_t *)(regs + 0x44) = 0;
	*(volatile uint8_t  *)(regs + 0x4E) = 0x02;

	*(volatile uint32_t *)(regs + 0x50) = ring_phys + 0x800;
	*(volatile uint32_t *)(regs + 0x54) = 0;
	*(volatile uint8_t  *)(regs + 0x5E) = 0x02;

	int timeout = 10000;
	*(volatile uint16_t *)(regs + 0x4A) = 0x8000;
	while (((*(volatile uint16_t *)(regs + 0x4A) & 0x8000) == 0) && --timeout);

	timeout = 10000;
	*(volatile uint16_t *)(regs + 0x4A) = 0x0000;
	while (((*(volatile uint16_t *)(regs + 0x4A) & 0x8000) != 0) && --timeout);

	*(volatile uint16_t *)(regs + 0x48) = 0;
	*(volatile uint16_t *)(regs + 0x58) = 0x8000;
	*(volatile uint16_t *)(regs + 0x5A) = 1;

	corb_wp = 0;
	rirb_rp = 0;

	*(volatile uint8_t *)(regs + 0x4C) |= 0x02;
	*(volatile uint8_t *)(regs + 0x5C) |= 0x03;
}


uint32_t hda_send_verb(uint32_t bar0, uint8_t codec_addr, uint32_t verb) {
	volatile uint8_t *regs = (volatile uint8_t *)(bar0 & ~0xF);

	*(volatile uint8_t *)(regs + 0x5D) = 0x05;

	corb_wp = (corb_wp + 1) & 0xFF;
	corb_buf[corb_wp] = ((uint32_t)codec_addr << 28) | verb;

	*(volatile uint16_t *)(regs + 0x48) = corb_wp;

	int timeout = 100000;
	while ((*(volatile uint16_t *)(regs + 0x58) & 0xFF) == rirb_rp && --timeout);

	if (timeout == 0) return 0xFFFFFFFF;

	rirb_rp = (rirb_rp + 1) & 0xFF;
	uint32_t resp = (uint32_t)(rirb_buf[rirb_rp] & 0xFFFFFFFF);

	*(volatile uint8_t *)(regs + 0x5D) = 0x05;

	return resp;
}

static inline void hda_stream_write32(uint32_t reg, uint32_t val){
	*(volatile uint32_t *)((uint8_t *)hda_comm_base + hda_oss_offset + reg) = val;
}

static inline uint32_t hda_stream_read32(uint32_t reg){
	return *(volatile uint32_t *)((uint8_t *)hda_comm_base + hda_oss_offset + reg);
}

static inline void hda_stream_write16(uint32_t reg, uint16_t val){
	*(volatile uint16_t *)((uint8_t *)hda_comm_base + hda_oss_offset + reg) = val;
}

int hda_configure_output_stream(uint32_t bdl_physical_addr, uint32_t total_buffer_size, uint16_t num_bdl_entries){
	uint32_t control_val = hda_stream_read32(HDA_SD_CTL) & 0x00FFFFFF;
	control_val &= ~2;
	hda_stream_write32(HDA_SD_CTL, control_val);

	int timeout = 10000;
	while ((hda_stream_read32(HDA_SD_CTL) & 2) && --timeout);

	hda_stream_write32(HDA_SD_BDPL, bdl_physical_addr);
	hda_stream_write32(HDA_SD_BDPU, 0);
	hda_stream_write32(HDA_SD_CBL, total_buffer_size);
	hda_stream_write16(HDA_SD_LVI, num_bdl_entries - 1);
	hda_stream_write16(HDA_SD_FMT, 0x0011);

	control_val = hda_stream_read32(HDA_SD_CTL) & 0x00FFFFFF;
	control_val &= ~(0xF << 20);
	control_val |= (1 << 20);
	hda_stream_write32(HDA_SD_CTL, control_val);

	return 0;
}

void hda_start_stream(void){
	uint32_t ctl = hda_stream_read32(HDA_SD_CTL) & 0x00FFFFFF;
	ctl |= 2;
	hda_stream_write32(HDA_SD_CTL, ctl);
}

void hda_stop_stream(void){
	uint32_t ctl = hda_stream_read32(HDA_SD_CTL) & 0x00FFFFFF;
	ctl &= ~2;
	hda_stream_write32(HDA_SD_CTL, ctl);

	int timeout = 10000;
	while ((hda_stream_read32(HDA_SD_CTL) & 2) && --timeout);
}


typedef struct {
	uint16_t *sample_data;
	uint32_t sample_count;
	uint32_t duration_ms;
} hda_async_req_t;

static task_t *audio_worker_task = NULL;
static volatile uint8_t hda_busy = 0;
static hda_async_req_t hda_req;

static void hda_audio_daemon(void) {
	while (1) {
		preempt_disable();
		if (!hda_busy) {
			current_task->state = TASK_BLOCKED;
			preempt_enable();
			__asm__ volatile("hlt");
			continue;
		}
		preempt_enable();

		uint16_t *sample_data = hda_req.sample_data;
		uint32_t sample_count = hda_req.sample_count;
		uint32_t duration_ms  = hda_req.duration_ms;

		uint32_t bytes_per_ms = 192;
		uint32_t total_bytes = duration_ms * bytes_per_ms;
		uint32_t block_size = 4096;
		uint32_t num_entries = (total_bytes + block_size - 1) / block_size;

		if (num_entries < 2) num_entries = 2;
		if (num_entries > 256) num_entries = 256;

		preempt_disable();

		uint32_t bdl_phys = pmm_alloc_block();
		vmm_map_page(bdl_phys, bdl_phys, PTE_PCD | PTE_PRESENT | PTE_RW);
		hda_comm_bdl_entry_t *bdl = (hda_comm_bdl_entry_t *)bdl_phys;

		uint32_t allocated_total = 0;
		uint32_t global_sample_idx = 0;

		for (uint32_t i = 0; i < num_entries; i++) {
			uint32_t pcm_phys = pmm_alloc_block();
			vmm_map_page(pcm_phys, pcm_phys, PTE_PCD | PTE_PRESENT | PTE_RW);
			uint16_t *buf = (uint16_t *)pcm_phys;

			uint32_t samples_in_block = block_size / 2;
			for (uint32_t j = 0; j < samples_in_block; j++) {
				buf[j] = sample_data[global_sample_idx % sample_count];
				global_sample_idx++;
			}

			bdl[i].address_l = pcm_phys;
			bdl[i].address_h = 0;
			bdl[i].length = block_size;
			bdl[i].ioc = 0;

			allocated_total += block_size;
		}

		hda_configure_output_stream(bdl_phys, allocated_total, (uint16_t)num_entries);
		hda_start_stream();

		preempt_enable();

		sleep_millis(duration_ms + 15);

		preempt_disable();
		hda_stop_stream();

		for (uint32_t i = 0; i < num_entries; i++) {
			pmm_free_block(bdl[i].address_l);
		}
		pmm_free_block(bdl_phys);

		hda_busy = 0;
		preempt_enable();
	}
}

void hda_start_daemon(void) {
	if (driver_hda && audio_worker_task == NULL) {
		audio_worker_task = task_create(hda_audio_daemon);
	}
}

int hda_play_sound(uint16_t *sample_data, uint32_t sample_count, uint32_t duration_ms) {
	if (!driver_hda || audio_worker_task == NULL) return -1;
	if (duration_ms == 0 || sample_count == 0) return -2;

	preempt_disable();
	if (hda_busy) {
		preempt_enable();
		return -3;
	}

	hda_req.sample_data  = sample_data;
	hda_req.sample_count = sample_count;
	hda_req.duration_ms  = duration_ms;
	hda_busy = 1;

	audio_worker_task->state = TASK_READY;
	preempt_enable();

	return 0;
}
