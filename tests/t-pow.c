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

zz_err
zz_ref_powm(const zz_t *u, const zz_t *v, const zz_t *w, zz_t *r)
{
    mpz_t z;
    TMP_MPZ(mu, u);
    TMP_MPZ(mv, v);
    TMP_MPZ(mw, w);
    if (TMP_OVERFLOW) {
        return ZZ_MEM;
    }
    mpz_init(z);
    mpz_powm(z, mu, mv, mw);
    if (zz_set_mpz_t(z, r)) {
        mpz_clear(z);
        return ZZ_MEM;
    }
    mpz_clear(z);
    return ZZ_OK;
}

zz_err
zz_ref_gcd(const zz_t *u, const zz_t *v, zz_t *w)
{
    mpz_t z;
    TMP_MPZ(mu, u);
    TMP_MPZ(mv, v);
    if (TMP_OVERFLOW) {
        return ZZ_MEM;
    }
    mpz_init(z);
    mpz_gcd(z, mu, mv);
    if (zz_set_mpz_t(z, w)) {
        mpz_clear(z);
        return ZZ_MEM;
    }
    mpz_clear(z);
    return ZZ_OK;
}

void
check_powm_bulk(void)
{
    zz_bitcnt_t bs = 512;

    for (size_t i = 0; i < nsamples; i++) {
        zz_t u, v, w, z;

        if (zz_init(&u) || zz_random(bs, true, &u)) {
            abort();
        }
        if (zz_init(&v) || zz_random(32, true, &v)) {
            abort();
        }
        if (zz_init(&w) || zz_random(bs, false, &w)) {
            abort();
        }
        if (zz_init(&z)) {
            abort();
        }
        if (zz_powm(&u, &v, &w, &z) == ZZ_OK) {
            zz_t r;

            if (zz_init(&r) || zz_ref_powm(&u, &v, &w, &r)
                || zz_cmp(&z, &r) != ZZ_EQ)
            {
                abort();
            }
            zz_clear(&r);
        }
        else {
            if (zz_ref_gcd(&u, &w, &z) || zz_cmp(&z, 1) == ZZ_EQ) {
                abort();
            }
        }
        if (zz_pos(&u, &z) == ZZ_OK && zz_powm(&z, &v, &w, &z) == ZZ_OK) {
            zz_t r;

            if (zz_init(&r) || zz_ref_powm(&u, &v, &w, &r)
                || zz_cmp(&z, &r) != ZZ_EQ)
            {
                abort();
            }
            zz_clear(&r);
        }
        else {
            if (zz_ref_gcd(&u, &w, &z) || zz_cmp(&z, 1) == ZZ_EQ) {
                abort();
            }
        }
        if (zz_pos(&v, &z) == ZZ_OK && zz_powm(&u, &z, &w, &z) == ZZ_OK) {
            zz_t r;

            if (zz_init(&r) || zz_ref_powm(&u, &v, &w, &r)
                || zz_cmp(&z, &r) != ZZ_EQ)
            {
                abort();
            }
            zz_clear(&r);
        }
        else {
            if (zz_ref_gcd(&u, &w, &z) || zz_cmp(&z, 1) == ZZ_EQ) {
                abort();
            }
        }
        if (zz_pos(&w, &z) == ZZ_OK && zz_powm(&u, &v, &z, &z) == ZZ_OK) {
            zz_t r;

            if (zz_init(&r) || zz_ref_powm(&u, &v, &w, &r)
                || zz_cmp(&z, &r) != ZZ_EQ)
            {
                abort();
            }
            zz_clear(&r);
        }
        else {
            if (zz_ref_gcd(&u, &w, &z) || zz_cmp(&z, 1) == ZZ_EQ) {
                abort();
            }
        }
        zz_clear(&u);
        zz_clear(&v);
        zz_clear(&w);
        zz_clear(&z);
    }
}

void
check_powm_examples(void)
{
    zz_t u, v, w;

    if (zz_init(&u) || zz_set(12, &u)) {
        abort();
    }
    if (zz_init(&v) || zz_set(4, &v)) {
        abort();
    }
    if (zz_init(&w) || zz_set(7, &w)) {
        abort();
    }
    if (zz_powm(&u, &v, &w, &u) || zz_cmp(&u, 2) != ZZ_EQ) {
        abort();
    }
    if (zz_set(12, &u) || zz_powm(&u, &v, &w, &v)
        || zz_cmp(&v, 2) != ZZ_EQ)
    {
        abort();
    }
    if (zz_set(4, &v) || zz_powm(&u, &v, &w, &w)
        || zz_cmp(&w, 2) != ZZ_EQ)
    {
        abort();
    }
    if (zz_set(0, &w) || zz_powm(&u, &v, &w, &w) != ZZ_VAL) {
        abort();
    }
    zz_clear(&u);
    zz_clear(&v);
    zz_clear(&w);
}

zz_err
zz_ref_pow(const zz_t *u, uint64_t v, zz_t *w)
{
    mpz_t z;
    TMP_MPZ(mu, u);
    if (TMP_OVERFLOW) {
        return ZZ_MEM;
    }
    mpz_init(z);
    mpz_pow_ui(z, mu, (unsigned long int)v);
    if (zz_set_mpz_t(z, w)) {
        mpz_clear(z);
        return ZZ_MEM;
    }
    mpz_clear(z);
    return ZZ_OK;
}

void
check_pow_bulk(void)
{
    zz_bitcnt_t bs = 512;

    for (size_t i = 0; i < nsamples; i++) {
        zz_t u, w;
        uint64_t v = (uint64_t)rand() % (rand() % 10 > 7 ? 1000 : 100);

        if (zz_init(&u) || zz_random(bs, true, &u)) {
            abort();
        }
        if (zz_init(&w)) {
            abort();
        }
        if (zz_pow(&u, v, &w) == ZZ_OK) {
            zz_t r;

            if (zz_init(&r) || zz_ref_pow(&u, v, &r)
                || zz_cmp(&w, &r) != ZZ_EQ)
            {
                abort();
            }
            zz_clear(&r);
        }
        else {
            abort();
        }
        if (zz_pos(&u, &w) == ZZ_OK && zz_pow(&w, v, &w) == ZZ_OK) {
            zz_t r;

            if (zz_init(&r) || zz_ref_pow(&u, v, &r)
                || zz_cmp(&w, &r) != ZZ_EQ)
            {
                abort();
            }
            zz_clear(&r);
        }
        else {
            abort();
        }
        zz_clear(&u);
        zz_clear(&w);
    }
}

void
check_pow_examples(void)
{
    zz_t u;

    if (zz_init(&u) || zz_set(2, &u)) {
        abort();
    }
    if (zz_pow(&u, 2, &u) || zz_cmp(&u, 4) != ZZ_EQ) {
        abort();
    }
    if (zz_pow(&u, 0, &u) || zz_cmp(&u, 1) != ZZ_EQ) {
        abort();
    }
    if (zz_pow(&u, 123, &u) || zz_cmp(&u, 1) != ZZ_EQ) {
        abort();
    }
    if (zz_set(0, &u) || zz_pow(&u, 123, &u) || zz_cmp(&u, 0) != ZZ_EQ)
    {
        abort();
    }
    zz_clear(&u);
}

int main(void)
{
    srand((unsigned int)time(NULL));
    zz_testinit();
    zz_setup();
    check_powm_bulk();
    check_powm_examples();
    check_pow_bulk();
    check_pow_examples();
    zz_finish();
    zz_testclear();
    return 0;
}
