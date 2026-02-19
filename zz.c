/*
    Copyright (C) 2024-2026 Sergey B Kirpichev

    This file is part of the ZZ Library.

    The ZZ Library is free software: you can redistribute it and/or modify it
    under the terms of the GNU Lesser General Public License (LGPL) as
    published by the Free Software Foundation; either version 3 of the License,
    or (at your option) any later version.  See
    <https://www.gnu.org/licenses/>.
*/

#include <ctype.h>
#include <float.h>
#include <inttypes.h>
#include <limits.h>
#include <math.h>

#include "zz-impl.h"

#undef zz_set
#undef zz_get
#undef zz_cmp
#undef zz_add
#undef zz_sub
#undef zz_mul
#undef zz_div

#if GMP_NAIL_BITS != 0
#  error "GMP_NAIL_BITS expected to be 0"
#endif
#if GMP_LIMB_BITS != 64
#  error "GMP_LIMB_BITS expected to be 64"
#endif

#if ZZ_DIGIT_T_BITS < DBL_MANT_DIG
#  error ZZ_DIGIT_T_BITS expected to be more than ZZ_DIGIT_T_BITS
#endif

const char *
zz_get_version(void)
{
    return VERSION;
}

zz_bitcnt_t
zz_get_bitcnt_max(void)
{
    return ZZ_BITS_MAX;
}

size_t
zz_sizeof(const zz_t *u)
{
    return sizeof(zz_t) + (size_t)GETALLOC(u)*sizeof(zz_digit_t);
}

static struct {
    void *(*default_allocate_func)(size_t);
    void *(*default_reallocate_func)(void *, size_t, size_t);
    void (*default_free_func)(void *, size_t);
    void *(*malloc)(size_t);
    void *(*realloc)(void *, size_t, size_t);
    void (*free)(void *, size_t);
} zz_state;

#define TRACKER_SIZE_INCR 64
_Thread_local struct {
    size_t size;
    size_t alloc;
    void **ptrs;
} zz_tracker = {0, 0, NULL};

/* Thin wrappers over system allocation routines to
   support GMP's argument convention. */

#define zz_malloc malloc

static void *
zz_realloc(void *ptr, size_t old_size, size_t new_size)
{
    return realloc(ptr, new_size);
}

static void
zz_free(void *ptr, size_t size)
{
    free(ptr);
}

/* Following functions are used to handle all *temporary* allocations in the
   GMP, apart from temporary space from alloca() if that function is available
   and GMP is configured to use it (bad idea).

   Calls to GMP's functions, which do memory allocations, must be preceded by
   *one* "if (TMP_OVERFLOW)" block (TMP_OVERFLOW macro calls setjmp), that will
   handle memory failure *after* freeing *all* GMP's allocations in the
   zz_reallocate_function().

   Basically, GMP's functions for integers fall into two camps.  First,
   low-level functions, prefixed by "mpn_", which we prefer.  (See zz_mul() as
   an example.)  Those will use only zz_allocate_function() and
   zz_free_function() to allocate temporary storage (not for output variables).
   This allocation happens essentially in LIFO way and we take that into
   account for optimization of the memory tracking.

   Not all mpn_*() functions do memory allocation.  Sometimes it's obvious
   (e.g. mpn_cmp() or mpn_add/sub()), sometimes - not (e.g. mpn_get/set_str()
   for power of 2 bases).  Though, these details aren't documented and if you
   feel that in the given case things might be changed - please add the "if
   (TMP_OVERFLOW)" block.

   Second, in few cases we use integer functions, working with objects of type
   mpz_t, prefixed by "mpz_".  Input variables for them must be created by the
   TMP_MPZ macro.  Output variables (allocated by the GMP) are considered
   temporary and cleared automatically on memory failure.  Here is an example:

       zz_err
       mul(const zz_t *u, const zz_t *v, zz_t *w)
       {
           mpz_t z;
           TMP_MPZ(a, u);
           TMP_MPZ(b, v);
           if (TMP_OVERFLOW) {
               // Memory allocation failure happened in mpz_mul.
               // No need to call mpz_clear(z)!
               return ZZ_MEM;
           }
           mpz_init(z);
           mpz_mul(z, a, b);
           // Success!  Resize w and copy z's content to it.
           if (zz_set_mpz_t(z, w)) {
               mpz_clear(z);
               return ZZ_MEM;
           }
           mpz_clear(z); // That finally clear all temporary allocations.
           return ZZ_OK;
       }

   Note that our memory allocation functions are generic enough to work also
   for usual GMP usage, without above assumptions.  Of course, unless memory
   allocation failure happens, which will lead to undefined behavior (calling
   longjmp() without a prior call to setjmp()).

   Don't forget known pitfails of working with setjmp/longjmp:

     * Don't use VLA (may introduce memory leaks).

     * Declare variables you care about (e.g. to free memory in
       case of failure) in the scope of the setjmp invocation - with
       volatile type qualifier.  See zz_gcd() as an example. */

static void *
zz_reallocate_function(void *ptr, size_t old_size, size_t new_size)
{
    if (zz_tracker.size >= zz_tracker.alloc) {
        /* Reallocation shouldn't be required.  Unless...
           you are using the mpz_t from the GNU GMP with
           our memory functions. */
        void **tmp = zz_tracker.ptrs;
        size_t old_alloc = zz_tracker.alloc;

        zz_tracker.alloc += TRACKER_SIZE_INCR;
        zz_tracker.ptrs = zz_state.realloc(zz_tracker.ptrs,
                                           old_alloc * sizeof(void *),
                                           zz_tracker.alloc * sizeof(void *));
        if (!zz_tracker.ptrs) {
            /* LCOV_EXCL_START */
            zz_tracker.alloc = old_alloc;
            zz_tracker.ptrs = tmp;
            goto err;
            /* LCOV_EXCL_STOP */
        }
    }
    if (!ptr) {
        void *ret = zz_state.malloc(new_size);

        if (!ret) {
            goto err;
        }
        zz_tracker.ptrs[zz_tracker.size] = ret;
        zz_tracker.size++;
        return ret;
    }

    size_t i = zz_tracker.size - 1;

    for (;; i--) {
        if (zz_tracker.ptrs[i] == ptr) {
            break;
        }
    }

    void *ret = zz_state.realloc(ptr, old_size, new_size);

    if (!ret) {
        goto err; /* LCOV_EXCL_LINE */
    }
    zz_tracker.ptrs[i] = ret;
    return ret;
err:
    i = zz_tracker.size - 1;
    while (zz_tracker.size > 0) {
        zz_state.free(zz_tracker.ptrs[i], 0);
        zz_tracker.ptrs[i] = NULL;
        zz_tracker.size--;
        i--;
    }
    zz_state.free(zz_tracker.ptrs, zz_tracker.alloc * sizeof(void *));
    zz_tracker.alloc = 0;
    zz_tracker.ptrs = NULL;
    longjmp(zz_env, 1);
}

static void *
zz_allocate_function(size_t size)
{
    return zz_reallocate_function(NULL, 0, size);
}

static void
zz_free_function(void *ptr, size_t size)
{
    for (size_t i = zz_tracker.size - 1; i >= 0; i--) {
        if (zz_tracker.ptrs[i] == ptr) {
            zz_tracker.ptrs[i] = NULL;
            break;
        }
    }
    zz_state.free(ptr, size);

    size_t i = zz_tracker.size - 1;

    while (zz_tracker.size > 0) {
        if (zz_tracker.ptrs[i]) {
            return;
        }
        zz_tracker.size--;
        i--;
    }
    zz_state.free(zz_tracker.ptrs, zz_tracker.alloc * sizeof(void *));
    zz_tracker.alloc = 0;
    zz_tracker.ptrs = NULL;
}

zz_err
zz_setup(void)
{
    mp_get_memory_functions(&zz_state.default_allocate_func,
                            &zz_state.default_reallocate_func,
                            &zz_state.default_free_func);
    mp_set_memory_functions(zz_allocate_function,
                            zz_reallocate_function,
                            zz_free_function);
    zz_state.malloc = &zz_malloc;
    zz_state.realloc = &zz_realloc;
    zz_state.free = &zz_free;
    return ZZ_OK;
}

void
zz_set_memory_funcs(void *(*malloc) (size_t),
                    void *(*realloc) (void *, size_t, size_t),
                    void (*free) (void *, size_t))
{
    if (!malloc) {
        malloc = zz_malloc;
    }
    if (!realloc) {
        realloc = zz_realloc;
    }
    if (!free) {
        free = zz_free;
    }
    zz_state.malloc = malloc;
    zz_state.realloc = realloc;
    zz_state.free = free;
}

size_t
zz_get_alloc_state(void)
{
    return zz_tracker.alloc;
}

void
zz_finish(void)
{
    mp_set_memory_functions(zz_state.default_allocate_func,
                            zz_state.default_reallocate_func,
                            zz_state.default_free_func);
    zz_state.malloc = &zz_malloc;
    zz_state.realloc = &zz_realloc;
    zz_state.free = &zz_free;
}

zz_err
zz_init(zz_t *u)
{
    SETNEG(false, u);
    SETALLOC(0, u);
    u->size = 0;
    u->digits = NULL;
    return ZZ_OK;
}

