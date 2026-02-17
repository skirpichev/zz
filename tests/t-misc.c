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

void
check_cmp(void)
{
    zz_t u;

    if (zz_init(&u) || zz_set(13, &u)) {
        abort();
    }
    if (zz_cmp(&u, 1) != ZZ_GT || zz_cmp(&u, 100) != ZZ_LT) {
        abort();
    }
    if (zz_cmp(&u, -100) != ZZ_GT) {
        abort();
    }
    if (zz_set(13, &u) || zz_cmp(&u, &u) != ZZ_EQ) {
        abort();
    }
    zz_clear(&u);
}

void
check_cmp_bulk(void)
{
    zz_bitcnt_t bs = 512;

    for (size_t i = 0; i < nsamples; i++) {
        zz_t u, v;

        if (zz_init(&u) || zz_random(bs, true, &u)) {
            abort();
        }
        if (zz_init(&v) || zz_random(bs, true, &v)) {
            abort();
        }

        TMP_MPZ(mu, &u);
        TMP_MPZ(mv, &v);
        if (zz_cmp(&u, &v) != mpz_cmp(mu, mv)) {
            abort();
        }
        zz_clear(&u);
        zz_clear(&v);
    }
}

void
check_lsbpos(void)
{
    zz_t u;

    if (zz_init(&u) || zz_set(0, &u)) {
        abort();
    }
    if (zz_lsbpos(&u) != 0) {
        abort();
    }
    zz_clear(&u);
}

void
check_bitcnt(void)
{
    zz_t u;

    if (zz_init(&u) || zz_set(0, &u)) {
        abort();
    }
    if (zz_bitcnt(&u) != 0) {
        abort();
    }
    if (zz_set(3, &u) || zz_bitcnt(&u) != 2) {
        abort();
    }
    zz_clear(&u);
}

zz_err
zz_ref_sqrtrem(const zz_t *u, zz_t *v, zz_t *w)
{
    mpz_t z, r;
    TMP_MPZ(mu, u);
    if (TMP_OVERFLOW) {
        return ZZ_MEM;
    }
    mpz_init(z);
    mpz_init(r);
    mpz_sqrtrem(z, r, mu);

    zz_t tmp = {z->_mp_size < 0, abs(z->_mp_size), abs(z->_mp_size), z->_mp_d};

    if (zz_pos(&tmp, v)) {
        mpz_clear(z);
        mpz_clear(r);
        return ZZ_MEM;
    }
    mpz_clear(z);
    tmp = (zz_t){r->_mp_size < 0, abs(r->_mp_size), abs(r->_mp_size), r->_mp_d};
    if (zz_pos(&tmp, w)) {
        mpz_clear(r);
        return ZZ_MEM;
    }
    mpz_clear(r);
    return ZZ_OK;
}

void
check_sqrtrem_bulk(void)
{
    zz_bitcnt_t bs = 512;

    for (size_t i = 0; i < nsamples; i++) {
        zz_t u, v, w, rv, rw;

        if (zz_init(&u) || zz_random(bs, false, &u)) {
            abort();
        }
        if (zz_init(&v) || zz_init(&w) || zz_init(&rv) || zz_init(&rw)) {
            abort();
        }
        if (zz_sqrtrem(&u, &v, &w) || zz_ref_sqrtrem(&u, &rv, &rw)
            || zz_cmp(&v, &rv) != ZZ_EQ || zz_cmp(&w, &rw) != ZZ_EQ)
        {
            abort();
        }
        if (zz_pos(&u, &v) || zz_sqrtrem(&v, &v, &w)
            || zz_cmp(&v, &rv) != ZZ_EQ || zz_cmp(&w, &rw) != ZZ_EQ)
        {
            abort();
        }
        if (zz_pos(&u, &w) || zz_sqrtrem(&w, &v, &w)
            || zz_cmp(&v, &rv) != ZZ_EQ || zz_cmp(&w, &rw) != ZZ_EQ)
        {
            abort();
        }
        zz_clear(&u);
        zz_clear(&v);
        zz_clear(&w);
        zz_clear(&rv);
        zz_clear(&rw);
    }
}

