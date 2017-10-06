/*
 * Utility functions shared by the application
 * Copyright (c) 2005-2009, Jouni Malinen <j@w1.fi>
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

#ifndef UTIL_H
#define UTIL_H


#ifdef __cplusplus
extern "C" {
#endif


// -----------------------------------  OS includes --------------------------------------------------
#include <stdlib.h>
#include <stddef.h>
#include <stdint.h>


// -----------------------------------  Types and macros --------------------------------------------------


#ifndef BIT
#define BIT(x) (1U << (x))
#endif


static inline uint16_t GET_BE16(uint8_t *buff)
{
    uint16_t val = ((uint16_t)buff[0]) << 8U;
    val += buff[1] & 0xFFU;
    return val;
}

static inline void SET_BE16(uint8_t *buff, uint16_t size)
{
    buff[0] = (size >> 8U) & 0xFFU;
    buff[1] = size & 0xFFU;
}

static inline uint8_t is_bit_set(uint8_t value, uint8_t bit)
{
    return ((value & BIT(bit)) == 0U) ? 0U : 1U;
}

static inline uint32_t GET_BE32(const uint8_t *a)
{
    return ((uint32_t) a[0] << 24) | (a[1] << 16) | (a[2] << 8) | a[3];
}

static inline void PUT_BE32(uint8_t *a, uint32_t val)
{
    a[0] = (val >> 24) & 0xff;
    a[1] = (val >> 16) & 0xff;
    a[2] = (val >> 8) & 0xff;
    a[3] = val & 0xff;
}

static inline void PUT_BE64(uint8_t *a, uint64_t val)
{
	a[0] = val >> 56;
	a[1] = val >> 48;
	a[2] = val >> 40;
	a[3] = val >> 32;
	a[4] = val >> 24;
	a[5] = val >> 16;
	a[6] = val >> 8;
	a[7] = val & 0xff;
}

static inline uint32_t div_round_up(uint32_t x, uint32_t y)
{
    return  (x + y - 1U) / y;
}

// -----------------------------------  Prototypes --------------------------------------------------

/**
 * os_memcmp_const - Constant time memory comparison
 * @a: First buffer to compare
 * @b: Second buffer to compare
 * @len: Number of octets to compare
 * Returns: 0 if buffers are equal, non-zero if not
 *
 * This function is meant for comparing passwords or hash values where
 * difference in execution time could provide external observer information
 * about the location of the difference in the memory buffers. The return value
 * does not behave like os_memcmp(), i.e., os_memcmp_const() cannot be used to
 * sort items into a defined order. Unlike os_memcmp(), execution time of
 * os_memcmp_const() does not depend on the contents of the compared memory
 * buffers, but only on the total compared length.
 */
int memcmp_const(const void *a, const void *b, size_t len);


void hex2bin(const char *in, uint8_t* out, size_t size);

void print_hex(const char *buf, int size);

#ifdef __cplusplus
}
#endif


#endif /* OS_H */
