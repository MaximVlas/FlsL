#define _GNU_SOURCE
#include <math.h>
#include <stdarg.h>
#include <sys/stat.h>
#include <unistd.h>

double fmod(double x, double y);
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>
#if defined(_WIN32)
#include <direct.h>
#endif
#include <ctype.h>
#include <dirent.h>
#include <limits.h>
#include <pthread.h>

#include "../std/include/dict.h"
#include "../std/include/io.h"
#include "../std/include/math.h"
#include "../std/include/random.h"
#include "common.h"
#include "compiler.h"
#include "debug.h"
#include "error.h"
#include "memory.h"
#include "object.h"
#include "profiler.h"
#include "vm.h"

// Helper to trim leading/trailing whitespace and quotes from a string,
// in-place.
static void trimString(char *str) {
  if (str == NULL)
    return;

  char *start = str;
  // Find the first non-whitespace, non-quote character
  while (isspace((unsigned char)*start) || *start == '\'' || *start == '"') {
    start++;
  }

  // Find the last non-whitespace, non-quote character
  char *end = str + strlen(str) - 1;
  while (end > start &&
         (isspace((unsigned char)*end) || *end == '\'' || *end == '"')) {
    end--;
  }

  // Null-terminate the trimmed string
  *(end + 1) = '\0';

  // Shift the trimmed string to the beginning if necessary
  if (str != start) {
    memmove(str, start, end - start + 2); // +2 to include the null terminator
  }
}

// Native 'len' function: returns the length of a string.
static Value stringLengthNative(int argCount, Value *args) {
  if (argCount != 1) {
    runtimeError("len() takes exactly 1 argument (%d given).", argCount);
    return NIL_VAL;
  }

  if (!IS_STRING(args[0])) {
    runtimeError("len() argument must be a string.");
    return NIL_VAL;
  }

  ObjString *string = AS_STRING(args[0]);
  return NUMBER_VAL(string->length);
}

// Native 'toString' function: converts a number to a string.
static Value toStringNative(int argCount, Value *args) {
  if (argCount != 1) {
    runtimeError("toString() takes exactly 1 argument (%d given).", argCount);
    return NIL_VAL;
  }

  if (IS_BOOL(args[0])) {
    bool val = AS_BOOL(args[0]);
    return OBJ_VAL(copyString(val ? "true" : "false", val ? 4 : 5));
  } else if (IS_NIL(args[0])) {
    return OBJ_VAL(copyString("nil", 3));
  } else if (IS_NUMBER(args[0])) {
    double number = AS_NUMBER(args[0]);
    char buffer[64];
    int length;

    if (number == (long long)number && number >= LLONG_MIN &&
        number <= LLONG_MAX) {
      length = snprintf(buffer, sizeof(buffer), "%lld", (long long)number);
    } else {
      length = snprintf(buffer, sizeof(buffer), "%.15g", number);
    }

    if (length < 0 || length >= (int)sizeof(buffer)) {
      runtimeError("toString() number conversion failed.");
      return NIL_VAL;
    }
    return OBJ_VAL(copyString(buffer, length));
  } else if (IS_STRING(args[0])) {
    return args[0];
  } else {
    runtimeError("toString() argument must be a number, bool, nil, or string.");
    return NIL_VAL;
  }
}

Value isStringNative(int argCount, Value *args) {
  if (argCount != 1) {
    runtimeError("isString() takes one argument.");
    return NIL_VAL;
  }
  return BOOL_VAL(IS_STRING(args[0]));
}

// Native 'lines' function: counts the number of lines in a string.
static Value countLinesNative(int argCount, Value *args) {
  if (argCount != 1) {
    runtimeError("lines() takes exactly 1 argument (%d given).", argCount);
    return NIL_VAL;
  }

  if (!IS_STRING(args[0])) {
    runtimeError("lines() argument must be a string.");
    return NIL_VAL;
  }

  ObjString *string = AS_STRING(args[0]);
  if (string->length == 0) {
    return NUMBER_VAL(0);
  }

  int line_count = 1;
  for (int i = 0; i < string->length; i++) {
    if (string->chars[i] == '\n') {
      line_count++;
    }
  }

  // A file ending with a newline shouldn't count the empty line after it.
  if (string->chars[string->length - 1] == '\n') {
    line_count--;
  }

  return NUMBER_VAL(line_count);
}

// Native 'listLen' function: returns the length of a list.
static Value listLenNative(int argCount, Value *args) {
  if (argCount != 1) {
    runtimeError("listLen() takes exactly 1 argument (%d given).", argCount);
    return NIL_VAL;
  }

  if (!IS_LIST(args[0])) {
    runtimeError("listLen() argument must be a list.");
    return NIL_VAL;
  }

  ObjList *list = AS_LIST(args[0]);
  return NUMBER_VAL(list->items->count);
}

// Native 'listGet' function: returns the item at a given index in a list.
static Value listGetNative(int argCount, Value *args) {
  if (argCount != 2) {
    runtimeError("listGet() takes exactly 2 arguments (%d given).", argCount);
    return NIL_VAL;
  }

  if (!IS_LIST(args[0])) {
    runtimeError("listGet() first argument must be a list.");
    return NIL_VAL;
  }

  if (!IS_NUMBER(args[1])) {
    runtimeError("listGet() second argument must be a number (index).");
    return NIL_VAL;
  }

  ObjList *list = AS_LIST(args[0]);
  int index = AS_NUMBER(args[1]);

  if (index < 0 || index >= list->items->count) {
    runtimeError("listGet() index out of bounds.");
    return NIL_VAL;
  }

  return list->items->values[index];
}

// Native 'listSet' function: sets the item at a given index in a list.
static Value listSetNative(int argCount, Value *args) {
  if (argCount != 3) {
    runtimeError("listSet() takes exactly 3 arguments (%d given).", argCount);
    return NIL_VAL;
  }

  if (!IS_LIST(args[0])) {
    runtimeError("listSet() first argument must be a list.");
    return NIL_VAL;
  }

  if (!IS_NUMBER(args[1])) {
    runtimeError("listSet() second argument must be a number (index).");
    return NIL_VAL;
  }

  ObjList *list = AS_LIST(args[0]);
  int index = AS_NUMBER(args[1]);

  if (index < 0 || index >= list->items->count) {
    runtimeError("listSet() index out of bounds.");
    return NIL_VAL;
  }

  list->items->values[index] = args[2];
  return args[2];
}

// Native 'listPush' function: adds an item to the end of a list.
static Value listPushNative(int argCount, Value *args) {
  if (argCount != 2) {
    runtimeError("listPush() takes exactly 2 arguments (%d given).", argCount);
    return NIL_VAL;
  }
  if (!IS_LIST(args[0])) {
    runtimeError("listPush() first argument must be a list.");
    return NIL_VAL;
  }

  ObjList *list = AS_LIST(args[0]);
  writeValueArray(list->items, args[1]);
  return args[1];
}