void
check_sqrtrem_examples(void)
{
    zz_t u, v;

    if (zz_init(&u) || zz_set(4, &u)) {
        abort();
    }
    if (zz_init(&v) || zz_set(0, &v)) {
        abort();
    }
    if (zz_sqrtrem(&u, &u, &v) || zz_cmp(&u, 2) != ZZ_EQ
        || zz_cmp(&v, 0) != ZZ_EQ)
    {
        abort();
    }
    if (zz_sqrtrem(&v, &v, &u) || zz_cmp(&u, 0) != ZZ_EQ) {
        abort();
    }
    if (zz_set(-1, &u) || zz_sqrtrem(&u, &v, NULL) != ZZ_VAL) {
        abort();
    }
    zz_clear(&u);
    zz_clear(&v);
}

void
check_bin(void)
{
    zz_t u;

    if (zz_init(&u) || zz_bin(13, 5, &u) || zz_cmp(&u, 1287) != ZZ_EQ) {
        abort();
    }
    zz_clear(&u);
}

void
check_isodd_bulk(void)
{
    zz_bitcnt_t bs = 512;

    for (size_t i = 0; i < nsamples; i++) {
        zz_t u;

        if (zz_init(&u) || zz_random(bs, true, &u)) {
            abort();
        }

        TMP_MPZ(mu, &u);
        if (zz_isodd(&u) != mpz_odd_p(mu)) {
            abort();
        }
        zz_clear(&u);
    }
}

void
check_isneg(void)
{
    zz_t u;

    if (zz_init(&u) || zz_set(-3, &u) || !zz_isneg(&u)) {
        abort();
    }
    zz_clear(&u);
}

zz_err
zz_ref_gcdext(const zz_t *u, const zz_t *v, zz_t *g, zz_t *s, zz_t *t)
{
    mpz_t zg, zs, zt;
    TMP_MPZ(mu, u);
    TMP_MPZ(mv, v);
    if (TMP_OVERFLOW) {
        return ZZ_MEM;
    }
    mpz_init(zg);
    mpz_init(zs);
    mpz_init(zt);
    mpz_gcdext(zg, zs, zt, mu, mv);

    zz_t tmp = {zg->_mp_size < 0, abs(zg->_mp_size), abs(zg->_mp_size),
                zg->_mp_d};

    if (zz_pos(&tmp, g)) {
        mpz_clear(zg);
        mpz_clear(zs);
        mpz_clear(zt);
        return ZZ_MEM;
    }
    mpz_clear(zg);
    tmp = (zz_t){zs->_mp_size < 0, abs(zs->_mp_size), abs(zs->_mp_size),
                 zs->_mp_d};
    if (zz_pos(&tmp, s)) {
        mpz_clear(zs);
        mpz_clear(zt);
        return ZZ_MEM;
    }
    mpz_clear(zs);
    tmp = (zz_t){zt->_mp_size < 0, abs(zt->_mp_size), abs(zt->_mp_size),
                 zt->_mp_d};
    if (zz_pos(&tmp, t)) {
        mpz_clear(zt);
        return ZZ_MEM;
    }
    mpz_clear(zt);
    return ZZ_OK;
}

