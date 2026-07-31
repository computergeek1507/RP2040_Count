#include "FseqPlayer.h"

#if defined(HAS_SD_CARD)

#include <SD.h>
#include <SPI.h>
#include <string.h>
#include <ctype.h>

static Adafruit_NeoPixel *sPixels = nullptr;
static Adafruit_SSD1306 *sOled = nullptr;
static uint16_t sPixelCount = 0;
static int sCsPin = -1;

static char sFileNames[FSEQ_MAX_FILES][FSEQ_MAX_NAME_LEN];
static int sFileCount = 0;

static File sFile;
static bool sPlaying = false;
static uint32_t sDataOffset = 0;
static uint32_t sChannelCount = 0;
static uint32_t sNumFrames = 0;
static uint32_t sCurrentFrame = 0;
static uint32_t sFrameIntervalMs = 50;
static unsigned long sLastFrameMillis = 0;
static uint8_t *sFrameBuf = nullptr;
static char sLastError[32] = "";

static void setError(const char *msg) {
  strncpy(sLastError, msg, sizeof(sLastError) - 1);
  sLastError[sizeof(sLastError) - 1] = '\0';
}

static bool hasFseqExtension(const char *name) {
  size_t len = strlen(name);
  if (len < 5) return false;
  const char *ext = name + (len - 5);
  return ext[0] == '.' && tolower(ext[1]) == 'f' && tolower(ext[2]) == 's' &&
         tolower(ext[3]) == 'e' && tolower(ext[4]) == 'q';
}

void fseqInit(int csPin, Adafruit_NeoPixel &pixels, Adafruit_SSD1306 &oled, uint16_t pixelCount) {
  sCsPin = csPin;
  sPixels = &pixels;
  sOled = &oled;
  sPixelCount = pixelCount;
  if (sFrameBuf == nullptr) {
    sFrameBuf = new uint8_t[(size_t)sPixelCount * 3];
  }
}

int fseqScanFiles() {
  sFileCount = 0;
  if (!SD.begin(sCsPin)) {
    setError("No SD card");
    return 0;
  }
  File dir = SD.open("/");
  if (!dir) {
    setError("SD open failed");
    return 0;
  }
  while (sFileCount < FSEQ_MAX_FILES) {
    File entry = dir.openNextFile();
    if (!entry) break;
    if (!entry.isDirectory() && hasFseqExtension(entry.name())) {
      strncpy(sFileNames[sFileCount], entry.name(), FSEQ_MAX_NAME_LEN - 1);
      sFileNames[sFileCount][FSEQ_MAX_NAME_LEN - 1] = '\0';
      sFileCount++;
    }
    entry.close();
  }
  dir.close();
  if (sFileCount == 0) {
    setError("No .fseq files");
  }
  return sFileCount;
}

int fseqFileCount() { return sFileCount; }

const char *fseqFileName(int index) {
  if (index < 0 || index >= sFileCount) return "";
  return sFileNames[index];
}

const char *fseqLastError() { return sLastError; }

// FSEQ v1/v2 header: the first 4 bytes are the "PSEQ" magic, and the fields
// through byte 18 (step time) are laid out identically for both versions so
// the file can be found and framed without caring which one it is. The
// compression-type byte (20) only means anything in v2; a nonzero value
// there indicates compressed channel data, which this player can't decode.
static bool parseHeader(File &f) {
  uint8_t buf[32];
  f.seek(0);
  if (f.read(buf, sizeof(buf)) != (int)sizeof(buf)) {
    setError("Short header");
    return false;
  }
  if (memcmp(buf, "PSEQ", 4) != 0) {
    setError("Not an FSEQ file");
    return false;
  }
  sDataOffset = buf[4] | ((uint32_t)buf[5] << 8);
  uint8_t majorVersion = buf[7];
  sChannelCount = buf[10] | ((uint32_t)buf[11] << 8) | ((uint32_t)buf[12] << 16) | ((uint32_t)buf[13] << 24);
  sNumFrames = buf[14] | ((uint32_t)buf[15] << 8) | ((uint32_t)buf[16] << 16) | ((uint32_t)buf[17] << 24);
  uint8_t stepTime = buf[18];
  sFrameIntervalMs = stepTime > 0 ? stepTime : 50;

  if (majorVersion >= 2 && buf[20] != 0) {
    setError("Compressed FSEQ unsupported");
    return false;
  }
  if (sChannelCount == 0 || sNumFrames == 0) {
    setError("Empty FSEQ");
    return false;
  }
  return true;
}

