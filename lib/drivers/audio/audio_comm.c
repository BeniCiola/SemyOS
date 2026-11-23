#define HDA_SD_CTL   0x00
#define HDA_SD_STS   0x03
#define HDA_SD_LPIB  0x04
#define HDA_SD_CBL   0x08
#define HDA_SD_LVI   0x0C
#define HDA_SD_FIFOW 0x0E
#define HDA_SD_FMT   0x12
#define HDA_SD_BDPL  0x18
#define HDA_SD_BDPU  0x1C

#define HDA_BLOCK_SIZE 4096
#define HDA_MAX_BDL_ENTRIES 256
#define HDA_BYTES_PER_MS 192

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

volatile uint32_t hda_request_id = 0;

typedef struct {
    uint16_t *sample_data;
    uint32_t sample_count;
    uint32_t duration_ms;
} hda_async_req_t;

static task_t *audio_worker_task = NULL;
static volatile uint8_t hda_busy = 0;
static hda_async_req_t hda_req;

void hda_comm_init(uint32_t bar0_virt_address, uint32_t oss_offset) {
    hda_comm_base = (volatile uint32_t *)(bar0_virt_address & ~0xF);
    hda_oss_offset = oss_offset;
}

void hda_init_corb_rirb(uint32_t bar0_virt_address) {
    volatile uint8_t *regs =
        (volatile uint8_t *)(bar0_virt_address & ~0xF);

    *(volatile uint8_t *)(regs + 0x4C) &= ~0x02;
    *(volatile uint8_t *)(regs + 0x5C) &= ~0x02;

    int t = 10000;

    while ((*(volatile uint8_t *)(regs + 0x4C) & 0x02) && --t);

    t = 10000;

    while ((*(volatile uint8_t *)(regs + 0x5C) & 0x02) && --t);

    uint32_t ring_phys = pmm_alloc_block();

    if (ring_phys == 0) {
        corb_buf = 0;
        rirb_buf = 0;
        return;
    }

    vmm_map_page(
        ring_phys,
        ring_phys,
        PTE_PCD | PTE_PRESENT | PTE_RW
    );

    for (uint32_t i = 0; i < 4096 / 4; i++) {
        ((uint32_t *)ring_phys)[i] = 0;
    }

    corb_buf = (volatile uint32_t *)ring_phys;
    rirb_buf = (volatile uint64_t *)(ring_phys + 0x800);

    *(volatile uint32_t *)(regs + 0x40) = ring_phys;
    *(volatile uint32_t *)(regs + 0x44) = 0;
    *(volatile uint8_t *)(regs + 0x4E) = 0x02;

    *(volatile uint32_t *)(regs + 0x50) = ring_phys + 0x800;
    *(volatile uint32_t *)(regs + 0x54) = 0;
    *(volatile uint8_t *)(regs + 0x5E) = 0x02;

    int timeout = 10000;

    *(volatile uint16_t *)(regs + 0x4A) = 0x8000;

    while (((*(volatile uint16_t *)(regs + 0x4A) & 0x8000) == 0)
           && --timeout);

    timeout = 10000;

    *(volatile uint16_t *)(regs + 0x4A) = 0x0000;

    while (((*(volatile uint16_t *)(regs + 0x4A) & 0x8000) != 0)
           && --timeout);

    *(volatile uint16_t *)(regs + 0x48) = 0;
    *(volatile uint16_t *)(regs + 0x58) = 0x8000;
    *(volatile uint16_t *)(regs + 0x5A) = 1;

    corb_wp = 0;
    rirb_rp = 0;

    *(volatile uint8_t *)(regs + 0x4C) |= 0x02;
    *(volatile uint8_t *)(regs + 0x5C) |= 0x03;
}

uint32_t hda_send_verb(
    uint32_t bar0,
    uint8_t codec_addr,
    uint32_t verb
) {
    if (corb_buf == 0 || rirb_buf == 0)
        return 0xFFFFFFFF;

    volatile uint8_t *regs =
        (volatile uint8_t *)(bar0 & ~0xF);

    *(volatile uint8_t *)(regs + 0x5D) = 0x05;

    corb_wp = (corb_wp + 1) & 0xFF;

    corb_buf[corb_wp] =
        ((uint32_t)codec_addr << 28) | verb;

    *(volatile uint16_t *)(regs + 0x48) = corb_wp;

    int timeout = 100000;

    while ((*(volatile uint16_t *)(regs + 0x58) & 0xFF) == rirb_rp
           && --timeout);

    if (timeout == 0)
        return 0xFFFFFFFF;

    rirb_rp = (rirb_rp + 1) & 0xFF;

    uint32_t resp =
        (uint32_t)(rirb_buf[rirb_rp] & 0xFFFFFFFF);

    *(volatile uint8_t *)(regs + 0x5D) = 0x05;

    return resp;
}

