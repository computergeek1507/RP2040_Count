#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_INA219.h>
#include <Adafruit_NeoPixel.h>
#include <OneButton.h>

//#define  VERSION1
#define  VERSION2
//#define  ON_BOARD_LED

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
int PIXEL2_Power = 11;
#endif

#define PIXEL_COUNT 401  // Number of NeoPixels
#define PIXEL2_COUNT 1  // Number of NeoPixels
#define PIXEL_CURRENT_DIFF 5
#define CURRENT_READ_DELAY 5

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels
#define OLED_RESET     -1 // Reset pin # (or -1 if sharing Arduino reset pin)
#define SCREEN_ADDRESS 0x3C  // Address 0x3C default

Adafruit_SSD1306 display = Adafruit_SSD1306(SCREEN_WIDTH, SCREEN_HEIGHT, &WIRE_DEVICE, OLED_RESET);
Adafruit_INA219 ina219;


Adafruit_NeoPixel strip(PIXEL_COUNT, PIXEL_PIN, NEO_RGB + NEO_KHZ800);
#if defined(ON_BOARD_LED)
Adafruit_NeoPixel strip2(PIXEL2_COUNT, PIXEL2_PIN, NEO_GRB + NEO_KHZ800);
#endif

OneButton button = OneButton(
  BUTTON_A_PIN,  // Input pin for the button
  true,        // Button is active LOW
  true         // Enable internal pull-up resistor
);

uint32_t pixel_count = 0;
uint16_t color = 0;
volatile int     mode     = 0; 

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
#if defined(ON_BOARD_LED)
#if defined(VERSION2)
  pinMode(PIXEL2_Power,OUTPUT);
  digitalWrite(PIXEL2_Power, HIGH);
#endif
#endif
  strip.begin(); // Initialize NeoPixel strip object (REQUIRED)
  strip.show();  // Initialize all pixels to 'off'

#if defined(ON_BOARD_LED) 
  strip2.begin();
  strip2.show(); 
#endif

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

  button.attachPress(ShortPress, &button);
  button.attachLongPressStart(LongPress, &button);
  button.setDebounceMs(20);
  button.setLongPressIntervalMs(800);
}

void loop() {
  button.tick();

  // Set the last-read button state to the old state.
  color++;
  if(color > 255) {
    color = 0;
  }
  //rainbowCycle(color);
  String mode_str;
  switch(mode) {           // Start the new animation...
        case 0:
            rainbowCycle(color);
            mode_str ="Rainbow";
          break;
        case 1:
          set_color(strip.Color(255,   0,   0));    // Red
          mode_str ="Red";
          break;
        case 2:
          set_color(strip.Color(  0, 255,   0));    // Green
          mode_str ="Green";
          break;
        case 3:
          set_color(strip.Color(  0,   0, 255));    // Blue
          mode_str ="Blue";
          break;
        case 4:
          set_color(strip.Color(  127,   127,   127));    // 50 White
          mode_str ="50% White";
          break;
        case 5:
          set_color(strip.Color(  255,   255,   255));    // 100 White
          mode_str ="100% White";
          break;
        case 6:
          set_color(strip.Color(  0,   0,   0));    // black
          mode_str ="Off";
          break;
      }


  delay(10);
  yield();
  update_power_display(pixel_count, mode_str);
  display.display();
}

void ShortPress(void *oneButton)
{
  if(++mode > 6){ mode = 0;} // Advance to next mode, wrap around after #8
      
}

// this function will be called when the button is released.
void LongPress(void *oneButton)
{
  for(int i = 0; i< strip.numPixels(); i++) {
        strip.setPixelColor(i, strip.Color(0,0,0));  
      }
      strip.show();  
#if defined(ON_BOARD_LED)
      for(int i = 0; i< strip2.numPixels(); i++) {
        strip2.setPixelColor(i, strip.Color(0,0,0));  
      }
      strip2.show();
#endif
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
        update_power_display(ledidx + 1, "Counting");
        display.display();

        if((on_current - off_current) < PIXEL_CURRENT_DIFF) {
          pixel_count = ledidx;
          break;
        }
      }
}


void update_power_display(int pixelNum, String const& mode_str  ) {
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

  display.setCursor(0, 47);
  display.print(mode_str);

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

void set_color(uint32_t color) {
  for(int i=0; i<strip.numPixels(); i++) { // For each pixel in strip...
    strip.setPixelColor(i, color);         //  Set pixel's color (in RAM)

  }
  strip.show();
#if defined(ON_BOARD_LED)
  for(int i=0; i<strip2.numPixels(); i++) { 
    strip2.setPixelColor(i, color); 
  }
  strip2.show();
#endif
}

void rainbowCycle(uint16_t color) {
    for(uint16_t i=0; i< strip.numPixels(); i++) {
      strip.setPixelColor(i, Wheel(((i * 256 / strip.numPixels()) + color) & 255));
    }
    strip.show();
#if defined(ON_BOARD_LED)
    for(uint16_t i=0; i< strip2.numPixels(); i++) {
      strip2.setPixelColor(i, Wheel(((i * 256 / strip.numPixels()) + color) & 255));
    }
    strip2.show();
#endif
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