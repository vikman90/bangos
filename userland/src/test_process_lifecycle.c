#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <stdint.h>

#define TEST_ASSERT(cond, msg) do { \
    if (!(cond)) { \
        printf("[FAIL] %s at %s:%d\n", msg, __FILE__, __LINE__); \
        exit(1); \
    } \
} while (0)

#define TEST_PASS(name) printf("[PASS] %s\n", name)

static void test_fork_exit_waitpid(void) {
    const int ITERATIONS = 5;
    for (int i = 0; i < ITERATIONS; i++) {
        int expected_exit_code = 40 + i;
        pid_t pid = fork();
        TEST_ASSERT(pid >= 0, "fork() must not fail");

        if (pid == 0) {
            // Child
            exit(expected_exit_code);
        } else {
            // Parent
            int status = 0;
            pid_t waited = waitpid(pid, &status, 0);
            TEST_ASSERT(waited == pid, "waitpid() must return the child PID");
            TEST_ASSERT(WIFEXITED(status), "Child must have terminated normally");
            TEST_ASSERT(WEXITSTATUS(status) == expected_exit_code, "Child exit code must match");
        }
    }
    TEST_PASS("Process Fork + Exit Status Waitpid Propagation");
}

int main(void) {
    printf("\n======================================================\n");
    printf("        BangOS Process Lifecycle Stress Suite         \n");
    printf("======================================================\n\n");

    test_fork_exit_waitpid();

    printf("\n[SUCCESS] All process lifecycle tests evaluated to PASS!\n");
    return 0;
}
