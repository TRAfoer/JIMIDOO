#ifndef TEST_NDS_H
#define TEST_NDS_H

#include <stdint.h>

#define KEY_SELECT (1U << 2)
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
