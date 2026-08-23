#ifndef TEST_MAXMOD9_H
#define TEST_MAXMOD9_H

#include <stdbool.h>

bool mmInitDefault(const char *path);
unsigned int mmLoadEffect(unsigned int sample_id);
unsigned int mmUnloadEffect(unsigned int sample_id);
unsigned int mmEffect(unsigned int sample_id);
void mmEffectCancelAll(void);

#endif
