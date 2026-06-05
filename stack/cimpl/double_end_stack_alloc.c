/*
 * File: double_end_stack_alloc.c
 * Author: Ashutosh Panigrahy
 * Created: 2026-06-02
 * Description: 
 * NOTE: keep it working & simple
 */

#include "double_end_stack_alloc.h"
#include <assert.h>
#include <stdlib.h>

/* ----------------------------- utilities --------------------------------------------*/

bool is_power_of_2(size_t x) {
  uintptr_t px = (uintptr_t)x;
  return (x && (px & (px-1)) == 0);
}

void adjust_offset(size_t *offset, GROWTH_DIRECTION dir, ptrdiff_t delta) {
 /*
  * GROWTH_FORWARD - towards buffer end and +delta - increase size
  * delta: assumes sane input in compliance with DES
  * responsibility: simply adjust the offset
  */

  if (!offset) { return; }
  if ((dir*(-dir)) != -1) { return; }

  *offset += dir * delta;
}

uintptr_t adjust_pointer(uintptr_t ptr, GROWTH_DIRECTION dir, ptrdiff_t delta) {
 /*
  * GROWTH_FORWARD - towards buffer end and +delta - increase size
  * delta: assumes sane input in compliance with DES
  * responsibility: simply adjust the pointer
  */

  if (!ptr) { return NULL; }
  if ((dir*(-dir)) != -1) { return NULL; }

  return (ptr += dir * delta);
}

size_t calc_padding_w_payload(
  uintptr_t ptr, GROWTH_DIRECTION dir,
  size_t alignment, size_t payload_size
) {

  /*
   * ptr: offset where curr alloc can ideally begin
   * padding using ptr alone is NOT enough : header needed
   *
   * requirements:
   * header is placed just before data - enough space for header
   * start pointer MUST be aligned
   *
   * responsibility: return aligned pad where data can be placed
   */

  assert(is_power_of_2(alignment));
  assert(payload_size != 0);

  // Compute misalignment of ptr relative to alignment boundary
  size_t misalign = (size_t)(ptr & ((uintptr_t)alignment - 1));

  // Compute the minimum padding needed to align ptr alone (no header)
  size_t align_pad = 0;
  if (misalign != 0) {
    if (dir == GROWTH_FORWARD) { align_pad = alignment - misalign; }
    else if (dir == GROWTH_BACKWARD) { align_pad = misalign; }
  }

  // If the natural alignment padding already fits the header, we're done
  if (align_pad >= payload_size) { return align_pad; }

  // Otherwise, extend padding by whole alignment multiples until header fits
  size_t shortfall = payload_size - align_pad;
  size_t extra = alignment * ((shortfall + alignment - 1) / alignment);
  return align_pad + extra;
}

size_t calc_forward_pad_w_header(
  uintptr_t ptr, size_t alignment, size_t header_size
) {
  return calc_padding_w_payload(ptr, GROWTH_FORWARD, alignment, header_size);
}

size_t calc_backward_pad_w_header(
  uintptr_t ptr, size_t alignment,
  size_t header_size, size_t data_size
) {
  return calc_padding_w_payload(ptr, GROWTH_BACKWARD, alignment, header_size + data_size);
}

/* ----------------------------- allocator mgmt ---------------------------------------*/

void restore_allocator(DES_Allocator *allocator) {
  if (!allocator) { return; }
  allocator->front.curr_offset = 0;
  allocator->front.prev_offset = 0;
  allocator->front.direction = GROWTH_FORWARD;

  allocator->back.curr_offset = allocator->capacity-1;
  allocator->back.prev_offset = allocator->capacity-1;
  allocator->back.direction = GROWTH_BACKWARD;
}

DES_Allocator *initialize_allocator(size_t capacity) {
  DES_Allocator *allocator = malloc(sizeof(DES_Allocator));
  if (!allocator) { return NULL; }

  allocator->buffer = malloc(capacity);
  if (!(allocator->buffer)) {
    free(allocator);
    return NULL;
  }

  allocator->capacity = capacity;

  restore_allocator(allocator);
  return allocator;
}

void destroy_allocator(DES_Allocator **allocator_ptr) {
  if (!allocator_ptr) { return; }

  free((*allocator_ptr)->buffer);
  (*allocator_ptr)->buffer = NULL;
  free((*allocator_ptr));
  *allocator_ptr = NULL;
}

/* ----------------------------- allocator utils ---------------------------------------*/

// markers represent complete allocator state - not front/back individually
DES_Marker get_marker(DES_Allocator *allocator) {
  return (DES_Marker){
    .front=allocator->front,
    .back=allocator->back
  };
}

bool verify_allocator(DES_Allocator const *allocator) {
  if (!allocator) { return false; }

  if (!(
      (allocator->front.curr_offset <= allocator->capacity) ||
      (allocator->back.curr_offset <= allocator->capacity)  ||
      (allocator->front.prev_offset <= allocator->capacity) ||
      (allocator->back.prev_offset <= allocator->capacity)
    )) { return false; }

  if (allocator->front.curr_offset > allocator->back.curr_offset) { return false; }

  return true;
}

void restore_to_marker(DES_Allocator *allocator, DES_Marker *mark) {
  if (!verify_allocator(allocator)) { return; }

  allocator->front.curr_offset = mark->front.curr_offset;
  allocator->front.prev_offset = mark->front.prev_offset;
  allocator->back.curr_offset = mark->back.curr_offset;
  allocator->back.curr_offset = mark->back.curr_offset;
}

size_t bytes_used(DES_Allocator const *allocator) {
  if (!verify_allocator(allocator)) { return 0; }
  return allocator->front.curr_offset + (allocator->capacity - allocator->back.curr_offset);
}

size_t bytes_remaining(DES_Allocator const *allocator) {
  if (!verify_allocator(allocator)) { return 0; }
  return (allocator->capacity - bytes_used(allocator));
}

/* ----------------------------- add / allocation -------------------------------------*/

