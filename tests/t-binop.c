/*
    Copyright (C) 2024-2026 Sergey B Kirpichev

    This file is part of the ZZ Library.

    The ZZ Library is free software: you can redistribute it and/or modify it
    under the terms of the GNU Lesser General Public License (LGPL) as
    published by the Free Software Foundation; either version 3 of the License,
    or (at your option) any later version.  See
    <https://www.gnu.org/licenses/>.
*/

#include "tests/tests.h"

#define ZZ_BINOP_REF(op)                                \
    zz_err                                              \
    zz_ref_##op(const zz_t *u, const zz_t *v, zz_t *w)  \
    {                                                   \
        mpz_t z;                                        \
        TMP_MPZ(mu, u);                                 \
        TMP_MPZ(mv, v);                                 \
        if (TMP_OVERFLOW) {                             \
            return ZZ_MEM;                              \
        }                                               \
        mpz_init(z);                                    \
        mpz_##op(z, mu, mv);                            \
        if (zz_set_mpz_t(z, w)) {                       \
            mpz_clear(z);                               \
            return ZZ_MEM;                              \
        }                                               \
        mpz_clear(z);                                   \
        return ZZ_OK;                                   \
    }

#define TEST_BINOP_EXAMPLE(op, lhs, rhs)                      \
    do {                                                      \
        zz_t u, v, w, r;                                      \
                                                              \
        if (zz_init(&u) || zz_init(&v) || zz_init(&r)         \
            || zz_init(&w))                                   \
        {                                                     \
            abort();                                          \
        }                                                     \
        if (zz_pos(lhs, &u) || zz_pos(rhs, &v)) {             \
            abort();                                          \
        }                                                     \
                                                              \
        zz_err ret = zz_##op(&u, &v, &w);                     \
                                                              \
        if (ret == ZZ_VAL) {                                  \
            zz_clear(&u);                                     \
            zz_clear(&v);                                     \
            zz_clear(&w);                                     \
            zz_clear(&r);                                     \
            continue;                                         \
        }                                                     \
        else if (ret != ZZ_OK) {                              \
            abort();                                          \
        }                                                     \
        if (zz_ref_##op(&u, &v, &r)                           \
            || zz_cmp(&w, &r) != ZZ_EQ)                       \
        {                                                     \
            abort();                                          \
        }                                                     \
        if (zz_pos(&u, &w) || zz_##op(&w, &v, &w)             \
            || zz_cmp(&w, &r) != ZZ_EQ) {                     \
            abort();                                          \
        }                                                     \
        if (zz_pos(&v, &w) || zz_##op(&u, &w, &w)             \
            || zz_cmp(&w, &r) != ZZ_EQ) {                     \
            abort();                                          \
        }                                                     \
        zz_clear(&u);                                         \
        zz_clear(&v);                                         \
        zz_clear(&w);                                         \
        zz_clear(&r);                                         \
    } while (0);

#define TEST_MIXBINOP_EXAMPLE(op, lhs, rhs)                   \
    TEST_BINOP_EXAMPLE(op, lhs, rhs)                          \
    do {                                                      \
        zz_t u, v, w, r;                                      \
                                                              \
        if (zz_init(&u) || zz_init(&v) || zz_init(&w)         \
            || zz_init(&r))                                   \
        {                                                     \
            abort();                                          \
        }                                                     \
        if (zz_pos(lhs, &u) || zz_pos(rhs, &v)) {             \
            abort();                                          \
        }                                                     \
                                                              \
        int64_t val;                                          \
                                                              \
        if (zz_get(&v, &val) == ZZ_OK) {                      \
            zz_err ret = zz_##op(&u, val, &w);                \
                                                              \
            if (ret == ZZ_VAL) {                              \
                zz_clear(&u);                                 \
                zz_clear(&v);                                 \
                zz_clear(&w);                                 \
                zz_clear(&r);                                 \
                continue;                                     \
            }                                                 \
            else if (ret) {                                   \
                abort();                                      \
            }                                                 \
            if (zz_ref_##op(&u, &v, &r)                       \
                || zz_cmp(&w, &r) != ZZ_EQ)                   \
            {                                                 \
                abort();                                      \
            }                                                 \
            if (zz_pos(&u, &w) || zz_##op(&w, val, &w)        \
                || zz_cmp(&w, &r) != ZZ_EQ)                   \
            {                                                 \
                abort();                                      \
            }                                                 \
        }                                                     \
        if (zz_get(&u, &val) == ZZ_OK) {                      \
            zz_err ret = zz_##op(val, &v, &w);                \
                                                              \
            if (ret == ZZ_VAL) {                              \
                zz_clear(&u);                                 \
                zz_clear(&v);                                 \
                zz_clear(&w);                                 \
                zz_clear(&r);                                 \
                continue;                                     \
            }                                                 \
            else if (ret) {                                   \
                abort();                                      \
            }                                                 \
            if (zz_ref_##op(&u, &v, &r)                       \
                || zz_cmp(&w, &r) != ZZ_EQ)                   \
            {                                                 \
                abort();                                      \
            }                                                 \
            if (zz_pos(&v, &w) || zz_##op(val, &w, &w)        \
                || zz_cmp(&w, &r) != ZZ_EQ)                   \
            {                                                 \
                abort();                                      \
            }                                                 \
        }                                                     \
        zz_clear(&u);                                         \
        zz_clear(&v);                                         \
        zz_clear(&w);                                         \
        zz_clear(&r);                                         \
    } while (0);

