#ifndef MY_MATH_H_
#define MY_MATH_H_

#include <stdint.h>
#include <stdlib.h>

#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))

uint32_t digits(uint32_t n);

#endif // MY_MATH_H_