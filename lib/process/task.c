
#define STACK_SIZE 4096

typedef enum {
	TASK_READY,
	TASK_RUNNING,
	TASK_BLOCKED,
	TASK_TERMINATED
} task_state_t;

typedef struct task {
	uint32_t *esp;
	uint32_t id;
	task_state_t state;

	void (*entry)(void);

	uint8_t *stack;
	struct task *next;
} task_t;

extern void context_switch(uint32_t **old_esp, uint32_t *new_esp);

void preempt_disable(void);
void preempt_enable(void);
void korolev_tick(void);

static task_t boot_task;
static task_t *task_head = NULL;
static task_t *current_task = NULL;

static uint32_t next_task_id = 1;
static volatile uint32_t preempt_count = 0;
static int scheduler_ready = 0;

static void task_bootstrap(void);
static void task_exit(void);
static void task_reap_terminated(void);


static void task_prepare_stack(task_t *task)
{
	uintptr_t top = (uintptr_t)(task->stack + STACK_SIZE);
	uint32_t *sp = (uint32_t *)top;

	*--sp = 0;
	*--sp = (uint32_t)(uintptr_t)task_bootstrap;

	*--sp = 0;
	*--sp = 0;
	*--sp = 0;
	*--sp = 0;

	task->esp = sp;
}


static void task_reap_terminated(void)
{
	preempt_disable();

	task_t *prev = task_head;
	task_t *cur = prev->next;

	while (cur != task_head) {
		if (cur->state == TASK_TERMINATED &&
			cur != current_task) {

			task_t *dead = cur;

			cur = cur->next;
			prev->next = cur;

			kfree(dead->stack);
			kfree(dead);
		} else {
			prev = cur;
			cur = cur->next;
		}
	}

	preempt_enable();
}


task_t *task_create(void (*entry)(void))
{
	if (!scheduler_ready || entry == NULL)
		return NULL;

	task_reap_terminated();

	task_t *task = kmalloc(sizeof(task_t));

	if (task == NULL)
		return NULL;

	task->stack = kmalloc(STACK_SIZE);

	if (task->stack == NULL) {
		kfree(task);
		return NULL;
	}

	task->entry = entry;
	task->state = TASK_READY;
	task->next = NULL;

	task_prepare_stack(task);

	preempt_disable();

	task->id = next_task_id++;

	task->next = task_head->next;
	task_head->next = task;

	preempt_enable();

	return task;
}


static void task_bootstrap(void)
{
	__asm__ volatile ("sti");

	current_task->entry();

	task_exit();
}


static void task_exit(void)
{
	preempt_disable();

	current_task->state = TASK_TERMINATED;

	preempt_enable();

	while(1){
		__asm__ volatile ("sti; hlt");
	}
}