// Native 'listPop' function: removes and returns the last item of a list.
static Value listPopNative(int argCount, Value *args) {
  if (argCount != 1) {
    runtimeError("listPop() takes exactly 1 argument (%d given).", argCount);
    return NIL_VAL;
  }
  if (!IS_LIST(args[0])) {
    runtimeError("listPop() first argument must be a list.");
    return NIL_VAL;
  }

  ObjList *list = AS_LIST(args[0]);
  if (list->items->count == 0) {
    runtimeError("listPop() called on an empty list.");
    return NIL_VAL;
  }

  return popValueArray(list->items);
}

// Native 'listClear' function: removes all items from a list.
static Value listClearNative(int argCount, Value *args) {
  if (argCount != 1) {
    runtimeError("listClear() takes exactly 1 argument (%d given).", argCount);
    return NIL_VAL;
  }
  if (!IS_LIST(args[0])) {
    runtimeError("listClear() first argument must be a list.");
    return NIL_VAL;
  }

  ObjList *list = AS_LIST(args[0]);
  freeValueArray(list->items);

  return NIL_VAL;
}

// Native 'listShift' function: removes and returns the first item of a list.
static Value listShiftNative(int argCount, Value *args) {
  if (argCount != 1) {
    runtimeError("listShift() takes exactly 1 argument (%d given).", argCount);
    return NIL_VAL;
  }
  if (!IS_LIST(args[0])) {
    runtimeError("listShift() first argument must be a list.");
    return NIL_VAL;
  }

  ObjList *list = AS_LIST(args[0]);
  if (list->items->count == 0) {
    runtimeError("listShift() called on an empty list.");
    return NIL_VAL;
  }

  return removeValueArray(list->items, 0);
}

// Native 'endsWith' function: checks if a string ends with a given suffix.
static Value endsWithNative(int argCount, Value *args) {
  if (argCount != 2) {
    runtimeError("endsWith() takes exactly 2 arguments (%d given).", argCount);
    return NIL_VAL;
  }

  if (!IS_STRING(args[0]) || !IS_STRING(args[1])) {
    runtimeError("endsWith() arguments must be strings.");
    return NIL_VAL;
  }

  ObjString *string = AS_STRING(args[0]);
  ObjString *suffix = AS_STRING(args[1]);

  if (suffix->length > string->length) {
    return BOOL_VAL(false);
  }

  return BOOL_VAL(memcmp(string->chars + string->length - suffix->length,
                         suffix->chars, suffix->length) == 0);
}

// Native 'toNum' function: converts a string to a number.
static Value toNumNative(int argCount, Value *args) {
  if (argCount != 1) {
    runtimeError("toNum() takes exactly 1 argument (%d given).", argCount);
    return NIL_VAL;
  }

  if (!IS_STRING(args[0])) {
    runtimeError("toNum() argument must be a string.");
    return NIL_VAL;
  }

  ObjString *string = AS_STRING(args[0]);
  if (string->chars == NULL) {
    return NIL_VAL;
  }

  char *end = NULL;
  double number = strtod(string->chars, &end);

  if (end == string->chars || *end != '\0') {
    return NIL_VAL;
  }

  return NUMBER_VAL(number);
}

static Value mapNative(int argCount, Value *args) {
  (void)args; // Unused.
  if (argCount != 0) {
    runtimeError("map() takes no arguments (%d given).", argCount);
    return NIL_VAL;
  }
  return OBJ_VAL(newMap());
}

// Native 'trim' function: removes leading/trailing whitespace from a string.
static Value trimNative(int argCount, Value *args) {
  if (argCount != 1) {
    runtimeError("trim() takes exactly 1 argument (%d given).", argCount);
    return NIL_VAL;
  }
  if (!IS_STRING(args[0])) {
    runtimeError("trim() argument must be a string.");
    return NIL_VAL;
  }

  ObjString *string = AS_STRING(args[0]);
  if (string->chars == NULL) {
    return NIL_VAL;
  }

  char *source = string->chars;
  char *end = source + string->length - 1;

  while (source <= end && isspace((unsigned char)*source))
    source++;

  if (source > end) {
    return OBJ_VAL(copyString("", 0));
  }

  while (end > source && isspace((unsigned char)*end))
    end--;

  ptrdiff_t len = end - source + 1;
  if (len < 0 || len > INT_MAX) {
    return NIL_VAL;
  }

  return OBJ_VAL(copyString(source, (int)len));
}

// Native 'toUpperCase' function: converts a string to uppercase.
static Value toUpperCaseNative(int argCount, Value *args) {
  if (argCount != 1) {
    runtimeError("toUpperCase() takes exactly 1 argument (%d given).",
                 argCount);
    return NIL_VAL;
  }
  if (!IS_STRING(args[0])) {
    runtimeError("toUpperCase() argument must be a string.");
    return NIL_VAL;
  }

  ObjString *string = AS_STRING(args[0]);
  if (string->chars == NULL) {
    return NIL_VAL;
  }

  char *newChars = ALLOCATE(char, (size_t)string->length + 1);
  for (int i = 0; i < string->length; i++) {
    newChars[i] = (char)toupper((unsigned char)string->chars[i]);
  }
  newChars[string->length] = '\0';

  return OBJ_VAL(takeString(newChars, string->length));
}

// Native 'toLowerCase' function: converts a string to lowercase.
static Value toLowerCaseNative(int argCount, Value *args) {
  if (argCount != 1) {
    runtimeError("toLowerCase() takes exactly 1 argument (%d given).",
                 argCount);
    return NIL_VAL;
  }
  if (!IS_STRING(args[0])) {
    runtimeError("toLowerCase() argument must be a string.");
    return NIL_VAL;
  }

  ObjString *string = AS_STRING(args[0]);
  if (string->chars == NULL) {
    return NIL_VAL;
  }

  char *newChars = ALLOCATE(char, (size_t)string->length + 1);
  for (int i = 0; i < string->length; i++) {
    newChars[i] = (char)tolower((unsigned char)string->chars[i]);
  }
  newChars[string->length] = '\0';

  return OBJ_VAL(takeString(newChars, string->length));
}

static Value mapSetNative(int argCount, Value *args) {
  if (argCount != 3) {
    runtimeError("mapSet() takes 3 arguments: map, key, value (%d given).",
                 argCount);
    return NIL_VAL;
  }
  if (!IS_MAP(args[0])) {
    runtimeError("First argument to mapSet() must be a map.");
    return NIL_VAL;
  }
  if (!IS_STRING(args[1])) {
    runtimeError("Second argument (key) to mapSet() must be a string.");
    return NIL_VAL;
  }

  ObjMap *map = AS_MAP(args[0]);
  ObjString *key = AS_STRING(args[1]);
  Value value = args[2];

  tableSet(&map->table, key, value);
  return value;
}

static Value mapGetNative(int argCount, Value *args) {
  if (argCount != 2) {
    runtimeError("mapGet() takes 2 arguments: map, key (%d given).", argCount);
    return NIL_VAL;
  }
  if (!IS_MAP(args[0])) {
    runtimeError("First argument to mapGet() must be a map.");
    return NIL_VAL;
  }
  if (!IS_STRING(args[1])) {
    runtimeError("Second argument (key) to mapGet() must be a string.");
    return NIL_VAL;
  }

  ObjMap *map = AS_MAP(args[0]);
  ObjString *key = AS_STRING(args[1]);
  Value value;

  if (!tableGet(&map->table, key, &value)) {
    return NIL_VAL;
  }

  return value;
}

