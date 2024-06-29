#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_INA219.h>
#include <Adafruit_NeoPixel.h>

//#define  VERSION1
#define  VERSION2

#if defined(VERSION1)
#define PIXEL_PIN    14  // Digital IO pin connected to the NeoPixels.
#define BUTTON_A_PIN  13
#define I2C_SDA_PIN  10
#define I2C_SCL_PIN  11
#define WIRE_DEVICE Wire1
#endif

#if defined(VERSION2)
#define PIXEL_PIN    D6  // Digital IO pin connected to the NeoPixels.
#define BUTTON_A_PIN  D7
#define I2C_SDA_PIN  PIN_WIRE0_SDA
#define I2C_SCL_PIN  PIN_WIRE0_SCL
#define WIRE_DEVICE Wire
#endif

#define PIXEL_COUNT 300  // Number of NeoPixels
#define PIXEL_CURRENT_DIFF 5
#define CURRENT_READ_DELAY 5

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels
#define OLED_RESET     -1 // Reset pin # (or -1 if sharing Arduino reset pin)
#define SCREEN_ADDRESS 0x3C  // Address 0x3C default

Adafruit_SSD1306 display = Adafruit_SSD1306(SCREEN_WIDTH, SCREEN_HEIGHT, &WIRE_DEVICE, OLED_RESET);
Adafruit_INA219 ina219;

// Declare our NeoPixel strip object:
Adafruit_NeoPixel strip(PIXEL_COUNT, PIXEL_PIN, NEO_RGB + NEO_KHZ800);

boolean oldState = HIGH;
uint32_t pixel_count = 0;
uint16_t color = 0;

void setup() {
  Serial.begin(115200);
  //delay(3000);//serial debug doesnt work without this
  Serial.println("Starting Pixel Tester");
  WIRE_DEVICE.setSDA(I2C_SDA_PIN);
  WIRE_DEVICE.setSCL(I2C_SCL_PIN);
  Serial.println("INA219 begin");
  if (!ina219.begin(&WIRE_DEVICE)) {
    Serial.println("Failed to find INA219 chip");
    while (1) { delay(10); }
  }
  pinMode(BUTTON_A_PIN, INPUT_PULLUP);
  strip.begin(); // Initialize NeoPixel strip object (REQUIRED)
  strip.show();  // Initialize all pixels to 'off'

  Serial.println("OLED begin");
  delay(250); // wait for the OLED to power up
  display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS); // Address 0x3C default

  display.setRotation(2);
  // Show image buffer on the display hardware.
  // Since the buffer is intialized with an Adafruit splashscreen
  // internally, this will display the splashscreen.
  //display.display();
  //delay(1000);

  // Clear the buffer.
  display.clearDisplay();
  display.display();

  // text display tests
  display.setTextSize(2);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0,0);

  display.display(); // actually display all of the above
}

void loop() {
  boolean newState = digitalRead(BUTTON_A_PIN);

  // Check if state changed from high to low (button press).
  if((newState == LOW) && (oldState == HIGH)) {
    // Short delay to debounce button.
    delay(20);
    // Check if button is still low after debounce.
    newState = digitalRead(BUTTON_A_PIN);
    if(newState == LOW) { 
      for(int i = 0; i< strip.numPixels(); i++) {
        strip.setPixelColor(i, strip.Color(0,0,0));  
      }
      strip.show();  
      delay(50);

      float off_current = ina219.getCurrent_mA(); //55ma on a 50pix bullet string all off

      for(int ledidx = 0; ledidx < strip.numPixels(); ledidx++) { 
        for(int dot = 0; dot < strip.numPixels(); dot++) {
          if(dot == ledidx) {
            strip.setPixelColor(dot, strip.Color(255,255,255));
          }
          else {
            strip.setPixelColor(dot, strip.Color(0,0,0));
          }
        }
        strip.show();
        delay(CURRENT_READ_DELAY);
        yield();

        float on_current = ina219.getCurrent_mA(); //100ma on on 50pix bullet string with one on
        update_power_display(ledidx + 1);
        display.display();

        if((on_current - off_current) < PIXEL_CURRENT_DIFF) {
          pixel_count = ledidx;
          break;
        }
      }
    }
  }

  // Set the last-read button state to the old state.
  oldState = newState;
  color++;
  if(color > 255) {
    color = 0;
  }
  rainbowCycle(color);
  delay(10);
  yield();
  update_power_display(pixel_count);
  display.display();
}

void update_power_display(int pixelNum) {
  // Read voltage and current from INA219.
  float shuntvoltage = ina219.getShuntVoltage_mV();
  float busvoltage = ina219.getBusVoltage_V();
  float current_mA = ina219.getCurrent_mA();

  // Compute load voltage
  float loadvoltage = busvoltage + (shuntvoltage / 1000);
  
  // Update display.
  display.clearDisplay();
  display.setCursor(0, 0);
  display.print("Pixels:");
  display.print(pixelNum);

  display.setCursor(0, 16);

  // Mode 0, display volts and amps.
  printSIValue(loadvoltage, "V:", 2, 10);
  display.setCursor(0, 32);
  printSIValue(current_mA/1000.0, "A:", 5, 10);

  display.display();
}

void printSIValue(float value, const char* units, int precision, int maxWidth) {
  // Print a value in SI units with the units left justified and value right justified.
  // Will switch to milli prefix if value is below 1.

  // Add milli prefix if low value.
  if (fabs(value) < 1.0) {
    display.print('m');
    maxWidth -= 1;
    value *= 1000.0;
    precision = max(0, precision-3);
  }

  // Print units.
  display.print(units);
  maxWidth -= strlen(units);

  // Leave room for negative sign if value is negative.
  if (value < 0.0) {
    maxWidth -= 1;
  }

  // Find how many digits are in value.
  int digits = ceil(log10(fabs(value)));
  if (fabs(value) < 1.0) {
    digits = 1; // Leave room for 0 when value is below 0.
  }

  // Handle if not enough width to display value, just print dashes.
  if (digits > maxWidth) {
    // Fill width with dashes (and add extra dash for negative values);
    for (int i=0; i < maxWidth; ++i) {
      display.print('-');
    }
    if (value < 0.0) {
      display.print('-');
    }
    return;
  }

  // Compute actual precision for printed value based on space left after
  // printing digits and decimal point.  Clamp within 0 to desired precision.
  int actualPrecision = constrain(maxWidth-digits-1, 0, precision);

  // Compute how much padding to add to right justify.
  int padding = maxWidth-digits-1-actualPrecision;
  for (int i=0; i < padding; ++i) {
    display.print(' ');
  }

  // Finally, print the value!
  display.print(value, actualPrecision);
}

void rainbowCycle(uint16_t color) {
    for(uint16_t i=0; i< strip.numPixels(); i++) {
      strip.setPixelColor(i, Wheel(((i * 256 / strip.numPixels()) + color) & 255));
    }
    strip.show();
}

// Input a value 0 to 255 to get a color value.
// The colours are a transition r - g - b - back to r.
uint32_t Wheel(byte WheelPos) {
  WheelPos = 255 - WheelPos;
  if(WheelPos < 85) {
    return strip.Color(255 - WheelPos * 3, 0, WheelPos * 3);
  }
  if(WheelPos < 170) {
    WheelPos -= 85;
    return strip.Color(0, WheelPos * 3, 255 - WheelPos * 3);
  }
  WheelPos -= 170;
  return strip.Color(WheelPos * 3, 255 - WheelPos * 3, 0);
}