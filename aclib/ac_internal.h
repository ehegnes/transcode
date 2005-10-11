/*
 * ac_internal.h -- internal include file for aclib functions
 * Written by Andrew Church <achurch@achurch.org>
 */

#ifndef ACLIB_AC_INTERNAL_H
#define ACLIB_AC_INTERNAL_H


/* Compiler hint that a condition is unlikely */
#ifdef __GNUC__
# define UNLIKELY(x) (__builtin_expect(!!(x), 0))
#else
# define UNLIKELY(x) (x)
#endif

/* Are _all_ of the given acceleration flags (`test') available? */
#define HAS_ACCEL(accel,test) (((accel) & (test)) == (test))

/* Initialization subfunctions */
extern int ac_average_init(int accel);
extern int ac_imgconvert_init(int accel);
extern int ac_memcpy_init(int accel);
extern int ac_rescale_init(int accel);


#endif  /* ACLIB_AC_INTERNAL_H */
