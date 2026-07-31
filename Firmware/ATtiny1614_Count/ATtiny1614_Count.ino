// ATtiny1614 port of RP2040_Count.
//
// Board (Boards Manager): megaTinyCore "ATtiny3224/1624/1614/1604/824/814/804/
// 424/414/404/214/204", chip=1614, no bootloader (UPDI programmer required).
// Flash/RAM budget is tight (16KB / 2KB) so this port avoids Adafruit_GFX,
// Adafruit_SSD1306, Adafruit_NeoPixel and Adafruit_INA219: those libraries
// assume a buffered strip/framebuffer and pull in float-format/BusIO code
// that doesn't fit. Swapped for FAB_LED (streams pixels with no strip
// buffer), SSD1306Ascii (text-only, no framebuffer), and a minimal
// register-level INA219 reader.
//
// Wiring (ATtiny1614, 14-pin):
//   PA4 (digital 0) -> NeoPixel data in
//   PA5 (digital 1) -> Button (active low, internal pullup)
//   PB1 (digital 6) -> SDA (hardware TWI0, default pins)
//   PB0 (digital 7) -> SCL (hardware TWI0, default pins)
//   PA0 (UPDI)      -> reserved for programming, not used as GPIO
// INA219 shunt is 0.1 ohm (matches RP2040_Count.kicad_sch, R "0.1 OHM 2W").
//
// Power/level-shifting (for whoever lays out the PCB): the SSD1306 needs a
// 3V supply, and I2C is open-drain, so ATtiny1614 + INA219 + OLED should all
// share a 3.3V rail rather than 5V (both chips are fine at 3.3V). That
// leaves the NeoPixel data line under-driven: WS2812B wants a data-high
// threshold near 0.7*Vdd (~3.5V on a 5V-powered strip), which a 3.3V GPIO
// won't reliably hit over a run this long (801 pixels). RP2040_Count.kicad_sch
// already solves this on the RP2040 board (same 3.3V-logic situation) with a
// 74AHCT1G125 level shifter on the pixel line - reuse that pattern here:
// 3.3V for MCU/I2C/OLED, level-shift just the PA4 pixel output up to 5V.

#include <Wire.h>
#include <FAB_LED.h>
#include <SSD1306Ascii.h>
#include <SSD1306AsciiWire.h>
#include <OneButton.h>

#define BUTTON_A_PIN  1  // PA5

#define PIXEL_COUNT 801  // Number of NeoPixels
#define PIXEL_CURRENT_DIFF1 10
#define PIXEL_CURRENT_DIFF2 5
#define PIXEL_CURRENT_DIFF3 3
#define CURRENT_READ_DELAY 10
#define CURRENT_REREAD_DELAY 3

#define NUM_OFF_READING 5
#define NUM_SINGLE_READING 3

#define SCREEN_ADDRESS 0x3C  // Address 0x3C default

#define INA219_ADDRESS 0x40
#define INA219_REG_SHUNTVOLTAGE 0x01
#define INA219_REG_BUSVOLTAGE   0x02
#define INA219_SHUNT_OHMS 0.1f  // matches R "0.1 OHM 2W" on RP2040_Count PCB

ws2812b<A, 4> strip;  // NeoPixel data on PA4
SSD1306AsciiWire oled;

OneButton button = OneButton(
  BUTTON_A_PIN,  // Input pin for the button
  true,          // Button is active LOW
  true           // Enable internal pull-up resistor
);

uint32_t pixel_count = 0;
uint16_t color = 0;
volatile int mode = 0;

// ---- Minimal INA219 register access (no calibration register, no BusIO) ----
// Relies on the INA219's power-on-reset default config (continuous
// shunt+bus conversion, 12-bit ADC, +-320mV shunt range), so no config
// write is needed at startup.

