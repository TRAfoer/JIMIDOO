#include "audio_service.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <maxmod9.h>
#include <nds.h>

#define MUSIC_BUFFER_SIZE 16384U
#define STREAM_SAMPLE_RATE 22050U
#define STREAM_BUFFER_SAMPLES 2048U

static FILE *music_file;
static long music_data_offset;
static uint32_t music_data_size;
static uint32_t music_bytes_remaining;
static uint8_t stream_buffer[MUSIC_BUFFER_SIZE];
static volatile unsigned int read_cursor;
static volatile unsigned int write_cursor;
static volatile unsigned int buffered_bytes;
static bool stream_open;

static uint16_t readLittle16(const uint8_t *data)
{
    return (uint16_t)(data[0] | ((uint16_t)data[1] << 8));
}

static uint32_t readLittle32(const uint8_t *data)
{
    return (uint32_t)data[0] |
        ((uint32_t)data[1] << 8) |
        ((uint32_t)data[2] << 16) |
        ((uint32_t)data[3] << 24);
}

static bool skipChunkBytes(uint32_t count)
{
    long distance = (long)count + (long)(count & 1U);
    return fseek(music_file, distance, SEEK_CUR) == 0;
}

static bool readWaveHeader(void)
{
    uint8_t riff[12];
    if (fread(riff, 1, sizeof(riff), music_file) != sizeof(riff) ||
        memcmp(riff, "RIFF", 4) != 0 || memcmp(riff + 8, "WAVE", 4) != 0) {
        return false;
    }

    bool format_found = false;
    for (;;) {
        uint8_t chunk[8];
        if (fread(chunk, 1, sizeof(chunk), music_file) != sizeof(chunk)) {
            return false;
        }
        uint32_t chunk_size = readLittle32(chunk + 4);

        if (memcmp(chunk, "fmt ", 4) == 0) {
            uint8_t format[16];
            if (chunk_size < sizeof(format) ||
                fread(format, 1, sizeof(format), music_file) != sizeof(format)) {
                return false;
            }
            if (readLittle16(format) != 1 ||
                readLittle16(format + 2) != 1 ||
                readLittle32(format + 4) != STREAM_SAMPLE_RATE ||
                readLittle16(format + 14) != 16) {
                return false;
            }
            if (!skipChunkBytes(chunk_size - sizeof(format))) {
                return false;
            }
            format_found = true;
        }
        else if (memcmp(chunk, "data", 4) == 0) {
            if (!format_found || chunk_size == 0 || (chunk_size & 1U) != 0) {
                return false;
            }
            music_data_offset = ftell(music_file);
            if (music_data_offset < 0) {
                return false;
            }
            if (fseek(music_file, 0, SEEK_END) != 0) {
                return false;
            }
            long file_end = ftell(music_file);
            if (file_end < music_data_offset ||
                (uint64_t)chunk_size >
                    (uint64_t)(file_end - music_data_offset) ||
                fseek(music_file, music_data_offset, SEEK_SET) != 0) {
                return false;
            }
            music_data_size = chunk_size;
            music_bytes_remaining = chunk_size;
            return true;
        }
        else if (!skipChunkBytes(chunk_size)) {
            return false;
        }
    }
}

static size_t readLoopingAudio(uint8_t *destination, size_t requested)
{
    size_t total = 0;
    while (total < requested) {
        if (music_bytes_remaining == 0) {
            if (fseek(music_file, music_data_offset, SEEK_SET) != 0) {
                break;
            }
            music_bytes_remaining = music_data_size;
        }

        size_t amount = requested - total;
        if (amount > music_bytes_remaining) {
            amount = music_bytes_remaining;
        }
        size_t got = fread(destination + total, 1, amount, music_file);
        if (got == 0) {
            break;
        }
        total += got;
        music_bytes_remaining -= (uint32_t)got;
    }
    return total;
}

static void fillStreamBuffer(void)
{
    while (true) {
        int interrupt_state = enterCriticalSection();
        unsigned int free_bytes = MUSIC_BUFFER_SIZE - buffered_bytes;
        unsigned int destination = write_cursor;
        unsigned int contiguous = MUSIC_BUFFER_SIZE - destination;
        if (contiguous > free_bytes) {
            contiguous = free_bytes;
        }
        leaveCriticalSection(interrupt_state);

        if (contiguous == 0) {
            return;
        }

        size_t got = readLoopingAudio(stream_buffer + destination, contiguous);
        if (got == 0) {
            return;
        }

        interrupt_state = enterCriticalSection();
        write_cursor = (write_cursor + (unsigned int)got) % MUSIC_BUFFER_SIZE;
        buffered_bytes += (unsigned int)got;
        leaveCriticalSection(interrupt_state);
    }
}

static mm_word streamCallback(mm_word length, mm_addr destination,
                              mm_stream_formats format)
{
    unsigned int bytes_per_sample = format == MM_STREAM_16BIT_STEREO ? 4U : 2U;
    unsigned int requested = (unsigned int)length * bytes_per_sample;
    unsigned int available = buffered_bytes;
    if (available > requested) {
        available = requested;
    }

    uint8_t *output = destination;
    unsigned int first = MUSIC_BUFFER_SIZE - read_cursor;
    if (first > available) {
        first = available;
    }
    memcpy(output, stream_buffer + read_cursor, first);
    memcpy(output + first, stream_buffer, available - first);
    if (available < requested) {
        memset(output + available, 0, requested - available);
    }

    read_cursor = (read_cursor + available) % MUSIC_BUFFER_SIZE;
    buffered_bytes -= available;
    return length;
}

bool musicStreamStart(MusicId id)
{
    const char *path;
    if (id == MUSIC_MENU) {
        path = "nitro:/audio/menu.wav";
    }
    else if (id == MUSIC_BATTLE) {
        path = "nitro:/audio/battle.wav";
    }
    else {
        return false;
    }

    if (stream_open) {
        mmStreamClose();
        stream_open = false;
    }
    if (music_file != NULL) {
        fclose(music_file);
        music_file = NULL;
    }

    music_file = fopen(path, "rb");
    if (music_file == NULL || !readWaveHeader()) {
        if (music_file != NULL) {
            fclose(music_file);
            music_file = NULL;
        }
        return false;
    }

    read_cursor = 0;
    write_cursor = 0;
    buffered_bytes = 0;
    fillStreamBuffer();
    if (buffered_bytes == 0) {
        fclose(music_file);
        music_file = NULL;
        return false;
    }

    mm_stream stream = {
        .sampling_rate = STREAM_SAMPLE_RATE,
        .buffer_length = STREAM_BUFFER_SAMPLES,
        .callback = streamCallback,
        .format = MM_STREAM_16BIT_MONO,
        .timer = MM_TIMER0,
        .manual = false
    };
    mmStreamOpen(&stream);
    stream_open = true;
    return true;
}

void musicStreamUpdate(void)
{
    if (stream_open && music_file != NULL) {
        fillStreamBuffer();
    }
}

void musicStreamSetVolume(unsigned int volume)
{
    if (stream_open) {
        if (volume > 127U) {
            volume = 127U;
        }
        mmStreamVolume((mm_byte)volume);
    }
}

void musicStreamClose(void)
{
    if (stream_open) {
        mmStreamClose();
        stream_open = false;
    }
    if (music_file != NULL) {
        fclose(music_file);
        music_file = NULL;
    }
    music_data_offset = 0;
    music_data_size = 0;
    music_bytes_remaining = 0;
    read_cursor = 0;
    write_cursor = 0;
    buffered_bytes = 0;
}
