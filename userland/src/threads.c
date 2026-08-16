#include "tui.h"
#include "synch.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define ITERATIONS 5000

static volatile int unsynced_counter = 0;
static volatile int synced_counter = 0;
static mutex_t counter_mutex;

static void *race_worker(void *arg) {
    (void)arg;
    for (int i = 0; i < ITERATIONS; i++) {
        // Read, modify, write without synchronization
        int temp = unsynced_counter;
        // Small yield/delay to encourage interleaving across preemption ticks
        for (volatile int j = 0; j < 20; j++);
        unsynced_counter = temp + 1;
    }
    return NULL;
}

static void *mutex_worker(void *arg) {
    (void)arg;
    for (int i = 0; i < ITERATIONS; i++) {
        mutex_lock(&counter_mutex);
        synced_counter++;
        mutex_unlock(&counter_mutex);
    }
    return NULL;
}

/* ==================================================================== */
/* Producer - Consumer Queue Demo                                       */
/* ==================================================================== */
#define QUEUE_SIZE 8
#define TOTAL_ITEMS 10

typedef struct {
    int   items[QUEUE_SIZE];
    int   head;
    int   tail;
    sem_t sem_empty;
    sem_t sem_full;
    mutex_t mutex;
} bounded_queue_t;

static bounded_queue_t bq;

static void *producer_func(void *arg) {
    (void)arg;
    for (int i = 1; i <= TOTAL_ITEMS; i++) {
        sem_wait(&bq.sem_empty);
        mutex_lock(&bq.mutex);

        bq.items[bq.tail] = i * 10;
        bq.tail = (bq.tail + 1) % QUEUE_SIZE;
        printf(ANSI_GREEN "  [Producer] Enqueued value: %d" ANSI_RESET "\n", i * 10);
        fflush(stdout);

        mutex_unlock(&bq.mutex);
        sem_post(&bq.sem_full);
    }
    return NULL;
}

static void *consumer_func(void *arg) {
    (void)arg;
    for (int i = 1; i <= TOTAL_ITEMS; i++) {
        sem_wait(&bq.sem_full);
        mutex_lock(&bq.mutex);

        int val = bq.items[bq.head];
        bq.head = (bq.head + 1) % QUEUE_SIZE;
        printf(ANSI_CYAN "  [Consumer] Dequeued value: %d" ANSI_RESET "\n", val);
        fflush(stdout);

        mutex_unlock(&bq.mutex);
        sem_post(&bq.sem_empty);
    }
    return NULL;
}

static void run_race_demo(void) {
    unsynced_counter = 0;
    printf("\n" ANSI_BOLD "[Test 1/3] Concurrent Race Condition (Unsynchronized Counter)" ANSI_RESET "\n");
    printf("Spawning 2 threads, each performing %d increments on shared memory...\n", ITERATIONS);
    fflush(stdout);

    thread_handle_t t1, t2;
    thread_create_handle(&t1, race_worker, NULL);
    thread_create_handle(&t2, race_worker, NULL);

    thread_join_handle(&t1);
    thread_join_handle(&t2);

    int expected = ITERATIONS * 2;
    printf("  - Expected Total: %d\n", expected);
    printf("  - Actual Total:   %d\n", unsynced_counter);
    if (unsynced_counter != expected) {
        printf(ANSI_YELLOW "  -> Result: DATA RACE DETECTED! Context preemption interleaved writes (%d lost updates)." ANSI_RESET "\n",
               expected - unsynced_counter);
    } else {
        printf("  -> Result: Total matched (try with higher iterations).\n");
    }
}

