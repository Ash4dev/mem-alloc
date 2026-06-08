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
  /*
   * front.curr_offset = bytes from start
   * back.curr_offset  = bytes from start
   */
  size_t offset;
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
  // connects previous and current allocation
  size_t prev_offset;
  size_t curr_size;
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

// explicit offset, pointer manipulation can lead to bugs easily
void adjust_offset(size_t * /*offset*/, GROWTH_DIRECTION /*dir*/, ptrdiff_t /*delta*/);

// be careful about mixing uintptr_t and ptrdiff_t
uintptr_t adjust_pointer(uintptr_t /*ptr*/, GROWTH_DIRECTION /*dir*/, ptrdiff_t /*delta*/);

/*
 * alignment is NOT the same from both directions!
 *
 * -(x)--|---(a-x) : forward (a-x) & backward (x) - intuition is WRONG
  * forward alignment: user start address is aligned
  * backward alignment: user end address is aligned - REQUIRE START
 * C interprets address only from forward direction
 * tradeoff: symmetry for functional correctness
 *
 * draw a diagram with some allocations - to understand better
 */

size_t calc_padding_w_payload(
  uintptr_t /*ptr*/, GROWTH_DIRECTION /*dir*/,
  size_t /*alignment*/, size_t /*payload_size*/
);

// front: h -> d -> h -> d
size_t calc_forward_pad_w_header(
  uintptr_t /*ptr*/, size_t /*align*/, size_t /*header_size*/
);

// back: h <- d <- h <- d
size_t calc_backward_pad_w_header(
  uintptr_t /*ptr*/, size_t /*align*/,
  size_t /*header_size*/, size_t /*data_size*/
);

/* ----------------------------- allocator mgmt ---------------------------------------*/
// NOTE: MOSTLY type information changed & NOT interface: templates suitable

void restore_allocator(DES_Allocator * /*allocator*/);
DES_Allocator *initialize_allocator(size_t /*capacity*/);

// need to modify the DES_Allocator * - if not ** - dangling pointer
void destroy_allocator(DES_Allocator ** /*allocator_ptr*/);

/*
 * initial intuition: front-end choice should NOT be user's concern
 * stack allocator (LIFO): strict lifetime ordering
 * reason behind double ended: support different lifetimes
 * potential use-cases:
   * lifetimes: front - long, back - short term
   * network: front - outgoing, back - incoming
 * FINAL OUTCOME: front and end have semantic meaning - user's responsibility
 */

/* ----------------------------- allocator utils ---------------------------------------*/

// markers represent complete allocator state - not front/back individually
DES_Marker get_marker(DES_Allocator * /*allocator*/);
bool verify_allocator(DES_Allocator const * /*allocator*/);
void restore_to_marker(DES_Allocator * /*allocator*/, DES_Marker * /*mark*/);

size_t bytes_used(DES_Allocator const * /*allocator*/);
size_t bytes_remaining(DES_Allocator const * /*allocator*/);

bool can_allocate(
    DES_Allocator const * /*allocator*/, size_t size /*req_size*/,
    size_t alignment /*alignment*/, GROWTH_DIRECTION /*dir*/
);

#ifndef DEFAULT_ALIGNMENT
#define DEFAULT_ALIGNMENT (2*sizeof(void *))
#endif

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