static zz_err
zz_resize(zz_size_t size, zz_t *u)
{
    if (GETALLOC(u) >= size) {
        u->size = size;
        if (!u->size) {
            SETNEG(false, u);
        }
        return ZZ_OK;
    }

    zz_size_t alloc = size;
    zz_digit_t *t = u->digits;

    u->digits = realloc(u->digits, (size_t)alloc * ZZ_DIGIT_T_BYTES);
    if (u->digits) {
        SETALLOC(alloc, u);
        u->size = alloc;
        return ZZ_OK;
    }
    /* LCOV_EXCL_START */
    u->digits = t;
    return ZZ_MEM;
    /* LCOV_EXCL_STOP */
}

void
zz_clear(zz_t *u)
{
    free(u->digits);
    SETNEG(false, u);
    SETALLOC(0, u);
    u->size = 0;
    u->digits = NULL;
}

inline static void
zz_normalize(zz_t *u)
{
    while (u->size && u->digits[u->size - 1] == 0) {
        u->size--;
    }
    if (!u->size) {
        SETNEG(false, u);
    }
}

zz_ord
zz_cmp(const zz_t *u, const zz_t *v)
{
    if (u == v) {
        return ZZ_EQ;
    }

    bool u_negative = ISNEG(u);
    zz_ord sign = u_negative ? ZZ_LT : ZZ_GT;

    if (u_negative != ISNEG(v)) {
        return sign;
    }
    else if (u->size != v->size) {
        return (u->size < v->size) ? -sign : sign;
    }

    zz_ord r = mpn_cmp(u->digits, v->digits, u->size);

    return u_negative ? -r : r;
}

zz_ord
zz_cmp_i64(const zz_t *u, int64_t v)
{
    bool u_negative = ISNEG(u);
    zz_ord sign = u_negative ? ZZ_LT : ZZ_GT;
    bool v_negative = v < 0;

    if (u_negative != v_negative) {
        return sign;
    }
    else if (u->size != 1) {
        return u->size ? sign : (v ? -sign : ZZ_EQ);
    }

    zz_digit_t digit = ABS_CAST(zz_digit_t, v);
    zz_ord r = u->digits[0] != digit;

    if (u->digits[0] < digit) {
        r = ZZ_LT;
    }
    else if (u->digits[0] > digit) {
        r = ZZ_GT;
    }
    return u_negative ? -r : r;
}

zz_err
zz_set_mpz_t(mpz_t u, zz_t *v)
{
    if (zz_resize(abs(u->_mp_size), v)) {
        return ZZ_MEM; /* LCOV_EXCL_LINE */
    }
    mpn_copyi(v->digits, u->_mp_d, v->size);
    SETNEG(u->_mp_size < 0, v);
    return ZZ_OK;
}

zz_err
zz_set_i32(int32_t u, zz_t *v)
{
    if (!u) {
        v->size = 0;
        SETNEG(false, v);
        return ZZ_OK;
    }
    if (zz_resize(1, v)) {
        return ZZ_MEM; /* LCOV_EXCL_LINE */
    }
    SETNEG(u < 0, v);
    v->digits[0] = (zz_digit_t)abs(u);
    return ZZ_OK;
}

zz_err
zz_set_i64(int64_t u, zz_t *v)
{
    if (!u) {
        v->size = 0;
        SETNEG(false, v);
        return ZZ_OK;
    }
    if (zz_resize(1, v)) {
        return ZZ_MEM; /* LCOV_EXCL_LINE */
    }
    SETNEG(u < 0, v);
    v->digits[0] = ABS_CAST(zz_digit_t, u);
    return ZZ_OK;
}

zz_err
zz_set_u64(uint64_t u, zz_t *v)
{
    if (!u) {
        v->size = 0;
        SETNEG(false, v);
        return ZZ_OK;
    }
    if (zz_resize(1, v)) {
        return ZZ_MEM; /* LCOV_EXCL_LINE */
    }
    SETNEG(false, v);
    v->digits[0] = u;
    return ZZ_OK;
}

#define DIGITS_PER_DOUBLE ((53 + ZZ_DIGIT_T_BITS - 2) / ZZ_DIGIT_T_BITS + 1)

zz_err
zz_set_double(double u, zz_t *v)
{
#if defined(__MINGW32__) && defined(__GNUC__)
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wfloat-conversion"
#endif
    if (isnan(u)) {
#if defined(__MINGW32__) && defined(__GNUC__)
#  pragma GCC diagnostic pop
#endif
        return ZZ_VAL;
    }
#if defined(__MINGW32__) && defined(__GNUC__)
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wfloat-conversion"
#endif
    if (isinf(u)) {
#if defined(__MINGW32__) && defined(__GNUC__)
#  pragma GCC diagnostic pop
#endif
        return ZZ_BUF;
    }
    if (zz_resize(DIGITS_PER_DOUBLE, v)) {
        return ZZ_MEM; /* LCOV_EXCL_LINE */
    }

    /* v has enough space to prevent new allocation by GMP */
    TMP_MPZ(z, v);
    mpz_set_d(z, u);
    SETNEG(u < 0, v);
    v->size = abs(z->_mp_size);
    return ZZ_OK;
}

zz_err
zz_get_i32(const zz_t *u, int32_t *v)
{
    zz_size_t n = u->size;

    if (!n) {
        *v = 0;
        return ZZ_OK;
    }
    if (n > 1) {
        return ZZ_BUF;
    }

    zz_digit_t uv = u->digits[0];

    if (ISNEG(u)) {
        if (uv <= INT32_MAX + (zz_digit_t)1) {
            *v = -1 - (int32_t)((uv - 1) & INT32_MAX);
            return ZZ_OK;
        }
    }
    else {
        if (uv <= INT32_MAX) {
            *v = (int32_t)uv;
            return ZZ_OK;
        }
    }
    return ZZ_BUF;
}

zz_err
zz_get_i64(const zz_t *u, int64_t *v)
{
    zz_size_t n = u->size;

    if (!n) {
        *v = 0;
        return ZZ_OK;
    }
    if (n > 1) {
        return ZZ_BUF;
    }

    zz_digit_t uv = u->digits[0];

    if (ISNEG(u)) {
        if (uv <= INT64_MAX + (zz_digit_t)1) {
            *v = -1 - (int64_t)((uv - 1) & INT64_MAX);
            return ZZ_OK;
        }
    }
    else {
        if (uv <= INT64_MAX) {
            *v = (int64_t)uv;
            return ZZ_OK;
        }
    }
    return ZZ_BUF;
}

zz_err
zz_get_u32(const zz_t *u, uint32_t *v)
{
    if (ISNEG(u)) {
        return ZZ_VAL;
    }
    if (!u->size) {
        *v = 0;
        return ZZ_OK;
    }
    if (u->size > 1 || u->digits[0] > UINT32_MAX) {
        return ZZ_BUF;
    }
    *v = (uint32_t)u->digits[0];
    return ZZ_OK;
}

zz_err
zz_get_u64(const zz_t *u, uint64_t *v)
{
    if (ISNEG(u)) {
        return ZZ_VAL;
    }
    if (u->size > 1) {
        return ZZ_BUF;
    }
    *v = u->size ? u->digits[0] : 0;
    return ZZ_OK;
}

bool
zz_iszero(const zz_t *u)
{
    return u->size == 0;
}

bool
zz_isneg(const zz_t *u)
{
    return ISNEG(u);
}

bool
zz_isodd(const zz_t *u)
{
    return u->size && u->digits[0] & 1;
}

zz_err
zz_pos(const zz_t *u, zz_t *v)
{
    if (u != v) {
        if (!u->size) {
            return zz_set_i64(0, v);
        }
        if (zz_resize(u->size, v)) {
            return ZZ_MEM; /* LCOV_EXCL_LINE */
        }
        SETNEG(ISNEG(u), v);
        mpn_copyi(v->digits, u->digits, u->size);
    }
    return ZZ_OK;
}

zz_err
zz_abs(const zz_t *u, zz_t *v)
{
    if (u != v && zz_pos(u, v)) {
        return ZZ_MEM; /* LCOV_EXCL_LINE */
    }
    SETNEG(false, v);
    return ZZ_OK;
}

zz_err
zz_neg(const zz_t *u, zz_t *v)
{
    if (u != v && zz_pos(u, v)) {
        return ZZ_MEM; /* LCOV_EXCL_LINE */
    }
    if (v->size) {
        SETNEG(!ISNEG(u), v);
    }
    return ZZ_OK;
}

zz_err
zz_sizeinbase(const zz_t *u, int base, size_t *len)
{
    const int abase = abs(base);

    if (abase < 2 || abase > 36) {
        return ZZ_VAL;
    }
    *len = mpn_sizeinbase(u->digits, u->size, abase);
    return ZZ_OK;
}

zz_err
zz_get_str(const zz_t *u, int base, char *str)
{
    /* Maps 1-byte integer to digit character for bases up to 36. */
    const char *NUM_TO_TEXT = "0123456789abcdefghijklmnopqrstuvwxyz";

    if (base < 0) {
        base = -base;
        NUM_TO_TEXT = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    }
    if (base < 2 || base > 36) {
        return ZZ_VAL;
    }

    unsigned char *p = (unsigned char *)str;
    size_t len;

    if (ISNEG(u)) {
        *(p++) = '-';
    }
    /* We use undocumented feature of mpn_get_str(): u->size can be 0 */
    if ((base & (base - 1)) == 0) {
        len = mpn_get_str(p, base, u->digits, u->size);
    }
    else { /* generic base, not power of 2, input might be clobbered */
        zz_digit_t *volatile tmp = malloc(ZZ_DIGIT_T_BYTES * (size_t)u->size);

        if (!tmp || TMP_OVERFLOW) {
            /* LCOV_EXCL_START */
            free(tmp);
            return ZZ_MEM;
            /* LCOV_EXCL_STOP */
        }
        mpn_copyi(tmp, u->digits, u->size);
        len = mpn_get_str(p, base, tmp, u->size);
        free(tmp);
    }
    for (size_t i = 0; i < len; i++) {
        *p = (unsigned char)NUM_TO_TEXT[*p];
        p++;
    }
    *p = '\0';
    return ZZ_OK;
}

