#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>

#include "profiler.h"

static uint64_t getTimeMs() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (uint64_t)(tv.tv_sec * 1000 + tv.tv_usec / 1000);
}

void initProfiler(Profiler* profiler) {
    profiler->plan_capacity = 256;
    profiler->plans = (MemoryPlan*)malloc(sizeof(MemoryPlan) * profiler->plan_capacity);
    profiler->plan_count = 0;
    
    profiler->loop_capacity = 64;
    profiler->loops = (LoopProfile*)malloc(sizeof(LoopProfile) * profiler->loop_capacity);
    profiler->loop_count = 0;
    
    profiler->total_allocations = 0;
    profiler->total_bytes_requested = 0;
    profiler->max_stack_depth = 0;
    profiler->max_recursion_depth = 0;
    profiler->output_operations = 0;
    
    profiler->profiling_mode = false;
    profiler->preflight_complete = false;
    profiler->infinite_loop_detected = false;
    
    profiler->start_time_ms = 0;
    profiler->elapsed_time_ms = 0;
}

void freeProfiler(Profiler* profiler) {
    if (profiler->plans) {
        free(profiler->plans);
        profiler->plans = NULL;
    }
    if (profiler->loops) {
        free(profiler->loops);
        profiler->loops = NULL;
    }
}

void resetProfiler(Profiler* profiler) {
    profiler->plan_count = 0;
    profiler->loop_count = 0;
    profiler->total_allocations = 0;
    profiler->total_bytes_requested = 0;
    profiler->max_stack_depth = 0;
    profiler->max_recursion_depth = 0;
    profiler->output_operations = 0;
    profiler->infinite_loop_detected = false;
    profiler->start_time_ms = getTimeMs();
    profiler->elapsed_time_ms = 0;
}

MemoryPlan* recordAllocation(Profiler* profiler, uint64_t token_id, size_t size) {
    if (!profiler->profiling_mode) {
        return NULL;
    }
    
    MemoryPlan* existing = findMemoryPlan(profiler, token_id);
    if (existing) {
        existing->access_count++;
        if (size > existing->max_observed_size) {
            existing->max_observed_size = size;
            existing->growth_events++;
        }
        return existing;
    }
    
    if (profiler->plan_count >= profiler->plan_capacity) {
        int new_capacity = profiler->plan_capacity * 2;
        if (new_capacity > MAX_MEMORY_PLANS) {
            new_capacity = MAX_MEMORY_PLANS;
        }
        if (profiler->plan_count >= new_capacity) {
            return NULL;
        }
        MemoryPlan* new_plans = (MemoryPlan*)realloc(profiler->plans, 
                                                      sizeof(MemoryPlan) * new_capacity);
        if (!new_plans) {
            return NULL;
        }
        profiler->plans = new_plans;
        profiler->plan_capacity = new_capacity;
    }
    
    MemoryPlan* plan = &profiler->plans[profiler->plan_count++];
    plan->token_id = token_id;
    plan->predicted_size = size;
    plan->max_observed_size = size;
    plan->growth_events = 0;
    plan->access_count = 1;
    
    profiler->total_allocations++;
    profiler->total_bytes_requested += size;
    
    return plan;
}

MemoryPlan* findMemoryPlan(Profiler* profiler, uint64_t token_id) {
    for (int i = 0; i < profiler->plan_count; i++) {
        if (profiler->plans[i].token_id == token_id) {
            return &profiler->plans[i];
        }
    }
    return NULL;
}

void recordGrowth(Profiler* profiler, uint64_t token_id, size_t new_size) {
    if (!profiler->profiling_mode) {
        return;
    }
    
    MemoryPlan* plan = findMemoryPlan(profiler, token_id);
    if (plan && new_size > plan->max_observed_size) {
        plan->max_observed_size = new_size;
        plan->growth_events++;
    }
}

