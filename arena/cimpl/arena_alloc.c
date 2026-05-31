/*
 * File: arena_alloc.c
 * Author: Ashutosh Panigrahy
 * Created: 2026-05-29
 * Description: Simple arena allocator implementation
 * NOTE: keep it working & simple
 */

#include "arena_alloc.h"
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>

typedef struct {
  double d; // 0-7
  long long ll; // 8-15
} s1; // alignment: max(8,8) | size: 16

typedef struct {
  float f; // 0-3
  int i; // 4-7
} s2; // alignment: max(4,4) | size: 8

typedef struct {
  short s; // 0-1
  char c; // 1-2
  // 2-3: tail pad
} s3; // alignment: max(2,1) | size: 4

typedef struct {
  s1 _s1; // 0-15
  s2 _s2; // 16-23
  s3 _s3; // 24-27
  // 28-31: tail-pad
} Foo; // alignment: max(8,4,2) | size: 32

size_t get_size() {
  switch(rand() % 4) {
    case 0:
      return sizeof(s1);
    case 1:
      return sizeof(s2);
    case 2:
      return sizeof(s3);
    case 3:
      return sizeof(Foo);
    default:
      return sizeof(Foo);
  }
  return -1;
}

void test_alignment(Arena_Allocator *a){
  free_arena(a);

  char *p1 = allocate_aligned(a, 1, 32);
  assert(((uintptr_t)p1 % 32) == 0);

  char *p2 = allocate_aligned(a, 1, 64);
  assert(((uintptr_t)p2 % 64) == 0);
}

void test_consecutive_allocations(Arena_Allocator *arena) {
  free_arena(arena);
  int const N_ITERS = 1;
  int const N_ALLOCS = 5;

  for (size_t i = 0; i < N_ITERS; ++i){
    free_arena(arena);

    for (int j = 0; j < N_ALLOCS; ++j){
      size_t size = get_size();
      printf("To allocate size of : %zu\n", size);
      printf("Before: %zu to %zu\n", arena->prev_offset, arena->curr_offset);
      allocate(arena, size);
      printf("After: %zu to %zu\n", arena->prev_offset, arena->curr_offset);
    }

    printf("-----------------------------------------------------------------\n");
  }
}

void test_exact_fit_alloc(Arena_Allocator *a) {
  free_arena(a);

  void *p = allocate(a, 128);

  assert(p != NULL);
  assert(a->curr_offset == 128);
  assert(allocate(a, 1) == NULL);
}

void test_grow_last_allocation(Arena_Allocator *a) {
  free_arena(a);

  char *p = allocate(a, 16);
  strcpy(p, "hello");

  char *q = resize_alloc(a, p, 16, 32);

  assert(q == p);
  assert(strcmp(q, "hello") == 0);
  assert(a->curr_offset == 32);
}

void test_grow_last_allocation_till_arena_size(Arena_Allocator *a){
  free_arena(a);

  char *p = allocate(a, 16);

  assert(
      resize_alloc(a, p, 16, 128) == p
  );

  assert(a->curr_offset == 128);
}

void test_shrink_last_allocation(Arena_Allocator *a) {
  free_arena(a);

  char *p = allocate(a, 32);
  char *q = resize_alloc(a, p, 32, 8);

  assert(q == p);
  assert(a->curr_offset == 8);
}

void test_grow_non_last_allocation(Arena_Allocator *a){
  free_arena(a);
  char *p1 = allocate(a, 16);
  char *p2 = allocate(a, 16);

  char *p3 = resize_alloc(a, p1, 16, 32);
  assert(p3 != p1);
  assert(a->curr_offset == 64);
}

void test_resize_beyond_capacity(Arena_Allocator *a) {
  free_arena(a);

  char *p = allocate(a, 32);

  assert(
      resize_alloc(a, p, 32, 256) == NULL
  );
}

// keeping things simple
int main() {
  printf("default alignment: %zu\n", DEFAULT_ALIGNMENT);

  Arena_Allocator *arena = initialize_arena(128);

  test_alignment(arena);
  test_consecutive_allocations(arena);
  test_grow_last_allocation(arena);
  test_grow_last_allocation_till_arena_size(arena);
  test_shrink_last_allocation(arena);
  test_grow_non_last_allocation(arena);
  test_resize_beyond_capacity(arena);

  destroy_arena(arena);
  return 0;
}

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

void destroy_arena(Arena_Allocator *arena) {
  // malloc cost - manual clean-up

  if (!arena) { return; }

  free(arena->buffer);
  arena->buffer = NULL;
  free(arena);
  arena = NULL;
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
