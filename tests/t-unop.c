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

#define ZZ_UNOP_REF(op)                 \
    zz_err                              \
    zz_ref_##op(const zz_t *u, zz_t *v) \
    {                                   \
        mpz_t z;                        \
        TMP_MPZ(mu, u);                 \
        if (TMP_OVERFLOW) {             \
            return ZZ_MEM;              \
        }                               \
        mpz_init(z);                    \
        mpz_##op(z, mu);                \
        if (zz_set_mpz_t(z, v)) {       \
            mpz_clear(z);               \
            return ZZ_MEM;              \
        }                               \
        mpz_clear(z);                   \
        return ZZ_OK;                   \
    }

#define TEST_UNOP_EXAMPLE(op, arg)                        \
    do {                                                  \
        zz_t u, v, r;                                     \
                                                          \
        if (zz_init(&u) || zz_init(&v)) {                 \
            abort();                                      \
        }                                                 \
        if (zz_pos(arg, &u)) {                            \
            abort();                                      \
        }                                                 \
        if (zz_init(&r) || zz_ref_##op(&u, &r)) {         \
            abort();                                      \
        }                                                 \
        if (zz_##op(&u, &v) || zz_cmp(&v, &r) != ZZ_EQ) { \
            abort();                                      \
        }                                                 \
        if (zz_pos(&u, &v) || zz_##op(&v, &v)             \
            || zz_cmp(&v, &r) != ZZ_EQ) {                 \
            abort();                                      \
        }                                                 \
        zz_clear(&u);                                     \
        zz_clear(&v);                                     \
        zz_clear(&r);                                     \
    } while (0);

#define TEST_UNOP(op, bs, neg)                                 \
    void                                                       \
    check_##op##_bulk(void)                                    \
    {                                                          \
        for (size_t i = 0; i < nsamples; i++) {                \
            zz_t arg;                                          \
                                                               \
            if (zz_init(&arg) || zz_random(bs, neg, &arg)) {   \
                abort();                                       \
            }                                                  \
            TEST_UNOP_EXAMPLE(op, &arg);                       \
            zz_clear(&arg);                                    \
        }                                                      \
    }

ZZ_UNOP_REF(neg)
ZZ_UNOP_REF(abs)

#define zz_com zz_invert
ZZ_UNOP_REF(com)

zz_err
zz_sqrt(const zz_t *u, zz_t *v)
{
    return zz_sqrtrem(u, v, NULL);
}
ZZ_UNOP_REF(sqrt)

TEST_UNOP(neg, 512, true)
TEST_UNOP(abs, 512, true)
TEST_UNOP(com, 512, true)

TEST_UNOP(sqrt, 512, false)

void
check_unary_examples(void)
{
    zz_t u;

    if (zz_init(&u) || zz_set(0, &u)) {
        abort();
    }
    if (zz_invert(&u, &u) || zz_cmp(&u, -1) != ZZ_EQ) {
        abort();
    }
    zz_clear(&u);
}

int main(void)
{
    srand((unsigned int)time(NULL));
    zz_testinit();
    zz_setup();
    check_neg_bulk();
    check_abs_bulk();
    check_com_bulk();
    check_sqrt_bulk();
    check_unary_examples();
    zz_finish();
    zz_testclear();
    return 0;
}
