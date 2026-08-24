#ifndef TEST_NDS_H
#define TEST_NDS_H

#include <stdint.h>

#define KEY_SELECT (1U << 2)
#define KEY_A (1U << 0)
#define KEY_B (1U << 1)
#define KEY_START (1U << 3)
#define KEY_RIGHT (1U << 4)
#define KEY_LEFT (1U << 5)
#define KEY_UP (1U << 6)
#define KEY_DOWN (1U << 7)
#define KEY_L (1U << 9)
#define RGB15(red, green, blue) \
    ((uint16_t)((red) | ((green) << 5) | ((blue) << 10)))

void soundDisable(void);
void soundEnable(void);

static inline int enterCriticalSection(void)
{
    return 0;
}

static inline void leaveCriticalSection(int state)
{
    (void)state;
}

#endif