#define TEST_BINOP(op, bs, neg)                                \
    void                                                       \
    check_##op##_bulk(void)                                    \
    {                                                          \
        for (size_t i = 0; i < nsamples; i++) {                \
            zz_t lhs, rhs;                                     \
                                                               \
            if (zz_init(&lhs) || zz_random(bs, neg, &lhs)) {   \
                abort();                                       \
            }                                                  \
            if (zz_init(&rhs) || zz_random(bs, neg, &rhs)) {   \
                abort();                                       \
            }                                                  \
            TEST_BINOP_EXAMPLE(op, &lhs, &rhs);                \
            zz_clear(&lhs);                                    \
            zz_clear(&rhs);                                    \
        }                                                      \
    }

#define TEST_MIXBINOP(op, bs, neg)                             \
    void                                                       \
    check_##op##_bulk(void)                                    \
    {                                                          \
        for (size_t i = 0; i < nsamples; i++) {                \
            zz_t lhs, rhs;                                     \
                                                               \
            if (zz_init(&lhs) || zz_random(bs, neg, &lhs)) {   \
                abort();                                       \
            }                                                  \
            if (zz_init(&rhs) || zz_random(bs, neg, &rhs)) {   \
                abort();                                       \
            }                                                  \
            TEST_MIXBINOP_EXAMPLE(op, &lhs, &rhs);             \
            zz_clear(&lhs);                                    \
            zz_clear(&rhs);                                    \
        }                                                      \
    }

ZZ_BINOP_REF(add)
ZZ_BINOP_REF(sub)
ZZ_BINOP_REF(mul)

#define zz_fdiv_q(U, V, W) zz_div((U), (V), (W), NULL)
#define zz_fdiv_r(U, V, W) zz_div((U), (V), NULL, (W))

ZZ_BINOP_REF(fdiv_q)
ZZ_BINOP_REF(fdiv_r)

ZZ_BINOP_REF(and)
#define zz_ior zz_or
ZZ_BINOP_REF(ior)
ZZ_BINOP_REF(xor)

zz_err
zz_gcd(const zz_t *u, const zz_t *v, zz_t *w)
{
    return zz_gcdext(u, v, w, NULL, NULL);
}

ZZ_BINOP_REF(gcd)
ZZ_BINOP_REF(lcm)

zz_err
zz_ref_mul_2exp(const zz_t *u, zz_bitcnt_t v, zz_t *w)
{
    mpz_t z;
    TMP_MPZ(mu, u);
    if (TMP_OVERFLOW) {
        return ZZ_MEM;
    }
    mpz_init(z);
    mpz_mul_2exp(z, mu, (mp_bitcnt_t)v);
    if (zz_set_mpz_t(z, w)) {
        mpz_clear(z);
        return ZZ_MEM;
    }
    mpz_clear(z);
    return ZZ_OK;
}

zz_err
zz_ref_quo_2exp(const zz_t *u, zz_bitcnt_t v, zz_t *w)
{
    mpz_t z;
    TMP_MPZ(mu, u);
    if (TMP_OVERFLOW) {
        return ZZ_MEM;
    }
    mpz_init(z);
    mpz_fdiv_q_2exp(z, mu, (mp_bitcnt_t)v);
    if (zz_set_mpz_t(z, w)) {
        mpz_clear(z);
        return ZZ_MEM;
    }
    mpz_clear(z);
    return ZZ_OK;
}