/* Table of digit values for 8-bit string->mpz conversion.
   Note that when converting a base B string, a char c is a legitimate
   base B digit iff DIGIT_VALUE_TAB[c] < B. */
const int DIGIT_VALUE_TAB[] =
{
  -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
  -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
  -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
  0, 1, 2, 3, 4, 5, 6, 7, 8, 9, -1,-1,-1,-1,-1,-1,
  -1,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,
  25,26,27,28,29,30,31,32,33,34,35,-1,-1,-1,-1,-1,
  -1,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,
  25,26,27,28,29,30,31,32,33,34,35,-1,-1,-1,-1,-1,
  -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
  -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
  -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
  -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
  -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
  -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
  -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
  -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
  0, 1, 2, 3, 4, 5, 6, 7, 8, 9, -1,-1,-1,-1,-1,-1,
  -1,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,
  25,26,27,28,29,30,31,32,33,34,35,-1,-1,-1,-1,-1,
  -1,36,37,38,39,40,41,42,43,44,45,46,47,48,49,50,
  51,52,53,54,55,56,57,58,59,60,61,-1,-1,-1,-1,-1,
  -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
  -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
  -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
  -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
  -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
  -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
  -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
  -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
};

zz_err
zz_set_str(const char *str, int base, zz_t *u)
{
    if (base && (base < 2 || base > 36)) {
        return ZZ_VAL;
    }

    size_t len = strlen(str);
    unsigned char *volatile buf = malloc(len), *p = (unsigned char *)buf;

    if (!buf) {
        return ZZ_MEM; /* LCOV_EXCL_LINE */
    }
    memcpy(buf, str, len);
    while (len && isspace(*p)) {
        p++;
        len--;
    }
    if (!len) {
        goto err;
    }

    bool negative = (p[0] == '-');

    p += negative;
    len -= negative;
    if (!len) {
        goto err;
    }
    if (!negative && p[0] == '+') {
        p++;
        len--;
    }
    if (!len || p[0] == '_') {
        goto err;
    }
    if (p[0] == '0' && base == 0) {
        if (len == 1) {
            free(buf);
            return zz_set_i64(0, u);
        }
        else if (tolower(p[1]) == 'b') {
            base = 2;
        }
        else if (tolower(p[1]) == 'o') {
            base = 8;
        }
        else if (tolower(p[1]) == 'x') {
            base = 16;
        }
        else if (!isspace(p[1])) {
            goto err;
        }
        p += 2;
        len -= 2;
        if (len && p[0] == '_') {
            p++;
            len--;
        }
    }
    if (p[0] == '0' && len >= 2
        && ((base == 2 && tolower(p[1]) == 'b')
            || (base == 8 && tolower(p[1]) == 'o')
            || (base == 16 && tolower(p[1]) == 'x')))
    {
        p += 2;
        len -= 2;
    }
    if (base == 0) {
        base = 10;
    }
    if (!len || (len && p[0] == '_')) {
        goto err;
    }

    size_t new_len = len;

    for (size_t i = 0; i < len; i++) {
        if (p[i] == '_') {
            if (i == len - 1 || p[i + 1] == '_') {
                goto err;
            }
            new_len--;
            memmove(p + i, p + i + 1, len - i - 1);
        }

        unsigned char c = (unsigned char)DIGIT_VALUE_TAB[p[i]];

        if (c < base) {
            p[i] = c;
        }
        else {
            if (!isspace(p[i])) {
                goto err;
            }
            new_len--;
            for (size_t j = i + 1; j < len; j++) {
                if (!isspace(p[j])) {
                    goto err;
                }
                new_len--;
            }
            break;
        }
    }
    len = new_len;
    new_len = 1 + len/2;
    if (new_len > ZZ_DIGITS_MAX) {
        return ZZ_BUF; /* LCOV_EXCL_LINE */
    }
    if (zz_resize((zz_size_t)new_len, u) || TMP_OVERFLOW) {
        /* LCOV_EXCL_START */
        free(buf);
        return ZZ_MEM;
        /* LCOV_EXCL_STOP */
    }
    SETNEG(negative, u);
    u->size = (zz_size_t)mpn_set_str(u->digits, p, len, base);
    free(buf);
    if (zz_resize(u->size, u)) {
        return ZZ_MEM; /* LCOV_EXCL_LINE */
    }
    zz_normalize(u);
    return ZZ_OK;
err:
    free(buf);
    return ZZ_VAL;
}

static bool
zz_tstbit(const zz_t *u, zz_bitcnt_t idx)
{
    zz_size_t digit_idx = (zz_size_t)(idx / ZZ_DIGIT_T_BITS);

    assert(u->size > digit_idx);
    return (u->digits[digit_idx] >> (idx%ZZ_DIGIT_T_BITS)) & 1;
}

zz_err
zz_get_double(const zz_t *u, double *d)
{
    if (u->size > DBL_MAX_EXP/ZZ_DIGIT_T_BITS + 1) {
        *d = ISNEG(u) ? -INFINITY : INFINITY;
        return ZZ_BUF;
    }

    zz_bitcnt_t bits = zz_bitlen(u);
    TMP_MPZ(z, u);
    *d = mpz_get_d(z); /* round towards zero */
    if (DBL_MANT_DIG < bits && bits <= DBL_MAX_EXP) {
        bits -= DBL_MANT_DIG + 1;
        if (zz_tstbit(u, bits)) {
            zz_bitcnt_t tz = zz_lsbpos(u);

            if (tz < bits || (tz == bits && zz_tstbit(u, bits + 1))) {
                *d = nextafter(*d, 2 * (*d)); /* round away from zero */
            }
        }
    }
#if defined(__MINGW32__) && defined(__GNUC__)
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wfloat-conversion"
#endif
    if (isinf(*d)) {
#if defined(__MINGW32__) && defined(__GNUC__)
#  pragma GCC diagnostic pop
#endif
        return ZZ_BUF;
    }
    return ZZ_OK;
}

zz_bitcnt_t
zz_bitlen(const zz_t *u)
{
    return u->size ? (zz_bitcnt_t)mpn_sizeinbase(u->digits, u->size, 2) : 0;
}

zz_bitcnt_t
zz_lsbpos(const zz_t *u)
{
#if ULONG_MAX == ZZ_DIGIT_T_MAX
    return u->size ? mpn_scan1(u->digits, 0) : 0;
#else
    zz_bitcnt_t lsb_pos = 0;

    for (zz_size_t i = 0; i < u->size; i++) {
        zz_digit_t digit = u->digits[i];

        if (digit) {
            for (;;) {
                if (digit & 1) {
                    return lsb_pos;
                }
                lsb_pos += 1;
                digit >>= 1;
            }
        }
        else {
            lsb_pos += 64;
        }
    }
    return lsb_pos;
#endif
}

zz_bitcnt_t
zz_bitcnt(const zz_t *u)
{
#if ULONG_MAX == ZZ_DIGIT_T_MAX
    return u->size ? mpn_popcount(u->digits, u->size) : 0;
#else
    zz_bitcnt_t count = 0;

    for (zz_size_t i = 0; i < u->size; i++) {
        zz_digit_t digit = u->digits[i];

        while (digit) {
            count += digit & 1;
            digit >>= 1;
        }
    }
    return count;
#endif
}

static const zz_layout native_layout = {
    .bits_per_digit = ZZ_DIGIT_T_BITS,
    .digit_size = sizeof(zz_digit_t),
    .digits_order = -1,
    .digit_endianness = 0,
};

const zz_layout *
zz_get_layout(void)
{
    return &native_layout;
}

zz_err
zz_import(size_t len, const void *digits, zz_layout layout, zz_t *u)
{
    size_t size = (len*layout.bits_per_digit
                   + (ZZ_DIGIT_T_BITS - 1))/ZZ_DIGIT_T_BITS;

    if (len > SIZE_MAX / layout.bits_per_digit || size > INT_MAX) {
        return ZZ_BUF; /* LCOV_EXCL_LINE */
    }
    if (zz_resize((zz_size_t)size, u)) {
        return ZZ_MEM; /* LCOV_EXCL_LINE */
    }
    if (layout.digit_size == 1 && layout.bits_per_digit == 8
        && layout.digits_order == 1 && !layout.digit_endianness)
    {
        u->size = (zz_size_t)mpn_set_str(u->digits, digits, len, 256);
        zz_normalize(u);
        return ZZ_OK;
    }

    TMP_MPZ(z, u);
    assert(layout.digit_size*8 >= layout.bits_per_digit);
    mpz_import(z, len, layout.digits_order, layout.digit_size,
               layout.digit_endianness,
               (size_t)(layout.digit_size*8 - layout.bits_per_digit),
               digits);
    u->size = z->_mp_size;
    return ZZ_OK;
}

