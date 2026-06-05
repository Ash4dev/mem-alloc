/*
 * File: main.c
 * Author: Ashutosh Panigrahy
 * Created: 2026-06-01
 * Description: 
 * NOTE: keep it working & simple
 */

#include "arena_alloc.h"
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

void test_invalid_marker(Arena_Allocator *a) {
  free_arena(a);

  Arena_Marker m = {0};
  m.is_valid = false;

  allocate(a, 16);

  size_t old_curr = a->curr_offset;

  restore_to_marker(a, &m);

  assert(a->curr_offset == old_curr);
}

void test_marker_restore(Arena_Allocator *a) {
  free_arena(a);

  char *p1 = allocate(a, 16);
  char *p2 = allocate(a, 16);

  Arena_Marker m = get_marker(a);

  char *p3 = allocate(a, 16);

  assert(a->curr_offset > m.curr_offset);

  restore_to_marker(a, &m);

  assert(a->curr_offset == m.curr_offset);
  assert(a->prev_offset == m.prev_offset);

  char *p4 = allocate(a, 16);

  // space previously occupied by p3 reused
  assert(p4 == p3);
}

void test_nested_markers(Arena_Allocator *a) {
  free_arena(a);

  allocate(a, 16); // A
  allocate(a, 16); // B

  Arena_Marker m1 = get_marker(a);

  char *c = allocate(a, 16); // C
  char *d = allocate(a, 16); // D

  Arena_Marker m2 = get_marker(a);

  char *e = allocate(a, 16); // E

  restore_to_marker(a, &m2);

  char *e2 = allocate(a, 16);

  assert(e2 == e);

  restore_to_marker(a, &m1);

  char *c2 = allocate(a, 16);

  assert(c2 == c);
}

void test_marker_does_not_restore_memory_contents(Arena_Allocator *a) {
  free_arena(a);

  char *p = allocate(a, 16);

  strcpy(p, "hello");

  Arena_Marker m = get_marker(a);

  strcpy(p, "world");

  restore_to_marker(a, &m);

  // marker restores allocation state only
  assert(strcmp(p, "world") == 0);
}

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

  test_invalid_marker(arena);
  test_marker_restore(arena);
  test_nested_markers(arena);
  test_marker_does_not_restore_memory_contents(arena);

  destroy_arena(&arena);
  return 0;
}

