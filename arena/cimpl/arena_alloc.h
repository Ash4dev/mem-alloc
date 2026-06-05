/*
 * File: arena_alloc.h
 * Author: Ashutosh Panigrahy
 * Created: 2026-05-29
 * Description: Simple arena allocator interface
 * NOTE: keep it working & simple
 */

// useful for objects with similar lifetimes: "frame: video-game"
// memory allocation != object construction

#ifndef ARENA_ALLOC_H
#define ARENA_ALLOC_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

// ------------------------------------utilities---------------------------------------

// https://stackoverflow.com/a/1845491
// uintptr_t: unsigned integer capable of storing any data pointer (address: int)

// https://stackoverflow.com/a/40941933
// uintptr_t & intptr_t : useful when numeric conversions are made on pointers
// void *: useful for storing a pointer to something

// similar to static member functions
bool is_power_of_2(size_t /*x*/);
uintptr_t align_forward(uintptr_t /*ptr*/, size_t /*align*/);

// ---------------------------------arena allocator-----------------------------------

// https://stackoverflow.com/a/24181368
// aim: obtain raw memory bytes for allocation - char : signed, void : no ptr math

typedef unsigned char buf_t;
typedef struct {
  buf_t *buffer;
  size_t capacity;

  size_t prev_offset;
  size_t curr_offset;

  /*
   * Add-ons: Basic telemetry
   * size_t peak_usage : max(peak_usage, curr_offset)
   * size_t operation_count: += 1 (allocate/deallocate)
   * size_t wasted_alignment_bytes: allocate : += new_offset - arena->curr_offset
   */
} Arena_Allocator;

// carries minimum info to restore allocation progress - not entire Arena state
typedef struct {
  // for current implementation - prev_offset, next_offset is enough
  // happens to be entire arena state - co-incidence
  size_t prev_offset; // required for resize
  size_t curr_offset;

  bool is_valid;
} Arena_Marker;

// why is this a good choice?
#ifndef DEFAULT_ALIGNMENT
#define DEFAULT_ALIGNMENT (2*sizeof(void *))
#endif

// C(gingerBill) to RAII-like pattern evolution
Arena_Allocator *initialize_arena(size_t /*capacity*/);
void destroy_arena(Arena_Allocator ** /*a*/);

// non-static member functions : first arg is Arena_Allocator - this
void *allocate_aligned(
    Arena_Allocator * /*a*/, size_t /*req_size*/, size_t /*alignment*/
);
void *allocate(Arena_Allocator * /*a*/, size_t /*req_size*/);

void free_alloc(Arena_Allocator * /*a*/, void * /*ptr*/);
void free_arena(Arena_Allocator * /*a*/);

void *resize_alloc_aligned(
    Arena_Allocator * /*a*/,
    void * /*old_mem_addr*/, size_t /*old_size*/,
    size_t /*new_size*/, size_t /*alignment*/
);
void *resize_alloc(
    Arena_Allocator * /*a*/,
    void * /*old_mem_addr*/, size_t /*old_size*/,
    size_t /*new_size*/
);

Arena_Marker get_marker(Arena_Allocator * /*a*/);
void restore_to_marker(Arena_Allocator * /*a*/, Arena_Marker * /*m*/);

/* Add-ons: Poison after restore
 * ARENA_POISON_PATTERN 0xAA
 * void restore_to_marker_poison(
    Arena_Allocator *arena,
    Arena_Marker *marker,
    uint8_t pattern
);
 * memset(arena->buffer + marker->curr_offset, pattern, arena->curr_offset - marker->curr_offset)
 * update arena prev_offset and curr_offset
 */
#endif