zz_err
zz_export(const zz_t *u, zz_layout layout, size_t len, void *digits)
{
    if (len < (zz_bitlen(u) + layout.bits_per_digit
               - 1)/layout.bits_per_digit || u->size > INT_MAX)
    {
        return ZZ_BUF;
    }
    if (layout.digit_size == 1 && layout.bits_per_digit == 8
        && layout.digits_order == 1 && !layout.digit_endianness)
    {
        /* We use undocumented feature of mpn_get_str(): u->size can be 0 */
        mpn_get_str(digits, 256, u->digits, u->size);
        return ZZ_OK;
    }

    TMP_MPZ(z, u);
    assert(layout.digit_size*8 >= layout.bits_per_digit);
    mpz_export(digits, NULL, layout.digits_order, layout.digit_size,
               layout.digit_endianness,
               (size_t)(layout.digit_size*8 - layout.bits_per_digit),
               z);
    return ZZ_OK;
}

static zz_err
zz_addsub(const zz_t *u, const zz_t *v, bool subtract, zz_t *w)
{
    bool negu = ISNEG(u), negv = subtract ? !ISNEG(v) : ISNEG(v);
    bool same_sign = negu == negv;
    zz_size_t u_size = u->size, v_size = v->size;

    if (u_size < v_size) {
        SWAP(const zz_t *, u, v);
        SWAP(bool, negu, negv);
        SWAP(zz_size_t, u_size, v_size);
    }
    if (same_sign && u_size == ZZ_DIGITS_MAX) {
        return ZZ_BUF; /* LCOV_EXCL_LINE */
    }
    if (zz_resize(u_size + same_sign, w)) {
        return ZZ_MEM; /* LCOV_EXCL_LINE */
    }
    SETNEG(negu, w);
    /* We use undocumented feature of mpn_add/sub(): v_size can be 0 */
    if (same_sign) {
        w->digits[w->size - 1] = mpn_add(w->digits, u->digits, u_size,
                                         v->digits, v_size);
    }
    else if (u_size != v_size) {
        mpn_sub(w->digits, u->digits, u_size, v->digits, v_size);
    }
    else {
        int cmp = mpn_cmp(u->digits, v->digits, u_size);

        if (cmp < 0) {
            mpn_sub_n(w->digits, v->digits, u->digits, u_size);
            SETNEG(negv, w);
        }
        else if (cmp > 0) {
            mpn_sub_n(w->digits, u->digits, v->digits, u_size);
        }
        else {
            w->size = 0;
        }
    }
    zz_normalize(w);
    return ZZ_OK;
}

static zz_err
zz_addsub_u64(const zz_t *u, uint64_t v, bool subtract, zz_t *w)
{
    bool negu = ISNEG(u), negv = subtract;
    bool same_sign = negu == negv;
    zz_size_t u_size = u->size, v_size = v != 0;

    if (!u_size || u_size < v_size) {
        assert(!u_size);
        if (zz_resize(v_size, w)) {
            return ZZ_MEM; /* LCOV_EXCL_LINE */
        }
        if (v_size) {
            w->digits[0] = v;
        }
        SETNEG(w->size ? negv : false, w);
        return ZZ_OK;
    }

    if (same_sign && u_size == ZZ_DIGITS_MAX) {
        return ZZ_BUF; /* LCOV_EXCL_LINE */
    }
    if (zz_resize(u_size + same_sign, w)) {
        return ZZ_MEM; /* LCOV_EXCL_LINE */
    }
    SETNEG(negu, w);
    if (same_sign) {
        w->digits[w->size - 1] = mpn_add_1(w->digits, u->digits, u_size, v);
    }
    else if (u_size != 1) {
        mpn_sub_1(w->digits, u->digits, u_size, v);
    }
    else {
        if (u->digits[0] < v) {
            w->digits[0] = v - u->digits[0];
            SETNEG(negv, w);
        }
        else {
            w->digits[0] = u->digits[0] - v;
        }
    }
    zz_normalize(w);
    return ZZ_OK;
}

zz_err
zz_addsub_i64(const zz_t *u, int64_t v, bool subtract, zz_t *w)
{
    uint64_t uv = ABS_CAST(uint64_t, v);

    return zz_addsub_u64(u, uv, subtract ? v >= 0 : v < 0, w);
}

zz_err
zz_add(const zz_t *u, const zz_t *v, zz_t *w)
{
    return zz_addsub(u, v, false, w);
}

zz_err
zz_sub(const zz_t *u, const zz_t *v, zz_t *w)
{
    return zz_addsub(u, v, true, w);
}

zz_err
zz_add_u64(const zz_t *u, uint64_t v, zz_t *w)
{
    return zz_addsub_u64(u, v, false, w);
}

zz_err
zz_sub_u64(const zz_t *u, uint64_t v, zz_t *w)
{
    return zz_addsub_u64(u, v, true, w);
}

zz_err
zz_u64_sub(uint64_t u, const zz_t *v, zz_t *w)
{
    if (zz_neg(v, w)) {
        return ZZ_MEM; /* LCOV_EXCL_LINE */
    }
    return zz_addsub_u64(w, u, false, w);
}

zz_err
zz_add_i64(const zz_t *u, int64_t v, zz_t *w)
{
    return zz_addsub_i64(u, v, false, w);
}

zz_err
zz_sub_i64(const zz_t *u, int64_t v, zz_t *w)
{
    return zz_addsub_i64(u, v, true, w);
}

zz_err
zz_i64_sub(int64_t u, const zz_t *v, zz_t *w)
{
    if (zz_neg(v, w)) {
        return ZZ_MEM; /* LCOV_EXCL_LINE */
    }
    return zz_addsub_i64(w, u, false, w);
}

zz_err
zz_mul(const zz_t *u, const zz_t *v, zz_t *w)
{
    if (u->size < v->size) {
        SWAP(const zz_t *, u, v);
    }
    if (v->size <= 1) {
        bool w_negative = ISNEG(u) != ISNEG(v); /* in case v == w */
        zz_err ret = zz_mul_u64(u, v->size ? v->digits[0] : 0, w);

        if (w->size) {
            SETNEG(w_negative, w);
        }
        return ret;
    }
    if (u == w) {
        zz_t tmp;

        if (zz_init(&tmp) || zz_pos(u, &tmp)) {
            /* LCOV_EXCL_START */
            zz_clear(&tmp);
            return ZZ_MEM;
            /* LCOV_EXCL_STOP */
        }

        zz_err ret = u == v ? zz_mul(&tmp, &tmp, w) : zz_mul(&tmp, v, w);

        zz_clear(&tmp);
        return ret;
    }
    if (v == w) {
        zz_t tmp;

        if (zz_init(&tmp) || zz_pos(v, &tmp)) {
            /* LCOV_EXCL_START */
            zz_clear(&tmp);
            return ZZ_MEM;
            /* LCOV_EXCL_STOP */
        }

        zz_err ret = zz_mul(u, &tmp, w);

        zz_clear(&tmp);
        return ret;
    }

    uint64_t w_size = (uint64_t)u->size + (uint64_t)v->size;

    if (w_size > ZZ_DIGITS_MAX) {
        return ZZ_BUF; /* LCOV_EXCL_LINE */
    }
    if (zz_resize((zz_size_t)w_size, w) || TMP_OVERFLOW) {
        return ZZ_MEM;
    }
    SETNEG(ISNEG(u) != ISNEG(v), w);
    if (u->size == v->size) {
        if (u != v) {
            mpn_mul_n(w->digits, u->digits, v->digits, u->size);
        }
        else {
            mpn_sqr(w->digits, u->digits, u->size);
        }
    }
    else {
        mpn_mul(w->digits, u->digits, u->size, v->digits, v->size);
    }
    w->size -= w->digits[w->size - 1] == 0;
    assert(w->size >= 1);
    return ZZ_OK;
}

zz_err
zz_mul_u64(const zz_t *u, uint64_t v, zz_t *w)
{
    if (!u->size || !v) {
        return zz_set_i64(0, w);
    }

    zz_size_t u_size = u->size;

    if (u_size == ZZ_DIGITS_MAX) {
        return ZZ_BUF; /* LCOV_EXCL_LINE */
    }
    if (zz_resize(u_size + 1, w) || TMP_OVERFLOW) {
        return ZZ_MEM; /* LCOV_EXCL_LINE */
    }
    SETNEG(ISNEG(u), w);
    w->digits[w->size - 1] = mpn_mul_1(w->digits, u->digits, u_size, v);
    w->size -= w->digits[w->size - 1] == 0;
    assert(w->size >= 1);
    return ZZ_OK;
}

zz_err
zz_mul_i64(const zz_t *u, int64_t v, zz_t *w)
{
    zz_err ret = zz_mul_u64(u, ABS_CAST(zz_digit_t, v), w);

    if (w->size) {
        SETNEG(ISNEG(u) != (v < 0), w);
    }
    return ret;
}