static Value mapDeleteNative(int argCount, Value *args) {
  if (argCount != 2) {
    runtimeError("mapDelete() takes 2 arguments: map, key (%d given).",
                 argCount);
    return NIL_VAL;
  }
  if (!IS_MAP(args[0])) {
    runtimeError("First argument to mapDelete() must be a map.");
    return NIL_VAL;
  }
  if (!IS_STRING(args[1])) {
    runtimeError("Second argument (key) to mapDelete() must be a string.");
    return NIL_VAL;
  }

  ObjMap *map = AS_MAP(args[0]);
  ObjString *key = AS_STRING(args[1]);

  if (tableDelete(&map->table, key)) {
    return BOOL_VAL(true);
  }

  return BOOL_VAL(false);
}

VM vm;

// Forward declaration for the runtime error function.

// --- Native C Functions ---

// Native 'clock' function: returns system time in seconds.
static Value clockNative(int argCount, Value *args) {
  // Suppress unused parameter warnings.
  (void)argCount;
  (void)args;
  return NUMBER_VAL((double)clock() / CLOCKS_PER_SEC);
}

// Native 'input' function: reads a line of user input from the console.
// Can take one optional argument as a prompt.
static Value inputNative(int argCount, Value *args) {
  if (argCount > 1) {
    runtimeError("input() takes at most 1 argument (%d given).", argCount);
    return NIL_VAL;
  }

  if (vm.profiler.profiling_mode) {
    return OBJ_VAL(copyString("", 0));
  }

  if (argCount == 1) {
    Value prompt = args[0];
    if (IS_STRING(prompt)) {
      ObjString *str = AS_STRING(prompt);
      if (str->chars != NULL) {
        printf("%s", str->chars);
        fflush(stdout);
      }
    } else {
      runtimeError("input() argument must be a string.");
      return NIL_VAL;
    }
  }

#define READ_BUFFER_SIZE 2048
  char buffer[READ_BUFFER_SIZE];
  if (fgets(buffer, sizeof(buffer), stdin) == NULL) {
    return NIL_VAL;
  }

  size_t len = strcspn(buffer, "\r\n");
  if (len < sizeof(buffer)) {
    buffer[len] = '\0';
  } else {
    buffer[sizeof(buffer) - 1] = '\0';
  }

  return OBJ_VAL(copyString(buffer, (int)strlen(buffer)));
}

// Helper function to recursively walk a directory.
static void walk(const char *dir, ObjList *list) {
  if (dir == NULL || list == NULL) {
    return;
  }

  struct dirent *dp;
  DIR *dfd = opendir(dir);
  if (dfd == NULL) {
    return;
  }

  while ((dp = readdir(dfd)) != NULL) {
    if (strcmp(dp->d_name, ".") == 0 || strcmp(dp->d_name, "..") == 0 ||
        dp->d_name[0] == '.') {
      continue;
    }

    char path[1024];
    int written = snprintf(path, sizeof(path), "%s/%s", dir, dp->d_name);
    if (written < 0 || written >= (int)sizeof(path)) {
      continue;
    }

    struct stat st;
    if (stat(path, &st) == -1) {
      continue;
    }

    if (S_ISDIR(st.st_mode)) {
    } else {
      size_t name_len = strlen(dp->d_name);
      if (name_len > INT_MAX) {
        continue;
      }
      Value pathValue = OBJ_VAL(copyString(dp->d_name, (int)name_len));
      writeValueArray(list->items, pathValue);
    }
  }
  closedir(dfd);
}

// --- High-performance Multi-threaded Code Analyzer (w/ Exclusions) ---

#define MAX_THREADS 128
#define QUEUE_CAPACITY 4096

// --- Data Structures for Threading ---
typedef enum { LOG_NONE, LOG_MINIMAL, LOG_VERBOSE } LogLevel;

typedef struct {
  char *path;
} Task;

typedef struct {
  Task *tasks[QUEUE_CAPACITY];
  int front, rear, count;
  pthread_mutex_t mutex;
  pthread_cond_t not_empty, not_full;
  bool finished_adding;
} TaskQueue;

// Per-thread result, no locking needed during analysis
typedef struct {
  long long files_processed;
  long long total_lines;
  long long total_chars;
} ThreadResult;

typedef struct {
  pthread_t thread_id;
  int thread_idx;
  TaskQueue *queue;
  ObjList *extensions;
  ThreadResult result; // Each thread has its own result
  LogLevel logLevel;
} ThreadData;

// --- Forward Declarations ---
static void *worker(void *arg);
static bool hasValidExtension(const char *filename, ObjList *extensions);
static void walkAndSubmitTasks(const char *dir, TaskQueue *queue,
                               ObjList *extensions, int depth,
                               LogLevel logLevel, ObjList *excluded_dirs);

// --- Task Queue Implementation ---
static void queue_init(TaskQueue *q) {
  q->front = 0;
  q->rear = -1;
  q->count = 0;
  q->finished_adding = false;
  pthread_mutex_init(&q->mutex, NULL);
  pthread_cond_init(&q->not_empty, NULL);
  pthread_cond_init(&q->not_full, NULL);
}

static void queue_destroy(TaskQueue *q) {
  while (q->count > 0) {
    Task *task = q->tasks[q->front];
    free(task->path);
    free(task);
    q->front = (q->front + 1) % QUEUE_CAPACITY;
    q->count--;
  }
  pthread_mutex_destroy(&q->mutex);
  pthread_cond_destroy(&q->not_empty);
  pthread_cond_destroy(&q->not_full);
}

static void queue_push(TaskQueue *q, Task *task) {
  pthread_mutex_lock(&q->mutex);
  while (q->count == QUEUE_CAPACITY) {
    pthread_cond_wait(&q->not_full, &q->mutex);
  }
  q->tasks[q->rear] = task;
  q->rear = (q->rear + 1) % QUEUE_CAPACITY;
  q->count++;
  pthread_cond_signal(&q->not_empty);
  pthread_mutex_unlock(&q->mutex);
}

static Task *queue_pop(TaskQueue *q) {
  pthread_mutex_lock(&q->mutex);
  while (q->count == 0 && !q->finished_adding) {
    pthread_cond_wait(&q->not_empty, &q->mutex);
  }
  if (q->count == 0) { // Must be finished adding
    pthread_mutex_unlock(&q->mutex);
    return NULL;
  }
  Task *task = q->tasks[q->front];
  q->front = (q->front + 1) % QUEUE_CAPACITY;
  q->count--;
  pthread_cond_signal(&q->not_full);
  pthread_mutex_unlock(&q->mutex);
  return task;
}

// Helper to read a file from disk into a string.

