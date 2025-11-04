#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>

#include "memory.h"
#include "object.h"
#include "vm.h"

void* reallocate(void* pointer, size_t oldSize, size_t newSize) {
  vm.bytesAllocated += newSize - oldSize;
  
  if (newSize == 0) {
    free(pointer);
    return NULL;
  }

  if (newSize > SIZE_MAX / 2) {
    fprintf(stderr, "Memory allocation too large\n");
    exit(1);
  }

  if (vm.profiler.profiling_mode) {
    uint64_t token_id = (uint64_t)pointer;
    if (pointer == NULL) {
      token_id = vm.profiler.total_allocations;
    }
    recordAllocation(&vm.profiler, token_id, newSize);
    
    if (oldSize > 0 && newSize > oldSize) {
      recordGrowth(&vm.profiler, token_id, newSize);
    }
    
    return pointer ? pointer : (void*)1;
  }

  if (vm.profiler.preflight_complete && pointer == NULL) {
    uint64_t token_id = vm.bytesAllocated;
    MemoryPlan* plan = findMemoryPlan(&vm.profiler, token_id);
    if (plan && plan->max_observed_size > newSize) {
      size_t optimized_size = (size_t)(plan->max_observed_size * 1.01);
      if (optimized_size > newSize && optimized_size < SIZE_MAX / 2) {
        newSize = optimized_size;
      }
    }
  }

  void* result = realloc(pointer, newSize);
  if (result == NULL) {
    fprintf(stderr, "Memory allocation failed\n");
    exit(1);
  }
  
  return result;
}

static void freeObject(Obj* object) {
  switch (object->type) {
    case OBJ_MODULE: {
      ObjModule* module = (ObjModule*)object;
      freeTable(&module->variables);
      FREE(ObjModule, object);
      break;
    }
    case OBJ_CLOSURE: {
      ObjClosure* closure = (ObjClosure*)object;
      FREE_ARRAY(ObjUpvalue*, closure->upvalues,
                 closure->upvalueCount);
      FREE(ObjClosure, object);
      break;
    }
    case OBJ_FUNCTION: {
      ObjFunction* function = (ObjFunction*)object;
      freeChunk(&function->chunk);
      FREE(ObjFunction, object);
      break;
    }
    case OBJ_LIST: {
      ObjList* list = (ObjList*)object;
      freeValueArray(list->items);
      FREE(ValueArray, list->items);
      FREE(ObjList, object);
      break;
    }
    case OBJ_MAP: {
      ObjMap* map = (ObjMap*)object;
      freeTable(&map->table);
      FREE(ObjMap, object);
      break;
    }
    case OBJ_NATIVE:
      FREE(ObjNative, object);
      break;
    case OBJ_STRING: {
      ObjString* string = (ObjString*)object;
      FREE_ARRAY(char, string->chars, string->length + 1);
      FREE(ObjString, object);
      break;
    }
    case OBJ_UPVALUE:
      FREE(ObjUpvalue, object);
      break;
  }
}

void freeObjects() {
  Obj* object = vm.objects;
  while (object != NULL) {
    Obj* next = object->next;
    freeObject(object);
    object = next;
  }
}