zz_err
zz_div(const zz_t *u, const zz_t *v, zz_t *q, zz_t *r)
{
    if (!v->size) {
        return ZZ_VAL;
    }
    if (!q || !r) {
        if (!q && !r) {
            return ZZ_OK;
        }
        if (!q) {
            zz_t tmp;

            if (zz_init(&tmp)) {
                return ZZ_MEM; /* LCOV_EXCL_LINE */
            }

            zz_err ret = zz_div(u, v, &tmp, r);

            zz_clear(&tmp);
            return ret;
        }
        else {
            zz_t tmp;

            if (zz_init(&tmp)) {
                return ZZ_MEM; /* LCOV_EXCL_LINE */
            }

            zz_err ret = zz_div(u, v, q, &tmp);

            zz_clear(&tmp);
            return ret;
        }
    }
    if (!u->size) {
        if (zz_set_i64(0, q) || zz_set_i64(0, r)) {
            goto err; /* LCOV_EXCL_LINE */
        }
    }
    else if (u->size < v->size) {
        if (ISNEG(u) != ISNEG(v)) {
            if (zz_set_i64(-1, q) || zz_add(u, v, r)) {
                goto err; /* LCOV_EXCL_LINE */
            }
        }
        else {
            if (zz_set_i64(0, q) || zz_pos(u, r)) {
                goto err; /* LCOV_EXCL_LINE */
            }
        }
    }
    else {
        if (u == q || u == r) {
            zz_t tmp;

            if (zz_init(&tmp) || zz_pos(u, &tmp)) {
                /* LCOV_EXCL_START */
                zz_clear(&tmp);
                return ZZ_MEM;
                /* LCOV_EXCL_STOP */
            }

            zz_err ret = zz_div(&tmp, v, q, r);

            zz_clear(&tmp);
            return ret;
        }
        if (v == q || v == r) {
            zz_t tmp;

            if (zz_init(&tmp) || zz_pos(v, &tmp)) {
                /* LCOV_EXCL_START */
                zz_clear(&tmp);
                return ZZ_MEM;
                /* LCOV_EXCL_STOP */
            }

            zz_err ret = zz_div(u, &tmp, q, r);

            zz_clear(&tmp);
            return ret;
        }

        zz_size_t u_size = u->size;

        if (zz_resize(u_size - v->size + 1, q)
            || zz_resize(v->size, r) || TMP_OVERFLOW)
        {
            goto err; /* LCOV_EXCL_LINE */
        }
        SETNEG(ISNEG(u) != ISNEG(v), q);
        SETNEG(ISNEG(v), r);
        mpn_tdiv_qr(q->digits, r->digits, 0, u->digits, u_size, v->digits,
                    v->size);
        q->size -= q->digits[q->size - 1] == 0;
        zz_normalize(r);
        if (ISNEG(q) && r->size) {
            /* Note that we can't get carry here for q of maximal size,
               as this would mean |v| == 1, thus: r == 0 */
            assert(q->size < ZZ_DIGITS_MAX);
            if (zz_sub_i64(q, 1, q)) {
                goto err; /* LCOV_EXCL_LINE */
            }
            r->size = v->size;
            mpn_sub_n(r->digits, v->digits, r->digits, v->size);
            zz_normalize(r);
        }
    }
    return ZZ_OK;
    /* LCOV_EXCL_START */
err:
    zz_clear(q);
    zz_clear(r);
    return ZZ_MEM;
    /* LCOV_EXCL_STOP */
}

zz_err
zz_div_i64(const zz_t *u, int64_t v, zz_t *q, zz_t *r)
{
    if (!v) {
        return ZZ_VAL;
    }

    zz_digit_t rl, uv = ABS_CAST(zz_digit_t, v);
    bool same_signs = ISNEG(u) == (v < 0);

    if (q) {
        if (u->size) {
            if (zz_resize(u->size, q)) {
                return ZZ_MEM; /* LCOV_EXCL_LINE */
            }
            rl = mpn_divrem_1(q->digits, 0, u->digits, u->size, uv);
            if (rl && !same_signs) {
                mpn_add_1(q->digits, q->digits, q->size, 1);
            }
            q->size -= q->digits[q->size - 1] == 0;
            SETNEG(q->size ? !same_signs : false, q);
        }
        else {
            (void)zz_set_i32(0, q);
        }
    }
    if (r) {
        if (!u->size) {
            return zz_set_i32(0, r);
        }

        zz_size_t u_size = u->size;

        if (zz_resize(1, r)) {
            return ZZ_MEM; /* LCOV_EXCL_LINE */
        }
        rl = mpn_mod_1(u->digits, u_size, uv);
        if (!rl) {
            (void)zz_set_i32(0, r);
        }
        else {
            if (!same_signs) {
                rl = uv - rl;
            }
            r->digits[0] = rl;
            SETNEG(v < 0, r);
        }
        return ZZ_OK;
    }
    return ZZ_OK;
}

static int64_t
fdiv_r(int64_t a, int64_t b)
{
    return a/b - (a%b != 0 && (a^b) < 0);
}

zz_err
zz_i64_div(int64_t u, const zz_t *v, zz_t *q, zz_t *r)
{
    if (!v->size) {
        return ZZ_VAL;
    }

    int64_t sv;

    if (q) {
        zz_err ret = ZZ_OK;

        if (zz_get_i64(v, &sv)) {
            ret = zz_set_i64((u < 0) == ISNEG(v) || !u ? 0 : -1, q);
        }
        else {
            ret = zz_set_i64(fdiv_r(u, sv), q);
        }
        if (ret || !r) {
            return ret; /* LCOV_EXCL_LINE */
        }
    }
    if (r) {
        if (zz_get_i64(v, &sv)) {
            if ((u < 0) == ISNEG(v) || !u) {
                return zz_set_i64(u, r);
            }
            return zz_add_i64(v, u, r);
        }
        return zz_set_i64(u - fdiv_r(u, sv)*sv, r);
    }
    return ZZ_OK;
}

zz_err
zz_quo_2exp(const zz_t *u, zz_bitcnt_t shift, zz_t *v)
{
    if (!u->size) {
        return zz_set_i32(0, v);
    }
    if (shift >= (zz_bitcnt_t)u->size*ZZ_DIGIT_T_BITS) {
        return zz_set_i64(ISNEG(u) ? -1 : 0, v);
    }

    zz_size_t whole = (zz_size_t)(shift / ZZ_DIGIT_T_BITS);
    zz_size_t size = u->size - whole;
    bool carry = false, extra = true;

    shift %= ZZ_DIGIT_T_BITS;
    for (mp_size_t i = 0; i < whole; i++) {
        if (u->digits[i]) {
            carry = ISNEG(u);
            break;
        }
    }
    for (mp_size_t i = whole; i < u->size; i++) {
        if (u->digits[i] != ZZ_DIGIT_T_MAX) {
            extra = 0;
            break;
        }
    }
    if (zz_resize(size + extra, v)) {
        return ZZ_MEM; /* LCOV_EXCL_LINE */
    }
    SETNEG(ISNEG(u), v);
    if (shift) {
        if (mpn_rshift(v->digits, u->digits + whole, size,
                       (unsigned int)shift))
        {
            carry = ISNEG(u);
        }
    }
    else {
        mpn_copyi(v->digits, u->digits + whole, size);
    }
    if (extra) {
        v->digits[size] = 0;
    }
    if (carry) {
        if (mpn_add_1(v->digits, v->digits, size, 1)) {
            v->digits[size] = 1;
        }
    }
    zz_normalize(v);
    return ZZ_OK;
}

zz_err
zz_mul_2exp(const zz_t *u, zz_bitcnt_t shift, zz_t *v)
{
    if (!u->size) {
        return zz_set_i32(0, v);
    }
    if (shift > ZZ_BITS_MAX) {
        return ZZ_BUF;
    }

    zz_size_t whole = (zz_size_t)(shift / ZZ_DIGIT_T_BITS);
    zz_size_t u_size = u->size;
    int64_t v_size = (int64_t)u_size + whole;

    shift %= ZZ_DIGIT_T_BITS;
    v_size += (bool)shift;
    if (v_size > ZZ_DIGITS_MAX) {
        return ZZ_BUF;
    }
    if (zz_resize((zz_size_t)v_size, v)) {
        return ZZ_MEM; /* LCOV_EXCL_LINE */
    }
    SETNEG(ISNEG(u), v);
    if (shift) {
        v->size -= !(bool)(v->digits[v->size - 1]
                           = mpn_lshift(v->digits + whole,
                                        u->digits, u_size,
                                        (unsigned int)shift));
    }
    else {
        mpn_copyd(v->digits + whole, u->digits, u_size);
    }
    mpn_zero(v->digits, whole);
    return ZZ_OK;
}

zz_err
zz_invert(const zz_t *u, zz_t *v)
{
    zz_size_t u_size = u->size;

    if (ISNEG(u)) {
        if (zz_resize(u_size, v)) {
            return ZZ_MEM; /* LCOV_EXCL_LINE */
        }
        mpn_sub_1(v->digits, u->digits, u_size, 1);
        v->size -= v->digits[u_size - 1] == 0;
    }
    else if (!u_size) {
        return zz_set_i64(-1, v);
    }
    else {
        if (u_size == ZZ_DIGITS_MAX) {
            return ZZ_BUF; /* LCOV_EXCL_LINE */
        }
        if (zz_resize(u_size + 1, v)) {
            return ZZ_MEM; /* LCOV_EXCL_LINE */
        }
        v->digits[u_size] = mpn_add_1(v->digits, u->digits, u_size, 1);
        v->size -= v->digits[u_size] == 0;
    }
    SETNEG(!ISNEG(u), v);
    return ZZ_OK;
}

