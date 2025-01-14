#ifndef FDT_H_
#define FDT_H_

/* Unsigned.  */
typedef unsigned char               uint8_t;
typedef unsigned short int          uint16_t;
#ifndef __uint32_t_defined
typedef unsigned int                uint32_t;
# define __uint32_t_defined
#endif
#if __WORDSIZE == 64
typedef unsigned long int           uint64_t;
#else
__extension__
typedef unsigned long long int      uint64_t;
#endif

#endif