#include <stdio.h>
#include <stdint.h>
#include <time.h>

// 1. Define Opcodes
#define OP_RETURN 0
#define OP_LIT    1
#define OP_DUP    2
#define OP_ADD    3
#define OP_DROP   4

#define MAX_PROGRAM_SIZE 1000

// 2. Setup Virtual Machine Environment
static uint8_t program[MAX_PROGRAM_SIZE];
static int32_t data_stack[1024];
static int32_t sp = -1;
static int32_t ip = 0;

/**
 * 3. Ultra-tight Forth Inner Interpreter
 * Clang compiles this switch statement into an optimized direct jump table (br).
 */
void interpret() {
    ip = 0;
    while (1) {
        switch (program[ip++]) {
            case OP_LIT:
                data_stack[++sp] = program[ip++];
                break;
            case OP_DUP:
                data_stack[sp + 1] = data_stack[sp];
                sp++;
                break;
            case OP_ADD:
                data_stack[sp - 1] = data_stack[sp - 1] + data_stack[sp];
                sp--;
                break;
            case OP_DROP:
                sp--;
                break;
            case OP_RETURN:
                return;
        }
    }
}

int main() {
    // 4. Compile the identical Forth sequence into the byte array
    int ops_per_run = 0;
    int compile_offset = 0;

    for (int i = 0; i < 150; i++) {
        program[compile_offset++] = OP_LIT;  ops_per_run++;
        program[compile_offset++] = 5;       // Data payload (Not a 'next' op)
        program[compile_offset++] = OP_DUP;  ops_per_run++;
        program[compile_offset++] = OP_ADD;  ops_per_run++;
        program[compile_offset++] = OP_DROP; ops_per_run++;
    }
    program[compile_offset++] = OP_RETURN;   ops_per_run++;

    const long iterations = 1000000; // 1 Million iterations
    printf("Executing Clang interpreter loop %ld times...\n", iterations);

    // 5. External Timing Setup
    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);

    for (long i = 0; i < iterations; i++) {
        interpret();
    }

    clock_gettime(CLOCK_MONOTONIC, &end);

    // 6. Metrics Calculation
    double final_elapsed_sec = (end.tv_sec - start.tv_sec) + 
                               (end.tv_nsec - start.tv_nsec) / 1e9;
    
    long long total_next_executed = (long long)ops_per_run * iterations;
    double next_per_second = (double)total_next_executed / final_elapsed_sec;

    printf("\n--- Clang C Loop Execution Results ---\n");
    printf("Total Run Time:       %.6f seconds\n", final_elapsed_sec);
    printf("Total 'next' Executed: %lld\n", total_next_executed);
    printf("Operations/Second:     %.0f Hz\n", next_per_second);

    return 0;
}