zz_err
zz_and(const zz_t *u, const zz_t *v, zz_t *w)
{
    if (!u->size || !v->size) {
        return zz_set_i64(0, w);
    }

    zz_size_t u_size = u->size, v_size = v->size;

    if (ISNEG(u) || ISNEG(v)) {
        zz_t o1, o2;
        zz_err ret = ZZ_MEM;

        if (zz_init(&o1) || zz_init(&o2)) {
            /* LCOV_EXCL_START */
err:
            zz_clear(&o1);
            zz_clear(&o2);
            return ret;
            /* LCOV_EXCL_STOP */
        }
        if (ISNEG(u)) {
            ret = zz_invert(u, &o1);
            if (ret) {
                goto err; /* LCOV_EXCL_LINE */
            }
            SETNEG(true, &o1);
            u = &o1;
            u_size = u->size;
        }
        if (ISNEG(v)) {
            ret = zz_invert(v, &o2);
            if (ret) {
                goto err; /* LCOV_EXCL_LINE */
            }
            SETNEG(true, &o2);
            v = &o2;
            v_size = v->size;
        }
        if (u_size < v_size) {
            SWAP(const zz_t *, u, v);
            SWAP(zz_size_t, u_size, v_size);
        }
        if (ISNEG(u) && ISNEG(v)) {
            if (!u_size) {
                zz_clear(&o1);
                zz_clear(&o2);
                return zz_set_i64(-1, w);
            }
            if (u_size == ZZ_DIGITS_MAX) {
                return ZZ_BUF; /* LCOV_EXCL_LINE */
            }
            if (zz_resize(u_size + 1, w)) {
                goto err; /* LCOV_EXCL_LINE */
            }
            SETNEG(true, w);
            mpn_copyi(&w->digits[v_size], &u->digits[v_size], u_size - v_size);
            if (v_size) {
                mpn_ior_n(w->digits, u->digits, v->digits, v_size);
            }
            w->digits[u_size] = mpn_add_1(w->digits, w->digits, u_size, 1);
            zz_normalize(w);
            zz_clear(&o1);
            zz_clear(&o2);
            return ZZ_OK;
        }
        else if (ISNEG(u)) {
            assert(v_size > 0);
            if (zz_resize(v_size, w)) {
                goto err; /* LCOV_EXCL_LINE */
            }
            SETNEG(false, w);
            mpn_andn_n(w->digits, v->digits, u->digits, v_size);
            zz_normalize(w);
            zz_clear(&o1);
            zz_clear(&o2);
            return ZZ_OK;
        }
        else {
            assert(u_size > 0);
            if (zz_resize(u_size, w)) {
                goto err; /* LCOV_EXCL_LINE */
            }
            SETNEG(false, w);
            if (v_size) {
                mpn_andn_n(w->digits, u->digits, v->digits, v_size);
            }
            mpn_copyi(&w->digits[v_size], &u->digits[v_size], u_size - v_size);
            zz_normalize(w);
            zz_clear(&o1);
            zz_clear(&o2);
            return ZZ_OK;
        }
    }
    if (u_size < v_size) {
        SWAP(const zz_t *, u, v);
        SWAP(zz_size_t, u_size, v_size);
    }
    SETNEG(false, w);
    for (zz_size_t i = v_size; --i >= 0;) {
        if (u->digits[i] & v->digits[i]) {
            v_size = i + 1;
            if (zz_resize(v_size, w)) {
                return ZZ_MEM; /* LCOV_EXCL_LINE */
            }
            mpn_and_n(w->digits, u->digits, v->digits, v_size);
            zz_normalize(w);
            return ZZ_OK;
        }
    }
    w->size = 0;
    return ZZ_OK;
}

zz_err
zz_or(const zz_t *u, const zz_t *v, zz_t *w)
{
    if (!u->size) {
        return zz_pos(v, w);
    }
    if (!v->size) {
        return zz_pos(u, w);
    }

    zz_size_t u_size = u->size, v_size = v->size;

    if (ISNEG(u) || ISNEG(v)) {
        zz_t o1, o2;
        zz_err ret = ZZ_MEM;

        if (zz_init(&o1) || zz_init(&o2)) {
            /* LCOV_EXCL_START */
err:
            zz_clear(&o1);
            zz_clear(&o2);
            return ret;
            /* LCOV_EXCL_STOP */
        }
        if (ISNEG(u)) {
            ret = zz_invert(u, &o1);
            if (ret) {
                goto err; /* LCOV_EXCL_LINE */
            }
            SETNEG(true, &o1);
            u = &o1;
            u_size = u->size;
        }
        if (ISNEG(v)) {
            ret = zz_invert(v, &o2);
            if (ret) {
                goto err; /* LCOV_EXCL_LINE */
            }
            SETNEG(true, &o2);
            v = &o2;
            v_size = v->size;
        }
        if (u_size < v_size) {
            SWAP(const zz_t *, u, v);
            SWAP(zz_size_t, u_size, v_size);
        }
        if (ISNEG(u) && ISNEG(v)) {
            if (!v_size) {
                zz_clear(&o1);
                zz_clear(&o2);
                return zz_set_i64(-1, w);
            }
            if (v_size == ZZ_DIGITS_MAX) {
                return ZZ_BUF; /* LCOV_EXCL_LINE */
            }
            if (zz_resize(v_size + 1, w)) {
                goto err; /* LCOV_EXCL_LINE */
            }
            SETNEG(true, w);
            mpn_and_n(w->digits, u->digits, v->digits, v_size);
            w->digits[v_size] = mpn_add_1(w->digits, w->digits, v_size, 1);
            zz_normalize(w);
            zz_clear(&o1);
            zz_clear(&o2);
            return ZZ_OK;
        }
        else if (ISNEG(u)) {
            assert(v_size > 0);
            if (u_size == ZZ_DIGITS_MAX) {
                return ZZ_BUF; /* LCOV_EXCL_LINE */
            }
            if (zz_resize(u_size + 1, w)) {
                goto err; /* LCOV_EXCL_LINE */
            }
            SETNEG(true, w);
            mpn_copyi(&w->digits[v_size], &u->digits[v_size], u_size - v_size);
            mpn_andn_n(w->digits, u->digits, v->digits, v_size);
            w->digits[u_size] = mpn_add_1(w->digits, w->digits, u_size, 1);
            zz_normalize(w);
            zz_clear(&o1);
            zz_clear(&o2);
            return ZZ_OK;
        }
        else {
            assert(u_size > 0);
            if (v_size == ZZ_DIGITS_MAX) {
                return ZZ_BUF; /* LCOV_EXCL_LINE */
            }
            if (zz_resize(v_size + 1, w)) {
                goto err; /* LCOV_EXCL_LINE */
            }
            SETNEG(true, w);
            if (v_size) {
                mpn_andn_n(w->digits, v->digits, u->digits, v_size);
                w->digits[v_size] = mpn_add_1(w->digits, w->digits, v_size, 1);
                zz_normalize(w);
            }
            else {
                w->digits[0] = 1;
            }
            zz_clear(&o1);
            zz_clear(&o2);
            return ZZ_OK;
        }
    }
    if (u_size < v_size) {
        SWAP(const zz_t *, u, v);
        SWAP(zz_size_t, u_size, v_size);
    }
    if (zz_resize(u_size, w)) {
        return ZZ_MEM; /* LCOV_EXCL_LINE */
    }
    SETNEG(false, w);
    mpn_ior_n(w->digits, u->digits, v->digits, v_size);
    if (u_size != v_size) {
        mpn_copyi(&w->digits[v_size], &u->digits[v_size], u_size - v_size);
    }
    return ZZ_OK;
}

zz_err
zz_xor(const zz_t *u, const zz_t *v, zz_t *w)
{
    if (!u->size) {
        return zz_pos(v, w);
    }
    if (!v->size) {
        return zz_pos(u, w);
    }

    zz_size_t u_size = u->size, v_size = v->size;

    if (ISNEG(u) || ISNEG(v)) {
        zz_t o1, o2;
        zz_err ret = ZZ_MEM;

        if (zz_init(&o1) || zz_init(&o2)) {
            /* LCOV_EXCL_START */
err:
            zz_clear(&o1);
            zz_clear(&o2);
            return ret;
            /* LCOV_EXCL_STOP */
        }
        if (ISNEG(u)) {
            ret = zz_invert(u, &o1);
            if (ret) {
                goto err; /* LCOV_EXCL_LINE */
            }
            SETNEG(true, &o1);
            u = &o1;
            u_size = u->size;
        }
        if (ISNEG(v)) {
            ret = zz_invert(v, &o2);
            if (ret) {
                goto err; /* LCOV_EXCL_LINE */
            }
            SETNEG(true, &o2);
            v = &o2;
            v_size = v->size;
        }
        if (u_size < v_size) {
            SWAP(const zz_t *, u, v);
            SWAP(zz_size_t, u_size, v_size);
        }
        if (ISNEG(u) && ISNEG(v)) {
            if (!u_size) {
                zz_clear(&o1);
                zz_clear(&o2);
                return zz_set_i64(0, w);
            }
            if (zz_resize(u_size, w)) {
                goto err; /* LCOV_EXCL_LINE */
            }
            SETNEG(false, w);
            mpn_copyi(&w->digits[v_size], &u->digits[v_size], u_size - v_size);
            if (v_size) {
                mpn_xor_n(w->digits, u->digits, v->digits, v_size);
            }
            zz_normalize(w);
            zz_clear(&o1);
            zz_clear(&o2);
            return ZZ_OK;
        }
        else if (ISNEG(u)) {
            assert(v_size > 0);
            if (u_size == ZZ_DIGITS_MAX) {
                return ZZ_BUF; /* LCOV_EXCL_LINE */
            }
            if (zz_resize(u_size + 1, w)) {
                goto err; /* LCOV_EXCL_LINE */
            }
            SETNEG(true, w);
            mpn_copyi(&w->digits[v_size], &u->digits[v_size], u_size - v_size);
            mpn_xor_n(w->digits, v->digits, u->digits, v_size);
            w->digits[u_size] = mpn_add_1(w->digits, w->digits, u_size, 1);
            zz_normalize(w);
            zz_clear(&o1);
            zz_clear(&o2);
            return ZZ_OK;
        }
        else {
            assert(u_size > 0);
            if (u_size == ZZ_DIGITS_MAX) {
                return ZZ_BUF; /* LCOV_EXCL_LINE */
            }
            if (zz_resize(u_size + 1, w)) {
                goto err; /* LCOV_EXCL_LINE */
            }
            SETNEG(true, w);
            mpn_copyi(&w->digits[v_size], &u->digits[v_size], u_size - v_size);
            if (v_size) {
                mpn_xor_n(w->digits, u->digits, v->digits, v_size);
            }
            w->digits[u_size] = mpn_add_1(w->digits, w->digits, u_size, 1);
            zz_normalize(w);
            zz_clear(&o1);
            zz_clear(&o2);
            return ZZ_OK;
        }
    }
    if (u_size < v_size) {
        SWAP(const zz_t *, u, v);
        SWAP(zz_size_t, u_size, v_size);
    }
    if (zz_resize(u_size, w)) {
        return ZZ_MEM; /* LCOV_EXCL_LINE */
    }
    SETNEG(false, w);
    mpn_xor_n(w->digits, u->digits, v->digits, v_size);
    if (u_size != v_size) {
        mpn_copyi(&w->digits[v_size], &u->digits[v_size], u_size - v_size);
    }
    else {
        zz_normalize(w);
    }
    return ZZ_OK;
}