TEST_MIXBINOP(add, 512, true)
TEST_MIXBINOP(sub, 512, true)
TEST_MIXBINOP(mul, 512, true)

TEST_MIXBINOP(fdiv_q, 512, true)
TEST_MIXBINOP(fdiv_r, 512, true)

TEST_BINOP(and, 512, true)
TEST_BINOP(ior, 512, true)
TEST_BINOP(xor, 512, true)

TEST_BINOP(gcd, 512, true)
TEST_BINOP(lcm, 512, true)

void
check_binop_examples(void)
{
    zz_t u, v;

    if (zz_init(&u) || zz_init(&v)) {
        abort();
    }
    if (zz_set(0, &u) || zz_set(0, &v) || zz_add(&u, &v, &u)
        || zz_cmp(&u, 0) != ZZ_EQ)
    {
        abort();
    }
    if (zz_set(1, &v) || zz_add(&u, &v, &u) || zz_cmp(&u, 1) != ZZ_EQ) {
        abort();
    }
    if (zz_set(0, &u) || zz_add(&u, 0, &u)
        || zz_cmp(&u, 0) != ZZ_EQ)
    {
        abort();
    }
    if (zz_set(0, &u) || zz_add(&u, 1, &u)
        || zz_cmp(&u, 1) != ZZ_EQ)
    {
        abort();
    }
    if (zz_set(0, &v) || zz_mul(&u, &v, &u) || zz_cmp(&u, 0) != ZZ_EQ) {
        abort();
    }
    if (zz_set(1, &u) || zz_mul(&u, 0, &u) || zz_cmp(&u, 0) != ZZ_EQ) {
        abort();
    }
    if (zz_div(&u, 1, &u, NULL) || zz_cmp(&u, 0) != ZZ_EQ) {
        abort();
    }
    if (zz_div(&u, 1, NULL, &u) || zz_cmp(&u, 0) != ZZ_EQ) {
        abort();
    }
    if (zz_set(2, &u) || zz_div(&u, 2, NULL, &u)
        || zz_cmp(&u, 0) != ZZ_EQ)
    {
        abort();
    }
    if (zz_set(2, &v) || zz_and(&u, &v, &u) || zz_cmp(&u, 0) != ZZ_EQ) {
        abort();
    }
    if (zz_set(-1, &u) || zz_set(-1, &v) || zz_and(&u, &v, &u)
        || zz_cmp(&u, -1) != ZZ_EQ)
    {
        abort();
    }
    if (zz_set(1, &u) || zz_set(2, &v) || zz_and(&u, &v, &u)
        || zz_cmp(&u, 0) != ZZ_EQ)
    {
        abort();
    }
    if (zz_set(2, &v) || zz_or(&u, &v, &u) || zz_cmp(&u, 2) != ZZ_EQ) {
        abort();
    }
    if (zz_set(0, &u) || zz_set(2, &v) || zz_or(&v, &u, &u)
        || zz_cmp(&u, 2) != ZZ_EQ)
    {
        abort();
    }
    if (zz_set(-1, &u) || zz_set(-1, &v) || zz_or(&u, &v, &u)
        || zz_cmp(&u, -1) != ZZ_EQ)
    {
        abort();
    }
    if (zz_set(12, &u) || zz_set(-1, &v) || zz_or(&u, &v, &u)
        || zz_cmp(&u, -1) != ZZ_EQ)
    {
        abort();
    }
    if (zz_set(0, &u) || zz_set(2, &v) || zz_xor(&v, &u, &u)
        || zz_cmp(&u, 2) != ZZ_EQ)
    {
        abort();
    }
    if (zz_set(0, &u) || zz_set(2, &v) || zz_xor(&u, &v, &u)
        || zz_cmp(&u, 2) != ZZ_EQ)
    {
        abort();
    }
    if (zz_set(-1, &u) || zz_set(-1, &v) || zz_xor(&u, &v, &u)
        || zz_cmp(&u, 0) != ZZ_EQ)
    {
        abort();
    }
    if (zz_set(0, &u) || zz_set(0, &v) || zz_lcm(&u, &v, &u)
        || zz_cmp(&u, 0) != ZZ_EQ)
    {
        abort();
    }
    if (zz_set(4, &u)) {
        abort();
    }
    if (zz_set(2, &v)) {
        abort();
    }
    if (zz_div(&u, &v, NULL, NULL) != ZZ_OK) {
        abort();
    }
    if (zz_div_i64(&u, 123, NULL, NULL) != ZZ_OK) {
        abort();
    }
    if (zz_i64_div(123, &v, NULL, NULL) != ZZ_OK) {
        abort();
    }
    if (zz_set(0, &v) || zz_div(&u, &v, &v, NULL) != ZZ_VAL) {
        abort();
    }
    if (zz_set(1, &u)) {
        abort();
    }
    if (zz_div(&u, 0, &u, NULL) != ZZ_VAL) {
        abort();
    }
    if (zz_set(0, &v)) {
        abort();
    }
    if (zz_div(1, &v, &v, NULL) != ZZ_VAL) {
        abort();
    }
    if (zz_set(1, &v) || zz_div(1, &v, NULL, NULL) != ZZ_OK) {
        abort();
    }
    if (zz_set(1, &u) || zz_add(&u, 1U, &u) || zz_cmp(&u, 2) != ZZ_EQ) {
        abort();
    }
    if (zz_set(3, &u) || zz_sub(&u, 1U, &u) || zz_cmp(&u, 2) != ZZ_EQ) {
        abort();
    }
    if (zz_set(3, &u) || zz_sub(1U, &u, &u) || zz_cmp(&u, -2) != ZZ_EQ) {
        abort();
    }
    if (zz_set(-3, &u) || zz_sub(1U, &u, &u) || zz_cmp(&u, 4) != ZZ_EQ) {
        abort();
    }
    zz_clear(&u);
    zz_clear(&v);
}

