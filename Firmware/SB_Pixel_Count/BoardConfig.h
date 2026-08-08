// Pin mapping per PCB revision. VERSION1/VERSION2/VERSION3 is normally set
// via a `-D` build flag (see .github/workflows/compile.yml); uncomment one
// of the #defines below to pick a board when building from the Arduino IDE.
#pragma once

//#define  VERSION1
//#define  VERSION2
//#define  VERSION3
//#define  ON_BOARD_LED

#if !defined(VERSION1) && !defined(VERSION2) && !defined(VERSION3)
#define VERSION2
#endif

#if defined(VERSION1)
#define PIXEL_PIN    14  // Digital IO pin connected to the NeoPixels.
#define BUTTON_A_PIN  13
#define I2C_SDA_PIN  10
#define I2C_SCL_PIN  11
#define WIRE_DEVICE Wire1
#define PIXEL2_PIN    16
#endif

#if defined(VERSION2)
#define PIXEL_PIN    D6  // Digital IO pin connected to the NeoPixels.
#define BUTTON_A_PIN  D7
#define I2C_SDA_PIN  PIN_WIRE0_SDA
#define I2C_SCL_PIN  PIN_WIRE0_SCL
#define WIRE_DEVICE Wire
#define PIXEL2_PIN    12
#endif

#if defined(VERSION3)
// Pro board: adds a second button (SD/FSEQ menu) and a microSD card
// (SPI0: CS=D7, SCK=D8, MISO=D9, MOSI=D10 - default XIAO RP2040 SPI pins).
#define PIXEL_PIN    D6  // Digital IO pin connected to the NeoPixels.
#define BUTTON_A_PIN  D2
#define BUTTON_B_PIN  D3
#define I2C_SDA_PIN  PIN_WIRE0_SDA
#define I2C_SCL_PIN  PIN_WIRE0_SCL
#define WIRE_DEVICE Wire
#define PIXEL2_PIN    12
#define SD_CS_PIN    D7
#define HAS_SD_CARD
#endif
