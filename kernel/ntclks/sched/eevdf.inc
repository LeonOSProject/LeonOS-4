/* EEVDF integration; included by sched.c and serialized by scheduler_lock.
 * Keep task lifecycle/state ownership in sched.c. The queues contain both
 * runnable entities and sleepers whose negative lag has not decayed yet. */
static struct eevdf_queue fair_queues[SMP_MAX_CPUS];
static struct task *fair_running[SMP_MAX_CPUS];
static uint64_t fair_started[SMP_MAX_CPUS];
static uint64_t fair_clock;

static uint64_t fair_now(void)
{
    uint64_t now = time_uptime_us();
    /* PIT interpolation can cross an unserviced tick with IRQs masked. */
    if (now > fair_clock) fair_clock = now;
    return fair_clock;
}

static struct task *fair_task(struct eevdf_entity *entity)
{
    return (struct task *)((char *)entity - __builtin_offsetof(struct task, fair));
}

static bool fair_runnable(const struct task *task)
{
    return task->kind == TASK_KIND_USER &&
        (task->state == TASK_READY || task->state == TASK_RUNNING) &&
        (!task->vfork_child || kernel_signal_fatal_pending(task)) &&
        (task->entry || (task->image && task->image_len) || (task->flags & TASK_FLAG_PENDING_LOAD)) &&
        task->stack_top && sched_task_as(task)->cr3;
}

static void fair_account_cpu(uint32_t cpu, uint64_t now)
{
    struct task *task = fair_running[cpu];
    if (task && task->fair.initialized && now > fair_started[cpu])
        eevdf_account(&task->fair, now - fair_started[cpu]);
    fair_started[cpu] = now;
}

static void fair_stop_cpu(uint32_t cpu)
{
    fair_account_cpu(cpu, fair_now());
    fair_running[cpu] = NULL;
}

static uint32_t fair_count(uint32_t cpu)
{
    uint32_t count = 0;
    for (struct eevdf_entity *e = fair_queues[cpu].head; e; e = e->next)
        if (!e->delayed) ++count;
    return count;
}

static uint32_t fair_choose_cpu(const struct task *task)
{
    uint32_t best = SCHED_CPU_NONE, count = UINT32_MAX;
    uint32_t cpus = smp_cpu_count();
    if (!cpus) cpus = 1;
    if (cpus > SMP_MAX_CPUS) cpus = SMP_MAX_CPUS;
    for (uint32_t cpu = 0; cpu < cpus; ++cpu) {
        if (!smp_cpu_online(cpu) || !task_cpu_allowed(task, cpu)) continue;
        uint32_t n = fair_count(cpu);
        if (n < count || (n == count && cpu == task->last_cpu)) {
            best = cpu; count = n;
        }
    }
    return best;
}

static void fair_forget(struct task *task)
{
    if (task->fair.queued) eevdf_dequeue(&fair_queues[task->fair.cpu], &task->fair);
    for (uint32_t cpu = 0; cpu < SMP_MAX_CPUS; ++cpu)
        if (fair_running[cpu] == task) fair_running[cpu] = NULL;
}

static void fair_sync(void)
{
    uint64_t now = fair_now();
    for (uint32_t cpu = 0; cpu < SMP_MAX_CPUS; ++cpu) fair_account_cpu(cpu, now);
    for (uint32_t i = 0; i < task_count; ++i) {
        struct task *task = tasks[i];
        struct eevdf_entity *e = &task->fair;
        bool runnable = fair_runnable(task);
        if (e->queued && !runnable) {
            if (task->state == TASK_EXITED) eevdf_dequeue(&fair_queues[e->cpu], e);
            else if (!e->delayed) eevdf_sleep(&fair_queues[e->cpu], e);
        }
        if (!runnable) continue;
        if (e->queued && e->weight != eevdf_weight(task->priority)) {
            eevdf_reweight(&fair_queues[e->cpu], e, task->priority);
        }
        if (e->queued && !task_cpu_allowed(task, e->cpu) && task->running_cpu == SCHED_CPU_NONE)
            eevdf_dequeue(&fair_queues[e->cpu], e);
        if (!e->queued) {
            uint32_t cpu = task->running_cpu != SCHED_CPU_NONE ? task->running_cpu : fair_choose_cpu(task);
            if (cpu == SCHED_CPU_NONE) continue;
            e->cpu = cpu;
            eevdf_enqueue(&fair_queues[cpu], e, task->priority);
        } else e->delayed = false;
    }
}

static struct task *fair_pick_cpu(uint32_t cpu)
{
    struct eevdf_entity *e = eevdf_pick(&fair_queues[cpu]);
    if (!e) return NULL;
    struct task *task = fair_task(e);
    if (task->state != TASK_READY ||
        (task->running_cpu != SCHED_CPU_NONE && task->running_cpu != cpu) ||
        !task_cpu_allowed(task, cpu)) return NULL;
    return task;
}

static struct task *fair_select(uint32_t cpu)
{
    fair_sync();
    struct task *task = fair_pick_cpu(cpu);
    if (task) return task;
    /* Idle CPUs pull runnable, unreserved work. Transfer bounded lag, not an
     * absolute virtual timestamp from a different CPU's fairness domain. */
    struct task *steal = NULL;
    uint32_t largest = 0;
    for (uint32_t i = 0; i < task_count; ++i) {
        struct task *candidate = tasks[i];
        struct eevdf_entity *e = &candidate->fair;
        if (!e->queued || e->delayed || e->cpu == cpu || candidate->state != TASK_READY ||
            candidate->running_cpu != SCHED_CPU_NONE || !task_cpu_allowed(candidate, cpu)) continue;
        uint32_t count = fair_count(e->cpu);
        if (!steal || count > largest) { steal = candidate; largest = count; }
    }
    if (steal) {
        struct eevdf_entity *e = &steal->fair;
        eevdf_dequeue(&fair_queues[e->cpu], e);
        e->cpu = cpu;
        eevdf_enqueue(&fair_queues[cpu], e, steal->priority);
        task = fair_pick_cpu(cpu);
    }
    return task;
}
