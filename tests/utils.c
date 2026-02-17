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

static gmp_randstate_t rnd_state;
int nsamples;

void
zz_testinit(void)
{
    gmp_randinit_default(rnd_state);

    char *val = getenv("NSAMPLES");
    const int nsamples_default = 10000;

    if (val) {
        nsamples = atoi(val);
        if (nsamples <= 0) {
            nsamples = nsamples_default;
        }
    }
    else {
        nsamples = nsamples_default;
    }
}

zz_err
zz_random(zz_bitcnt_t bc, bool s, zz_t *u)
{
    mpz_t z;

    if (TMP_OVERFLOW) {
        return ZZ_MEM;
    }
    mpz_init(z);

    int n = (rand() % 10);
    void (*f)(mpz_t, gmp_randstate_t, mp_bitcnt_t);

    f = rand() % 2 ? mpz_urandomb : mpz_rrandomb;
    if (n >= 7) {
        f(z, rnd_state, (mp_bitcnt_t)bc);
    }
    else if (n >= 5) {
        f(z, rnd_state, (mp_bitcnt_t)bc/4);
    }
    else {
        f(z, rnd_state, (mp_bitcnt_t)bc/8);
    }
    if (zz_set_mpz_t(z, u)) {
        mpz_clear(z);
        return ZZ_MEM;
    }
    mpz_clear(z);
    if (s && rand() % 2) {
        zz_neg(u, u);
    }
    return ZZ_OK;
}

void
zz_testclear(void)
{
    gmp_randclear(rnd_state);
}

_Thread_local size_t total_size = 0;
_Thread_local size_t max_size = 0;

void *
my_malloc(size_t size)
{
    if (total_size + size > max_size) {
        return NULL;
    }

    void *ptr = malloc(size);

    if (ptr) {
        total_size += size;
    }
    return ptr;
}

void *
my_realloc(void *ptr, size_t old_size, size_t new_size)
{
    if (total_size + new_size - old_size > max_size) {
        return NULL;
    }

    void *new_ptr = realloc(ptr, new_size);

    if (new_ptr) {
        if (old_size > new_size) {
            total_size -= old_size - new_size;
        }
        else {
            total_size += new_size - old_size;
        }
    }
    return new_ptr;
}

void
my_free(void *ptr, size_t size)
{
    free(ptr);
    total_size -= size;
}

void *
square_worker(void *args)
{
    int *d = (int *)args;
    zz_t z;

    if (total_size || zz_init(&z) || zz_set(*d, &z)) {
        *d = 1;
        return NULL;
    }
    while (1) {
        zz_err ret = zz_mul(&z, &z, &z);

        if (ret != ZZ_OK) {
            if (ret == ZZ_MEM) {
                break;
            }
            *d = 1;
            return NULL;
        }
    }
    zz_clear(&z);
    *d = zz_get_alloc_state() != 0;
    total_size = 0;
    return NULL;
}
