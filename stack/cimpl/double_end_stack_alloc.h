/*
 * File: double_end_stack_alloc.h
 * Author: Ashutosh Panigrahy
 * Created: 2026-06-02
 * Description: 
 * NOTE: keep it working & simple
 */

#ifndef DOUBLE_END_STACK_ALLOC_H
#define DOUBLE_END_STACK_ALLOC_H

// size_t, ptrdiff_t
#include <stddef.h>
// bool
#include <stdbool.h>
// uintptr_t
#include <stdint.h>

typedef enum {
  GROWTH_BACKWARD = -1,
  GROWTH_FORWARD = 1,
} GROWTH_DIRECTION;

typedef struct DE_Stack_Allocator_State {
  // TODO: if header has prev_offset why carry?
  size_t prev_offset;
  /*
   * front.curr_offset = bytes from start
   * back.curr_offset  = bytes from start
   */
  size_t curr_offset;
  GROWTH_DIRECTION direction;
} DES_State;

// markers can be nested
typedef struct DE_Stack_Allocator_Marker {
  // NOT strongly tied to State - just happens to be - co-incidence
  DES_State front;
  DES_State back;
} DES_Marker;

// NOTE: actual new addition
typedef struct DE_Stack_Allocator_Header {
  // TODO: get more clarity on what's required & not

  // connects previous and current allocation
  size_t past_offset;
  size_t pad_reqd;
  // prev_offset - curr_offset enough? is below optional?
  size_t curr_size; // use: free_alloc (curr size info)
} DES_Header;

typedef unsigned char buffer_t;
typedef struct DE_Stack_Allocator {
  buffer_t *buffer;

  // also possible with arena
  DES_State front;
  DES_State back;

  size_t capacity;
  /* Add-ons: minimal telemetry
   * size_t operation_count;
   *
   * buffer-related with buffer owner
   * size_t peak_usage;
   * size_t wasted_alignment_bytes;
   */
} DES_Allocator;

/* ----------------------------- utilities --------------------------------------------*/

bool is_power_of_2(size_t /*x*/);

// explicit +,- can lead to bugs
// dir (1) - towards buffer end
// delta (+) - increase size
void adjust_offset(size_t * /*offset*/, GROWTH_DIRECTION /*dir*/, ptrdiff_t /*delta*/);
// be careful about mixing uintptr_t and ptrdiff_t
uintptr_t adjust_pointer(uintptr_t /*ptr*/, GROWTH_DIRECTION /*dir*/, ptrdiff_t /*delta*/);

// TODO: forward & backward implementation - initial padding calculate differently
size_t calc_padding_w_header(
    uintptr_t /*ptr*/, GROWTH_DIRECTION /*dir*/,
    size_t /*align*/, size_t /*header_size*/
);

/* ----------------------------- allocator mgmt ---------------------------------------*/

// NOTE: MOSTLY type information changed & NOT interface: templates suitable
DES_Allocator *initialize_allocator(size_t /*capacity*/);

// buffer != NULL
// front.curr_offset <= capacity && back.curr_offset <= capacity
// front.curr_offset <= back.curr_offset

bool verify_allocator(const DES_Allocator * /*allocator*/);
void destroy_allocator(DES_Allocator * /*allocator*/);
void clear_allocator(DES_Allocator * /*allocator*/);

// markers represent complete allocator state - not front/back individually
DES_Marker get_marker(DES_Allocator * /*allocator*/);

// performs validity checks
bool verify_allocator(const DES_Allocator * /*allocator*/);
void restore_to_marker(DES_Allocator * /*allocator*/, DES_Marker * /*mark*/);

/*
 * initial intuition: front-end choice should NOT be user's concern
 * stack allocator (LIFO): strict lifetime ordering
 * reason behind double ended: support different lifetimes
 * potential use-cases:
   * lifetimes: front - long, back - short term
   * network: front - outgoing, back - incoming
 * FINAL OUTCOME: front and end have semantic meaning - user's responsibility
 */

size_t bytes_used(const DES_Allocator * /*allocator*/);
size_t bytes_remaining(const DES_Allocator * /*allocator*/);

bool has_space(
    const DES_Allocator * /*allocator*/, size_t size /*req_size*/,
    size_t alignment /*alignment*/, GROWTH_DIRECTION /*dir*/
);

/* ----------------------------- add / allocation -------------------------------------*/

void *allocate_aligned(
    DES_Allocator * /*allocator*/, size_t /*req_size*/,
    GROWTH_DIRECTION /*dir*/, size_t /*alignment*/
);

void *push_front(DES_Allocator * /*allocator*/, size_t /*req_size*/);
void *push_back(DES_Allocator * /*allocator*/, size_t /*req_size*/);

/* ----------------------------- free / deallocation ----------------------------------*/

void pop_front(DES_Allocator * /*allocator*/, void * /*ptr*/);
void pop_back(DES_Allocator * /*allocator*/, void * /*ptr*/);

void clear_front(DES_Allocator * /*allocator*/);
void clear_back(DES_Allocator * /*allocator*/);

/* ----------------------------- resize / reallocation --------------------------------*/

void *resize_alloc_aligned(
    DES_Allocator * /*allocator*/, void * /*old_mem_addr*/,
    size_t /*new_size*/, GROWTH_DIRECTION /*dir*/,
    size_t /*alignment*/
);

void *resize_alloc_front(
    DES_Allocator * /*allocator*/, void * /*old_mem_addr*/, size_t /*new_size*/
);
void *resize_alloc_back(
    DES_Allocator * /*allocator*/, void * /*old_mem_addr*/, size_t /*new_size*/
);

#endif // !DOUBLE_END_STACK_ALLOC_H
