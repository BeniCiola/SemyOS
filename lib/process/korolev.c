static void korolev_init(void)
{
	boot_task.esp = NULL;
	boot_task.id = 0;
	boot_task.state = TASK_RUNNING;
	boot_task.entry = NULL;
	boot_task.stack = NULL;
	boot_task.next = &boot_task;

	task_head = &boot_task;
	current_task = &boot_task;

	next_task_id = 1;
	preempt_count = 0;
	scheduler_ready = 1;
}

static uint32_t irq_save(void)
{
	uint32_t flags;

	__asm__ volatile (
		"pushfl\n\t"
		"popl %0\n\t"
		"cli"
		: "=r"(flags)
		:
		: "memory"
	);

	return flags;
}

static void irq_restore(uint32_t flags)
{
	__asm__ volatile (
		"pushl %0\n\t"
		"popfl"
		:
		: "r"(flags)
		: "memory", "cc"
	);
}

void preempt_disable(void)
{
	uint32_t flags = irq_save();

	preempt_count++;

	irq_restore(flags);
}

void preempt_enable(void)
{
	uint32_t flags = irq_save();

	if (preempt_count > 0)
		preempt_count--;

	irq_restore(flags);
}


void korolev_tick(void)
{
	if (!scheduler_ready || preempt_count != 0)
		return;

	task_t *old = current_task;
	task_t *next = old->next;

	while (next != old && next->state != TASK_READY)
		next = next->next;

	if (next == old)
		return;

	if (old->state == TASK_RUNNING)
		old->state = TASK_READY;

	next->state = TASK_RUNNING;
	current_task = next;

	context_switch(&old->esp, next->esp);
}
