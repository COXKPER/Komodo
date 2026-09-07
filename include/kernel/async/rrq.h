/*
 *
 *      rrq.h
 *      Kernel request/response registry (asynchronous helper)
 *
 *      2026/9/7 Komodo
 *      Copyright (C) 2020 ViudiraTech, based on the Apache 2.0 license.
 *
 *      A small, self-contained kernel facility that pairs a monotonic, non-zero
 *      opaque request ID with a caller blocking on a two-phase wait queue until
 *      a completion arrives.  Intended as the foundation for future kernel <->
 *      userspace async service calls (VFS-as-service, net, GPU, ...) WITHOUT a
 *      custom Linux ABI: the public ABI stays Linux; this facility is internal
 *      to the kernel and is used by the code that marshals replies.
 *
 *      Modeled on the request/response ID pattern from the Lux/Komodo
 *      microkernel, adapted to Komodo's spinlock + wait_queue idioms.
 */

#ifndef INCLUDE_RRQ_H_
#define INCLUDE_RRQ_H_

#include <libs/std/stdint.h>
#include <process/task.h>
#include <sync/spin_lock.h>

/* Opaque caller-supplied payload carried alongside the reply. */
typedef void *rrq_payload_t;

/* A single outstanding request/response pair. */
typedef struct rrq_entry {
        uint64_t         id;        /* non-zero opaque request ID */
        int              status;    /* reply status (errno, -EINPROGRESS while pending) */
        rrq_payload_t    payload;   /* caller-owned, copied on complete() */
        spinlock_t       lock;
        wait_queue_t     wq;
        struct rrq_entry *next;     /* registry list linkage */
} rrq_entry_t;

/* Initialize the request/response registry (idempotent). */
void rrq_init(void);

/*
 * Register a new outstanding request.  Returns the non-zero request ID, or a
 * negative errno on failure.  The returned entry is owned by the registry
 * until rrq_complete()/rrq_release().
 *
 *   If `entry` (out) is non-NULL, it receives the backing entry pointer so the
 *   caller can wait on it.  The caller must NOT free it.
 */
int64_t rrq_register(rrq_entry_t **entry);

/* Complete a pending request.  Wakes the waiting caller (if any). */
void rrq_complete(uint64_t id, int status, rrq_payload_t payload);

/*
 * Wait (blocking) for a request to complete.  Returns the completion status,
 * or -EINTR if killed while waiting.  On success `*payload` receives the
 * payload passed to rrq_complete().
 */
int rrq_wait(rrq_entry_t *entry, rrq_payload_t *payload);

/* Timed variant.  Returns completion status, -EAGAIN on timeout, -EINTR on kill. */
int rrq_wait_timed(rrq_entry_t *entry, uint64_t deadline_ticks, rrq_payload_t *payload);

/*
 * Look up a pending entry by ID (e.g. a service completing a request).
 * Returns NULL if the ID is not outstanding.
 */
rrq_entry_t *rrq_find(uint64_t id);

/* Pop a payload from the component.  Frees registry-owned storage for id=0 only. */
void rrq_release(rrq_entry_t *entry);

#endif /* INCLUDE_RRQ_H_ */
