/*
    Copyright (C) 2024-2026 Sergey B Kirpichev

    This file is part of the ZZ Library.

    The ZZ Library is free software: you can redistribute it and/or modify it
    under the terms of the GNU Lesser General Public License (LGPL) as
    published by the Free Software Foundation; either version 3 of the License,
    or (at your option) any later version.  See
    <https://www.gnu.org/licenses/>.
*/

#ifndef IMPL_ZZ_H
#define IMPL_ZZ_H

#include "config.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <setjmp.h>

#if defined(__MINGW32__) && defined(__GNUC__)
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wconversion"
#endif
#ifdef __GNUC__
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wsign-conversion"
#endif
#include <gmp.h>
#ifdef __GNUC__
#  pragma GCC diagnostic pop
#endif
#if defined(__MINGW32__) && defined(__GNUC__)
#  pragma GCC diagnostic pop
#endif

#include "zz.h"

#define ZZ_DIGIT_T_MAX UINT64_MAX
#define ZZ_DIGIT_T_BYTES 8
#define ZZ_DIGIT_T_BITS 64
#ifndef _WIN32
#  define ZZ_BITS_MAX UINT64_MAX
#  define ZZ_DIGITS_MAX (zz_size_t)(ZZ_BITS_MAX/ZZ_DIGIT_T_BITS)
#else
   /* Set little smaller than maximal zz_size_t value to
      avoid overflows in zz_size_t on _WIN32.  See computation
      of s in zz_gcdext() */
#  define ZZ_DIGITS_MAX (INT32_MAX - 1)
#  define ZZ_BITS_MAX (zz_bitcnt_t)ZZ_DIGITS_MAX*ZZ_DIGIT_T_BITS
#endif

#ifdef __GNUC__
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wunused-variable"
#endif
static _Thread_local jmp_buf zz_env;
#if defined(__GNUC__)
#  pragma GCC diagnostic pop
#endif
#define TMP_OVERFLOW (setjmp(zz_env) == 1)

#define ISNEG(u) ((u)->negative)

#define TMP_MPZ(z, u)                                   \
    mpz_t z;                                            \
                                                        \
    assert((u)->size <= INT_MAX);                       \
    z->_mp_d = (u)->digits;                             \
    z->_mp_size = (ISNEG(u) ? -1 : 1) * (int)(u)->size; \
    z->_mp_alloc = (int)(u)->alloc;

#define SWAP(T, a, b) \
    do {              \
        T _tmp = a;   \
        a = b;        \
        b = _tmp;     \
    } while (0);

#define MAX(a, b) ((a) >= (b) ? (a) : (b))
#define MIN(a, b) ((a) < (b) ? (a) : (b))

void zz_set_memory_funcs(void *(*malloc) (size_t),
                         void *(*realloc) (void *, size_t, size_t),
                         void (*free) (void *, size_t));
size_t zz_get_alloc_state(void);
zz_err zz_inverse_euclidext(const zz_t *u, const zz_t *v, zz_t *t);
zz_err zz_set_mpz_t(mpz_t u, zz_t *v);

#endif /* IMPL_ZZ_H */
