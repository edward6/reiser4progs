/*
  math.h -- some math functions which are needed by libaal.
  
  Copyright (C) 2001, 2002 by Hans Reiser, licensing governed by
  reiser4progs/COPYING.
*/

#ifndef AAL_MATH_H
#define AAL_MATH_H

extern int aal_log2(unsigned long n);
extern int aal_pow_of_two(unsigned long n);

extern long long int aal_fact(int64_t n);
extern unsigned int aal_adler32(char *buff, unsigned int n);

#endif

