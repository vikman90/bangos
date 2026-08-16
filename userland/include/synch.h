#ifndef SYNCH_H
#define SYNCH_H

#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/syscall.h>

#ifndef SYS_CLONE
#define SYS_CLONE 56
#endif

#ifndef SYS_EXIT
#define SYS_EXIT 60
#endif

#ifndef SYS_FUTEX
#define SYS_FUTEX 202
#endif

#define FUTEX_WAIT 0
#define FUTEX_WAKE 1

#define CLONE_VM      0x00000100
#define CLONE_FS      0x00000200
#define CLONE_FILES   0x00000400
#define CLONE_SIGHAND 0x00000800
#define CLONE_THREAD  0x00010000

/* ==================================================================== */
/* Spinlock Primitives                                                  */
/* ==================================================================== */
typedef struct {
    volatile int lock;
} spinlock_t;

static inline void spinlock_init(spinlock_t *s) {
    s->lock = 0;
}

static inline void spinlock_lock(spinlock_t *s) {
    while (__atomic_test_and_set(&s->lock, __ATOMIC_ACQUIRE)) {
        __asm__ volatile ("pause");
    }
}

static inline void spinlock_unlock(spinlock_t *s) {
    __atomic_clear(&s->lock, __ATOMIC_RELEASE);
}

/* ==================================================================== */
/* Mutex Primitives (Fast Atomic CAS + SYS_FUTEX Fallback)              */
/* ==================================================================== */
typedef struct {
    volatile int val;
} mutex_t;

static inline void mutex_init(mutex_t *m) {
    m->val = 0;
}

static inline void mutex_lock(mutex_t *m) {
    int c = 0;
    if (__atomic_compare_exchange_n(&m->val, &c, 1, 0, __ATOMIC_ACQUIRE, __ATOMIC_RELAXED)) {
        return;
    }
    if (c != 2) {
        c = __atomic_exchange_n(&m->val, 2, __ATOMIC_ACQUIRE);
    }
    while (c != 0) {
        syscall(SYS_FUTEX, &m->val, FUTEX_WAIT, 2, NULL, NULL, 0);
        c = __atomic_exchange_n(&m->val, 2, __ATOMIC_ACQUIRE);
    }
}

static inline void mutex_unlock(mutex_t *m) {
    if (__atomic_fetch_sub(&m->val, 1, __ATOMIC_RELEASE) != 1) {
        m->val = 0;
        syscall(SYS_FUTEX, &m->val, FUTEX_WAKE, 1, NULL, NULL, 0);
    }
}

/* ==================================================================== */
/* Semaphore Primitives                                                 */
/* ==================================================================== */
typedef struct {
    volatile int count;
    mutex_t      lock;
} sem_t;

static inline void sem_init(sem_t *s, int val) {
    s->count = val;
    mutex_init(&s->lock);
}

static inline void sem_wait(sem_t *s) {
    while (1) {
        mutex_lock(&s->lock);
        if (s->count > 0) {
            s->count--;
            mutex_unlock(&s->lock);
            return;
        }
        mutex_unlock(&s->lock);
        syscall(SYS_FUTEX, &s->count, FUTEX_WAIT, 0, NULL, NULL, 0);
    }
}

static inline void sem_post(sem_t *s) {
    mutex_lock(&s->lock);
    s->count++;
    mutex_unlock(&s->lock);
    syscall(SYS_FUTEX, &s->count, FUTEX_WAKE, 1, NULL, NULL, 0);
}

/* ==================================================================== */
/* Thread Management Helpers (SYS_CLONE & SYS_FUTEX)                    */
/* ==================================================================== */
#define THREAD_STACK_SIZE (64 * 1024)

typedef struct thread_handle {
    void *(*func)(void *);
    void *arg;
    void *ret;
    int   tid;
    volatile int exited;
    void *stack_mem;
} thread_handle_t;

static int clone_thread(unsigned long flags, void *child_stack, int *ptid, int *ctid, void *newtls) {
    long ret;
    __asm__ volatile (
        "mov %1, %%rdi\n"
        "mov %2, %%rsi\n"
        "mov %3, %%rdx\n"
        "mov %4, %%r10\n"
        "mov %5, %%r8\n"
        "mov $56, %%rax\n"
        "syscall\n"
        : "=a"(ret)
        : "g"(flags), "g"(child_stack), "g"(ptid), "g"(ctid), "g"(newtls)
        : "rcx", "r11", "memory", "rdi", "rsi", "rdx", "r10", "r8"
    );
    return (int)ret;
}

static void thread_trampoline(thread_handle_t *h) {
    void *r = h->func(h->arg);
    h->ret = r;
    h->exited = 1;
    syscall(SYS_FUTEX, &h->exited, FUTEX_WAKE, 1, NULL, NULL, 0);
    syscall(SYS_EXIT, 0);
}

static inline int thread_create_handle(thread_handle_t *handle, void *(*func)(void *), void *arg) {
    if (!handle) return -1;

    void *stack_mem = malloc(THREAD_STACK_SIZE);
    if (!stack_mem) return -1;

    handle->func = func;
    handle->arg = arg;
    handle->ret = NULL;
    handle->exited = 0;
    handle->stack_mem = stack_mem;

    uint64_t *sp = (uint64_t *)((uintptr_t)stack_mem + THREAD_STACK_SIZE);
    sp = (uint64_t *)((uintptr_t)sp & ~0xFULL);
    *(--sp) = 0; // 8-byte padding for 16-byte System V ABI alignment
    *(--sp) = (uint64_t)handle;

    unsigned long flags = CLONE_VM | CLONE_FS | CLONE_FILES | CLONE_SIGHAND | CLONE_THREAD;

    int tid = clone_thread(flags, (void *)sp, &handle->tid, NULL, NULL);
    if (tid < 0) {
        free(stack_mem);
        return tid;
    }

    if (tid == 0) {
        thread_handle_t *h;
        __asm__ volatile ("mov (%%rsp), %0" : "=r"(h));
        thread_trampoline(h);
        return 0;
    }

    handle->tid = tid;
    return tid;
}

static inline void thread_join_handle(thread_handle_t *handle) {
    if (!handle) return;
    while (!handle->exited) {
        syscall(SYS_FUTEX, &handle->exited, FUTEX_WAIT, 0, NULL, NULL, 0);
    }
    if (handle->stack_mem) {
        free(handle->stack_mem);
        handle->stack_mem = NULL;
    }
}

#endif /* SYNCH_H */
