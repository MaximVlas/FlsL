#ifndef FLS_TABLE_H
#define FLS_TABLE_H

#include "common.h"
#include "value.h"

// An entry in the hash table.
typedef struct {
    ObjString* key;
    Value value;
} Entry;

// The hash table itself.
typedef struct {
    int count;
    int capacity;
    Entry* entries;
} Table;

// Initializes a hash table.
void initTable(Table* table);

// Frees all memory associated with a hash table.
void freeTable(Table* table);

// Gets a value from the table. Returns true if the key was found.
bool tableGet(Table* table, ObjString* key, Value* value);

// Adds a key-value pair to the table. Returns true if it's a new key.
bool tableSet(Table* table, ObjString* key, Value value);

// Deletes a key from the table. Returns true if the key was found and deleted.
bool tableDelete(Table* table, ObjString* key);

// Copies all entries from one table to another.
void tableAddAll(Table* from, Table* to);

// Finds a string key within the table's entry array.
ObjString* tableFindString(Table* table, const char* chars, int length, uint32_t hash);

#endif // FLS_TABLE_H