void
check_gcdext_bulk(void)
{
    zz_bitcnt_t bs = 512;

    for (size_t i = 0; i < nsamples; i++) {
        zz_t u, v, g, s, t, rg, rs, rt;

        if (zz_init(&u) || zz_random(bs, true, &u)) {
            abort();
        }
        if (zz_init(&v) || zz_random(bs, true, &v)) {
            abort();
        }
        if (zz_init(&g) || zz_init(&s) || zz_init(&t)
            || zz_init(&rg) || zz_init(&rs) || zz_init(&rt))
        {
            abort();
        }
        if (rand() % 2) {
            zz_t c;

            if (zz_init(&c) || zz_random(bs, true, &c)
                || zz_mul(&c, &u, &u) || zz_mul(&c, &v, &v))
            {
                abort();
            }
            zz_clear(&c);
        }
        if (zz_gcdext(&u, &v, &g, &s, &t)
            || zz_ref_gcdext(&u, &v, &rg, &rs, &rt)
            || zz_cmp(&g, &rg) != ZZ_EQ || zz_cmp(&s, &rs) != ZZ_EQ
            || zz_cmp(&t, &rt) != ZZ_EQ)
        {
            abort();
        }
        if (zz_gcdext(&u, &v, &g, NULL, NULL) || zz_cmp(&g, &rg) != ZZ_EQ) {
            abort();
        }
        if (zz_pos(&u, &g) || zz_gcdext(&g, &v, &g, &s, &t)
            || zz_cmp(&g, &rg) != ZZ_EQ || zz_cmp(&s, &rs) != ZZ_EQ
            || zz_cmp(&t, &rt) != ZZ_EQ)
        {
            abort();
        }
        if (zz_pos(&u, &s) || zz_gcdext(&s, &v, &g, &s, &t)
            || zz_cmp(&g, &rg) != ZZ_EQ || zz_cmp(&s, &rs) != ZZ_EQ
            || zz_cmp(&t, &rt) != ZZ_EQ)
        {
            abort();
        }
        if (zz_pos(&u, &t) || zz_gcdext(&t, &v, &g, &s, &t)
            || zz_cmp(&g, &rg) != ZZ_EQ || zz_cmp(&s, &rs) != ZZ_EQ
            || zz_cmp(&t, &rt) != ZZ_EQ)
        {
            abort();
        }
        if (zz_pos(&v, &g) || zz_gcdext(&u, &g, &g, &s, &t)
            || zz_cmp(&g, &rg) != ZZ_EQ || zz_cmp(&s, &rs) != ZZ_EQ
            || zz_cmp(&t, &rt) != ZZ_EQ)
        {
            abort();
        }
        if (zz_pos(&v, &s) || zz_gcdext(&u, &s, &g, &s, &t)
            || zz_cmp(&g, &rg) != ZZ_EQ || zz_cmp(&s, &rs) != ZZ_EQ
            || zz_cmp(&t, &rt) != ZZ_EQ)
        {
            abort();
        }
        if (zz_pos(&v, &t) || zz_gcdext(&u, &t, &g, &s, &t)
            || zz_cmp(&g, &rg) != ZZ_EQ || zz_cmp(&s, &rs) != ZZ_EQ
            || zz_cmp(&t, &rt) != ZZ_EQ)
        {
            abort();
        }
        zz_clear(&u);
        zz_clear(&v);
        zz_clear(&g);
        zz_clear(&s);
        zz_clear(&t);
        zz_clear(&rg);
        zz_clear(&rs);
        zz_clear(&rt);
    }
}

void
check_gcdext_examples(void)
{
    zz_t u, v, a, b;

    if (zz_init(&u) || zz_init(&v) || zz_set(-2, &u)
        || zz_set(6, &v))
    {
        abort();
    }
    if (zz_init(&a) || zz_gcdext(&u, &v, &a, NULL, NULL)
        || zz_cmp(&a, 2) != ZZ_EQ)
    {
        abort();
    }
    if (zz_gcdext(&u, &v, NULL, &a, NULL) || zz_cmp(&a, -1) != ZZ_EQ) {
        abort();
    }
    if (zz_gcdext(&u, &v, NULL, NULL, &a) || zz_cmp(&a, 0) != ZZ_EQ) {
        abort();
    }
    if (zz_set(0, &u) || zz_gcdext(&u, &v, &a, NULL, NULL)
        || zz_cmp(&a, 6) != ZZ_EQ)
    {
        abort();
    }
    if (zz_gcdext(&u, &v, NULL, &a, NULL) || zz_cmp(&a, 0) != ZZ_EQ) {
        abort();
    }
    if (zz_gcdext(&u, &v, NULL, NULL, &a) || zz_cmp(&a, 1) != ZZ_EQ) {
        abort();
    }
    if (zz_init(&b) || zz_gcdext(&u, &v, &a, &b, NULL)
        || zz_cmp(&b, 0) != ZZ_EQ)
    {
        abort();
    }
    if (zz_gcdext(&u, &v, NULL, NULL, NULL) != ZZ_OK) {
        abort();
    }
    zz_clear(&u);
    zz_clear(&v);
    zz_clear(&a);
    zz_clear(&b);
}

