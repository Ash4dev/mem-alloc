/*
 * File: double_end_stack_alloc.c
 * Author: Ashutosh Panigrahy
 * Created: 2026-06-02
 * Description: 
 * NOTE: keep it working & simple
 */

#include "double_end_stack_alloc.h"
#include <assert.h>
#include <stddef.h>
#include <stdint.h>

bool is_power_of_2(size_t x) {
  uintptr_t px = (uintptr_t)x;
  return (x && (px & (px-1)) == 0);
}

void adjust_offset(size_t *offset, GROWTH_DIRECTION dir, ptrdiff_t delta) {
  if (!offset) { return; }
  if ((dir*(-dir)) != -1) { return; }

  // dir (1) - towards buffer end
  // delta (+) - increase size

  // delta: assumes sane input in compliance with DES
  // responsibility: simply adjust the offset
  *offset += dir * delta;
}

uintptr_t adjust_pointer(uintptr_t ptr, GROWTH_DIRECTION dir, ptrdiff_t delta) {
  if (!ptr) { return NULL; }
  if ((dir*(-dir)) != -1) { return NULL; }

  // dir (1) - towards buffer end
  // delta (+) - increase size

  // delta: assumes sane input in compliance with DES
  // responsibility: simply adjust the pointer
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
   * header is placed just before data - enough space for header
   * start ptr MUST be aligned
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
