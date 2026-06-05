/*
 * File: arena_alloc.c
 * Author: Ashutosh Panigrahy
 * Created: 2026-05-29
 * Description: Simple arena allocator implementation
 * NOTE: keep it working & simple
 */

#include "arena_alloc.h"
#include <stdlib.h>
#include <assert.h>
#include <string.h>

bool is_power_of_2(size_t x) {
  uintptr_t px = (uintptr_t)x;
  return (x && (px & (px-1)) == 0);
}

uintptr_t align_forward(uintptr_t ptr, size_t align) {
  assert(is_power_of_2(align));
  uintptr_t align_ptr = (uintptr_t)align;

  // take a pen & paper - draw it
  uintptr_t modulo = (ptr & (align_ptr-1)); // fast-modulo - pow 2
  if (modulo != 0) {
    ptr += (align_ptr - modulo);
  }
  return ptr;
}

Arena_Allocator *initialize_arena(size_t capacity) {
  // malloc : uninitialized - continuous - system call penalty

  // malloc - longer lifetime
  Arena_Allocator *arena = malloc(sizeof(Arena_Allocator));
  if (!arena) { return NULL; }

  // size NOT known ahead / calloc - init cleaned memory - slower
  arena->buffer = calloc(capacity, sizeof(*(arena->buffer)));
  if (!arena->buffer) { free(arena); return NULL; }

  arena->capacity = capacity;
  arena->prev_offset = 0;
  arena->curr_offset = 0;

  return arena;
}

void destroy_arena(Arena_Allocator **arena_ptr) {
  // malloc cost - manual clean-up

  if (!arena_ptr) { return; }

  free((*arena_ptr)->buffer);
  (*arena_ptr)->buffer = NULL;
  free((*arena_ptr));
  *arena_ptr = NULL;
}

void *allocate_aligned(Arena_Allocator *arena, size_t size, size_t alignment) {
  if ((!arena)|| (!is_power_of_2(alignment))) { return NULL; }

  // establish current state
  uintptr_t curr_ptr = (uintptr_t)arena->buffer + (uintptr_t)arena->curr_offset;

  // current allocation has the responsibility to align the curr offset
  uintptr_t aligned_ptr = align_forward(curr_ptr, alignment);
  size_t new_offset = (size_t)(aligned_ptr - (uintptr_t)arena->buffer);

  // decide on allocation - size_t comp. intuitive than ptr comp.
  // size_t: unsigned - wrap around defined on addition - subtraction pattern
  if (size > (arena->capacity - new_offset)) { return NULL; }

  // actual memory allocation - NOT construction - user's responsibility
  void *allocated_ptr = arena->buffer + new_offset;
  memset(allocated_ptr, 0, size); // mark as ZERO-like - courtesy

  // update state after allocation
  arena->prev_offset = new_offset;
  arena->curr_offset = new_offset + size;

  return allocated_ptr;
}
void *allocate(Arena_Allocator *arena, size_t size) {
  return allocate_aligned(arena, size, DEFAULT_ALIGNMENT);
}

void free_alloc(Arena_Allocator *arena, void *ptr) {
  /* DOES NOTHING. JUST FOR COMPLETENESS */
}
void free_arena(Arena_Allocator *arena) {
  if (!arena) { return; }
  arena->prev_offset = arena->curr_offset = 0;
}

void *resize_alloc_aligned(Arena_Allocator *arena, void *old_address, size_t old_size, size_t new_size, size_t alignment) {
  // input validation
  if (!arena) { return NULL; }

  if (
    (!old_address) ||
    ((uintptr_t)old_address < (uintptr_t)arena->buffer) ||
    ((uintptr_t)old_address >= ((uintptr_t)arena->buffer + arena->capacity))
  ) { return NULL; }

  if (
    (old_size == 0) || (new_size == 0)
  ) { return NULL; }

  if ((!is_power_of_2(alignment))) { return NULL; }

  /*
  * PRECONDITION:
  * old_address and old_size must correspond
  * to a previous arena allocation.
  */

  // trivial case : no point in resizing
  if (old_size == new_size) { return old_address; }

  // overflow-check
  size_t old_offset = (size_t)((uintptr_t)old_address - (uintptr_t)arena->buffer);
  if (new_size > (arena->capacity - old_offset)) { return NULL; };

  // was this allocated just previously?
  if (old_offset == arena->prev_offset) {
    // resize it

    if (old_size >= new_size) {
      arena->curr_offset -= (old_size-new_size);
      return old_address;
    }

    void *curr_ptr = arena->buffer + arena->curr_offset;
    size_t growth = new_size - old_size;
    memset(curr_ptr, 0, growth);
    arena->curr_offset += growth;
    return old_address;
  }

  // allocate it at the end
  void *new_address = allocate_aligned(arena, new_size, alignment);
  if (!new_address) { return NULL; }

  // copy content from old to new
  memmove(new_address, old_address, (new_size > old_size) ? old_size : new_size);
  return new_address;
}
void *resize_alloc(Arena_Allocator *arena, void *old_address, size_t old_size, size_t new_size) {
  return resize_alloc_aligned(arena, old_address, old_size, new_size, DEFAULT_ALIGNMENT);
}

Arena_Marker get_marker(Arena_Allocator *arena) {
  Arena_Marker m;
  if (!arena) {
    m.is_valid = false;
    return m;
  }

  m.prev_offset = arena->prev_offset;
  m.curr_offset = arena->curr_offset;
  m.is_valid = true;
  return m;
}

void restore_to_marker(Arena_Allocator *arena, Arena_Marker *marker) {
  if ((!arena) || (!marker)) { return; }

  if (!marker->is_valid) { return; }

  // A B (M1) C D (M2) : arena state
  // M2 to M1 : don't delete C, D - may restore back to M2

  arena->prev_offset = marker->prev_offset;
  arena->curr_offset = marker->curr_offset;
}