// Helper to read a file from disk into a string.
static char *readFile(const char *path) {
  if (path == NULL) {
    return NULL;
  }

  FILE *file = fopen(path, "rb");
  if (file == NULL) {
    return NULL;
  }

  if (fseek(file, 0L, SEEK_END) != 0) {
    fclose(file);
    return NULL;
  }

  long fileSize = ftell(file);
  if (fileSize < 0 || fileSize > 100 * 1024 * 1024) {
    fclose(file);
    return NULL;
  }
  rewind(file);

  char *buffer = (char *)malloc((size_t)fileSize + 1);
  if (buffer == NULL) {
    fclose(file);
    return NULL;
  }

  size_t bytesRead = fread(buffer, sizeof(char), (size_t)fileSize, file);
  if (bytesRead < (size_t)fileSize) {
    free(buffer);
    fclose(file);
    return NULL;
  }

  buffer[bytesRead] = '\0';
  fclose(file);
  return buffer;
}

// --- File Analysis Logic for Workers ---
static pthread_mutex_t print_mutex = PTHREAD_MUTEX_INITIALIZER;

static void analyzeFileForWorker(const char *path, ThreadResult *result,
                                 LogLevel logLevel) {
  if (logLevel == LOG_VERBOSE) {
    pthread_mutex_lock(&print_mutex);
    printf("    -> Analyzing: %s\n", path);
    fflush(stdout);
    pthread_mutex_unlock(&print_mutex);
  }

  char *source = readFile(path);
  if (source == NULL)
    return;

  size_t len = strlen(source);
  int line_count = (len > 0) ? 1 : 0;
  for (size_t i = 0; i < len; i++) {
    if (source[i] == '\n')
      line_count++;
  }
  if (len > 0 && source[len - 1] == '\n')
    line_count--;

  result->files_processed++;
  result->total_lines += line_count;
  result->total_chars += len;

  free(source);
}

// --- Thread Pool Worker and Directory Walker ---
static void *worker(void *arg) {
  ThreadData *data = (ThreadData *)arg;
  LogLevel logLevel = data->logLevel;
  while (true) {
    Task *task = queue_pop(data->queue);
    if (task == NULL)
      break;
    analyzeFileForWorker(task->path, &data->result, logLevel);
    free(task->path);
    free(task);
  }
  return NULL;
}

static bool isPathExcluded(const char *path, ObjList *excluded_dirs) {
  if (excluded_dirs == NULL)
    return false;
  for (int i = 0; i < excluded_dirs->items->count; i++) {
    Value excluded_val = excluded_dirs->items->values[i];
    if (IS_STRING(excluded_val)) {
      const char *excluded_path = AS_CSTRING(excluded_val);
      size_t excluded_len = strlen(excluded_path);
      if (strncmp(path, excluded_path, excluded_len) == 0) {
        // Ensure it's a full path match or a directory prefix match
        if (path[excluded_len] == '\0' || path[excluded_len] == '/') {
          return true;
        }
      }
    }
  }
  return false;
}

static void walkAndSubmitTasks(const char *dir, TaskQueue *queue,
                               ObjList *extensions, int depth,
                               LogLevel logLevel, ObjList *excluded_dirs) {
  if (dir == NULL || queue == NULL)
    return;

  if (depth > 100)
    return;

  if (isPathExcluded(dir, excluded_dirs)) {
    if (logLevel >= LOG_MINIMAL) {
      printf("   -> Skipping excluded directory: %s\n", dir);
      fflush(stdout);
    }
    return;
  }

  if (logLevel >= LOG_MINIMAL && depth < 3) {
    char indent[32] = {0};
    int indent_len = depth * 2;
    if (indent_len > 30)
      indent_len = 30;
    for (int i = 0; i < indent_len; ++i)
      indent[i] = ' ';
    indent[indent_len] = '\0';
    printf("%s-> Scanning %s...\n", indent, dir);
    fflush(stdout);
  }

  struct dirent *dp;
  DIR *dfd = opendir(dir);
  if (dfd == NULL)
    return;

  while ((dp = readdir(dfd)) != NULL) {
    if (strcmp(dp->d_name, ".") == 0 || strcmp(dp->d_name, "..") == 0 ||
        dp->d_name[0] == '.')
      continue;

    char path[PATH_MAX];
    int written = snprintf(path, sizeof(path), "%s/%s", dir, dp->d_name);
    if (written < 0 || written >= (int)sizeof(path))
      continue;

    struct stat st;
    if (stat(path, &st) == -1)
      continue;

    if (S_ISDIR(st.st_mode)) {
      walkAndSubmitTasks(path, queue, extensions, depth + 1, logLevel,
                         excluded_dirs);
    } else {
      if (hasValidExtension(path, extensions)) {
        Task *task = (Task *)malloc(sizeof(Task));
        if (task == NULL)
          continue;
        task->path = strdup(path);
        if (task->path == NULL) {
          free(task);
          continue;
        }
        queue_push(queue, task);
      }
    }
  }
  closedir(dfd);
}

// --- Native 'analyze' function (multi-threaded, with exclusions) ---
// --- Filesystem Native Functions ---

Value listDirNative(int argCount, Value *args) {
  if (argCount != 1 || !IS_STRING(args[0])) {
    runtimeError("listDir() expects one string argument (directory path).");
    return NIL_VAL;
  }

  if (vm.profiler.profiling_mode) {
    return OBJ_VAL(newList());
  }

  const char *dir = AS_CSTRING(args[0]);
  ObjList *list = newList();

  push(OBJ_VAL(list));
  walk(dir, list);
  pop();

  return OBJ_VAL(list);
}

static Value systemNative(int argCount, Value *args) {
  if (argCount != 1) {
    runtimeError("system() takes exactly 1 argument (%d given).", argCount);
    return NIL_VAL;
  }
  if (!IS_STRING(args[0])) {
    runtimeError("system() argument must be a string command.");
    return NIL_VAL;
  }

  if (vm.profiler.profiling_mode) {
    return OBJ_VAL(copyString("", 0));
  }

  ObjString *cmdStr = AS_STRING(args[0]);
  if (cmdStr->chars == NULL || cmdStr->length == 0) {
    runtimeError("system() command cannot be empty.");
    return NIL_VAL;
  }

  FILE *pipe = popen(cmdStr->chars, "r");
  if (!pipe) {
    runtimeError("Failed to execute command.");
    return NIL_VAL;
  }

  size_t buffer_capacity = 4096;
  size_t max_output = 10 * 1024 * 1024;
  char *result = malloc(buffer_capacity);
  if (result == NULL) {
    pclose(pipe);
    runtimeError("Memory allocation failed for system() output.");
    return NIL_VAL;
  }
  result[0] = '\0';
  size_t total_size = 0;

  char buffer[256];
  while (fgets(buffer, sizeof(buffer), pipe) != NULL) {
    size_t chunk_len = strlen(buffer);

    if (total_size + chunk_len + 1 > max_output) {
      free(result);
      pclose(pipe);
      runtimeError("system() output exceeds maximum size.");
      return NIL_VAL;
    }

    if (total_size + chunk_len + 1 > buffer_capacity) {
      size_t new_capacity = buffer_capacity * 2;
      if (new_capacity > max_output) {
        new_capacity = max_output;
      }
      char *new_result = realloc(result, new_capacity);
      if (new_result == NULL) {
        free(result);
        pclose(pipe);
        runtimeError("Memory reallocation failed for system() output.");
        return NIL_VAL;
      }
      result = new_result;
      buffer_capacity = new_capacity;
    }

    memcpy(result + total_size, buffer, chunk_len);
    total_size += chunk_len;
    result[total_size] = '\0';
  }

  pclose(pipe);

  ObjString *output = copyString(result, (int)total_size);
  free(result);
  return OBJ_VAL(output);
}

