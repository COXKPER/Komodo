/*
 *
 *      rrq.c
 *      Kernel request/response registry (asynchronous helper)
 *
 *      2026/9/7 Komodo
 *      Copyright (C) 2020 ViudiraTech, based on the Apache 2.0 license.
 *
 *      See rrq.h for the contract.  This facility is deliberately small and
 *      self-contained so it can be extended into the async kernel <->
 *      userspace service layer without disturbing the Linux ABI.
 *
 *      Wait/block handling mirrors ipc/futex.c's two-phase wait_queue idiom
 *      (wait_queue_prepare -> wait_queue_wait[_timed] -> signal re-check).
 */

#include <kernel/async/rrq.h>
#include <kernel/errno.h>
#include <kernel/printk.h>
#include <libs/std/stdbool.h>
#include <libs/std/stddef.h>
#include <libs/std/stdint.h>
#include <libs/std/string.h>
#include <mem/heap.h>
#include <process/process.h>
#include <process/sched.h>
#include <process/task.h>
#include <sync/signal.h>

static void rrq_selftest(void);

/* Registry state. */
static struct {
        spinlock_t   lock;
        rrq_entry_t *list;  /* singly linked, newest first */
        uint64_t     next;  /* next candidate request ID */
} rrq = {
        .lock = {.lock = 0},
        .list = NULL,
        .next = 1,
};

/* Whether the calling process has an interruptible pending signal. */
static bool rrq_signal_pending(void)
{
    process_t *proc = process_current();
    if (!proc) return false;
    spin_lock(&proc->signal.lock);
    bool pending = signal_has_interrupting_pending(&proc->signal);
    spin_unlock(&proc->signal.lock);
    return pending;
}

/* ------------------------------------------------------------------ */
/* Internal helpers                                                    */
/* ------------------------------------------------------------------ */

/* Generate a fresh non-zero ID that is not currently outstanding. */
static uint64_t rrq_new_id(void)
{
    uint64_t id;

    for (;;) {
        id = rrq.next++;
        if (id == 0) id = rrq.next++;   /* never hand out zero */

        bool in_use = false;
        for (rrq_entry_t *it = rrq.list; it; it = it->next) {
            if (it->id == id) {
                in_use = true;
                break;
            }
        }
        if (!in_use) return id;
    }
}

/* ------------------------------------------------------------------ */
/* API                                                                 */
/* ------------------------------------------------------------------ */

void rrq_init(void)
{
    spin_lock(&rrq.lock);
    if (!rrq.list) {
        rrq.next = 1;
    }
    spin_unlock(&rrq.lock);

    rrq_selftest();
}

int64_t rrq_register(rrq_entry_t **entry_out)
{
    rrq_entry_t *e = malloc(sizeof(rrq_entry_t));
    if (!e) return -ENOMEM;

    memset(e, 0, sizeof(rrq_entry_t));
    wait_queue_init(&e->wq);

    spin_lock(&rrq.lock);
    e->id = rrq_new_id();
    e->status = -EINPROGRESS;
    e->payload = NULL;
    e->next = rrq.list;     /* prepend */
    rrq.list = e;
    spin_unlock(&rrq.lock);

    if (entry_out) *entry_out = e;
    return (int64_t)e->id;
}

rrq_entry_t *rrq_find(uint64_t id)
{
    rrq_entry_t *it;

    spin_lock(&rrq.lock);
    for (it = rrq.list; it; it = it->next) {
        if (it->id == id) break;
    }
    spin_unlock(&rrq.lock);
    return it;
}

void rrq_complete(uint64_t id, int status, rrq_payload_t payload)
{
    rrq_entry_t *it;

    spin_lock(&rrq.lock);
    for (it = rrq.list; it; it = it->next) {
        if (it->id == id) break;
    }
    if (!it) {
        /* Unknown/expired ID: nothing to wake; drop the message. */
        spin_unlock(&rrq.lock);
        return;
    }
    it->status = status;
    it->payload = payload;
    spin_unlock(&rrq.lock);

    wait_queue_wake_one(&it->wq);
}

