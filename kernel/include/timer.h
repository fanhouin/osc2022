#ifndef	_TIMER_H
#define	_TIMER_H

#include <gpio.h>

#define TIMER_CS        (MMIO_BASE+0x00003000)
#define TIMER_CLO       (MMIO_BASE+0x00003004)
#define TIMER_CHI       (MMIO_BASE+0x00003008)
#define TIMER_C0        (MMIO_BASE+0x0000300C)
#define TIMER_C1        (MMIO_BASE+0x00003010)
#define TIMER_C2        (MMIO_BASE+0x00003014)
#define TIMER_C3        (MMIO_BASE+0x00003018)

#define TIMER_CS_M0	(1 << 0)
#define TIMER_CS_M1	(1 << 1)
#define TIMER_CS_M2	(1 << 2)
#define TIMER_CS_M3	(1 << 3)

#endif