static Value analyzeNative(int argCount, Value *args) {
  if (argCount < 2 || argCount > 4) {
    runtimeError("analyze() takes 2-4 arguments (root_dir, extensions, "
                 "[log_level], [excluded_dirs]).");
    return NIL_VAL;
  }
  if (!IS_STRING(args[0])) {
    runtimeError("First argument must be a string (root_dir).");
    return NIL_VAL;
  }
  if (!IS_LIST(args[1])) {
    runtimeError("Second argument must be a list (extensions_list).");
    return NIL_VAL;
  }

  LogLevel logLevel = LOG_MINIMAL;
  if (argCount >= 3) {
    if (!IS_STRING(args[2])) {
      runtimeError("Third argument (log_level) must be a string.");
      return NIL_VAL;
    }
    char *levelStr = AS_CSTRING(args[2]);
    if (strcmp(levelStr, "none") == 0)
      logLevel = LOG_NONE;
    else if (strcmp(levelStr, "minimal") == 0)
      logLevel = LOG_MINIMAL;
    else if (strcmp(levelStr, "verbose") == 0)
      logLevel = LOG_VERBOSE;
    else {
      runtimeError("Invalid log level. Use 'none', 'minimal', or 'verbose'.");
      return NIL_VAL;
    }
  }

  ObjList *excluded_dirs = NULL;
  if (argCount == 4) {
    if (!IS_LIST(args[3])) {
      runtimeError("Fourth argument (excluded_dirs) must be a list.");
      return NIL_VAL;
    }
    excluded_dirs = AS_LIST(args[3]);
  }

  char *rootDir = AS_CSTRING(args[0]);
  ObjList *extensions = AS_LIST(args[1]);

  int num_cores = sysconf(_SC_NPROCESSORS_ONLN);
  if (num_cores <= 0)
    num_cores = 2;
  if (num_cores > MAX_THREADS)
    num_cores = MAX_THREADS;

  TaskQueue queue;
  queue_init(&queue);

  ThreadData thread_data[MAX_THREADS];

  for (int i = 0; i < num_cores; i++) {
    thread_data[i].thread_idx = i;
    thread_data[i].queue = &queue;
    thread_data[i].extensions = extensions;
    thread_data[i].result = (ThreadResult){0, 0, 0};
    thread_data[i].logLevel = logLevel;
    pthread_create(&thread_data[i].thread_id, NULL, worker, &thread_data[i]);
  }

  walkAndSubmitTasks(rootDir, &queue, extensions, 0, logLevel, excluded_dirs);

  pthread_mutex_lock(&queue.mutex);
  queue.finished_adding = true;
  pthread_cond_broadcast(&queue.not_empty);
  pthread_mutex_unlock(&queue.mutex);

  long long total_files = 0, total_lines = 0, total_chars = 0;
  for (int i = 0; i < num_cores; i++) {
    pthread_join(thread_data[i].thread_id, NULL);
    total_files += thread_data[i].result.files_processed;
    total_lines += thread_data[i].result.total_lines;
    total_chars += thread_data[i].result.total_chars;
  }

  queue_destroy(&queue);

  ObjList *resultList = newList();
  push(OBJ_VAL(resultList));
  writeValueArray(resultList->items, NUMBER_VAL((double)total_files));
  writeValueArray(resultList->items, NUMBER_VAL((double)total_lines));
  writeValueArray(resultList->items, NUMBER_VAL((double)total_chars));
  pop();
  return OBJ_VAL(resultList);
}

// Helper to check if a file has a valid extension.
bool hasValidExtension(const char *filename, ObjList *extensions) {
  if (extensions == NULL || extensions->items->count == 0) {
    return true; // No extensions to check against, so all files are valid
  }

  const char *dot = strrchr(filename, '.');
  if (dot == NULL) {
    return false; // No extension
  }

  for (int i = 0; i < extensions->items->count; i++) {
    Value extVal = extensions->items->values[i];
    if (!IS_STRING(extVal))
      continue;

    ObjString *ext = AS_STRING(extVal);
    if (strcmp(dot, ext->chars) == 0) {
      return true;
    }
  }
  return false;
}

// --- VM Internals ---

void resetStack() {
  vm.stackTop = vm.stack;
  vm.frameCount = 0;
}

// This function is now defined in error.c to be shared across the codebase.
// We keep the declaration here to avoid modifying all native function calls.
void runtimeError(const char *format, ...);

void defineNative(const char *name, NativeFn function) {
  push(OBJ_VAL(copyString(name, (int)strlen(name))));
  push(OBJ_VAL(newNative(function)));
  tableSet(&vm.globals, AS_STRING(vm.stack[0]), vm.stack[1]);

  pop();
  pop();
}

void defineGlobal(const char *name, Value value) {
  push(OBJ_VAL(copyString(name, (int)strlen(name))));
  push(value);
  tableSet(&vm.globals, AS_STRING(vm.stack[0]), vm.stack[1]);
  pop();
  pop();
}

void initVM() {
  vm.frameCount = 0;
  vm.stackTop = vm.stack;
  vm.objects = NULL;
  vm.hadError = false;
  vm.bytesAllocated = 0;
  vm.nextGC = 1024 * 1024;
  vm.enable_preflight = false;
  vm.instruction_count = 0;

  initTable(&vm.globals);
  initTable(&vm.modules);
  initTable(&vm.strings);
  initProfiler(&vm.profiler);

  // Define all native functions.
  defineNative("clock", clockNative);
  defineNative("input", inputNative);
  defineNative("readFile", readFileNative);
  defineNative("listDir", listDirNative);
  defineNative("print", printNative);
  defineNative("println", printlnNative);

  // Math functions
  defineNative("sqrt", sqrtNative);
  defineNative("sin", sinNative);
  defineNative("cos", cosNative);
  defineNative("tan", tanNative);
  defineNative("abs", absNative);
  defineNative("len", stringLengthNative);
  defineNative("isString", isStringNative);
  defineNative("toString", toStringNative);

  // Dict
  defineNative("newDict", newDictNative);
  defineNative("dictSet", dictSetNative);
  defineNative("dictGet", dictGetNative);
  defineNative("dictDelete", dictDeleteNative);
  defineNative("dictExists", dictExistsNative);
  defineNative("lines", countLinesNative);
  defineNative("listLen", listLenNative);
  defineNative("listGet", listGetNative);
  defineNative("listSet", listSetNative);
  defineNative("listPush", listPushNative);
  defineNative("listPop", listPopNative);
  defineNative("listClear", listClearNative);
  defineNative("listShift", listShiftNative);
  defineNative("endsWith", endsWithNative);
  defineNative("toNum", toNumNative);
  defineNative("map", mapNative);
  defineNative("mapSet", mapSetNative);
  defineNative("mapGet", mapGetNative);
  defineNative("mapDelete", mapDeleteNative);
  defineNative("analyze", analyzeNative);
  defineNative("system", systemNative);

  // Filesystem
  defineNative("readFile", readFileNative);
  defineNative("writeFile", writeFileNative);
  defineNative("appendFile", appendFileNative);
  defineNative("pathExists", pathExistsNative);
  defineNative("deleteFile", deleteFileNative);
  defineNative("rename", renameNative);
  defineNative("createDir", createDirNative);
  defineNative("removeDir", removeDirNative);
  defineNative("fileSize", fileSizeNative);
  defineNative("isDir", isDirNative);
  defineNative("isFile", isFileNative);
  defineNative("listDir", listDirNative);

  // String utils
  defineNative("startsWith", startsWithNative);
  defineNative("substring", substringNative);
  defineNative("split", splitNative);
  defineNative("trim", trimNative);
  defineNative("toUpperCase", toUpperCaseNative);
  defineNative("toLowerCase", toLowerCaseNative);

  initMathLibrary();
  initRandomLibrary();
}

