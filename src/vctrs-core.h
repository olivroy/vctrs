#ifndef VCTRS_CORE_H
#define VCTRS_CORE_H

#include <rlang.h>
#include "globals.h"
#include "rlang-dev.h"
#include "type-info.h"

#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>


extern bool vctrs_debug_verbose;

#define VCTRS_ASSERT(condition) ((void)sizeof(char[1 - 2*!(condition)]))


// An ERR indicates either a C NULL in case of no error, or a
// condition object otherwise
#define ERR SEXP

struct vec_error_info {
  struct vctrs_arg* arg;
  struct r_lazy call;
};


// Annex F of C99 specifies that `double` should conform to the IEEE 754
// type `binary64`, which is defined as:
// * 1  bit : sign
// * 11 bits: exponent
// * 52 bits: significand
//
// R stores the value "1954" in the last 32 bits: this payload marks
// the value as a NA, not a regular NaN.
//
// On big endian systems, this corresponds to the second element of an
// integer array of size 2. On little endian systems, this is flipped
// and the NA marker is in the first element.
//
// The type assumptions made here are asserted in `vctrs_init_utils()`

#ifdef WORDS_BIGENDIAN
static const int vctrs_indicator_pos = 1;
#else
static const int vctrs_indicator_pos = 0;
#endif

union vctrs_dbl_indicator {
  double value;        // 8 bytes
  unsigned int key[2]; // 4 * 2 bytes
};

enum vctrs_dbl_class {
  vctrs_dbl_number,
  vctrs_dbl_missing,
  vctrs_dbl_nan
};

enum vctrs_dbl_class dbl_classify(double x);


#endif