zz_err
zz_ref_invert(const zz_t *u, zz_t *v, zz_t *w)
{
    mpz_t z, g;
    TMP_MPZ(mu, u);
    TMP_MPZ(mv, v);
    if (TMP_OVERFLOW) {
        return ZZ_MEM;
    }
    mpz_init(z);
    mpz_init(g);
    if (v->size < u->size) {
        mpz_gcdext(g, z, NULL, mu, mv);
    }
    else {
        mpz_gcdext(g, NULL, z, mv, mu);
    }
    if (mpz_cmp_ui(g, 1) != 0) {
        mpz_clear(z);
        mpz_clear(g);
        return ZZ_VAL;
    }
    mpz_clear(g);

    zz_t tmp = {z->_mp_size < 0, abs(z->_mp_size), abs(z->_mp_size), z->_mp_d};

    if (zz_pos(&tmp, w)) {
        mpz_clear(z);
        return ZZ_MEM;
    }
    mpz_clear(z);
    return ZZ_OK;
}

void
check_invert_euclidext_bulk(void)
{
    zz_bitcnt_t bs = 512;

    for (size_t i = 0; i < nsamples; i++) {
        zz_t u, v, w, rw, rg;

        if (zz_init(&u) || zz_random(bs, true, &u)) {
            abort();
        }
        if (zz_init(&v) || zz_random(bs, true, &v)) {
            abort();
        }
        if (zz_init(&w) || zz_init(&rw) || zz_init(&rg)) {
            abort();
        }
        if (rand() % 2) {
            zz_t c;

            if (zz_init(&c) || zz_random(bs, true, &c)
                || zz_mul(&c, &u, &u) || zz_mul(&c, &v, &v))
            {
                abort();
            }
            zz_clear(&c);
        }
        if (zz_ref_gcdext(&u, &v, &rg, &w, &rw)) {
            abort();
        }
        if (zz_cmp(&rg, 1) != ZZ_EQ && zz_ref_invert(&u, &v, &rw) != ZZ_VAL) {
            abort();
        }
        if (zz_cmp(&rg, 1) != ZZ_EQ
            && zz_inverse_euclidext(&u, &v, &w) != ZZ_VAL)
        {
            abort();
        }
        if (zz_cmp(&rg, 1) != ZZ_EQ
            && (zz_div(&u, &rg, &u, NULL) || zz_div(&v, &rg, &v, NULL)))
        {
            abort();
        }
        if (zz_ref_invert(&u, &v, &rw)
            || zz_inverse_euclidext(&u, &v, &w) || zz_cmp(&w, &rw) != ZZ_EQ)
        {
            abort();
        }
        zz_clear(&u);
        zz_clear(&v);
        zz_clear(&w);
        zz_clear(&rw);
        zz_clear(&rg);
    }
}