static inline void hda_stream_write32(uint32_t reg, uint32_t val) {
    *(volatile uint32_t *)(
        (uint8_t *)hda_comm_base + hda_oss_offset + reg
    ) = val;
}

static inline uint32_t hda_stream_read32(uint32_t reg) {
    return *(volatile uint32_t *)(
        (uint8_t *)hda_comm_base + hda_oss_offset + reg
    );
}

static inline void hda_stream_write16(uint32_t reg, uint16_t val) {
    *(volatile uint16_t *)(
        (uint8_t *)hda_comm_base + hda_oss_offset + reg
    ) = val;
}

int hda_configure_output_stream(
    uint32_t bdl_physical_addr,
    uint32_t total_buffer_size,
    uint16_t num_bdl_entries
) {
    if (hda_comm_base == 0 || num_bdl_entries == 0)
        return -1;

    uint32_t control_val =
        hda_stream_read32(HDA_SD_CTL) & 0x00FFFFFF;

    control_val &= ~2;

    hda_stream_write32(HDA_SD_CTL, control_val);

    int timeout = 10000;

    while ((hda_stream_read32(HDA_SD_CTL) & 2) && --timeout);

    if (timeout == 0)
        return -1;

    hda_stream_write32(HDA_SD_BDPL, bdl_physical_addr);
    hda_stream_write32(HDA_SD_BDPU, 0);

    hda_stream_write32(HDA_SD_CBL, total_buffer_size);

    hda_stream_write16(
        HDA_SD_LVI,
        num_bdl_entries - 1
    );

    hda_stream_write16(HDA_SD_FMT, 0x0011);

    control_val =
        hda_stream_read32(HDA_SD_CTL) & 0x00FFFFFF;

    control_val &= ~(0xF << 20);
    control_val |= (1 << 20);

    hda_stream_write32(HDA_SD_CTL, control_val);

    return 0;
}

void hda_start_stream(void) {
    uint32_t ctl =
        hda_stream_read32(HDA_SD_CTL) & 0x00FFFFFF;

    ctl |= 2;

    hda_stream_write32(HDA_SD_CTL, ctl);
}

void hda_stop_stream(void) {
    uint32_t ctl =
        hda_stream_read32(HDA_SD_CTL) & 0x00FFFFFF;

    ctl &= ~2;

    hda_stream_write32(HDA_SD_CTL, ctl);

    int timeout = 10000;

    while ((hda_stream_read32(HDA_SD_CTL) & 2) && --timeout);
}

void hda_free_audio_buffers(
    uint32_t bdl_phys,
    hda_comm_bdl_entry_t *bdl,
    uint32_t allocated_entries
) {
    if (bdl != 0) {
        for (uint32_t i = 0; i < allocated_entries; i++) {
            if (bdl[i].address_l != 0) {
                pmm_free_block(bdl[i].address_l);
            }
        }
    }

    if (bdl_phys != 0) {
        pmm_free_block(bdl_phys);
    }
}

