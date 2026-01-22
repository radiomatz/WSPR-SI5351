#include <si5351.h>
#include <JTEncode.h>
#include <rs_common.h>
#include <int.h>
#include <string.h>
#include "Wire.h"

// Hardware defines
#define BUTTON 3
#define LED_PIN LED_BUILTIN

// Class instantiation
Si5351 si5351;
JTEncode jtencode;

// Global variables
unsigned long freq;

#define WSPR_TONE_SPACING 146  // ~1.46 Hz
#define WSPR_DELAY 683         // Delay value for WSPR
#define WSPR_DEFAULT_FREQ_40m  7038600UL // 40m
#define WSPR_DEFAULT_FREQ_20m 14095600UL // 20m

// #####################################
// below are all user configuratable items
// #####################################

// Calibration: see "Etherkit SI5351" Library: si5351_calibration example
#define CALIBRATION 147400L // calib: lower_cal=higher_freq, 1 Hz =~ 100 cal

unsigned long mainQRG = WSPR_DEFAULT_FREQ_20m;
char call[13] = "DM2HR"; // size: max 12 + NULL
char loc[7] = "JN58";    // size: max 6 + NULL
uint8_t dbm = 15;
unsigned int wsprQRG = 1700; // Standard QRG for Push Button start

// #####################################
//     end USER config
// #####################################

String sein;
int qrgin = -1;
unsigned long now = 0;
char c;
uint8_t id = 0;
uint8_t tx_buffer[255];
uint8_t symbol_count;
uint16_t tone_delay, tone_spacing;


// Loop through the string, transmitting one character at a time.
void encode() {
  static uint8_t i;

  // Encode the message in the transmit buffer
  // This is RAM intensive and should be done separately from other subroutines
  // Set the proper frequency, tone spacing, symbol count, and
  // tone delay depending on mode
  symbol_count = WSPR_SYMBOL_COUNT;  // From the library defines
  tone_spacing = WSPR_TONE_SPACING;
  tone_delay = WSPR_DELAY;

  // Clear out the transmit buffer
  memset(tx_buffer, 0, 255);
  // Set the proper frequency and timer CTC depending on mode
  jtencode.wspr_encode(call, loc, dbm, tx_buffer);


  // Reset the tone to the base frequency and turn on the output
  si5351.output_enable(SI5351_CLK0, 1);
  digitalWrite(LED_PIN, HIGH);

  // Now transmit the channel symbols
  for (i = 0; i < symbol_count; i++) {
    si5351.set_freq((freq * 100) + (tx_buffer[i] * tone_spacing), SI5351_CLK0);
    delay(tone_delay);
  }

  // Turn off the output
  si5351.output_enable(SI5351_CLK0, 0);
  digitalWrite(LED_PIN, LOW);
}


void showconf(){
    Serial.println("\nConfiguration:");
    Serial.println("*call=" + String(call));
    Serial.println("*loc=" + String(loc));
    Serial.println("*dbm=" + String(dbm));
    Serial.println("*wsprQRG=" + String(wsprQRG));
    Serial.println("*mainQRG=" + String(mainQRG));
}


void setup() {
  Serial.begin(115200);
  Serial.setTimeout(3600000UL);
  // Initialize the Si5351
  // Change the 2nd parameter in init if using a ref osc other
  // than 25 MHz
  if (!si5351.init(SI5351_CRYSTAL_LOAD_8PF, 0, 0))
    Serial.println("Error init!");
  si5351.set_correction(CALIBRATION, SI5351_PLL_INPUT_XO);
  si5351.set_pll(SI5351_PLL_FIXED, SI5351_PLLA);
  si5351.pll_reset(SI5351_PLLB);
  si5351.set_pll(SI5351_PLL_FIXED, SI5351_PLLB);
  si5351.pll_reset(SI5351_PLLB);
  // si5351.set_correction(CALIBRATION, SI5351_PLL_INPUT_XO);

  // Use the Arduino's on-board LED as a keying indicator.
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  // Use a button connected to pin 12 as a transmit trigger
  pinMode(BUTTON, INPUT_PULLUP);

  showconf(); // TODO: what to do when with our saved config

  // Set CLK0 output
  si5351.drive_strength(SI5351_CLK0, SI5351_DRIVE_8MA);  // Set for max(8MA) power if desired
  si5351.output_enable(SI5351_CLK0, 0);                  // Disable the clock initially
  Serial.print("READY>");
}


void loop() {
  // Debounce the button and trigger TX on push
  if (digitalRead(BUTTON) == LOW || Serial.available()) {
    delay(50);  // delay to debounce
    if (digitalRead(BUTTON) == LOW || Serial.available()) {
      if (Serial.available()) {
        sein = "";
        do {
          if ( Serial.available() ) {
            c = Serial.read();
            Serial.print(c);
            sein += c;
          }
        } while ( c != 0x0d && c != 0x0a);
        while (Serial.available()) {  // NL, CR and any accidentally typed things
          c = Serial.read();
          Serial.print(c);
        }
        if ( sein != "" ) {
            qrgin = sein.toInt();
        }
        // normally 1400-1600, but for testing purposes greater for not disturbing others
        if (qrgin > 0 && qrgin <= 2700) {
          wsprQRG = qrgin;
        }
      }
      freq = mainQRG + wsprQRG;
      now = millis() / 1000;
      if ( wsprQRG > 0 ) {
        Serial.println("\r ... sending now(" + 
            String(now) + ") on " + 
            String(mainQRG) + " + " + 
            String(wsprQRG) + " = " + 
            String(freq) );
        encode();
      }
      sein = "";
      Serial.print("READY>");
      delay(50);  //delay to avoid extra triggers
    }
  }
  delay(50);  //delay to avoid extra triggers
}
