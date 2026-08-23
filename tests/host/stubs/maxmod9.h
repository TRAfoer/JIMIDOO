#ifndef TEST_MAXMOD9_H
#define TEST_MAXMOD9_H

#include <stdbool.h>

typedef unsigned int mm_word;
typedef unsigned char mm_byte;
typedef void *mm_addr;

typedef enum mm_stream_formats {
    MM_STREAM_8BIT_MONO,
    MM_STREAM_8BIT_STEREO,
    MM_STREAM_16BIT_MONO,
    MM_STREAM_16BIT_STEREO
} mm_stream_formats;

typedef mm_word (*mm_stream_func)(mm_word length, mm_addr destination,
                                  mm_stream_formats format);

typedef struct mm_stream {
    mm_word sampling_rate;
    mm_word buffer_length;
    mm_stream_func callback;
    mm_word format;
    mm_word timer;
    bool manual;
} mm_stream;

#define MM_TIMER0 0U

bool mmInitDefault(const char *path);
mm_word mmLoadEffect(mm_word sample_id);
mm_word mmUnloadEffect(mm_word sample_id);
mm_word mmEffect(mm_word sample_id);
void mmEffectCancelAll(void);
void mmStreamOpen(mm_stream *stream);
void mmStreamClose(void);
void mmStreamVolume(mm_byte volume);

#endif