zz_err
zz_pow(const zz_t *u, uint64_t v, zz_t *w)
{
    if (u == w) {
        zz_t tmp;

        if (zz_init(&tmp) || zz_pos(u, &tmp)) {
            /* LCOV_EXCL_START */
            zz_clear(&tmp);
            return ZZ_MEM;
            /* LCOV_EXCL_STOP */
        }

        zz_err ret = zz_pow(&tmp, v, w);

        zz_clear(&tmp);
        return ret;
    }
    if (!v) {
        return zz_set_i64(1, w);
    }
    if (!u->size) {
        return zz_set_i64(0, w);
    }
    if (zz_cmp_i64(u, 1) == ZZ_EQ) {
        return zz_set_i64(1, w);
    }
    if (v > ZZ_DIGITS_MAX / u->size) {
        return ZZ_BUF;
    }

    zz_size_t w_size = (zz_size_t)(v * (zz_digit_t)u->size);
    zz_digit_t *tmp = malloc((size_t)w_size * ZZ_DIGIT_T_BYTES);

    if (!tmp || zz_resize(w_size, w)) {
        /* LCOV_EXCL_START */
        free(tmp);
        zz_clear(w);
        return ZZ_MEM;
        /* LCOV_EXCL_STOP */
    }
    SETNEG(ISNEG(u) && v%2, w);
    w->size = (zz_size_t)mpn_pow_1(w->digits, u->digits, u->size, v, tmp);
    free(tmp);
    if (zz_resize(w->size, w)) {
        /* LCOV_EXCL_START */
        zz_clear(w);
        return ZZ_MEM;
        /* LCOV_EXCL_STOP */
    }
    return ZZ_OK;
}

static zz_err
zz_gcd(const zz_t *u, const zz_t *v, zz_t *w)
{
    if (u->size < v->size) {
        SWAP(const zz_t *, u, v);
    }
    if (!v->size) {
        return zz_abs(u, w);
    }

    zz_bitcnt_t shift = MIN(zz_lsbpos(u), zz_lsbpos(v));
    zz_t *volatile o1 = malloc(sizeof(zz_t));
    zz_t *volatile o2 = malloc(sizeof(zz_t));

    if (!o1 || !o2) {
        goto free; /* LCOV_EXCL_LINE */
    }
    if (zz_init(o1) || zz_init(o2) || zz_abs(u, o1) || zz_abs(v, o2)) {
        goto clear; /* LCOV_EXCL_LINE */
    }
    if (shift && (zz_quo_2exp(o1, shift, o1) || zz_quo_2exp(o2, shift, o2))) {
        goto clear; /* LCOV_EXCL_LINE */
    }
    u = o1;
    v = o2;
    assert(v->size);
    if (zz_resize(v->size, w) || TMP_OVERFLOW) {
        goto clear; /* LCOV_EXCL_LINE */
    }
    if (v->size == 1) {
        w->digits[0] = mpn_gcd_1(u->digits, u->size, v->digits[0]);
    }
    else {
        w->size = (zz_size_t)mpn_gcd(w->digits, u->digits, u->size, v->digits,
                                     v->size);
    }
    SETNEG(false, w);
    zz_clear(o1);
    zz_clear(o2);
    free(o1);
    free(o2);
    return zz_mul_2exp(w, shift, w);
    /* LCOV_EXCL_START */
clear:
    zz_clear(o1);
    zz_clear(o2);
free:
    free(o1);
    free(o2);
    return ZZ_MEM;
    /* LCOV_EXCL_STOP */
}

zz_err
zz_inverse_euclidext(const zz_t *u, const zz_t *v, zz_t *t)
{
    zz_t r, newt, newr, q, t1, t2;
    bool u_neg = ISNEG(u);

    if (zz_init(&r) || zz_init(&newt) || zz_init(&newr)
        || zz_init(&q) || zz_init(&t1) || zz_init(&t2)
        || zz_pos(v, &r) || zz_set_i32(1, &newt)
        || zz_pos(u, &newr) || zz_set_i32(0, t))
    {
        /* LCOV_EXCL_START */
clear2:
        zz_clear(&r);
        zz_clear(&newt);
        zz_clear(&newr);
        zz_clear(&q);
        zz_clear(&t1);
        zz_clear(&t2);
        return ZZ_MEM;
        /* LCOV_EXCL_STOP */
    }
    while (zz_cmp_i64(&newr, 0) != ZZ_EQ) {
        if (zz_div(&r, &newr, &q, NULL)) {
            goto clear2; /* LCOV_EXCL_LINE */
        }
        if (zz_pos(&newt, &t1) || zz_mul(&q, &newt, &t2)
            || zz_sub(t, &t2, &newt) || zz_pos(&t1, t))
        {
            goto clear2; /* LCOV_EXCL_LINE */
        }
        if (zz_pos(&newr, &t1) || zz_mul(&q, &newr, &t2)
            || zz_sub(&r, &t2, &newr) || zz_pos(&t1, &r))
        {
            goto clear2; /* LCOV_EXCL_LINE */
        }
    }
    zz_clear(&newt);
    zz_clear(&newr);
    zz_clear(&q);
    zz_clear(&t1);
    zz_clear(&t2);
    if (t->size) {
        SETNEG(ISNEG(t) != u_neg, t);
    }
    (void)zz_abs(&r, &r);
    if (zz_cmp_i64(&r, 1) != ZZ_EQ) {
        zz_clear(&r);
        return ZZ_VAL;
    }
    zz_clear(&r);
    return ZZ_OK;
}