void
check_lshift_bulk(void)
{
    zz_bitcnt_t bs = 512;

    for (size_t i = 0; i < nsamples; i++) {
        zz_t u, w, r;
        zz_bitcnt_t v = (zz_bitcnt_t)rand() % 12345;

        if (zz_init(&u) || zz_random(bs, true, &u)) {
            abort();
        }
        if (zz_init(&w) || zz_mul_2exp(&u, v, &w)) {
            abort();
        }
        if (zz_init(&r) || zz_ref_mul_2exp(&u, v, &r)
            || zz_cmp(&w, &r) != ZZ_EQ)
        {
            abort();
        }
        if (zz_pos(&u, &w) || zz_mul_2exp(&w, v, &w)
            || zz_cmp(&w, &r) != ZZ_EQ)
        {
            abort();
        }
        zz_clear(&u);
        zz_clear(&w);
        zz_clear(&r);
    }
}

void
check_rshift_bulk(void)
{
    zz_bitcnt_t bs = 512;

    for (size_t i = 0; i < nsamples; i++) {
        zz_t u, w, r;
        zz_bitcnt_t v = (zz_bitcnt_t)rand();

        if (zz_init(&u) || zz_random(bs, true, &u)) {
            abort();
        }
        if (zz_init(&w) || zz_quo_2exp(&u, v, &w)) {
            abort();
        }
        if (zz_init(&r) || zz_ref_quo_2exp(&u, v, &r)
            || zz_cmp(&w, &r) != ZZ_EQ)
        {
            abort();
        }
        if (zz_pos(&u, &w) || zz_quo_2exp(&w, v, &w)
            || zz_cmp(&w, &r) != ZZ_EQ)
        {
            abort();
        }
        zz_clear(&u);
        zz_clear(&w);
        zz_clear(&r);
    }
}

#define zz_set_dec(s, u) zz_set_str(s, 10, u)

