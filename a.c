#if HAVE_LIMB_BIG_ENDIAN
#define HOST_ENDIAN     1
#endif
#if HAVE_LIMB_LITTLE_ENDIAN
#define HOST_ENDIAN     (-1)
#endif
#ifndef HOST_ENDIAN
static const mp_limb_t  endian_test = (CNST_LIMB(1) << (GMP_LIMB_BITS-7)) - 1;
#define HOST_ENDIAN     (* (signed char *) &endian_test)
#endif

void *
mpn_export(void *data, size_t *countp, int order,
           size_t size, int endian, size_t nail, mpz_srcptr z)
{
    mp_size_t zsize;
    mp_srcptr zp;
    size_t count, dummy;
    unsigned long numb;
    unsigned align;

    ASSERT (order == 1 || order == -1);
    ASSERT (endian == 1 || endian == 0 || endian == -1);
    ASSERT (nail <= 8*size);
    ASSERT (nail <  8*size || SIZ(z) == 0); /* nail < 8*size+(SIZ(z)==0) */

    if (countp == NULL) {
        countp = &dummy;
    }
    zsize = SIZ(z);
    if (zsize == 0) {
        *countp = 0;
        return data;
    }
    zsize = ABS (zsize);
    zp = PTR(z);
    numb = 8*size - nail;
    MPN_SIZEINBASE_2EXP (count, zp, zsize, numb);
    *countp = count;
    if (data == NULL) {
        data = (*__gmp_allocate_func) (count*size);
    }
    if (endian == 0) {
        endian = HOST_ENDIAN;
    }
    align = ((char *) data - (char *) NULL) % sizeof (mp_limb_t);
    if (nail == GMP_NAIL_BITS) {
        if (size == sizeof (mp_limb_t) && align == 0) {
            if (order == -1 && endian == HOST_ENDIAN) {
                MPN_COPY ((mp_ptr) data, zp, (mp_size_t) count);
                return data;
            }
            if (order == 1 && endian == HOST_ENDIAN) {
                MPN_REVERSE ((mp_ptr) data, zp, (mp_size_t) count);
                return data;
            }
            if (order == -1 && endian == -HOST_ENDIAN) {
                MPN_BSWAP ((mp_ptr) data, zp, (mp_size_t) count);
                return data;
            }
            if (order == 1 && endian == -HOST_ENDIAN) {
                MPN_BSWAP_REVERSE ((mp_ptr) data, zp, (mp_size_t) count);
                return data;
            }
        }
    }
    {
        mp_limb_t limb, wbitsmask;
        size_t i, numb;
        mp_size_t j, wbytes, woffset;
        unsigned char *dp;
        int lbits, wbits;
        mp_srcptr zend;

        numb = size * 8 - nail;
        /* whole bytes per word */
        wbytes = numb / 8;
        /* possible partial byte */
        wbits = numb % 8;
        wbitsmask = (CNST_LIMB(1) << wbits) - 1;
        /* offset to get to the next word */
        woffset = (endian >= 0 ? size : - (mp_size_t) size)
                   + (order < 0 ? size : - (mp_size_t) size);
        /* least significant byte */
        dp = (unsigned char *) data + ((order >= 0 ? (count-1)*size : 0)
                                       + (endian >= 0 ? size-1 : 0));

#define EXTRACT(N, MASK)                                      \
        do {                                                  \
            if (lbits >= (N)) {                               \
                *dp = limb MASK;                              \
                limb >>= (N);                                 \
                lbits -= (N);                                 \
            }                                                 \
            else {                                            \
                mp_limb_t newlimb = (zp == zend ? 0 : *zp++); \
                *dp = (limb | (newlimb << lbits)) MASK;       \
                limb = newlimb >> ((N)-lbits);                \
                lbits += GMP_NUMB_BITS - (N);                 \
            }                                                 \
        } while (0)
    
        zend = zp + zsize;
        lbits = 0;
        limb = 0;
        for (i = 0; i < count; i++) {
            for (j = 0; j < wbytes; j++) {
                EXTRACT (8, + 0);
                dp -= endian;
            }
            if (wbits != 0) {
                EXTRACT (wbits, & wbitsmask);
                dp -= endian;
                j++;
            }
            for ( ; j < size; j++) {
                *dp = '\0';
                dp -= endian;
            }
            dp += woffset;
        }
        ASSERT (zp == PTR(z) + ABSIZ(z));
        /* low byte of word after most significant */
        ASSERT (dp == (unsigned char *) data
	            + (order < 0 ? count*size : - (mp_size_t) size)
	            + (endian >= 0 ? (mp_size_t) size - 1 : 0));
    }
    return data;
}