zz_err
zz_gcdext(const zz_t *u, const zz_t *v, zz_t *g, zz_t *s, zz_t *t)
{
    if (!s && !t) {
        if (!g) {
            return ZZ_OK;
        }
        return zz_gcd(u, v, g);
    }
    if (u->size < v->size) {
        SWAP(const zz_t *, u, v);
        SWAP(zz_t *, s, t);
    }
    if (!v->size) {
        if (g) {
            if (zz_abs(u, g)) {
                return ZZ_MEM; /* LCOV_EXCL_LINE */
            }
        }
        if (s) {
            if (zz_set_i64(ISNEG(u) ? -1 : 1, s)) {
                return ZZ_MEM; /* LCOV_EXCL_LINE */
            }
            s->size = u->size > 0; /* make 0 if u == 0 */
        }
        if (t) {
            t->size = 0;
            SETNEG(false, t);
        }
        return ZZ_OK;
    }

    zz_t *volatile o1 = malloc(sizeof(zz_t));
    zz_t *volatile o2 = malloc(sizeof(zz_t));
    zz_t *volatile tmp_g = malloc(sizeof(zz_t));
    zz_t *volatile tmp_s = malloc(sizeof(zz_t));
    /* v->size + 1 can be > ZZ_DIGITS_MAX, but it should
       not overflow zz_size_t type */
    zz_digit_t *volatile tmp_s_digits = malloc(ZZ_DIGIT_T_BYTES
                                               * (size_t)(v->size + 1));

    if (!o1 || !o2 || !tmp_g || !tmp_s || !tmp_s_digits) {
        goto free; /* LCOV_EXCL_LINE */
    }
    if (zz_init(o1) || zz_init(o2)
        || zz_init(tmp_g) || zz_init(tmp_s)
        || zz_pos(u, o1) || zz_pos(v, o2)
        || zz_resize(v->size, tmp_g)
        || TMP_OVERFLOW)
    {
        goto clear; /* LCOV_EXCL_LINE */
    }

    mp_size_t ssize;

    tmp_g->size = (zz_size_t)mpn_gcdext(tmp_g->digits, tmp_s_digits, &ssize,
                                        o1->digits, u->size, o2->digits,
                                        v->size);
    SETNEG(false, tmp_g);
    /* Now s either 1 or |s| < v/(2g) */
    if (zz_resize(labs(ssize), tmp_s)) {
        goto clear; /* LCOV_EXCL_LINE */
    }
    mpn_copyi(tmp_s->digits, tmp_s_digits, tmp_s->size);
    SETNEG((ISNEG(u) && ssize > 0) || (!ISNEG(u) && ssize < 0), tmp_s);
    free(tmp_s_digits);
    tmp_s_digits = NULL;
    if (t) {
        /* Use t = (g - u*s)/v, if no integer overflow is possible,
           else compute t by Extended Euclidean algorithm */
        if (MAX(u->size + tmp_s->size, tmp_g->size) < ZZ_DIGITS_MAX) {
            if (zz_mul(u, tmp_s, o2) || zz_sub(tmp_g, o2, o2)
                || zz_div(o2, v, t, NULL))
            {
                goto clear; /* LCOV_EXCL_LINE */
            }
        }
        else {
            /* LCOV_EXCL_START */
            if (zz_div(u, tmp_g, o1, NULL) || zz_div(v, tmp_g, o2, NULL)
                || zz_inverse_euclidext(o2, o1, t))
            {
                goto clear;
            }
            /* LCOV_EXCL_STOP */
        }
    }
    zz_clear(o1);
    zz_clear(o2);
    free(o1);
    free(o2);
    o1 = NULL;
    o2 = NULL;
    if (s && zz_pos(tmp_s, s)) {
        goto clear; /* LCOV_EXCL_LINE */
    }
    if (g && zz_pos(tmp_g, g)) {
        goto clear; /* LCOV_EXCL_LINE */
    }
    zz_clear(tmp_s);
    zz_clear(tmp_g);
    free(tmp_g);
    free(tmp_s);
    return ZZ_OK;
    /* LCOV_EXCL_START */
clear:
    zz_clear(o1);
    zz_clear(o2);
    zz_clear(tmp_g);
    zz_clear(tmp_s);
free:
    free(o1);
    free(o2);
    free(tmp_g);
    free(tmp_s);
    free(tmp_s_digits);
    return ZZ_MEM;
    /* LCOV_EXCL_STOP */
}

static zz_err
zz_inverse(const zz_t *u, const zz_t *v, zz_t *w)
{
    zz_t g;

    if (zz_init(&g) || zz_gcdext(u, v, &g, w, NULL)) {
        /* LCOV_EXCL_START */
        zz_clear(&g);
        return ZZ_MEM;
        /* LCOV_EXCL_STOP */
    }
    if (zz_cmp_i64(&g, 1) == ZZ_EQ) {
        zz_clear(&g);
        return ZZ_OK;
    }
    zz_clear(&g);
    return ZZ_VAL;
}

zz_err
zz_lcm(const zz_t *u, const zz_t *v, zz_t *w)
{
    zz_t g;
    zz_err ret = ZZ_MEM;

    if (zz_init(&g) || zz_gcd(u, v, &g)) {
        goto end; /* LCOV_EXCL_LINE */
    }
    if (zz_iszero(&g)) {
        zz_clear(&g);
        return zz_set_i64(0, w);
    }
    if (zz_div(u, &g, &g, NULL)) {
        goto end; /* LCOV_EXCL_LINE */
    }
    ret = zz_mul(&g, v, w);
end:
    zz_clear(&g);
    (void)zz_abs(w, w);
    return ret;
}

zz_err
zz_powm(const zz_t *u, const zz_t *v, const zz_t *w, zz_t *res)
{
    if (!w->size) {
        return ZZ_VAL;
    }
    if (u == res) {
        zz_t tmp;

        if (zz_init(&tmp) || zz_pos(u, &tmp)) {
            /* LCOV_EXCL_START */
            zz_clear(&tmp);
            return ZZ_MEM;
            /* LCOV_EXCL_STOP */
        }

        zz_err ret = zz_powm(&tmp, v, w, res);

        zz_clear(&tmp);
        return ret;
    }
    if (v == res) {
        zz_t tmp;

        if (zz_init(&tmp) || zz_pos(v, &tmp)) {
            /* LCOV_EXCL_START */
            zz_clear(&tmp);
            return ZZ_MEM;
            /* LCOV_EXCL_STOP */
        }

        zz_err ret = zz_powm(u, &tmp, w, res);

        zz_clear(&tmp);
        return ret;
    }
    if (w == res) {
        zz_t tmp;

        if (zz_init(&tmp) || zz_pos(w, &tmp)) {
            /* LCOV_EXCL_START */
            zz_clear(&tmp);
            return ZZ_MEM;
            /* LCOV_EXCL_STOP */
        }

        zz_err ret = zz_powm(u, v, &tmp, res);

        zz_clear(&tmp);
        return ret;
    }

    zz_t o1, o2;

    if (zz_init(&o1) || zz_init(&o2)) {
        /* LCOV_EXCL_START */
mem:
        zz_clear(&o1);
        zz_clear(&o2);
        return ZZ_MEM;
        /* LCOV_EXCL_STOP */
    }
    if (ISNEG(v)) {
        zz_err ret = zz_inverse(u, w, &o2);

        if (ret == ZZ_VAL) {
            zz_clear(&o1);
            zz_clear(&o2);
            return ZZ_VAL;
        }
        if (ret == ZZ_MEM || zz_abs(v, &o1)) {
            goto mem; /* LCOV_EXCL_LINE */
        }
        u = &o2;
        v = &o1;
    }
    if (u->size > INT_MAX || v->size > INT_MAX || w->size > INT_MAX) {
        return ZZ_MEM; /* LCOV_EXCL_LINE */
    }

    mpz_t z;
    TMP_MPZ(b, u)
    TMP_MPZ(e, v)
    TMP_MPZ(m, w)
    if (TMP_OVERFLOW) {
        return ZZ_MEM; /* LCOV_EXCL_LINE */
    }
    mpz_init(z);
    mpz_powm(z, b, e, m);
    if (zz_set_mpz_t(z, res)) {
        /* LCOV_EXCL_START */
        mpz_clear(z);
        goto mem;
        /* LCOV_EXCL_STOP */
    }
    mpz_clear(z);
    if (ISNEG(w) && res->size && zz_add(w, res, res)) {
        goto mem; /* LCOV_EXCL_LINE */
    }
    zz_clear(&o1);
    zz_clear(&o2);
    return ZZ_OK;
}

zz_err
zz_sqrtrem(const zz_t *u, zz_t *v, zz_t *w)
{
    if (ISNEG(u)) {
        return ZZ_VAL;
    }
    SETNEG(false, v);
    if (!u->size) {
        v->size = 0;
        if (w) {
            w->size = 0;
            SETNEG(false, w);
        }
        return ZZ_OK;
    }
    if (u == v) {
        zz_t tmp;

        if (zz_init(&tmp) || zz_pos(v, &tmp)) {
            /* LCOV_EXCL_START */
            zz_clear(&tmp);
            return ZZ_MEM;
            /* LCOV_EXCL_STOP */
        }

        zz_err ret = zz_sqrtrem(&tmp, v, w);

        zz_clear(&tmp);
        return ret;
    }
    if (zz_resize((u->size + 1)/2, v) || TMP_OVERFLOW) {
        return ZZ_MEM; /* LCOV_EXCL_LINE */
    }
    if (w) {
        SETNEG(false, w);
        if (zz_resize(u->size, w)) {
            return ZZ_MEM; /* LCOV_EXCL_LINE */
        }
        w->size = (zz_size_t)mpn_sqrtrem(v->digits, w->digits, u->digits,
                                         u->size);
    }
    else {
        mpn_sqrtrem(v->digits, NULL, u->digits, u->size);
    }
    return ZZ_OK;
}

zz_err
zz_fac(uint64_t u, zz_t *v)
{
#if ULONG_MAX < ZZ_DIGIT_T_MAX
    if (u > ULONG_MAX) {
        return ZZ_BUF;
    }
#endif
    if (TMP_OVERFLOW) {
        return ZZ_MEM;
    }

    mpz_t z;

    mpz_init(z);
    mpz_fac_ui(z, (unsigned long)u);
    if (zz_set_mpz_t(z, v)) {
        /* LCOV_EXCL_START */
        mpz_clear(z);
        return ZZ_MEM;
        /* LCOV_EXCL_STOP */
    }
    mpz_clear(z);
    return ZZ_OK;
}

zz_err
zz_bin(uint64_t n, uint64_t k, zz_t *v)
{
#if ULONG_MAX < ZZ_DIGIT_T_MAX
    if (n > ULONG_MAX || k > ULONG_MAX) {
        return ZZ_BUF;
    }
#endif
    if (TMP_OVERFLOW) {
        return ZZ_MEM; /* LCOV_EXCL_LINE */
    }

    mpz_t z;

    mpz_init(z);
    mpz_bin_uiui(z, (unsigned long)n, (unsigned long)k);
    if (zz_set_mpz_t(z, v)) {
        /* LCOV_EXCL_START */
        mpz_clear(z);
        return ZZ_MEM;
        /* LCOV_EXCL_STOP */
    }
    mpz_clear(z);
    return ZZ_OK;
}
