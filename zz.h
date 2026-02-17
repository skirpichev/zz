/*
    Copyright (C) 2024-2026 Sergey B Kirpichev

    This file is part of the ZZ Library.

    The ZZ Library is free software: you can redistribute it and/or modify it
    under the terms of the GNU Lesser General Public License (LGPL) as
    published by the Free Software Foundation; either version 3 of the License,
    or (at your option) any later version.  See
    <https://www.gnu.org/licenses/>.
*/

#ifndef ZZ_H
#define ZZ_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifndef __APPLE__
typedef uint64_t zz_digit_t;
#else
typedef unsigned long zz_digit_t;
#endif
typedef uint64_t zz_bitcnt_t;
#ifndef _WIN32
typedef int64_t zz_size_t;
#else
typedef int32_t zz_size_t;
#endif

typedef struct {
    bool negative;
    zz_size_t alloc;
    zz_size_t size;
    zz_digit_t *digits;
} zz_t;

typedef enum {
    ZZ_OK = 0,
    ZZ_MEM = -1,
    ZZ_VAL = -2,
    ZZ_BUF = -3,
} zz_err;

zz_err zz_setup(void);
void zz_finish(void);

zz_err zz_init(zz_t *u);
void zz_clear(zz_t *u);

zz_err zz_set_i32(int32_t u, zz_t *v);
zz_err zz_set_i64(int64_t u, zz_t *v);
zz_err zz_set_u64(uint64_t u, zz_t *v);
zz_err zz_set_double(double u, zz_t *v);

#define zz_set(U, V)                         \
    _Generic((U),                            \
             int: zz_set_i32,                \
             long: zz_set_i64,               \
             long long: zz_set_i64,          \
             unsigned int: zz_set_u64,       \
             unsigned long: zz_set_u64,      \
             unsigned long long: zz_set_u64, \
             float: zz_set_double,           \
             double: zz_set_double)(U, V)

zz_err zz_set_str(const char *str, int base, zz_t *u);

zz_err zz_get_i32(const zz_t *u, int32_t *v);
zz_err zz_get_i64(const zz_t *u, int64_t *v);
zz_err zz_get_u32(const zz_t *u, uint32_t *v);
zz_err zz_get_u64(const zz_t *u, uint64_t *v);
zz_err zz_get_double(const zz_t *u, double *d);

#define zz_get(U, V)                                           \
    _Generic((U),                                              \
             default: _Generic((V),                            \
                               int32_t *: zz_get_i32,          \
                               int64_t *: zz_get_i64,          \
                               uint32_t *: zz_get_u32,         \
                               uint64_t *: zz_get_u64,         \
                               double *: zz_get_double))(U, V)

zz_err zz_get_str(const zz_t *u, int base, char *str);

zz_err zz_add(const zz_t *u, const zz_t *v, zz_t *w);
zz_err zz_add_i64(const zz_t *u, int64_t v, zz_t *w);
zz_err zz_add_u64(const zz_t *u, uint64_t v, zz_t *w);
zz_err zz_sub(const zz_t *u, const zz_t *v, zz_t *w);
zz_err zz_sub_i64(const zz_t *u, int64_t v, zz_t *w);
zz_err zz_i64_sub(int64_t u, const zz_t *v, zz_t *w);
zz_err zz_sub_u64(const zz_t *u, uint64_t v, zz_t *w);
zz_err zz_u64_sub(uint64_t u, const zz_t *v, zz_t *w);
zz_err zz_pos(const zz_t *u, zz_t *v);
zz_err zz_neg(const zz_t *u, zz_t *v);
zz_err zz_abs(const zz_t *u, zz_t *v);
zz_err zz_mul(const zz_t *u, const zz_t *v, zz_t *w);
zz_err zz_mul_i64(const zz_t *u, int64_t v, zz_t *w);
zz_err zz_mul_u64(const zz_t *u, uint64_t v, zz_t *w);
zz_err zz_div(const zz_t *u, const zz_t *v, zz_t *q, zz_t *r);
zz_err zz_div_i64(const zz_t *u, int64_t v, zz_t *q, zz_t *r);
zz_err zz_i64_div(int64_t u, const zz_t *v, zz_t *q, zz_t *r);

static inline zz_err
zz_i64_add(int64_t u, const zz_t *v, zz_t *w)
{
    return zz_add_i64(v, u, w);
}

static inline zz_err
zz_u64_add(uint64_t u, const zz_t *v, zz_t *w)
{
    return zz_add_u64(v, u, w);
}

static inline zz_err
zz_i64_mul(int64_t u, const zz_t *v, zz_t *w)
{
    return zz_mul_i64(v, u, w);
}

static inline zz_err
zz_u64_mul(uint64_t u, const zz_t *v, zz_t *w)
{
    return zz_mul_u64(v, u, w);
}

#define zz_add(U, V, W)                                         \
    _Generic((U),                                               \
             int: _Generic((V),                                 \
                           default: zz_i64_add),                \
             long: _Generic((V),                                \
                            default: zz_i64_add),               \
             long long: _Generic((V),                           \
                                 default: zz_i64_add),          \
             unsigned int: _Generic((V),                        \
                                    default: zz_u64_add),       \
             unsigned long: _Generic((V),                       \
                                     default: zz_u64_add),      \
             unsigned long long: _Generic((V),                  \
                                          default: zz_u64_add), \
             default: _Generic((V),                             \
                               int: zz_add_i64,                 \
                               long: zz_add_i64,                \
                               long long: zz_add_i64,           \
                               unsigned int: zz_add_u64,        \
                               unsigned long: zz_add_u64,       \
                               unsigned long long: zz_add_u64,  \
                               default: zz_add))(U, V, W)
