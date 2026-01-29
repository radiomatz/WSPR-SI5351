#include <si5351.h>
#include <JTEncode.h>
#include <rs_common.h>
#include <int.h>
#include <string.h>
#include "Wire.h"
#include <EEPROM.h>

/*
 * modified Code from Etherkit JTEncode Example by DM2HR, dm2hr@darc.de
*/

// Hardware defines
#define BUTTON 3
#define PWRPIN 4
#define LED_PIN LED_BUILTIN

// Class instantiation
Si5351 si5351;
JTEncode jtencode;

#define PROGID 42                         // die Antwort auf alles
#define WSPR_TONE_SPACING 146             // ~1.46 Hz
#define WSPR_DELAY 683                    // Delay value for WSPR
#define WSPR_DEFAULT_FREQ_2190m   13600UL   // 2190m
#define WSPR_DEFAULT_FREQ_630m   474200UL   // 630m
#define WSPR_DEFAULT_FREQ_160m  1836600UL   // 160m
#define WSPR_DEFAULT_FREQ_80m   3568600UL   // 80m
#define WSPR_DEFAULT_FREQ_60m   5365600UL   // 60m
#define WSPR_DEFAULT_FREQ_40m   7038600UL   // 40m
#define WSPR_DEFAULT_FREQ_30m  10138700UL  // 30m
#define WSPR_DEFAULT_FREQ_20m  14095600UL  // 20m
#define WSPR_DEFAULT_FREQ_17m  18104600UL  // 17m
#define WSPR_DEFAULT_FREQ_15m  21094600UL  // 15m
#define WSPR_DEFAULT_FREQ_12m  24924600UL  // 12m
#define WSPR_DEFAULT_FREQ_10m  28124600UL  // 10m
#define WSPR_DEFAULT_FREQ_6m   50293000UL  // 6m

// Global variables
unsigned long freq;

// #####################################
// below are all user configuratable items
// #####################################

// Calibration: see "Etherkit SI5351" Library: si5351_calibration example
// calib: lower_cal=higher_freq, 1.46 Hz =~ 100 cal
// SI5351 is very unstable in his frequency when temperature changes on its surface!

long calibration=147300L;  // at the moment, if room is warm or even not :-)

unsigned long mainQRG = WSPR_DEFAULT_FREQ_20m;
char call[13] = "DM2HR";  // size: max 12 + NULL
char loc[7] = "JN58";     // size: max 6 + NULL
uint8_t dbm = 13;
unsigned int wsprQRG = 1700;  // Standard QRG for Push Button start

// #####################################
//     end USER config
// #####################################

String sein = "";
int qrgin = -1;
unsigned long now = 0;
char c = 0;
uint8_t id = 0;

uint8_t tx_buffer[255];
uint8_t symbol_count;
uint16_t tone_delay, tone_spacing;


void setcalib(int inc) {
  calibration += inc;
  EEPROM.put(PROGID + sizeof(uint8_t) + sizeof(unsigned long)+sizeof(unsigned int), calibration);
  si5351.set_correction(calibration, SI5351_PLL_INPUT_XO);
  si5351.set_pll(SI5351_PLL_FIXED, SI5351_PLLA);
  si5351.pll_reset(SI5351_PLLB);
  si5351.set_pll(SI5351_PLL_FIXED, SI5351_PLLB);
  si5351.pll_reset(SI5351_PLLB);
  Serial.print(F("Calibration now: "));
  Serial.println(calibration);
  // Set CLK0 output
  si5351.drive_strength(SI5351_CLK0, SI5351_DRIVE_8MA);  // Set for max(8MA) power if desired
  si5351.output_enable(SI5351_CLK0, 0);                  // Disable the clock initially
}


void getconf() {
  uint8_t id;
  unsigned long f;
  unsigned int off;

  if ( digitalRead(BUTTON) == LOW ) {
    // if button pressed at startup, reset settings 
    Serial.println(F("resetting EEPROM ..."));
    Serial.flush();
    for (int i = 0 ; i < EEPROM.length() ; i++) {
      EEPROM.write(i, 0);
    }
    Serial.println(F("now press RESET!"));
    Serial.flush();
    while(1);
  }
  EEPROM.get(PROGID, id);
  if (id == PROGID) {
    EEPROM.get(PROGID + sizeof(uint8_t), f);
    if (f > 1799000UL && f < 29501000UL) {
      mainQRG = f;
    }
    EEPROM.get(PROGID + sizeof(uint8_t) + sizeof(unsigned long), off);
    if (off > 1399 && off < 1601) {
      wsprQRG = off;
    }
    EEPROM.get(PROGID + sizeof(uint8_t) + sizeof(unsigned long) + sizeof(unsigned int), calibration);
    EEPROM.get(PROGID + sizeof(uint8_t) + sizeof(unsigned long) + sizeof(unsigned int) + sizeof(unsigned long), dbm);
  }
}


void saveconf() {
  EEPROM.put(PROGID, PROGID);
  EEPROM.put(PROGID + sizeof(uint8_t), mainQRG);
  EEPROM.put(PROGID + sizeof(uint8_t) + sizeof(unsigned long), wsprQRG);
  EEPROM.put(PROGID + sizeof(uint8_t) + sizeof(unsigned long) + sizeof(unsigned int), calibration);
  EEPROM.put(PROGID + sizeof(uint8_t) + sizeof(unsigned long) + sizeof(unsigned int) + sizeof(unsigned long), dbm);
}


void printhelp() {
  Serial.println(F("Help: Enter QRG (1400-1600) to send"));
  Serial.println(F("      or <xx>m for Band (sends@1700), ie 6m..15m..2160m"));
  Serial.println(F("      or <xx>dbm"));
  Serial.println(F("      or +/- for changing Calibration +/- 1.46Hz"));
}