void
check_fromto_double(void)
{
    zz_t u;
    double d;

    if (zz_init(&u) || zz_set(INFINITY, &u) != ZZ_BUF) {
        abort();
    }
    if (zz_init(&u) || zz_set(NAN, &u) != ZZ_VAL) {
        abort();
    }
    if (zz_set(1092.2666666666667, &u) || zz_cmp(&u, 1092) != ZZ_EQ) {
        abort();
    }
    if (zz_set(1, &u) || zz_mul_2exp(&u, 2000, &u)) {
        abort();
    }
    if (zz_get(&u, &d) != ZZ_BUF) {
        abort();
    }
    if (zz_set(9007199254740993, &u) || zz_get(&u, &d)
        || d != 9007199254740992.0)
    {
        abort();
    }
    if (zz_set(18014398509481987, &u) || zz_get(&u, &d)
        || d != 1.8014398509481988e+16)
    {
        abort();
    }
    if (zz_set(1, &u) || zz_mul_2exp(&u, 1024, &u)
        || zz_get(&u, &d) != ZZ_BUF)
    {
        abort();
    }
    zz_clear(&u);
}

void
check_sizeinbase(void)
{
    zz_t u;

    if (zz_init(&u) || zz_set(1, &u)
        || zz_sizeinbase(&u, 42, NULL) != ZZ_VAL)
    {
        abort();
    }
    zz_clear(&u);
}

void
check_fromto_i32(void)
{
    zz_t u;
    int32_t v = 123, val;
    uint32_t uval;

    if (zz_init(&u) || zz_set(v, &u)) {
        abort();
    }
    if (zz_get(&u, &val) || val != v) {
        abort();
    }
    v = -42;
    if (zz_set(v, &u)) {
        abort();
    }
    if (zz_get(&u, &val) || val != v) {
        abort();
    }
    v = 0;
    if (zz_set(v, &u)) {
        abort();
    }
    if (zz_get(&u, &val) || val != v) {
        abort();
    }
    if (zz_set(1LL<<33, &u)) {
        abort();
    }
    if (zz_get(&u, &val) != ZZ_BUF) {
        abort();
    }
    if (zz_set(-(1LL<<33), &u)) {
        abort();
    }
    if (zz_get(&u, &val) != ZZ_BUF) {
        abort();
    }
    if (zz_set(1, &u) || zz_mul_2exp(&u, 33, &u)
        || zz_get(&u, &val) != ZZ_BUF)
    {
        abort();
    }
    if (zz_set(1, &u) || zz_mul_2exp(&u, 64, &u)
        || zz_get(&u, &val) != ZZ_BUF)
    {
        abort();
    }
    if (zz_set(1U, &u) || zz_cmp(&u, 1) != ZZ_EQ) {
        abort();
    }
    if (zz_get(&u, &uval) || uval != 1) {
        abort();
    }
    if (zz_set(0U, &u) || zz_cmp(&u, 0) != ZZ_EQ) {
        abort();
    }
    if (zz_get(&u, &uval) || uval != 0) {
        abort();
    }
    if (zz_set(1, &u) || zz_mul_2exp(&u, 33, &u)
        || zz_get(&u, &uval) != ZZ_BUF)
    {
        abort();
    }
    if (zz_set(-1, &u) || zz_get(&u, &uval) != ZZ_VAL) {
        abort();
    }
    zz_clear(&u);
}

void
check_fromto_i64(void)
{
    zz_t u;
    int64_t val;
    uint64_t uval;

    if (zz_init(&u) || zz_set(0, &u)) {
        abort();
    }
    if (zz_get(&u, &val) || val) {
        abort();
    }
    if (zz_set(1ULL, &u) || zz_cmp(&u, 1) != ZZ_EQ) {
        abort();
    }
    if (zz_get(&u, &uval) || uval != 1) {
        abort();
    }
    if (zz_set(1, &u) || zz_mul_2exp(&u, 65, &u)
        || zz_get(&u, &uval) != ZZ_BUF)
    {
        abort();
    }
    if (zz_set(-1, &u) || zz_get(&u, &uval) != ZZ_VAL) {
        abort();
    }
    zz_clear(&u);
}