void
mpn_import (mpz_ptr z, size_t count, int order,
            size_t size, int endian, size_t nail, const void *data)
{
    mp_size_t zsize;
    mp_ptr zp;

    ASSERT (order == 1 || order == -1);
    ASSERT (endian == 1 || endian == 0 || endian == -1);
    ASSERT (nail <= 8*size);

    zsize = BITS_TO_LIMBS (count * (8*size - nail));
    zp = MPZ_NEWALLOC (z, zsize);

    if (endian == 0) {
        endian = HOST_ENDIAN;
    }

    /* Can't use these special cases with nails currently, since they don't
       mask out the nail bits in the input data.  */
    if (nail == 0 && GMP_NAIL_BITS == 0
        && size == sizeof (mp_limb_t)
        && (((char *) data
             - (char *) NULL) % sizeof (mp_limb_t)) == 0 /* align */)
    {
        if (order == -1) {
            if (endian == HOST_ENDIAN) {
                MPN_COPY (zp, (mp_srcptr) data, (mp_size_t) count);
            }
            else /* if (endian == - HOST_ENDIAN) */ {
                MPN_BSWAP (zp, (mp_srcptr) data, (mp_size_t) count);
            }
        } else /* if (order == 1) */ {
            if (endian == HOST_ENDIAN) {
                MPN_REVERSE (zp, (mp_srcptr) data, (mp_size_t) count);
            }
            else /* if (endian == - HOST_ENDIAN) */ {
                MPN_BSWAP_REVERSE (zp, (mp_srcptr) data, (mp_size_t) count);
            }
        }
    }
    else {
        mp_limb_t limb, byte, wbitsmask;
        size_t i, j, numb, wbytes;
        mp_size_t woffset;
        unsigned char *dp;
        int lbits, wbits;

        numb = size * 8 - nail;
        /* whole bytes to process */
        wbytes = numb / 8;
        /* partial byte to process */
        wbits = numb % 8;
        wbitsmask = (CNST_LIMB(1) << wbits) - 1;
        /* offset to get to the next word after processing wbytes and wbits */
        woffset = (numb + 7) / 8;
        woffset = ((endian >= 0 ? woffset : -woffset)
                   + (order < 0 ? size : - (mp_size_t) size));

        /* least significant byte */
        dp = ((unsigned char *) data
              + (order >= 0 ? (count-1)*size : 0) + (endian >= 0 ? size-1 : 0));

#define ACCUMULATE(N)                                     \
        do {                                              \
            ASSERT (lbits < GMP_NUMB_BITS);               \
            ASSERT (limb <= (CNST_LIMB(1) << lbits) - 1); \
                                                          \
            limb |= (mp_limb_t) byte << lbits;            \
            lbits += (N);                                 \
            if (lbits >= GMP_NUMB_BITS) {                 \
                *zp++ = limb & GMP_NUMB_MASK;             \
                lbits -= GMP_NUMB_BITS;                   \
                ASSERT (lbits < (N));                     \
                limb = byte >> ((N) - lbits);             \
            }                                             \
        } while (0)

        limb = 0;
        lbits = 0;
        for (i = 0; i < count; i++) {
            for (j = 0; j < wbytes; j++) {
                byte = *dp;
                dp -= endian;
                ACCUMULATE (8);
            }
            if (wbits != 0) {
                byte = *dp & wbitsmask;
                dp -= endian;
                ACCUMULATE (wbits);
            }
            dp += woffset;
        }
        if (lbits != 0) {
            ASSERT (lbits <= GMP_NUMB_BITS);
            ASSERT_LIMB (limb);
            *zp++ = limb;
        }
        ASSERT (zp == PTR(z) + zsize);
        /* low byte of word after most significant */
        ASSERT (dp == (unsigned char *) data
                + (order < 0 ? count*size : - (mp_size_t) size)
                + (endian >= 0 ? (mp_size_t) size - 1 : 0));
    }
    zp = PTR(z);
    MPN_NORMALIZE (zp, zsize);
    SIZ(z) = zsize;
}
