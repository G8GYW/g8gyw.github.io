// QRP_POWER_METER.ino - https://github.com/G8GYW/QRP-RF-power-meter
//
// Copyright (c) 2021 Mike G8GYW
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.

// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

// This program is designed to run on an Atmega328P with internal 8MHz clock and internal bandgap voltage reference.
// It takes the outputs from a Stockton bridge using a 10:1 transformer turns ratio and 1N5711 Scottky diodes,
// applies calibration factors then calculates and displays the average forward power, VSWR and battery voltage.

// ATMEGA FUSE VALUES
// E:FF   H:D7   L:E2

#include <Wire.h>
#include <Adafruit_SSD1306.h>
#include <Filter.h>

#define VERSION   "v1.05"
#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels
#define OLED_RESET    -1 // Reset pin # (or -1 if sharing Arduino reset pin)

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// DEFINE VARIABLES

float Vfwd;   // Forward voltage
float Vrev;   // Reverse voltage
float Pfwd;   // Forward power
float Prev;   // Reverse power
float SWR;    // VSWR
float Gamma;  // Reflection coefficient
float Vbat;   // Battery voltage

// CALIBRATION FACTORS
// The tolerance on the Atmega328P's internal voltage reference is 1.0 to 1.2 Volts.
// The actual voltage should be measured on pin 21 and entered below.

// The response of the 1N5711 diodes follows a curve represented by the equation:
// Pfwd = aVfwd^2 + bVfwd
// where a and b are constants determined by plotting Power In vs Vfwd and performing a curve fit.
// An excellent tool for this can be found at https://veusz.github.io/
// The values of a and b below can be adjusted if necessary to improve accuracy.

const float IntRef = 1.1;
const float a = 1.0;
const float b = 0.75;

// FUNCTIONS TO CALCULATE FORWARD AND REVERSE POWER

float CalculatePfwd ()
{
  float Vadc0 = analogRead(A0); // Read ADC0 (pin 23)
  Vadc0 = constrain(Vadc0, 1, 1023); // Prevent divide-by-zero when calculating Gamma
  Vfwd = 2.8 * ((Vadc0 + 0.5) * IntRef / 1024); // Scaled up by R3 & R4
  Pfwd = a * sq(Vfwd) + b * Vfwd;
  return Pfwd;
}

float CalculateSWR ()
{
  float Vadc1 = analogRead(A1); // Read ADC1 (pin 24)
  Vrev = 2.8 * ((Vadc1 + 0.5) * IntRef / 1024); // Scaled up by R7 & R8
  Prev = a * sq(Vrev) + b * Vrev;
  if (Prev < 0.01)
  { //exclude residual values
    Prev = 0;
  }
  Gamma = sqrt(Prev / Pfwd); // Calculate refection coefficient

  SWR = (1 + Gamma) / (1 - Gamma);
  SWR = constrain(SWR, 1, 99.9);
  return SWR;
}

// ADC FILTER
// A recursive filter removes jitter from the readings using the MegunoLINK filter library:
// https://www.megunolink.com/documentation/arduino-libraries/exponential-filter/

// Create new exponential filters with a weight of 90 and initial value of 0
// Adjust as required for a stable display

ExponentialFilter<float> FilteredPfwd(90, 0);
ExponentialFilter<float> FilteredVSWR(90, 0);

void setup()
{
  ADCSRA &= ~(bit (ADPS0) | bit (ADPS1) | bit (ADPS2)); // Clear prescaler bits
  ADCSRA |= bit (ADPS1) | bit (ADPS2);                  // Set prescaler to 64

// Enable the internal voltage reference and disable unused digital buffers
  analogReference (INTERNAL);
  bitSet (DIDR0, ADC3D);
  bitSet (DIDR0, ADC4D);
  bitSet (DIDR0, ADC5D);

  analogRead (A2);  // Read ADC2
  delay (500); // Allow ADC to settle
  float Vpot = analogRead (A2); // Read ADC again
  Vbat = 4.9 * ((Vpot + 0.5) * IntRef / 1024); // Calculate battery voltage scaled by R9 & R10

  Serial.begin(9600);
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);  //Initialize with the I2C address 0x3C.
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(0, 0);
  display.print("   POWER/SWR METER   ");
  display.setCursor(0, 16);
  display.print("    12 Watts maximum    ");
  display.setCursor(0, 32);
  display.print("Battery volts = ");
  display.print(Vbat);
  display.setCursor(0, 48);
  display.print("--------");
  display.print(VERSION);
  display.print("--------");
  display.display();
  delay(5000);        // delay 5 seconds

}

void loop()
{
  float Power = CalculatePfwd(); // Get new value of forward power
  FilteredPfwd.Filter(Power); // Apply filter to new value
  float SmoothPower = FilteredPfwd.Current(); // Return current value of filter output

  float VSWR = CalculateSWR(); // Get new value of VSWR
  FilteredVSWR.Filter(VSWR); // Apply filter to new value
  float SmoothVSWR = FilteredVSWR.Current(); // Return current value of filter output

  display.clearDisplay();
  display.setTextSize(2);
  display.setCursor (12, 12);
  display.print("PWR: ");
  display.setCursor (60, 12);
  display.print(SmoothPower, 1); // Display forward power to one decimal place
  display.print("W");
  display.setCursor (12, 36);
  display.print("SWR: ");
  display.setCursor (60, 36);
  display.print(SmoothVSWR, 1); // Display VSWR to one decimal place
  display.display();

}