void
check_exportimport_roundtrip(void)
{
    zz_bitcnt_t bs = 512;
    const zz_layout bytes_layout = {8, 1, 1, 0};
    const zz_layout pyint_layout = {30, 4, -1, 0};
    const zz_layout *native_layout = zz_get_layout();
    size_t len;
    void *buf;

    for (size_t i = 0; i < nsamples; i++) {
        zz_t u, v;

        if (zz_init(&u) || zz_random(bs, false, &u)) {
            abort();
        }
        len = (zz_bitlen(&u) + 7)/8;
        buf = malloc(len);
        if (!buf || zz_export(&u, bytes_layout, len, buf)) {
            abort();
        }
        if (zz_init(&v) || zz_import(len, buf, bytes_layout, &v)) {
            abort();
        }
        free(buf);
        if (zz_cmp(&u, &v) != ZZ_EQ) {
            abort();
        }
        len = (zz_bitlen(&u) + 29)/30;
        buf = malloc(len*4);
        if (!buf || zz_export(&u, pyint_layout, len, buf)) {
            abort();
        }
        if (zz_import(len, buf, pyint_layout, &v)) {
            abort();
        }
        free(buf);
        if (zz_cmp(&u, &v) != ZZ_EQ) {
            abort();
        }
        len = (zz_bitlen(&u) + native_layout->bits_per_digit
               - 1)/native_layout->bits_per_digit;
        buf = malloc(len*native_layout->digit_size);
        if (!buf || zz_export(&u, *native_layout, len, buf)) {
            abort();
        }
        if (zz_import(len, buf, *native_layout, &v)) {
            abort();
        }
        free(buf);
        if (zz_cmp(&u, &v) != ZZ_EQ) {
            abort();
        }
        zz_clear(&u);
        zz_clear(&v);
    }
}

void
check_exportimport_examples(void)
{
    zz_t u;
    const zz_layout pyint_layout = {30, 4, -1, 0};

    if (zz_init(&u) || zz_set(123, &u)) {
        abort();
    }
    if (zz_export(&u, pyint_layout, 0, 0) != ZZ_BUF) {
        abort();
    }
    zz_clear(&u);
}

void
check_fac_outofmem(void)
{
    zz_set_memory_funcs(my_malloc, my_realloc, my_free);
    max_size = 16*1000*1000;
    if (total_size) {
        abort();
    }
    for (size_t i = 0; i < 7; i++) {
        uint64_t x = 12811 + (uint64_t)(rand() % 12173);
        zz_t mx;

        if (zz_init(&mx)) {
            abort();
        }
        while (1) {
            zz_err r = zz_fac(x, &mx);

            if (r != ZZ_OK) {
                if (r == ZZ_MEM) {
                    break;
                }
                abort();
            }
            x *= 2;
        }
        zz_clear(&mx);
        if (zz_get_alloc_state()) {
            abort();
        }
        total_size = 0;
    }
    zz_set_memory_funcs(NULL, NULL, NULL);
}

int main(void)
{
    zz_testinit();
    zz_setup();
    if (strcmp(zz_get_version(), VERSION)) {
        abort();
    }
    if (zz_get_bitcnt_max() != ZZ_BITS_MAX) {
        abort();
    }
    check_cmp();
    check_cmp_bulk();
    check_lsbpos();
    check_bitcnt();
    check_sqrtrem_bulk();
    check_sqrtrem_examples();
    check_bin();
    check_isodd_bulk();
    check_isneg();
    check_gcdext_bulk();
    check_gcdext_examples();
    check_invert_euclidext_bulk();
    check_fromto_double();
    check_sizeinbase();
    check_fromto_i32();
    check_fromto_i64();
    check_exportimport_roundtrip();
    check_exportimport_examples();
#ifdef HAVE_SYS_RESOURCE_H
    struct rlimit new, old;

    /* to trigger crash for GMP builds with alloca() enabled */
    if (getrlimit(RLIMIT_STACK, &old)) {
        perror("getrlimit");
        return 1;
    }
    new.rlim_max = old.rlim_max;
    new.rlim_cur = 128*1000;
    if (setrlimit(RLIMIT_STACK, &new)) {
        perror("setrlimit");
        return 1;
    }
    check_fac_outofmem();
#endif
    zz_finish();
    zz_testclear();
    return 0;
}