void freeVM() {
  freeTable(&vm.globals);
  freeTable(&vm.modules);
  freeTable(&vm.strings);
  freeObjects();
  freeProfiler(&vm.profiler);
}

void push(Value value) {
  if (vm.stackTop >= vm.stack + STACK_MAX) {
    fprintf(stderr, "Stack overflow\n");
    exit(1);
  }
  *vm.stackTop = value;
  vm.stackTop++;
}

Value pop() {
  if (vm.stackTop <= vm.stack) {
    fprintf(stderr, "Stack underflow\n");
    exit(1);
  }
  vm.stackTop--;
  return *vm.stackTop;
}

static Value peek(int distance) {
  if (vm.stackTop - distance - 1 < vm.stack) {
    fprintf(stderr, "Stack peek out of bounds\n");
    exit(1);
  }
  return vm.stackTop[-1 - distance];
}

static bool isFalsey(Value value) {
  return IS_NIL(value) || (IS_BOOL(value) && !AS_BOOL(value)) ||
         (IS_NUMBER(value) && AS_NUMBER(value) == 0);
}

static void concatenate() {
  ObjString *b = AS_STRING(peek(0));
  ObjString *a = AS_STRING(peek(1));

  if (a->chars == NULL || b->chars == NULL) {
    runtimeError("Cannot concatenate null strings.");
    return;
  }

  if (a->length > INT_MAX - b->length) {
    runtimeError("String concatenation overflow.");
    return;
  }

  int length = a->length + b->length;
  char *chars = ALLOCATE(char, (size_t)length + 1);
  memcpy(chars, a->chars, (size_t)a->length);
  memcpy(chars + a->length, b->chars, (size_t)b->length);
  chars[length] = '\0';

  ObjString *result = takeString(chars, length);
  pop();
  pop();
  push(OBJ_VAL(result));
}

static bool call(ObjFunction *function, int argCount) {
  if (argCount != function->arity) {
    runtimeError("Expected %d arguments but got %d.", function->arity,
                 argCount);
    return false;
  }

  if (vm.frameCount == FRAMES_MAX) {
    runtimeError("Stack overflow.");
    return false;
  }

  CallFrame *frame = &vm.frames[vm.frameCount++];
  frame->function = function;
  frame->ip = function->chunk.code;
  frame->slots = vm.stackTop - argCount - 1;
  return true;
}

static bool callValue(Value callee, int argCount) {
  if (IS_OBJ(callee)) {
    switch (OBJ_TYPE(callee)) {
    case OBJ_FUNCTION:
      return call(AS_FUNCTION(callee), argCount);
    case OBJ_NATIVE: {
      NativeFn native = AS_NATIVE(callee);
      Value result = native(argCount, vm.stackTop - argCount);
      if (vm.hadError)
        return false;
      vm.stackTop -= argCount + 1;
      push(result);
      return true;
    }
    default:
      break; // Non-callable object type.
    }
  }
  runtimeError("Can only call functions and classes.");
  return false;
}

