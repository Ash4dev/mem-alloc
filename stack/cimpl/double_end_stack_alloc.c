/*
 * File: double_end_stack_alloc.c
 * Author: Ashutosh Panigrahy
 * Created: 2026-06-02
 * Description: 
 * NOTE: keep it working & simple
 */

#include "double_end_stack_alloc.h"
#include <assert.h>
#include <stdalign.h>
#include <stdlib.h>
#include <string.h>

/* memory buffer of bytes - allocated from heap
 * offsets: [data] allocation can begin(end) from here - best-case
   * offsets are boundaries, NOT memory block
   * f: fwd (begin), b: bwd (end)
 * addresses need alignment, NOT offsets
 *
 * 0           ->            <-               CAP
 *            f             b
 * [= = = = = = = = = = = = = = = = = = = = = =]
 *    [hdr]-> [data]        [hdr]-> [data]
 *
 * free space: [f, b) - close-open interval
   * valid DES allocator invariant : f < b
 *
 * allocation: aligned data start pointer to the user
   * fwd: needs ONLY header size
   * bwd: needs ONLY data size
     * C interprets bytes ONLY in fwd direction
 *
 * ----------------------------------------------------
 *
 * https://medium.com/@aliaksandr.kavalchuk/%D1%81-interview-questions-structure-alignment-in-c-07c8ea7e1d27
 * padding logic is direction-agnostic
 *      al      p           au
 * [= = = = = = = = = = = = = = = = = = = = = =]
 *                [payload] y
 *                [payload info]
 *
 * p = buffer + f OR b
 * misalign: p%alignment
   * alignment must be power of 2 - ease of math
   * pad := fwd : alignment-misalign, bwd: misalign
 *
 * [open to suggestions for a better name]: payload
 * pad may NOT be enough to store payload
   * fwd: payload_size := header_size
   * bwd: payload_size := data_size
 *
 * shortfall = payload_size - pad
 * pad += alignment * ceil(shortfall/alignment)
 *
 * data_ptr = p + dir * pad - MUST be aligned
 * head_ptr = data_ptr - sizeof(DES_Header)
 *
 * ----------------------------------------------------
 *
 * dp (data start pointer) = f OR b + dir * calc_pad...
 * distance from cursor to the aligned user pointer
 * such that the required payload fits on the opposite side
 * payload immediately preceeds - corr. to direction
 *
 * IDENTICAL layout
 * Forward: [dp-hsz .. dp-1] [dp .. dp+dsz-1]
 * Backward: [dp-hsz .. dp-1] [dp .. dp+dsz-1]
 *
 * For example,
 * alignment = 16 B, HS = 8 B, DS = 16 B, capacity = 1024 B
 * Forward:
 * (08) (09) ... (14) (15)  || (16) (17) ... (30) (31)
 *  |   |         |    |        |    |        |    |
 * (09) (10) ... (15) (16)  || (17) (18) ... (31) (32)
 *
 * Backward:
 * (1008) (1009)  ... (1006) (1007)  || (1008) (1009) ... (1022)  (1023)
 *   |      |           |      |          |      |          |       |
 * (1009) (1010) ... (1005) (1008)  || (1009) (1010) ... (1023) (1024)
 * ----------------------------------------------------
 *
 * alignment of N (=2^p*3^q*...) ensures
   * addr % N == 0 => addr % 2^x == 0 (x=0..p)
   * generally addr % factor == 0
 * if header_size is a factor of N - then header is also aligned
 *
 * NOTE: if alignment is forced to be power of 2 - so must be the header_size
 */

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
  if (dir == GROWTH_FORWARD)
    *offset += (size_t)delta;
  else
    *offset -= (size_t)delta;
}

uintptr_t adjust_pointer(uintptr_t ptr, GROWTH_DIRECTION dir, ptrdiff_t delta) {
 /*
  * GROWTH_FORWARD - towards buffer end and +delta - increase size
  * delta: assumes sane input in compliance with DES
  * responsibility: simply adjust the pointer
  */

  if (!ptr) { return NULL; }
  if (dir == GROWTH_FORWARD)
    ptr += (size_t)delta;
  else
    ptr -= (size_t)delta;
  return ptr;
}

size_t calc_padding_w_payload(
  uintptr_t ptr, GROWTH_DIRECTION dir,
  size_t alignment, size_t payload_size
) {

  /*
   * ptr: offset where curr alloc can ideally begin
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
  return calc_padding_w_payload(ptr, GROWTH_BACKWARD, alignment, data_size);
}

/* ----------------------------- allocator mgmt ---------------------------------------*/

void restore_allocator(DES_Allocator *allocator) {
  if (!allocator) { return; }
  // data allocation may begin from here - best case
  allocator->front.offset = 0;
  allocator->front.direction = GROWTH_FORWARD;

  // data allocation may end at here - best case
  allocator->back.offset = allocator->capacity;
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
      (allocator->front.offset <= allocator->capacity) &&
      (allocator->back.offset <= allocator->capacity)
    )) { return false; }

  // f == b : exhausted allocator - still valid - NOT allocatable
  if (allocator->front.offset > allocator->back.offset) { return false; }

  return true;
}

