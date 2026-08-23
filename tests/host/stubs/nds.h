#ifndef TEST_NDS_H
#define TEST_NDS_H

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