int16_t ina219_read16(uint8_t reg) {
  Wire.beginTransmission(INA219_ADDRESS);
  Wire.write(reg);
  Wire.endTransmission();
  Wire.requestFrom(INA219_ADDRESS, (uint8_t)2);
  uint16_t value = (uint16_t)Wire.read() << 8;
  value |= Wire.read();
  return (int16_t)value;
}

float ina219_getShuntVoltage_mV() {
  return ina219_read16(INA219_REG_SHUNTVOLTAGE) * 0.01f;  // LSB = 10uV
}

float ina219_getBusVoltage_V() {
  uint16_t raw = (uint16_t)ina219_read16(INA219_REG_BUSVOLTAGE);
  return (raw >> 3) * 0.004f;  // top 13 bits, LSB = 4mV
}

float ina219_getCurrent_mA() {
  return ina219_getShuntVoltage_mV() / INA219_SHUNT_OHMS;
}

// ---- Streaming NeoPixel helpers (FAB_LED holds no strip buffer) ----

void setAllColor(uint8_t r, uint8_t g, uint8_t b) {
  grb pix[1];
  pix[0].r = r;
  pix[0].g = g;
  pix[0].b = b;
  const uint8_t oldSREG = SREG;
  cli();
  for (uint16_t i = 0; i < PIXEL_COUNT; i++) {
    strip.sendPixels(1, pix);
  }
  SREG = oldSREG;
}

void turnOnLED(uint16_t ledidx) {
  grb pix[1];
  const uint8_t oldSREG = SREG;
  cli();
  for (uint16_t i = 0; i < PIXEL_COUNT; i++) {
    if (i == ledidx) {
      pix[0].r = 255; pix[0].g = 255; pix[0].b = 255;
    } else {
      pix[0].r = 0; pix[0].g = 0; pix[0].b = 0;
    }
    strip.sendPixels(1, pix);
  }
  SREG = oldSREG;
  delay(CURRENT_READ_DELAY);
}

// Input 0-255, returns a packed 0x00RRGGBB color transitioning r-g-b-r.
uint32_t wheel(uint8_t wheelPos) {
  wheelPos = 255 - wheelPos;
  if (wheelPos < 85) {
    return ((uint32_t)(255 - wheelPos * 3) << 16) | (uint32_t)(wheelPos * 3);
  }
  if (wheelPos < 170) {
    wheelPos -= 85;
    return ((uint32_t)(wheelPos * 3) << 8) | (uint32_t)(255 - wheelPos * 3);
  }
  wheelPos -= 170;
  return ((uint32_t)(255 - wheelPos * 3) << 8) | (uint32_t)(wheelPos * 3);
}

void rainbowCycle(uint16_t colorOffset) {
  grb pix[1];
  const uint8_t oldSREG = SREG;
  cli();
  for (uint16_t i = 0; i < PIXEL_COUNT; i++) {
    uint32_t c = wheel((uint8_t)(((uint32_t)i * 256 / PIXEL_COUNT + colorOffset) & 255));
    pix[0].r = (c >> 16) & 0xFF;
    pix[0].g = (c >> 8) & 0xFF;
    pix[0].b = c & 0xFF;
    strip.sendPixels(1, pix);
  }
  SREG = oldSREG;
}

void setColorWith50() {
  grb pix[1];
  const uint8_t oldSREG = SREG;
  cli();
  for (uint16_t i = 0; i < PIXEL_COUNT; i++) {
    bool marker = ((i + 1) % 50 == 0) && (i != 0);
    pix[0].r = 255;
    pix[0].g = marker ? 255 : 0;
    pix[0].b = marker ? 255 : 0;
    strip.sendPixels(1, pix);
  }
  SREG = oldSREG;
}

// ---- Display ----