// for powering on a external PA, here: BS170, see Circuit description at github
void poweron ( bool onoff ) {
  if ( onoff )
    digitalWrite(PWRPIN, HIGH);
  else
    digitalWrite(PWRPIN, LOW);
}


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


void showconf() {
  Serial.print(F("Config:"));
  Serial.print(F("call="));
  Serial.print( call );
  Serial.print(F(", loc=")); 
  Serial.print( loc );
  Serial.print(F(", dBm="));
  Serial.print( dbm );
  Serial.print(F(", wsprQRG=")); 
  Serial.print( wsprQRG );
  Serial.print(F(", mainQRG=")); 
  Serial.println( mainQRG );
}


void prompt() {
    Serial.print(F("READY@") );
    Serial.print(wsprQRG);
    Serial.print(F(">"));
}



void setup() {
  Serial.begin(115200);
  Serial.setTimeout(3600000UL);
 
  // Use the Arduino's on-board LED as a keying indicator.
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  pinMode(PWRPIN, OUTPUT);
  digitalWrite(PWRPIN, LOW);

  // Use a button connected to pin deined with "BUTTON" above as a transmit trigger
  pinMode(BUTTON, INPUT_PULLUP);

  getconf();

  // Initialize the Si5351
  // Change the 2nd parameter in init if using a ref osc other
  // than 25 MHz
  while (!si5351.init(SI5351_CRYSTAL_LOAD_8PF, 0, 0)) {
    Serial.println(F("\nError init SI5351, check Cables!"));
    delay(2500);
  }
  setcalib(0);
  Serial.println(F("\nSI5351 started successfully."));

  showconf();

  printhelp();

  prompt();
}


void loop() {
  if (digitalRead(BUTTON) == LOW || Serial.available()) {
    if (Serial.available()) {

      /* is better, but has no echo 
      sein = "";
      sein = Serial.readStringUntil('\r');
      */

      do {
        if (Serial.available()) {
          c = Serial.read();
          Serial.print(c);
          
          switch(c) {
            case 0x08:
              if (c == 8 && sein.length() > 0)  // ^H, Backspace
                sein.remove(sein.length() - 1);    
              break;
            case '+':
              setcalib(100);
              break;
            case '-':
              setcalib(-100);
              break;
            case 'h':
            case '?':
              printhelp();
              goto out;
            case 'c':
              showconf();
              goto out;
            default:
              sein += c;
              break;
          }
        }
      } while (c != 0x0d && c != 0x0a);
      sein.remove(sein.length() - 1);
      
      while (Serial.available()) {  // NL, CR and any accidentally typed things
        c = Serial.read();
      }

      if ( sein.length() > 0 ) {
        if ( sein.indexOf("dbm") > 0 ) {
            int temp = sein.toInt();
            if ( temp > 0 && temp < 43 )
              dbm = temp;
            showconf();
            goto out;
        } else if ( sein.indexOf("m") > 0 ) {
          if ( sein.equals("6m") ) {
            mainQRG = WSPR_DEFAULT_FREQ_6m;
            qrgin = 1700;
          } else if ( sein.equals("10m") ) {
            mainQRG = WSPR_DEFAULT_FREQ_10m;
            qrgin = 1700;
          } else if ( sein.equals("12m") ) {
            mainQRG = WSPR_DEFAULT_FREQ_12m;
            qrgin = 1700;
          } else if ( sein.equals("15m") ) {
            mainQRG = WSPR_DEFAULT_FREQ_15m;
            qrgin = 1700;
          } else if ( sein.equals("17m") ) {
            mainQRG = WSPR_DEFAULT_FREQ_17m;
            qrgin = 1700;
          } else if ( sein.equals("20m") ) {
            mainQRG = WSPR_DEFAULT_FREQ_20m;
            qrgin = 1700;
          } else if ( sein.equals("30m") ) {
            mainQRG = WSPR_DEFAULT_FREQ_30m;
            qrgin = 1700;
          } else if ( sein.equals("40m") ) {
            mainQRG = WSPR_DEFAULT_FREQ_40m;
            qrgin = 1700;
          } else if ( sein.equals("60m") ) {
            mainQRG = WSPR_DEFAULT_FREQ_60m;
            qrgin = 1700;
          } else if ( sein.equals("80m") ) {
            mainQRG = WSPR_DEFAULT_FREQ_80m;
            qrgin = 1700;
          } else if ( sein.equals("160m") ) {
            mainQRG = WSPR_DEFAULT_FREQ_160m;
            qrgin = 1700;
          } else if ( sein.equals("630m") ) {
            mainQRG = WSPR_DEFAULT_FREQ_630m;
            qrgin = 1700;
          } else if ( sein.equals("2190m") ) {
            mainQRG = WSPR_DEFAULT_FREQ_2190m;
            qrgin = 1700;
          }
          showconf();
          goto out;
        } else { 
          qrgin = sein.toInt();
        }
      }
      // normally 1400-1600, but for testing purposes greater for not disturbing others
      if (qrgin > 0 && qrgin <= 2700) {
        wsprQRG = qrgin;
      }
    }

    freq = mainQRG + wsprQRG;
    now = millis() / 1000;

    if (wsprQRG > 0) {
      Serial.println();
      Serial.print(F(" ... sending now("));
      Serial.print( now );
      Serial.print(F(") on "));
      Serial.print( mainQRG );
      Serial.print(F(" + "));
      Serial.print( wsprQRG );
      Serial.print(F(" = "));
      Serial.println( freq );

      poweron(true);
      saveconf();
      encode();
      poweron(false);
    }

    out:

    sein = "";
    prompt();
  }
  delay(50);
}
