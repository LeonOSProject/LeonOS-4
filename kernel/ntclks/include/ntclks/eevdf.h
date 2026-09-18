/* LeonOS EEVDF policy. Time values are microseconds, serialized by sched lock. */
#ifndef NTCLKS_EEVDF_H
#define NTCLKS_EEVDF_H
#include <ntclks/types.h>

#define EEVDF_SLICE_US 10000ULL
struct eevdf_entity {
    struct eevdf_entity *next;
    int64_t vruntime, deadline, lag;
    uint64_t remainder, runtime;
    uint32_t weight, cpu;
    bool initialized, queued, delayed;
};
struct eevdf_queue { struct eevdf_entity *head; int64_t clock; };
/* Linux-compatible nice weights; policy code is independently implemented.
 * Reference: Linux 6.12 fair.c (eligibility, placement, delayed dequeue). */
static inline uint32_t eevdf_weight(int nice)
{
    static const uint32_t weights[40] = {
        88761,71755,56483,46273,36291,29154,23254,18705,14949,11916,
        9548,7620,6100,4904,3906,3121,2501,1991,1586,1277,
        1024,820,655,526,423,335,272,215,172,137,110,87,70,56,45,36,29,23,18,15
    };
    if (nice < -20) nice = -20;
    if (nice > 19) nice = 19;
    return weights[nice + 20];
}
static inline int64_t eevdf_slice(const struct eevdf_entity *e)
{ return (int64_t)(EEVDF_SLICE_US * 1024 / e->weight); }

static inline int64_t eevdf_average(struct eevdf_queue *q, uint64_t *load)
{
    int64_t sum = 0;
    *load = 0;
    for (struct eevdf_entity *e = q->head; e; e = e->next) {
        sum += (e->vruntime - q->clock) * e->weight;
        *load += e->weight;
    }
    if (!*load) return q->clock;
    /* Floor, including negative values, keeps an entity at V eligible. */
    return q->clock + sum / (int64_t)*load - (sum < 0 && sum % (int64_t)*load != 0);
}

static inline void eevdf_dequeue(struct eevdf_queue *q, struct eevdf_entity *e)
{
    uint64_t load;
    int64_t average = eevdf_average(q, &load);
    e->lag = average - e->vruntime;
    int64_t limit = 2 * eevdf_slice(e);
    if (e->lag > limit) e->lag = limit;
    if (e->lag < -limit) e->lag = -limit;
    struct eevdf_entity **link = &q->head;
    while (*link && *link != e) link = &(*link)->next;
    if (*link) *link = e->next;
    e->next = NULL;
    e->queued = false;
    e->delayed = false;
    q->clock = average;
}

static inline void eevdf_enqueue(struct eevdf_queue *q, struct eevdf_entity *e, int nice)
{
    uint64_t load;
    int64_t average = eevdf_average(q, &load);
    e->weight = eevdf_weight(nice);
    int64_t lag = e->initialized && load ? e->lag : 0;
    if (load) lag += lag * e->weight / (int64_t)load;
    e->vruntime = average - lag;
    e->deadline = e->vruntime + eevdf_slice(e) / (e->initialized ? 1 : 2);
    e->initialized = true;
    e->queued = true;
    e->delayed = false;
    e->remainder = 0;
    e->next = q->head;
    q->head = e;
    q->clock = average;
}
static inline void eevdf_account(struct eevdf_entity *e, uint64_t delta)
{
    e->runtime += delta;
    uint64_t scaled = delta * 1024 + e->remainder;
    e->vruntime += (int64_t)(scaled / e->weight);
    e->remainder = scaled % e->weight;
    if (e->vruntime >= e->deadline) e->deadline = e->vruntime + eevdf_slice(e);
}

static inline void eevdf_sleep(struct eevdf_queue *q, struct eevdf_entity *e)
{
    uint64_t load;
    if (e->vruntime > eevdf_average(q, &load)) e->delayed = true;
    else eevdf_dequeue(q, e);
}

static inline void eevdf_yield(struct eevdf_entity *e)
{ e->deadline += eevdf_slice(e); }

static inline void eevdf_reweight(struct eevdf_queue *q, struct eevdf_entity *e, int nice)
{
    uint32_t weight = eevdf_weight(nice), old = e->weight;
    if (weight == old) return;
    if (!e->queued) {
        /* Stored lag is in virtual units of the previous weight. */
        e->lag = e->lag * old / weight;
        e->weight = weight;
        e->remainder = 0;
        return;
    }
    int64_t remaining = e->deadline - e->vruntime;
    bool delayed = e->delayed;
    eevdf_dequeue(q, e);
    e->lag = e->lag * old / weight;
    eevdf_enqueue(q, e, nice);
    e->deadline = e->vruntime + remaining * old / weight;
    e->delayed = delayed;
}

static inline struct eevdf_entity *eevdf_pick(struct eevdf_queue *q)
{
    for (;;) {
        uint64_t load;
        int64_t average = eevdf_average(q, &load);
        struct eevdf_entity *best = NULL, *delayed = NULL;
        for (struct eevdf_entity *e = q->head; e; e = e->next) {
            if (e->vruntime > average) continue;
            if (e->delayed) { delayed = e; break; }
            if (!best || e->deadline < best->deadline) best = e;
        }
        if (!delayed) { q->clock = average; return best; }
        eevdf_dequeue(q, delayed);
        /* Once sleep debt has decayed, it must not become a wakeup bonus. */
        delayed->lag = 0;
    }
}
#endif