bool fseqStart(int index) {
  fseqStop();
  if (index < 0 || index >= sFileCount) {
    setError("Bad index");
    return false;
  }

  char path[FSEQ_MAX_NAME_LEN + 1];
  path[0] = '/';
  strncpy(path + 1, sFileNames[index], FSEQ_MAX_NAME_LEN - 1);
  path[FSEQ_MAX_NAME_LEN] = '\0';

  sFile = SD.open(path);
  if (!sFile) {
    setError("Open failed");
    return false;
  }
  if (!parseHeader(sFile)) {
    sFile.close();
    return false;
  }

  sCurrentFrame = 0;
  sLastFrameMillis = millis();
  sPlaying = true;
  return true;
}

bool fseqUpdate() {
  if (!sPlaying) return false;

  unsigned long now = millis();
  if (now - sLastFrameMillis < sFrameIntervalMs) {
    return true; // not time for the next frame yet
  }
  sLastFrameMillis += sFrameIntervalMs;

  if (sCurrentFrame >= sNumFrames) {
    return false; // reached the end of the file
  }

  uint32_t bytesToRead = sChannelCount;
  uint32_t maxBytes = (uint32_t)sPixelCount * 3;
  if (bytesToRead > maxBytes) bytesToRead = maxBytes;

  sFile.seek(sDataOffset + sCurrentFrame * sChannelCount);
  int got = sFile.read(sFrameBuf, bytesToRead);
  if (got < 0) got = 0;

  uint16_t pixelsSet = got / 3;
  for (uint16_t p = 0; p < pixelsSet; p++) {
    sPixels->setPixelColor(p, sPixels->Color(sFrameBuf[p * 3], sFrameBuf[p * 3 + 1], sFrameBuf[p * 3 + 2]));
  }
  for (uint16_t p = pixelsSet; p < sPixelCount; p++) {
    sPixels->setPixelColor(p, 0);
  }
  sPixels->show();

  sCurrentFrame++;
  return true;
}

void fseqStop() {
  if (sFile) {
    sFile.close();
  }
  sPlaying = false;
  if (sPixels != nullptr) {
    for (uint16_t p = 0; p < sPixelCount; p++) {
      sPixels->setPixelColor(p, 0);
    }
    sPixels->show();
  }
}

void fseqDrawMenu(int selectedIndex) {
  if (sOled == nullptr) return;
  sOled->clearDisplay();
  sOled->setTextSize(1);
  sOled->setCursor(0, 0);
  sOled->print("FSEQ Menu");

  sOled->setCursor(0, 12);
  if (sFileCount == 0) {
    sOled->println(sLastError);
  } else {
    sOled->print(selectedIndex + 1);
    sOled->print("/");
    sOled->println(sFileCount);
    sOled->setCursor(0, 26);
    sOled->println(fseqFileName(selectedIndex));
  }

  sOled->setCursor(0, 54);
  sOled->println("Nxt=B Play=holdB");
  sOled->display();
}

void fseqDrawPlaying(int selectedIndex) {
  if (sOled == nullptr) return;
  sOled->clearDisplay();
  sOled->setTextSize(1);
  sOled->setCursor(0, 0);
  sOled->print("Playing:");
  sOled->setCursor(0, 12);
  sOled->println(fseqFileName(selectedIndex));

  sOled->setCursor(0, 30);
  sOled->print("Frame ");
  sOled->print(sCurrentFrame);
  sOled->print("/");
  sOled->println(sNumFrames);

  sOled->setCursor(0, 54);
  sOled->println("Stop=B");
  sOled->display();
}

#endif // HAS_SD_CARD