static int rrq_wait_common(rrq_entry_t *entry, uint64_t *deadline_ticks, rrq_payload_t *payload)
{
    int ret;

    if (!entry) return -EINVAL;

    wait_queue_prepare(&entry->wq);

    if (entry->status != -EINPROGRESS) {
        /* Already completed before we blocked. */
        wait_queue_cancel(&entry->wq);
        ret = entry->status;
    } else if (rrq_signal_pending()) {
        wait_queue_cancel(&entry->wq);
        ret = -EINTR;
    } else if (deadline_ticks) {
        ret = wait_queue_wait_timed(&entry->wq, *deadline_ticks);
        if (ret == 0) ret = entry->status;          /* woken by complete() */
        else if (rrq_signal_pending()) ret = -EINTR; /* killed while waiting */
        else ret = -EAGAIN;                          /* deadline expired */
    } else {
        wait_queue_wait(&entry->wq);
        ret = entry->status;
        if (rrq_signal_pending() && !ret) ret = -EINTR;
    }

    if (payload) *payload = entry->payload;

    /* Remove the completed/expired entry from the registry. */
    spin_lock(&rrq.lock);
    rrq_entry_t **link = &rrq.list;
    while (*link && (*link)->id != entry->id) link = &(*link)->next;
    if (*link) *link = entry->next;
    spin_unlock(&rrq.lock);

    return ret;
}

int rrq_wait(rrq_entry_t *entry, rrq_payload_t *payload)
{
    return rrq_wait_common(entry, NULL, payload);
}

int rrq_wait_timed(rrq_entry_t *entry, uint64_t deadline_ticks, rrq_payload_t *payload)
{
    return rrq_wait_common(entry, &deadline_ticks, payload);
}

void rrq_release(rrq_entry_t *entry)
{
    if (entry) free(entry);
}

/*
 * Boot-time self-test.  Verifies register/complete/wait round-trips and the
 * timed-wait timeout path without panicking: any failure is logged and counted
 * so it is caught in the boot log, but never takes the system down.
 *
 * Runs inside rrq_init(), which is called from syscall_init() BEFORE the
 * scheduler has started (sched_start).  Blocking would hang early boot, so
 * both exercises use the non-blocking paths only:
 *   - the round-trip waits on an already-completed entry (never blocks);
 *   - the timed wait passes an already-expired deadline, so
 *     wait_queue_wait_timed() takes the finish_wait_locked(TASK_WAKE_TIMEOUT)
 *     path and returns without sleeping.
 */
static void rrq_selftest(void)
{
    int failures = 0;

    /* 1. Register -> complete -> wait round-trip. */
    rrq_entry_t *e = NULL;
    rrq_payload_t payload = (rrq_payload_t)(uintptr_t)0x1234;
    int64_t id = rrq_register(&e);
    if (id <= 0 || !e) {
        plogk("rrq: selftest FAILED to register (id=%lld)\n", (long long)id);
        failures++;
        return;
    }

    /* Complete it unconditionally (simulates a service replying). */
    rrq_complete((uint64_t)id, EOK, payload);

    /* Wait on the entry we hold: registry already detached the completed
     * entry on complete(), so we observe EOK immediately (no block). */
    int status = rrq_wait(e, &payload);
    if (status != EOK) {
        plogk("rrq: selftest FAILED round-trip status=%d (want EOK)\n", status);
        failures++;
    }
    if (payload != (rrq_payload_t)(uintptr_t)0x1234) {
        plogk("rrq: selftest FAILED payload round-trip\n");
        failures++;
    }
    rrq_release(e);

    /* 2. rrq_find() must no longer see the completed ID. */
    int64_t done_id = id;
    id = 0;
    if (rrq_find((uint64_t)done_id)) {
        plogk("rrq: selftest FAILED completed ID still findable\n");
        failures++;
    }

    /* 3. Timed wait must time out with -EAGAIN for a never-completed ID. */
    e = NULL;
    id = rrq_register(&e);
    if (id <= 0 || !e) {
        plogk("rrq: selftest FAILED second register\n");
        failures++;
        return;
    }
    status = rrq_wait_timed(e, sched_ticks(), &payload);
    if (status != -EAGAIN) {
        plogk("rrq: selftest FAILED timed timeout status=%d (want -EAGAIN)\n", status);
        failures++;
    }
    rrq_release(e);

    plogk("rrq: selftest %s (%d failures)\n",
          failures ? "FAILED" : "passed", failures);
}