void hda_audio_daemon(void) {
    while (1) {

        preempt_disable();

        if (!hda_busy) {
            current_task->state = TASK_BLOCKED;

            preempt_enable();

            __asm__ volatile("hlt");

            continue;
        }

        uint32_t request_id = hda_request_id;

        uint16_t *sample_data = hda_req.sample_data;
        uint32_t sample_count = hda_req.sample_count;
        uint32_t duration_ms = hda_req.duration_ms;

        if (sample_data == 0 || sample_count == 0) {
            hda_busy = 0;
            preempt_enable();
            continue;
        }

        uint64_t total_bytes =
            (uint64_t)duration_ms * HDA_BYTES_PER_MS;

        uint32_t num_entries =
            (uint32_t)(
                (total_bytes + HDA_BLOCK_SIZE - 1)
                / HDA_BLOCK_SIZE
            );

        if (num_entries < 2)
            num_entries = 2;

        if (num_entries > HDA_MAX_BDL_ENTRIES)
            num_entries = HDA_MAX_BDL_ENTRIES;

        uint32_t bdl_phys = pmm_alloc_block();

        if (bdl_phys == 0) {
            if (hda_request_id == request_id)
                hda_busy = 0;

            preempt_enable();
            continue;
        }

        vmm_map_page(
            bdl_phys,
            bdl_phys,
            PTE_PCD | PTE_PRESENT | PTE_RW
        );

        hda_comm_bdl_entry_t *bdl =
            (hda_comm_bdl_entry_t *)bdl_phys;

        uint32_t allocated_entries = 0;
        uint32_t allocated_total = 0;
        uint32_t global_sample_idx = 0;

        uint8_t allocation_failed = 0;

        for (uint32_t i = 0; i < num_entries; i++) {

            uint32_t pcm_phys = pmm_alloc_block();

            if (pcm_phys == 0) {
                allocation_failed = 1;
                break;
            }

            vmm_map_page(
                pcm_phys,
                pcm_phys,
                PTE_PCD | PTE_PRESENT | PTE_RW
            );

            uint16_t *buf = (uint16_t *)pcm_phys;

            uint32_t samples_in_block =
                HDA_BLOCK_SIZE / 2;

            for (uint32_t j = 0; j < samples_in_block; j++) {

                buf[j] =
                    sample_data[global_sample_idx % sample_count];

                global_sample_idx++;
            }

            bdl[i].address_l = pcm_phys;
            bdl[i].address_h = 0;
            bdl[i].length = HDA_BLOCK_SIZE;
            bdl[i].ioc = 0;

            allocated_entries++;
            allocated_total += HDA_BLOCK_SIZE;
        }

        if (allocation_failed) {

            hda_free_audio_buffers(
                bdl_phys,
                bdl,
                allocated_entries
            );

            if (hda_request_id == request_id)
                hda_busy = 0;

            preempt_enable();
            continue;
        }

        int configure_result = hda_configure_output_stream(
            bdl_phys,
            allocated_total,
            (uint16_t)num_entries
        );

        if (configure_result != 0) {

            hda_free_audio_buffers(
                bdl_phys,
                bdl,
                allocated_entries
            );

            if (hda_request_id == request_id)
                hda_busy = 0;

            preempt_enable();
            continue;
        }

		hda_start_stream();

        preempt_enable();

        uint64_t elapsed = 0;
        uint64_t target_duration = (uint64_t)duration_ms;

        while (elapsed < target_duration) {

            uint64_t step = (target_duration - elapsed >= 20)
                ? 20
                : (target_duration - elapsed);

            sleep_millis(step);
            elapsed += step;

            preempt_disable();
            uint8_t interrupted = (hda_request_id != request_id);
            preempt_enable();

            if (interrupted)
                break;
        }

        preempt_disable();

        hda_stop_stream();

        hda_free_audio_buffers(
            bdl_phys,
            bdl,
            allocated_entries
        );

        if (hda_request_id == request_id) {
            hda_busy = 0;
        }

        preempt_enable();
    }
}

void hda_start_daemon(void) {
    if (!driver_hda) return;
    audio_worker_task = task_create(hda_audio_daemon);
}

int hda_play_sound(
    uint16_t *sample_data,
    uint32_t sample_count,
    uint32_t duration_ms
) {
    if (!driver_hda || audio_worker_task == NULL)
        return -1;

    if (sample_data == 0 ||
        sample_count == 0 ||
        duration_ms == 0)
        return -2;

    preempt_disable();

    hda_req.sample_data = sample_data;
    hda_req.sample_count = sample_count;
    hda_req.duration_ms = duration_ms;

    hda_request_id++;

    uint8_t was_idle = !hda_busy;

    hda_busy = 1;

    if (was_idle) {
        audio_worker_task->state = TASK_READY;
    }

    preempt_enable();

    return 0;
}

int hda_play_tone(uint32_t frequency, uint32_t duration_ms){
    if (!driver_hda || audio_worker_task == NULL)return -1;

    if (frequency == 0 || frequency > 24000 || duration_ms == 0) return -2;

    static uint16_t tone[48000];

    uint32_t sample_count = 48000 / frequency;
    uint32_t half_period = sample_count / 2;

    preempt_disable();

    for (uint32_t i = 0; i < sample_count; i++) {

        tone[i] = (i < half_period)
            ? 4000
            : (uint16_t)-4000;
    }

    hda_req.sample_data = tone;
    hda_req.sample_count = sample_count;
    hda_req.duration_ms = duration_ms;

    hda_request_id++;

    uint8_t was_idle = !hda_busy;

    hda_busy = 1;

    if (was_idle) {
        audio_worker_task->state = TASK_READY;
    }

    preempt_enable();

    return 0;
}

typedef struct {
    uint16_t freq;
    uint16_t ms;
} note_t;

void hda_play_notes(const note_t *notes, uint32_t count) {
    for (uint32_t i = 0; i < count; i++) {
        if (notes[i].freq > 0)
            hda_play_tone(notes[i].freq, notes[i].ms);

        sleep_millis(notes[i].ms + 20);
    }
}