void update_power_display(uint16_t pixelNum, const char* mode_str) {
  float shuntvoltage = ina219_getShuntVoltage_mV();
  float busvoltage = ina219_getBusVoltage_V();
  float current_mA = ina219_getCurrent_mA();
  float loadvoltage = busvoltage + (shuntvoltage / 1000.0f);

  oled.clear();
  oled.setCursor(0, 0);
  oled.print("Pixels:");
  oled.print(pixelNum);

  oled.setCursor(0, 2);
  oled.print("V:");
  oled.print(loadvoltage, 2);

  oled.setCursor(0, 4);
  oled.print("A:");
  oled.print(current_mA / 1000.0f, 3);

  oled.setCursor(0, 6);
  oled.print(mode_str);
}

// ---- Pixel counting (binary search on current draw), ported 1:1 ----

bool IsLEDOn(uint16_t ledidx, float off_current, float current_diff) {
  turnOnLED(ledidx);

  float on_current_sum = 0.0f;
  for (int i = 0; i < NUM_SINGLE_READING; i++) {
    on_current_sum += ina219_getCurrent_mA();
    delay(CURRENT_REREAD_DELAY);
  }
  float on_current = on_current_sum / (float)NUM_SINGLE_READING;

  return (on_current - off_current) >= current_diff;
}

int findTransitionPoint(int n, float off_current, float current_diff) {
  int lb = 0;
  int ub = n - 1;

  while (lb <= ub) {
    int mid = (lb + ub) / 2;
    char modeBuf[10];
    snprintf(modeBuf, sizeof(modeBuf), "Cnt %d", (int)current_diff);
    update_power_display(mid, modeBuf);

    if (IsLEDOn(mid, off_current, current_diff)) {
      lb = mid + 1;
    } else {
      if (mid == 0 || (mid > 0 && IsLEDOn(mid - 1, off_current, current_diff))) {
        return mid;
      }
      ub = mid - 1;
    }
  }
  return -1;
}

void countPixels() {
  strip.clear(PIXEL_COUNT);
  delay(50);

  float off_current_sum = 0.0f;
  for (int i = 0; i < NUM_OFF_READING; i++) {
    off_current_sum += ina219_getCurrent_mA();
    delay(CURRENT_REREAD_DELAY);
  }
  float off_current = off_current_sum / (float)NUM_OFF_READING;

  float current_diffs[3] = { PIXEL_CURRENT_DIFF1, PIXEL_CURRENT_DIFF2, PIXEL_CURRENT_DIFF3 };

  for (auto const& cur_diff : current_diffs) {
    int count = findTransitionPoint(PIXEL_COUNT, off_current, cur_diff);
    if (count > 0) {
      pixel_count = count;
      break;
    }
  }
}

// ---- Button callbacks ----

void ShortPress(void* oneButton) {
  if (++mode > 7) {
    mode = 0;
  }
}

void LongPress(void* oneButton) {
  countPixels();
}

// ---- Setup / loop ----

void setup() {
  Wire.begin();
  Wire.setClock(400000L);

  oled.begin(&Adafruit128x64, SCREEN_ADDRESS);
  oled.setFont(System5x7);
  oled.clear();

  strip.clear(PIXEL_COUNT);

  button.attachPress(ShortPress, &button);
  button.attachLongPressStart(LongPress, &button);
  button.setDebounceMs(20);
  button.setLongPressIntervalMs(800);
  delay(100);
  countPixels();
}

void loop() {
  button.tick();

  color++;
  if (color > 255) {
    color = 0;
  }

  const char* mode_str;
  switch (mode) {
    case 0: rainbowCycle(color); mode_str = "Rainbow"; break;
    case 1: setColorWith50(); mode_str = "Every 50"; break;
    case 2: setAllColor(255, 0, 0); mode_str = "Red"; break;
    case 3: setAllColor(0, 255, 0); mode_str = "Green"; break;
    case 4: setAllColor(0, 0, 255); mode_str = "Blue"; break;
    case 5: setAllColor(127, 127, 127); mode_str = "50% White"; break;
    case 6: setAllColor(255, 255, 255); mode_str = "100% White"; break;
    case 7: setAllColor(0, 0, 0); mode_str = "Off"; break;
  }

  delay(10);
  update_power_display(pixel_count, mode_str);
}