static InterpretResult run() {
  CallFrame *frame = &vm.frames[vm.frameCount - 1];

#define READ_BYTE() (*frame->ip++)
#define READ_SHORT()                                                           \
  (frame->ip += 2, (uint16_t)((frame->ip[-2] << 8) | frame->ip[-1]))
#define READ_CONSTANT() (frame->function->chunk.constants.values[READ_BYTE()])
#define READ_STRING() AS_STRING(READ_CONSTANT())

#define BINARY_OP(valueType, op)                                               \
  do {                                                                         \
    if (!IS_NUMBER(peek(0)) || !IS_NUMBER(peek(1))) {                          \
      runtimeError("Operands must be numbers.");                               \
      return INTERPRET_RUNTIME_ERROR;                                          \
    }                                                                          \
    double b = AS_NUMBER(pop());                                               \
    double a = AS_NUMBER(pop());                                               \
    push(valueType(a op b));                                                   \
  } while (false)

  for (;;) {
    if (vm.profiler.profiling_mode) {
      vm.instruction_count++;

      if (vm.instruction_count % 10000 == 0) {
        if (checkTimeout(&vm.profiler)) {
          if (vm.profiler.infinite_loop_detected) {
            fprintf(stderr,
                    "Preflight aborted: potential infinite loop detected\n");
          } else {
            fprintf(stderr, "Preflight aborted: timeout exceeded\n");
          }
          return INTERPRET_RUNTIME_ERROR;
        }
      }

      if (!checkRecursionDepth(&vm.profiler, vm.frameCount)) {
        fprintf(stderr, "Preflight aborted: excessive recursion depth\n");
        return INTERPRET_RUNTIME_ERROR;
      }

      size_t stack_depth = (size_t)(vm.stackTop - vm.stack);
      if (stack_depth > vm.profiler.max_stack_depth) {
        vm.profiler.max_stack_depth = stack_depth;
      }
    }
#ifdef DEBUG_TRACE_EXECUTION
    printf("          ");
    for (Value *slot = vm.stack; slot < vm.stackTop; slot++) {
      printf("[ ");
      printValue(*slot);
      printf(" ]");
    }
    printf("\n");
    disassembleInstruction(&frame->function->chunk,
                           (int)(frame->ip - frame->function->chunk.code));
#endif

    uint8_t instruction;
    switch (instruction = READ_BYTE()) {
    case OP_CONSTANT: {
      Value constant = READ_CONSTANT();
      push(constant);
      break;
    }
    case OP_NIL:
      push(NIL_VAL);
      break;
    case OP_TRUE:
      push(BOOL_VAL(true));
      break;
    case OP_FALSE:
      push(BOOL_VAL(false));
      break;
    case OP_POP:
      pop();
      break;
    case OP_GET_LOCAL: {
      uint8_t slot = READ_BYTE();
      if (frame->slots + slot >= vm.stackTop) {
        runtimeError("Local variable access out of bounds.");
        return INTERPRET_RUNTIME_ERROR;
      }
      push(frame->slots[slot]);
      break;
    }
    case OP_SET_LOCAL: {
      uint8_t slot = READ_BYTE();
      if (frame->slots + slot >= vm.stackTop) {
        runtimeError("Local variable assignment out of bounds.");
        return INTERPRET_RUNTIME_ERROR;
      }
      frame->slots[slot] = peek(0);
      break;
    }
    case OP_GET_GLOBAL: {
      ObjString *name = READ_STRING();
      Value value;
      if (!tableGet(&vm.globals, name, &value)) {
        runtimeError("Undefined variable '%s'.", name->chars);
        return INTERPRET_RUNTIME_ERROR;
      }
      push(value);
      break;
    }

    case OP_SET_GLOBAL: {
      ObjString *name = READ_STRING();
      if (tableSet(&vm.globals, name, peek(0))) {
        tableDelete(&vm.globals, name);
        runtimeError("Undefined variable '%s'.", name->chars);
        return INTERPRET_RUNTIME_ERROR;
      }
      break;
    }
    case OP_EXPORT_VAR: {
      ObjString *name = READ_STRING();
      Value value;
      // Check if the variable is in the globals table first.
      if (tableGet(&vm.globals, name, &value)) {
        tableSet(&frame->function->module->variables, name, value);
      } else {
        // Fallback to the stack for locally-defined exports.
        tableSet(&frame->function->module->variables, name, peek(0));
      }
      break;
    }
    case OP_DEFINE_GLOBAL: {
      ObjString *name = READ_STRING();
      tableSet(&vm.globals, name, peek(0));
      pop();
      break;
    }
    case OP_EQUAL: {
      Value b = peek(0);
      Value a = peek(1);
      vm.stackTop -= 2;
      push(BOOL_VAL(valuesEqual(a, b)));
      break;
    }
    case OP_GREATER: {
      if (!IS_NUMBER(peek(0)) || !IS_NUMBER(peek(1))) {
        runtimeError("Operands must be numbers.");
        return INTERPRET_RUNTIME_ERROR;
      }
      double b = AS_NUMBER(peek(0));
      double a = AS_NUMBER(peek(1));
      vm.stackTop -= 2;
      push(BOOL_VAL(a > b));
      break;
    }
    case OP_LESS: {
      if (!IS_NUMBER(peek(0)) || !IS_NUMBER(peek(1))) {
        runtimeError("Operands must be numbers.");
        return INTERPRET_RUNTIME_ERROR;
      }
      double b = AS_NUMBER(peek(0));
      double a = AS_NUMBER(peek(1));
      vm.stackTop -= 2;
      push(BOOL_VAL(a < b));
      break;
    }
    case OP_ADD: {
      Value b = peek(0);
      Value a = peek(1);
      if (IS_NUMBER(a) && IS_NUMBER(b)) {
        double result = AS_NUMBER(a) + AS_NUMBER(b);
        vm.stackTop -= 2;
        push(NUMBER_VAL(result));
      } else if (IS_STRING(a) && IS_STRING(b)) {
        concatenate();
      } else {
        runtimeError("Operands must be two numbers or two strings.");
        return INTERPRET_RUNTIME_ERROR;
      }
      break;
    }
    case OP_SUBTRACT: {
      if (!IS_NUMBER(peek(0)) || !IS_NUMBER(peek(1))) {
        runtimeError("Operands must be numbers.");
        return INTERPRET_RUNTIME_ERROR;
      }
      double b = AS_NUMBER(peek(0));
      double a = AS_NUMBER(peek(1));
      vm.stackTop -= 2;
      push(NUMBER_VAL(a - b));
      break;
    }
    case OP_MULTIPLY: {
      if (!IS_NUMBER(peek(0)) || !IS_NUMBER(peek(1))) {
        runtimeError("Operands must be numbers.");
        return INTERPRET_RUNTIME_ERROR;
      }
      double b = AS_NUMBER(peek(0));
      double a = AS_NUMBER(peek(1));
      vm.stackTop -= 2;
      push(NUMBER_VAL(a * b));
      break;
    }
    case OP_DIVIDE: {
      if (!IS_NUMBER(peek(0)) || !IS_NUMBER(peek(1))) {
        runtimeError("Operands must be numbers.");
        return INTERPRET_RUNTIME_ERROR;
      }
      double b = AS_NUMBER(peek(0));
      double a = AS_NUMBER(peek(1));
      if (b == 0.0) {
        runtimeError("Division by zero.");
        return INTERPRET_RUNTIME_ERROR;
      }
      vm.stackTop -= 2;
      push(NUMBER_VAL(a / b));
      break;
    }
    case OP_MODULO: {
      if (!IS_NUMBER(peek(0)) || !IS_NUMBER(peek(1))) {
        runtimeError("Operands must be numbers.");
        return INTERPRET_RUNTIME_ERROR;
      }
      double b = AS_NUMBER(peek(0));
      double a = AS_NUMBER(peek(1));
      if (b == 0.0) {
        runtimeError("Modulo by zero.");
        return INTERPRET_RUNTIME_ERROR;
      }
      vm.stackTop -= 2;
      push(NUMBER_VAL(fmod(a, b)));
      break;
    }
    case OP_NOT: {
      Value val = peek(0);
      vm.stackTop[-1] = BOOL_VAL(isFalsey(val));
      break;
    }
    case OP_NEGATE: {
      if (!IS_NUMBER(peek(0))) {
        runtimeError("Operand must be a number.");
        return INTERPRET_RUNTIME_ERROR;
      }
      Value val = peek(0);
      vm.stackTop[-1] = NUMBER_VAL(-AS_NUMBER(val));
      break;
    }
    case OP_PRINT: {
      Value val = pop();
      if (vm.profiler.profiling_mode) {
        vm.profiler.output_operations++;
      } else {
        printValue(val);
        printf("\n");
      }
      break;
    }
    case OP_JUMP: {
      uint16_t offset = READ_SHORT();
      frame->ip += offset;
      break;
    }
    case OP_JUMP_IF_FALSE: {
      uint16_t offset = READ_SHORT();
      if (isFalsey(peek(0)))
        frame->ip += offset;
      break;
    }
    case OP_LOOP: {
      uint16_t offset = READ_SHORT();

      if (vm.profiler.profiling_mode) {
        uint64_t loop_id = (uint64_t)(frame->ip - frame->function->chunk.code);
        frame->loop_counter++;

        recordLoopIteration(&vm.profiler, loop_id);

        size_t stack_depth = (size_t)(vm.stackTop - vm.stack);
        if (!checkLoopSafety(&vm.profiler, loop_id, stack_depth)) {
          fprintf(stderr,
                  "Preflight: Loop appears infinite (no progress after %d "
                  "iterations)\n",
                  MAX_LOOP_ITERATIONS);
          return INTERPRET_RUNTIME_ERROR;
        }
      }

      frame->ip -= offset;
      break;
    }
    case OP_CALL: {
      int argCount = READ_BYTE();
      if (!callValue(peek(argCount), argCount)) {
        return INTERPRET_RUNTIME_ERROR;
      }
      frame = &vm.frames[vm.frameCount - 1];
      break;
    }
    case OP_NEW_LIST: {
      ObjList *list = newList();
      push(OBJ_VAL(list));
      break;
    }
    case OP_LIST_APPEND: {
      Value item = pop();
      ObjList *list = AS_LIST(peek(0));
      writeValueArray(list->items, item);
      break;
    }
    case OP_GET_SUBSCRIPT: {
      Value indexVal = peek(0);
      Value listVal = peek(1);

      if (!IS_LIST(listVal)) {
        runtimeError("Can only subscript lists.");
        return INTERPRET_RUNTIME_ERROR;
      }
      ObjList *list = AS_LIST(listVal);

      if (!IS_NUMBER(indexVal)) {
        runtimeError("List index must be a number.");
        return INTERPRET_RUNTIME_ERROR;
      }

      double indexDouble = AS_NUMBER(indexVal);
      if (indexDouble != (int)indexDouble) {
        runtimeError("List index must be an integer.");
        return INTERPRET_RUNTIME_ERROR;
      }

      int index = (int)indexDouble;
      if (index < 0)
        index = list->items->count + index;

      if (index < 0 || index >= list->items->count) {
        runtimeError("List index out of bounds.");
        return INTERPRET_RUNTIME_ERROR;
      }

      vm.stackTop -= 2;
      push(list->items->values[index]);
      break;
    }

    case OP_SET_SUBSCRIPT: {
      Value value = peek(0);
      Value indexVal = peek(1);
      Value listVal = peek(2);

      if (!IS_LIST(listVal)) {
        runtimeError("Can only subscript lists.");
        return INTERPRET_RUNTIME_ERROR;
      }
      ObjList *list = AS_LIST(listVal);

      if (!IS_NUMBER(indexVal)) {
        runtimeError("List index must be a number.");
        return INTERPRET_RUNTIME_ERROR;
      }

      double indexDouble = AS_NUMBER(indexVal);
      if (indexDouble != (int)indexDouble) {
        runtimeError("List index must be an integer.");
        return INTERPRET_RUNTIME_ERROR;
      }

      int index = (int)indexDouble;
      if (index < 0)
        index = list->items->count + index;

      if (index < 0 || index >= list->items->count) {
        runtimeError("List index out of bounds.");
        return INTERPRET_RUNTIME_ERROR;
      }

      list->items->values[index] = value;
      vm.stackTop -= 3;
      push(value);
      break;
    }
    case OP_IMPORT: {
      ObjString *moduleName = AS_STRING(pop());
      Value moduleValue;

      if (tableGet(&vm.modules, moduleName, &moduleValue)) {
        push(moduleValue);
      } else {
        if (moduleName == NULL || moduleName->chars == NULL) {
          runtimeError("Invalid module name.");
          return INTERPRET_RUNTIME_ERROR;
        }
        char *source = readFile(moduleName->chars);
        if (source == NULL) {
          runtimeError("Could not open module '%s'.", moduleName->chars);
          return INTERPRET_RUNTIME_ERROR;
        }

        ObjModule *module = newModule(moduleName);
        push(OBJ_VAL(module));

        tableSet(&vm.modules, moduleName, OBJ_VAL(module));

        ObjFunction *func = compile(source, module);
        free(source);

        if (func == NULL) {
          tableDelete(&vm.modules, moduleName);
          pop(); // Pop the module.
          return INTERPRET_COMPILE_ERROR;
        }

        // The module is on the stack. We pop it, push the function, and call
        // it.
        pop(); // Pop the module.
        push(OBJ_VAL(func));
        call(func, 0);
        frame = &vm.frames[vm.frameCount - 1];

        // The module has been executed. Now, copy its exported variables
        // to the global scope.
        for (int i = 0; i < module->variables.capacity; i++) {
          Entry *entry = &module->variables.entries[i];
          if (entry->key != NULL) {
            tableSet(&vm.globals, entry->key, entry->value);
          }
        }

        // The import statement leaves the module object on the stack.
        vm.stackTop[-1] = OBJ_VAL(module);
      }
      break;
    }
    case OP_EXPORT: {
      ObjString *varName = READ_STRING();
      ObjModule *module = frame->function->module;
      if (module == NULL) {
        runtimeError("Cannot export from top-level script.");
        return INTERPRET_RUNTIME_ERROR;
      }
      tableSet(&module->variables, varName, peek(0));
      // Unlike OP_DEFINE_GLOBAL, we keep the value on the stack
      // for the export statement to use.
      break;
    }
    case OP_RETURN: {
      Value result = pop();
      vm.frameCount--;

      if (vm.frameCount == 0) {
        pop(); // Pop main script function.
        return INTERPRET_OK;
      }

      vm.stackTop = frame->slots;
      push(result);

      // After returning, the current frame is the one we are returning to.
      // We need to update our local 'frame' variable to point to it.
      frame = &vm.frames[vm.frameCount - 1];
      break;
    }
    }
  }

#undef READ_BYTE
#undef READ_SHORT
#undef READ_CONSTANT
#undef READ_STRING
#undef BINARY_OP
}

