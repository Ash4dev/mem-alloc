/*
 * File: double_end_stack_alloc.c
 * Author: Ashutosh Panigrahy
 * Created: 2026-06-02
 * Description: 
 * NOTE: keep it working & simple
 */

#include "double_end_stack_alloc.h"
#include <assert.h>
#include <math.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

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

size_t calc_padding_w_header(uintptr_t ptr, size_t alignment, size_t header_size) {
  assert(is_power_of_2(alignment));
  assert(header_size != 0);

  if (!ptr) { return NULL; }

  uintptr_t align_ptr = (uintptr_t)alignment;
  uintptr_t modulo = (ptr & (align_ptr-1));

  size_t no_head_pad = (size_t)(modulo != 0) * (alignment - modulo);
  size_t head_pad = no_head_pad;

  if (no_head_pad < header_size) {
    head_pad += (size_t)ceil((header_size - no_head_pad) * 1.0 / alignment);
  }
  return head_pad;
}