LoopProfile* recordLoopIteration(Profiler* profiler, uint64_t loop_id) {
    if (!profiler->profiling_mode) {
        return NULL;
    }
    
    for (int i = 0; i < profiler->loop_count; i++) {
        if (profiler->loops[i].loop_id == loop_id) {
            profiler->loops[i].iteration_count++;
            if (profiler->loops[i].iteration_count > profiler->loops[i].max_iterations) {
                profiler->loops[i].max_iterations = profiler->loops[i].iteration_count;
            }
            return &profiler->loops[i];
        }
    }
    
    if (profiler->loop_count >= profiler->loop_capacity) {
        int new_capacity = profiler->loop_capacity * 2;
        LoopProfile* new_loops = (LoopProfile*)realloc(profiler->loops,
                                                        sizeof(LoopProfile) * new_capacity);
        if (!new_loops) {
            return NULL;
        }
        profiler->loops = new_loops;
        profiler->loop_capacity = new_capacity;
    }
    
    LoopProfile* loop = &profiler->loops[profiler->loop_count++];
    loop->loop_id = loop_id;
    loop->iteration_count = 1;
    loop->max_iterations = 1;
    loop->last_check_iteration = 0;
    loop->last_check_stack_depth = 0;
    loop->last_check_allocations = 0;
    loop->potentially_infinite = false;
    loop->making_progress = true;
    
    return loop;
}

bool checkLoopSafety(Profiler* profiler, uint64_t loop_id, uint64_t stack_depth) {
    if (!profiler->profiling_mode) {
        return true;
    }
    
    for (int i = 0; i < profiler->loop_count; i++) {
        if (profiler->loops[i].loop_id == loop_id) {
            LoopProfile* loop = &profiler->loops[i];
            
            if (loop->iteration_count % LOOP_PROGRESS_CHECK_INTERVAL == 0) {
                bool progress_made = false;
                
                if (profiler->total_allocations > loop->last_check_allocations) {
                    progress_made = true;
                }
                
                if (stack_depth != loop->last_check_stack_depth) {
                    progress_made = true;
                }
                
                if (profiler->output_operations > 0) {
                    progress_made = true;
                }
                
                loop->last_check_iteration = loop->iteration_count;
                loop->last_check_stack_depth = stack_depth;
                loop->last_check_allocations = profiler->total_allocations;
                loop->making_progress = progress_made;
                
                if (!progress_made && loop->iteration_count > MAX_LOOP_ITERATIONS) {
                    loop->potentially_infinite = true;
                    profiler->infinite_loop_detected = true;
                    return false;
                }
            }
            
            return true;
        }
    }
    return true;
}

bool checkTimeout(Profiler* profiler) {
    if (!profiler->profiling_mode) {
        return false;
    }
    
    profiler->elapsed_time_ms = getTimeMs() - profiler->start_time_ms;
    return profiler->elapsed_time_ms > PREFLIGHT_TIMEOUT_MS;
}

bool checkRecursionDepth(Profiler* profiler, uint64_t depth) {
    if (!profiler->profiling_mode) {
        return true;
    }
    
    if (depth > profiler->max_recursion_depth) {
        profiler->max_recursion_depth = depth;
    }
    
    return depth < MAX_RECURSION_DEPTH;
}

void dumpProfile(Profiler* profiler) {
    printf("\n=== Preflight Profile ===\n");
    printf("Total allocations: %llu\n", (unsigned long long)profiler->total_allocations);
    printf("Total bytes requested: %llu\n", (unsigned long long)profiler->total_bytes_requested);
    printf("Max stack depth: %llu\n", (unsigned long long)profiler->max_stack_depth);
    printf("Max recursion depth: %llu\n", (unsigned long long)profiler->max_recursion_depth);
    printf("Elapsed time: %llu ms\n", (unsigned long long)profiler->elapsed_time_ms);
    printf("Memory plans: %d\n", profiler->plan_count);
    printf("Loop profiles: %d\n", profiler->loop_count);
    
    if (profiler->infinite_loop_detected) {
        printf("WARNING: Potentially infinite loop detected!\n");
    }
    
    printf("\nTop memory consumers:\n");
    for (int i = 0; i < profiler->plan_count && i < 10; i++) {
        MemoryPlan* plan = &profiler->plans[i];
        printf("  Token %llu: %zu bytes (max: %zu, growth: %u, accesses: %u)\n",
               (unsigned long long)plan->token_id,
               plan->predicted_size,
               plan->max_observed_size,
               plan->growth_events,
               plan->access_count);
    }
    printf("========================\n\n");
}