void restore_to_marker(DES_Allocator *allocator, DES_Marker *mark) {
  if (!verify_allocator(allocator)) { return; }

  allocator->front.offset = mark->front.offset;
  allocator->back.offset = mark->back.offset;
}

size_t bytes_remaining(DES_Allocator const *allocator) {
  if (!verify_allocator(allocator)) { return 0; }
  return (allocator->back.offset - allocator->front.offset);
}

size_t bytes_used(DES_Allocator const *allocator) {
  if (!verify_allocator(allocator)) { return 0; }
  return allocator->capacity - bytes_remaining(allocator);
}

/* ----------------------------- add / allocation -------------------------------------*/

bool can_allocate(
    DES_Allocator const *allocator, size_t data_size,
    size_t aligned_pad, GROWTH_DIRECTION dir
) {
  if (!verify_allocator(allocator)) { return false; }

  // final state:                                     => fo_new < bo_new
  // fwd:                                             => fo + aligned_pad + data_size < bo
  // bwd: fo < bo - aligned_pad - header_size         => fo + aligned_pad + header_size < bo
  size_t front_off = allocator->front.offset;
  size_t back_off = allocator->back.offset;

  return (
  front_off + aligned_pad +
  (dir == GROWTH_FORWARD) * data_size + (dir == GROWTH_BACKWARD) * sizeof(DES_Header)
  ) < back_off;
}

void *allocate_aligned(
    DES_Allocator * allocator, size_t data_size,
    GROWTH_DIRECTION dir, size_t alignment
) {
  // sanity checks
  if (!verify_allocator(allocator)) { return NULL; }
  if (dir != GROWTH_FORWARD && dir != GROWTH_BACKWARD) { return NULL; }
  if (!is_power_of_2(alignment)) { return NULL; }

  // alignof - compile time - operator - determines alignment requirement
  // alignment requirements are always powers of 2 - same as sizeof - for fundamental types
  alignment = (alignment < alignof(DES_Header)) ? alignof(DES_Header) : alignment;

  // current position in the buffer
  size_t *curr_offset_ptr = (dir == GROWTH_FORWARD) ? &allocator->front.offset : &allocator->back.offset;
  // NOTE: offsets are measured from buffer start - GROWTH_FORWARD fixed
  uintptr_t curr_ptr = adjust_pointer((uintptr_t)allocator->buffer, GROWTH_FORWARD, *curr_offset_ptr);

  // calculate amount of padding to align the payload
  size_t payload_size = (dir == GROWTH_FORWARD) * sizeof(DES_Header) + (dir == GROWTH_BACKWARD) * data_size;
  size_t aligned_pad = calc_padding_w_payload(curr_ptr, dir, alignment, payload_size);

  // check if allocation is possible
  if (!can_allocate(allocator, data_size, aligned_pad, dir)) { return NULL; }
  uintptr_t data_ptr = adjust_pointer(curr_ptr, dir, aligned_pad);

  // mark the header
  DES_Header *header = (DES_Header *)adjust_pointer(data_ptr, dir, (-dir) * sizeof(DES_Header));
  header->prev_offset = *curr_offset_ptr;
  header->curr_size = data_size;

  // adjust current offset
  size_t true_delta = aligned_pad +
    (dir == GROWTH_FORWARD) * data_size + (dir == GROWTH_BACKWARD) * sizeof(DES_Header);
  adjust_offset(curr_offset_ptr, dir, true_delta);

  // return data pointer after initializing with 0-like
  return memset((void *)data_ptr, 0, data_size);
}

void *push_front(DES_Allocator *allocator, size_t data_size) {
  return allocate_aligned(allocator, data_size, GROWTH_FORWARD, DEFAULT_ALIGNMENT);
}
void *push_back(DES_Allocator *allocator, size_t data_size) {
  return allocate_aligned(allocator, data_size, GROWTH_BACKWARD, DEFAULT_ALIGNMENT);
}

/* ----------------------------- free / deallocation ----------------------------------*/
void pop_last_element(DES_Allocator *allocator, void *ptr, GROWTH_DIRECTION dir) {
  // validation
  if (!ptr) { return; }
  if (!verify_allocator(allocator)) { return; }
  if (!inside_buffer(allocator, ptr)) { return; }

  // check for double free
  if (beyond_cursor(allocator, ptr, dir)) { return; }

  if (!is_last_allocation(allocator, ptr, dir)) { return; }

  DES_Header *header = (DES_Header *)((uintptr_t)ptr - sizeof(DES_Header));
  if (dir == GROWTH_FORWARD)
    allocator->front.offset = header->prev_offset;
  else
    allocator->back.offset = header->prev_offset;
}

void pop_front(DES_Allocator *allocator, void *ptr) {
  pop_last_element(allocator, ptr, GROWTH_FORWARD);
}
void pop_back(DES_Allocator *allocator, void *ptr) {
  pop_last_element(allocator, ptr, GROWTH_BACKWARD);
}

