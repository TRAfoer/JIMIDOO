#include "audio_service.h"

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "maxmod9.h"

#undef fopen

bool musicStreamStart(MusicId id);
void musicStreamUpdate(void);
void musicStreamSetVolume(unsigned int volume);
void musicStreamClose(void);

static mm_stream_func captured_callback;
static mm_stream_formats captured_format;
static int stream_open_calls;
static int stream_close_calls;
static unsigned int stream_volume;

static FILE *openFile(const char *path, const char *mode)
{
    FILE *file = NULL;
    if (fopen_s(&file, path, mode) != 0) {
        return NULL;
    }
    return file;
}

FILE *testFopen(const char *path, const char *mode)
{
    if (strcmp(path, "nitro:/audio/menu.wav") == 0) {
        return openFile("test_music_menu.wav", mode);
    }
    if (strcmp(path, "nitro:/audio/battle.wav") == 0) {
        return openFile("test_music_battle.wav", mode);
    }
    return NULL;
}

void mmStreamOpen(mm_stream *stream)
{
    captured_callback = stream->callback;
    captured_format = (mm_stream_formats)stream->format;
    ++stream_open_calls;
}

void mmStreamClose(void)
{
    ++stream_close_calls;
    captured_callback = NULL;
}

void mmStreamVolume(mm_byte volume)
{
    stream_volume = volume;
}

static void write16(FILE *file, uint16_t value)
{
    unsigned char bytes[2] = {
        (unsigned char)(value & 0xFFU),
        (unsigned char)(value >> 8)
    };
    assert(fwrite(bytes, 1, sizeof(bytes), file) == sizeof(bytes));
}

static void write32(FILE *file, uint32_t value)
{
    unsigned char bytes[4] = {
        (unsigned char)(value & 0xFFU),
        (unsigned char)((value >> 8) & 0xFFU),
        (unsigned char)((value >> 16) & 0xFFU),
        (unsigned char)(value >> 24)
    };
    assert(fwrite(bytes, 1, sizeof(bytes), file) == sizeof(bytes));
}

static void writeWave(const char *path, uint32_t declared_data_size,
                      const unsigned char *data, size_t actual_data_size)
{
    FILE *file = openFile(path, "wb");
    assert(file != NULL);
    assert(fwrite("RIFF", 1, 4, file) == 4);
    write32(file, 36U + declared_data_size + (declared_data_size & 1U));
    assert(fwrite("WAVEfmt ", 1, 8, file) == 8);
    write32(file, 16);
    write16(file, 1);
    write16(file, 1);
    write32(file, 22050);
    write32(file, 44100);
    write16(file, 2);
    write16(file, 16);
    assert(fwrite("data", 1, 4, file) == 4);
    write32(file, declared_data_size);
    assert(fwrite(data, 1, actual_data_size, file) == actual_data_size);
    if ((actual_data_size & 1U) != 0) {
        assert(fputc(0, file) != EOF);
    }
    assert(fclose(file) == 0);
}

static void testValidLoopRingUnderflowAndLifecycle(void)
{
    static const unsigned char loop_data[] = { 1, 2, 3, 4, 5, 6 };
    unsigned char first[8];
    unsigned char large[18000];
    unsigned char refill[4];

    writeWave("test_music_menu.wav", sizeof(loop_data),
              loop_data, sizeof(loop_data));
    writeWave("test_music_battle.wav", sizeof(loop_data),
              loop_data, sizeof(loop_data));

    assert(musicStreamStart(MUSIC_MENU));
    assert(stream_open_calls == 1);
    assert(captured_callback != NULL);
    assert(captured_format == MM_STREAM_16BIT_MONO);
    assert(captured_callback(4, first, captured_format) == 4);
    for (size_t i = 0; i < sizeof(first); ++i) {
        assert(first[i] == loop_data[i % sizeof(loop_data)]);
    }

    memset(large, 0xAA, sizeof(large));
    assert(captured_callback(9000, large, captured_format) == 9000);
    assert(large[16375] != 0);
    for (size_t i = 16376; i < sizeof(large); ++i) {
        assert(large[i] == 0);
    }

    musicStreamUpdate();
    assert(captured_callback(2, refill, captured_format) == 2);
    assert(refill[0] == 5 && refill[1] == 6 &&
           refill[2] == 1 && refill[3] == 2);

    musicStreamSetVolume(200);
    assert(stream_volume == 127);
    assert(musicStreamStart(MUSIC_BATTLE));
    assert(stream_close_calls == 1);
    assert(stream_open_calls == 2);
    musicStreamClose();
    musicStreamClose();
    assert(stream_close_calls == 2);
}

static void testRejectsTruncatedDataChunk(void)
{
    static const unsigned char short_data[] = { 1, 2, 3, 4 };
    writeWave("test_music_menu.wav", 8, short_data, sizeof(short_data));
    assert(!musicStreamStart(MUSIC_MENU));
    assert(stream_open_calls == 2);
}

static void testRejectsOddPcm16DataSize(void)
{
    static const unsigned char odd_data[] = { 1, 2, 3 };
    writeWave("test_music_menu.wav", sizeof(odd_data),
              odd_data, sizeof(odd_data));
    assert(!musicStreamStart(MUSIC_MENU));
    assert(stream_open_calls == 2);
}

int main(void)
{
    testValidLoopRingUnderflowAndLifecycle();
    testRejectsTruncatedDataChunk();
    testRejectsOddPcm16DataSize();
    assert(remove("test_music_menu.wav") == 0);
    assert(remove("test_music_battle.wav") == 0);
    return 0;
}
