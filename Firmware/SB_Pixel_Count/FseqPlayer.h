// SD-card FSEQ (xLights/Falcon Player sequence) file browsing and playback.
// Only used on VERSION3 (Pro) boards, which have a microSD slot. Supports
// uncompressed FSEQ v1/v2 files, mapped straight through as sequential RGB
// channels (channel 0-2 = pixel 0, 3-5 = pixel 1, ...).
#pragma once

#include "BoardConfig.h"

#if defined(HAS_SD_CARD)

#include <Adafruit_NeoPixel.h>
#include <Adafruit_SSD1306.h>

#define FSEQ_MAX_FILES 40
#define FSEQ_MAX_NAME_LEN 48

// Must be called once from setup() before any other fseq* call.
void fseqInit(int csPin, Adafruit_NeoPixel &pixels, Adafruit_SSD1306 &oled, uint16_t pixelCount);

// Re-mounts the SD card and rescans the root directory for *.fseq files.
// Returns the number of files found (0 if no card / no files).
int fseqScanFiles();

int fseqFileCount();
const char *fseqFileName(int index);

// Opens fseqFileName(index), parses its header, and begins playback timing.
// Returns false (and leaves nothing playing) if the file can't be opened,
// isn't a recognized FSEQ file, or uses unsupported compression.
bool fseqStart(int index);

// Call every loop iteration while playing. Advances to the next frame when
// its time has come. Returns false once playback has reached the end of
// the file (caller should then treat playback as finished).
bool fseqUpdate();

// Stops playback (if any), closes the file, and blanks the strip.
void fseqStop();

const char *fseqLastError();

void fseqDrawMenu(int selectedIndex);
void fseqDrawPlaying(int selectedIndex);

#endif // HAS_SD_CARD