static void run_mutex_demo(void) {
    synced_counter = 0;
    mutex_init(&counter_mutex);

    printf("\n" ANSI_BOLD "[Test 2/3] Mutex Synchronized Concurrent Increments" ANSI_RESET "\n");
    printf("Spawning 2 threads with mutex_lock() / mutex_unlock() on %d increments...\n", ITERATIONS);
    fflush(stdout);

    thread_handle_t t1, t2;
    thread_create_handle(&t1, mutex_worker, NULL);
    thread_create_handle(&t2, mutex_worker, NULL);

    thread_join_handle(&t1);
    thread_join_handle(&t2);

    int expected = ITERATIONS * 2;
    printf("  - Expected Total: %d\n", expected);
    printf("  - Actual Total:   %d\n", synced_counter);
    if (synced_counter == expected) {
        printf(ANSI_GREEN "  -> Result: MUTEX SYNCHRONIZATION SUCCESS! 100%% consistency preserved." ANSI_RESET "\n");
    } else {
        printf(ANSI_RED "  -> Result: MUTEX FAILURE! Count discrepancy: %d\n" ANSI_RESET, expected - synced_counter);
    }
}

static void run_prod_cons_demo(void) {
    printf("\n" ANSI_BOLD "[Test 3/3] Producer-Consumer Queue (Semaphores & Mutexes)" ANSI_RESET "\n");
    printf("Spawning Producer & Consumer threads with bounded buffer (capacity %d)...\n", QUEUE_SIZE);
    fflush(stdout);

    bq.head = 0;
    bq.tail = 0;
    sem_init(&bq.sem_empty, QUEUE_SIZE);
    sem_init(&bq.sem_full, 0);
    mutex_init(&bq.mutex);

    thread_handle_t prod, cons;
    thread_create_handle(&prod, producer_func, NULL);
    thread_create_handle(&cons, consumer_func, NULL);

    thread_join_handle(&prod);
    thread_join_handle(&cons);

    printf(ANSI_GREEN "  -> Result: Producer-Consumer pipeline completed %d synchronized operations." ANSI_RESET "\n", TOTAL_ITEMS);
}

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;

    while (1) {
        printf("\n" ANSI_BOLD ANSI_BLUE "======================================================================" ANSI_RESET "\n");
        printf(ANSI_BOLD ANSI_CYAN "       BangOS Multithreading & Synchronization Suite (/bin/threads)    " ANSI_RESET "\n");
        printf(ANSI_DIM "     Kernel CLONE_VM shared address space, Futexes, Mutexes & Semaphores" ANSI_RESET "\n");
        printf(ANSI_BOLD ANSI_BLUE "======================================================================" ANSI_RESET "\n\n");

        printf("  " ANSI_BOLD ANSI_GREEN "[1]" ANSI_RESET " Race Condition Demonstration     (Unsynchronized Threads)\n");
        printf("  " ANSI_BOLD ANSI_GREEN "[2]" ANSI_RESET " Mutex Synchronized Increment     (Atomic CAS + Futex)\n");
        printf("  " ANSI_BOLD ANSI_GREEN "[3]" ANSI_RESET " Producer-Consumer Queue Pipeline (Counting Semaphores)\n");
        printf("  " ANSI_BOLD ANSI_GREEN "[4]" ANSI_RESET " Run All Synchronization Tests\n");
        printf("  " ANSI_BOLD ANSI_RED   "[5]" ANSI_RESET " Return to Main Menu\n\n");

        printf(ANSI_BOLD ANSI_YELLOW "Select an option [1-5]: " ANSI_RESET);
        fflush(stdout);

        char line[64];
        if (tui_read_line(line, sizeof(line)) != 0) {
            continue;
        }

        if (strcmp(line, "1") == 0) {
            run_race_demo();
            tui_pause();
        } else if (strcmp(line, "2") == 0) {
            run_mutex_demo();
            tui_pause();
        } else if (strcmp(line, "3") == 0) {
            run_prod_cons_demo();
            tui_pause();
        } else if (strcmp(line, "4") == 0) {
            run_race_demo();
            run_mutex_demo();
            run_prod_cons_demo();
            tui_pause();
        } else if (strcmp(line, "5") == 0 || strcmp(line, "exit") == 0 || strcmp(line, "quit") == 0) {
            break;
        } else if (strlen(line) > 0) {
            printf(ANSI_RED "Invalid option '%s'. Please select 1-5.\n" ANSI_RESET, line);
            tui_pause();
        }
    }

    return 0;
}