#define zz_sub(U, V, W)                                         \
    _Generic((U),                                               \
             int: _Generic((V),                                 \
                           default: zz_i64_sub),                \
             long: _Generic((V),                                \
                            default: zz_i64_sub),               \
             long long: _Generic((V),                           \
                                 default: zz_i64_sub),          \
             unsigned int: _Generic((V),                        \
                                    default: zz_u64_sub),       \
             unsigned long: _Generic((V),                       \
                                     default: zz_u64_sub),      \
             unsigned long long: _Generic((V),                  \
                                          default: zz_u64_sub), \
             default: _Generic((V),                             \
                               int: zz_sub_i64,                 \
                               long: zz_sub_i64,                \
                               long long: zz_sub_i64,           \
                               unsigned int: zz_sub_u64,        \
                               unsigned long: zz_sub_u64,       \
                               unsigned long long: zz_sub_u64,  \
                               default: zz_sub))(U, V, W)
#define zz_mul(U, V, W)                                         \
    _Generic((U),                                               \
             int: _Generic((V),                                 \
                           default: zz_i64_mul),                \
             long: _Generic((V),                                \
                            default: zz_i64_mul),               \
             long long: _Generic((V),                           \
                                 default: zz_i64_mul),          \
             unsigned int: _Generic((V),                        \
                                    default: zz_u64_mul),       \
             unsigned long: _Generic((V),                       \
                                     default: zz_u64_mul),      \
             unsigned long long: _Generic((V),                  \
                                          default: zz_u64_mul), \
             default: _Generic((V),                             \
                               int: zz_mul_i64,                 \
                               long: zz_mul_i64,                \
                               long long: zz_mul_i64,           \
                               unsigned int: zz_mul_u64,        \
                               unsigned long: zz_mul_u64,       \
                               unsigned long long: zz_mul_u64,  \
                               default: zz_mul))(U, V, W)
#define zz_div(U, V, Q, R)                                   \
    _Generic((U),                                            \
             int: _Generic((V),                              \
                           default: zz_i64_div),             \
             long: _Generic((V),                             \
                            default: zz_i64_div),            \
             long long: _Generic((V),                        \
                                 default: zz_i64_div),       \
             default: _Generic((V),                          \
                               int: zz_div_i64,              \
                               long: zz_div_i64,             \
                               long long: zz_div_i64,        \
                               default: zz_div))(U, V, Q, R)

zz_err zz_pow(const zz_t *u, uint64_t v, zz_t *w);
zz_err zz_powm(const zz_t *u, const zz_t *v, const zz_t *w, zz_t *x);

typedef enum {
    ZZ_GT = +1,
    ZZ_EQ = 0,
    ZZ_LT = -1,
} zz_ord;

zz_ord zz_cmp(const zz_t *u, const zz_t *v);
zz_ord zz_cmp_i64(const zz_t *u, int64_t v);

#define zz_cmp(U, V)                                   \
    _Generic((U),                                      \
             default: _Generic((V),                    \
                               int: zz_cmp_i64,        \
                               long: zz_cmp_i64,       \
                               long long: zz_cmp_i64,  \
                               default: zz_cmp))(U, V)

zz_err zz_invert(const zz_t *u, zz_t *v);
zz_err zz_and(const zz_t *u, const zz_t *v, zz_t *w);
zz_err zz_or(const zz_t *u, const zz_t *v, zz_t *w);
zz_err zz_xor(const zz_t *u, const zz_t *v, zz_t *w);
zz_err zz_mul_2exp(const zz_t *u, zz_bitcnt_t v, zz_t *w);
zz_err zz_quo_2exp(const zz_t *u, zz_bitcnt_t v, zz_t *w);

zz_err zz_sqrtrem(const zz_t *u, zz_t *v, zz_t *w);
zz_err zz_gcdext(const zz_t *u, const zz_t *v, zz_t *g, zz_t *s, zz_t *t);
zz_err zz_lcm(const zz_t *u, const zz_t *v, zz_t *w);

zz_err zz_fac(uint64_t u, zz_t *v);
zz_err zz_bin(uint64_t n, uint64_t k, zz_t *v);

typedef struct {
    uint8_t bits_per_digit;
    uint8_t digit_size;
    int8_t digits_order;
    int8_t digit_endianness;
} zz_layout;

const zz_layout * zz_get_layout(void);

zz_err zz_import(size_t len, const void *data, zz_layout layout, zz_t *u);
zz_err zz_export(const zz_t *u, zz_layout layout, size_t len, void *data);

zz_err zz_sizeinbase(const zz_t *u, int base, size_t *size);
zz_bitcnt_t zz_bitlen(const zz_t *u);
zz_bitcnt_t zz_lsbpos(const zz_t *u);
zz_bitcnt_t zz_bitcnt(const zz_t *u);
bool zz_iszero(const zz_t *u);
bool zz_isneg(const zz_t *u);
bool zz_isodd(const zz_t *u);

const char * zz_get_version(void);

zz_bitcnt_t zz_get_bitcnt_max(void);

#endif /* ZZ_H */
