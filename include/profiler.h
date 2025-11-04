#ifndef FLS_PROFILER_H
#define FLS_PROFILER_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#define MAX_MEMORY_PLANS 8192
#define MAX_LOOP_ITERATIONS 10000000
#define PREFLIGHT_TIMEOUT_MS 5000
#define MAX_RECURSION_DEPTH 256
#define LOOP_PROGRESS_CHECK_INTERVAL 100000

typedef struct {
    uint64_t token_id;
    size_t predicted_size;
    size_t max_observed_size;
    uint32_t growth_events;
    uint32_t access_count;
} MemoryPlan;

typedef struct {
    uint64_t loop_id;
    uint64_t iteration_count;
    uint64_t max_iterations;
    uint64_t last_check_iteration;
    uint64_t last_check_stack_depth;
    uint64_t last_check_allocations;
    bool potentially_infinite;
    bool making_progress;
} LoopProfile;

typedef struct {
    MemoryPlan* plans;
    int plan_count;
    int plan_capacity;
    
    LoopProfile* loops;
    int loop_count;
    int loop_capacity;
    
    uint64_t total_allocations;
    uint64_t total_bytes_requested;
    uint64_t max_stack_depth;
    uint64_t max_recursion_depth;
    uint64_t output_operations;
    
    bool profiling_mode;
    bool preflight_complete;
    bool infinite_loop_detected;
    
    uint64_t start_time_ms;
    uint64_t elapsed_time_ms;
} Profiler;

void initProfiler(Profiler* profiler);
void freeProfiler(Profiler* profiler);
void resetProfiler(Profiler* profiler);

MemoryPlan* recordAllocation(Profiler* profiler, uint64_t token_id, size_t size);
MemoryPlan* findMemoryPlan(Profiler* profiler, uint64_t token_id);
void recordGrowth(Profiler* profiler, uint64_t token_id, size_t new_size);

LoopProfile* recordLoopIteration(Profiler* profiler, uint64_t loop_id);
bool checkLoopSafety(Profiler* profiler, uint64_t loop_id, uint64_t stack_depth);

bool checkTimeout(Profiler* profiler);
bool checkRecursionDepth(Profiler* profiler, uint64_t depth);

void dumpProfile(Profiler* profiler);

#endif