void
check_shift_examples(void)
{
    zz_t u, v;

    if (zz_init(&u) || zz_set(0, &u) || zz_init(&v)) {
        abort();
    }
    if (zz_mul_2exp(&u, 123, &v) || zz_cmp(&v, 0)) {
        abort();
    }
    if (zz_quo_2exp(&u, 123, &v) || zz_cmp(&v, 0)) {
        abort();
    }
    if (zz_set_dec("-340282366920938463444927863358058659840", &u)
        || zz_quo_2exp(&u, 64, &v))
    {
        abort();
    }
    if (zz_set_dec("-18446744073709551615", &u)
        || zz_cmp(&u, &v) != ZZ_EQ)
    {
        abort();
    }
    if (zz_set_dec("-514220174162876888173427869549172"
                    "032807104958010493707296440352", &u)
        || zz_quo_2exp(&u, 206, &v) || zz_cmp(&v, -6) != ZZ_EQ)
    {
        abort();
    }
    if (zz_set_dec("-62771017353866807634955070562867279"
                    "52638980837032266301441", &u)
        || zz_quo_2exp(&u, 128, &v))
    {
        abort();
    }
    if (zz_set_dec("-18446744073709551616", &u) || zz_cmp(&u, &v)) {
        abort();
    }
    if (zz_set(-1, &u) || zz_quo_2exp(&u, 1, &v)
        || zz_cmp(&v, -1) != ZZ_EQ)
    {
        abort();
    }
    if (zz_set(1, &u) || zz_mul_2exp(&u, 64, &u)
        || zz_mul_2exp(&u, ZZ_BITS_MAX, &u) != ZZ_BUF)
    {
        abort();
    }
#if _WIN32
    if (zz_set(1, &u)
        || zz_mul_2exp(&u, UINT64_MAX, &u) != ZZ_BUF)
    {
        abort();
    }
#endif
    if (zz_set(0x7fffffffffffffffLL, &u)) {
        abort();
    }
    if (zz_mul_2exp(&u, 1, &u) || zz_add(&u, 1, &u)
        || zz_mul_2exp(&u, 64, &u) || zz_quo_2exp(&u, 64, &u))
    {
        abort();
    }
    if (u.negative || u.alloc < 1 || u.size != 1
        || u.digits[0] != 0xffffffffffffffffULL)
    {
        abort();
    }
    if (zz_set(0x7fffffffffffffffLL, &v)) {
        abort();
    }
    if (zz_mul_2exp(&v, 1, &v) || zz_add(&v, 1, &v)
        || zz_cmp(&u, &v) != ZZ_EQ)
    {
        abort();
    }
#if ZZ_DIGIT_T_BITS == 64
    if (zz_set(1, &u) || zz_mul_2exp(&u, 64, &u)
        || zz_pow(&u, ((uint64_t)1<<63), &u) != ZZ_BUF)
    {
        abort();
    }
#endif
    zz_clear(&u);
    zz_clear(&v);
}

void
check_square_outofmem(void)
{
    zz_set_memory_funcs(my_malloc, my_realloc, my_free);
    max_size = 8*1000*1000;
    if (total_size) {
        abort();
    }
    for (size_t i = 0; i < 7; i++) {
        int64_t x = 49846727467293 + rand();
        zz_t mx;

        if (zz_init(&mx) || zz_set(x, &mx)) {
            abort();
        }
        while (1) {
            zz_err r = zz_mul(&mx, &mx, &mx);

            if (r != ZZ_OK) {
                if (r == ZZ_MEM) {
                    break;
                }
                abort();
            }
        }
        zz_clear(&mx);
        if (zz_get_alloc_state()) {
            abort();
        }
        total_size = 0;
    }
    zz_set_memory_funcs(NULL, NULL, NULL);
}

#if HAVE_PTHREAD_H
void
check_square_outofmem_pthread(void)
{
    zz_set_memory_funcs(my_malloc, my_realloc, my_free);
    max_size = 8*1000*1000;

    size_t nthreads = 7;
    pthread_t *tid = malloc(nthreads * sizeof(pthread_t));
    int *d = malloc(nthreads * sizeof(int));

    for (size_t i = 0; i < nthreads; i++) {
        d[i] = 10 + 201*(int)i;
        if (pthread_create(&tid[i], NULL, square_worker, (void *)(d + i))) {
            abort();
        }
    }
    for (size_t i = 0; i < nthreads; i++) {
        pthread_join(tid[i], NULL);
        if (d[i]) {
            abort();
        }
    }
    free(d);
    free(tid);
    zz_set_memory_funcs(NULL, NULL, NULL);
}
#endif /* HAVE_PTHREAD_H */

int
main(void)
{
    srand((unsigned int)time(NULL));
    zz_testinit();
    zz_setup();
    check_add_bulk();
    check_sub_bulk();
    check_mul_bulk();
    check_fdiv_q_bulk();
    check_fdiv_r_bulk();
    check_and_bulk();
    check_ior_bulk();
    check_xor_bulk();
    check_gcd_bulk();
    check_lcm_bulk();
    check_binop_examples();
    check_lshift_bulk();
    check_rshift_bulk();
    check_shift_examples();
    check_square_outofmem();
#if HAVE_PTHREAD_H
    check_square_outofmem_pthread();
#endif
    zz_finish();
    zz_testclear();
    return 0;
}