static InterpretResult runPreflight(ObjFunction *function) {
  vm.profiler.profiling_mode = true;
  vm.profiler.preflight_complete = false;
  vm.instruction_count = 0;
  resetProfiler(&vm.profiler);

  resetStack();
  push(OBJ_VAL(function));
  call(function, 0);

  InterpretResult result = run();

  vm.profiler.profiling_mode = false;
  vm.profiler.preflight_complete = true;

  if (result == INTERPRET_OK && vm.profiler.total_allocations > 0) {
    size_t avg_alloc =
        vm.profiler.total_bytes_requested / vm.profiler.total_allocations;
    if (avg_alloc < 1024 && vm.profiler.total_allocations < 1000) {
    }
  }

  return result;
}

static InterpretResult runOptimized(ObjFunction *function) {
  resetStack();
  push(OBJ_VAL(function));
  call(function, 0);

  return run();
}

InterpretResult interpret(const char *path, const char *source) {
  ObjModule *mainModule =
      newModule(copyString(path, path == NULL ? 0 : strlen(path)));

  ObjFunction *function = compile(source, mainModule);
  if (function == NULL)
    return INTERPRET_COMPILE_ERROR;

  if (vm.enable_preflight) {
    InterpretResult preflight_result = runPreflight(function);

    if (preflight_result == INTERPRET_RUNTIME_ERROR) {
      if (vm.profiler.infinite_loop_detected) {
        fprintf(stderr, "Error: Code contains potentially infinite loop\n");
        return INTERPRET_RUNTIME_ERROR;
      }
    }

    if (vm.profiler.total_allocations > 0) {
      size_t predicted_memory = vm.profiler.total_bytes_requested;
      size_t optimized_memory = (size_t)(predicted_memory * 1.01);

      if (vm.nextGC < optimized_memory) {
        vm.nextGC = optimized_memory;
      }
    }
  }

  return runOptimized(function);
}
