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
#define PWRPIN 2
#define LED_PIN LED_BUILTIN
#define PROGID 42  // the answer to the universe and to all 

// Class instantiation
Si5351 si5351;
JTEncode jtencode;

#define WSPR_TONE_SPACING 146             // ~1.46 Hz
#define WSPR_DELAY 683                    // Delay value for WSPR
#define WSPR_DEFAULT_FREQ_2190m 136000UL  // 2190m
#define WSPR_DEFAULT_FREQ_630m 474200UL   // 630m
#define WSPR_DEFAULT_FREQ_160m 1836600UL  // 160m
#define WSPR_DEFAULT_FREQ_80m 3568600UL   // 80m
#define WSPR_DEFAULT_FREQ_60m 5364700UL   // 60m
#define WSPR_DEFAULT_FREQ_40m 7038600UL   // 40m
#define WSPR_DEFAULT_FREQ_30m 10138700UL  // 30m
#define WSPR_DEFAULT_FREQ_20m 14095600UL  // 20m
#define WSPR_DEFAULT_FREQ_17m 18104600UL  // 17m
#define WSPR_DEFAULT_FREQ_15m 21094600UL  // 15m
#define WSPR_DEFAULT_FREQ_12m 24924600UL  // 12m
#define WSPR_DEFAULT_FREQ_10m 28124600UL  // 10m
#define WSPR_DEFAULT_FREQ_6m 50293000UL   // 6m

// Global variables
unsigned long freq;

// #####################################
// below are all user configuratable items
// #####################################

// Calibration: see "Etherkit SI5351" Library: si5351_calibration example
// calib: lower_cal=higher_freq, 1.46 Hz =~ 100 cal
// SI5351/resp the quartz is very unstable in his frequency when temperature changes on its surface!

long calibration = 74850L;  // 147300L;  // at the moment, if room is warm or even not :-)

unsigned long mainQRG = WSPR_DEFAULT_FREQ_20m;
char call[13] = "DM2HR";  // size: max 12 + NULL
char loc[7] = "JN58II";     // size: max 6 + NULL
uint8_t dbm = 13;
unsigned int wsprQRG = 1700;  // Standard QRG for Push Button start

// #####################################
//     end USER config
// #####################################

String sein = "";
int qrgin = -1;

#define DELAY 50
unsigned long now = 0, offset = 0, ltime = 0;
uint8_t lmin = 0, lsec = 0, oldsec = 0;
char c = 0;
uint8_t id = 0;

uint8_t tx_buffer[255];
uint8_t symbol_count;
uint16_t tone_delay, tone_spacing;
bool out = false;
bool iauto = false;
uint8_t intervall = 0;

void setcalib(int inc) {
  calibration += inc;
  EEPROM.put(PROGID + sizeof(uint8_t) + sizeof(unsigned long) + sizeof(unsigned int), calibration);
  si5351.set_correction(calibration, SI5351_PLL_INPUT_XO);
  si5351.set_pll(SI5351_PLL_FIXED, SI5351_PLLA);
  si5351.pll_reset(SI5351_PLLB);
  si5351.set_pll(SI5351_PLL_FIXED, SI5351_PLLB);
  si5351.pll_reset(SI5351_PLLB);
  Serial.print(F("*Calibration now: "));
  Serial.println(calibration);
  // Set CLK0 output
  si5351.drive_strength(SI5351_CLK0, SI5351_DRIVE_8MA);  // Set for max(8MA) power if desired
}


void getconf() {
  uint8_t id;
  unsigned long f;
  unsigned int off;

  if (sein.compareTo("RESET") == 0 || digitalRead(BUTTON) == LOW) {
    // if button pressed at startup, reset settings
    Serial.println(F("resetting EEPROM ..."));
    Serial.flush();
    for (int i = 0; i < EEPROM.length(); i++) {
      EEPROM.write(i, 0);
    }
    Serial.println(F("now press RESET!"));
    Serial.flush();
    while (1);
  }
  EEPROM.get(PROGID, id);
  if (id == PROGID) {
    EEPROM.get(PROGID + sizeof(uint8_t), f);
    mainQRG = f;
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
  Serial.println(F("*Help - Enter:"));
  Serial.println(F("  <QRG> (1400-1600) or <RETURN> to send"));
  Serial.println(F("  <xx>m for Band: [6m/10m/12m/15m/17m/20m/30m/40m/60m/80m/160m/630m/2190m]"));
  Serial.println(F("  <xx>dbm for setting db info field"));
  Serial.println(F("  auto <i>/off for sending automatically every <i> / off minutes"));
  Serial.println(F("  c   to see current config"));
  Serial.println(F("  +/- for changing Calibration -/+ 1.46Hz"));
  Serial.println(F("  S/s to send Signal@1700(for calib) ON/off"));
  Serial.println(F("  P/p to switch ON/off PA"));
  Serial.println(F("  RESET to set EEPROM to defaults"));
  Serial.println(F("  <ESC> to cancel input"));
}


// for powering on a external PA, here: BS170, see Circuit description at github
void poweron(bool onoff) {
  if (onoff)
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
  Serial.print(F("*Config:"));
  Serial.print(F("call="));
  Serial.print(call);
  Serial.print(F(", loc="));
  Serial.print(loc);
  Serial.print(F(", dBm="));
  Serial.print(dbm);
  Serial.print(F(", wsprQRG="));
  Serial.print(wsprQRG);
  Serial.print(F(", mainQRG="));
  Serial.println(mainQRG);
}


void prompt() {
  if ( !iauto )
    Serial.print(F("READY@"));
  else {
    disptime();
    Serial.print('@');
  }
  Serial.print(wsprQRG);
  Serial.print(F(">"));
}


void disptime() {
  if ( lmin <  10 )
    Serial.print('0');
  Serial.print(lmin);
  Serial.print(':');
  if ( lsec < 10 )
    Serial.print('0');
  Serial.print(lsec);
}


void printtime(unsigned long t) {
static unsigned long ulrest;
  lmin = t / 60000;
  lsec = (t / 1000) % 60;
  ulrest = t - lmin * 60000 - lsec * 1000;
  disptime();
  Serial.print('.');
  Serial.print(ulrest);
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
  Serial.println(F("\ninit SI5351 now...."));
  while (!si5351.init(SI5351_CRYSTAL_LOAD_8PF, 0, 0)) {
    Serial.println(F("\nError init SI5351, check Cables!"));
    delay(2500);
  }
  Serial.println(F("\nSI5351 started successfully."));

  setcalib(0);
  si5351.output_enable(SI5351_CLK0, 0);  // Disable the clock initially

  showconf();

  printhelp();

  prompt();
}


void loop() {
static bool run = false;

  out = false;

  if ( iauto && offset > 0 ) {
    now = millis();
    ltime = now - offset;
    lmin = ltime / 60000;
    lsec = (ltime / 1000) % 60;
    if ( ltime % (intervall * 60000UL) <= DELAY ) {
      run = true;
    } else {
      run = false;
      if ( oldsec != lsec ) {
        oldsec = lsec;
        Serial.print(F("\r"));
        prompt();
      }
    }
  }

  if (digitalRead(BUTTON) == LOW || Serial.available() || run ) {

    if (Serial.available()) {
      do {

        if (Serial.available()) {
          c = Serial.read();
          Serial.print(c);

          switch (c) {
            case 27:   // ESC
            case 127:  // DEL (sometimes)
              Serial.println(F("*"));
              out = true;
              break;

            case '+':
              setcalib(50);
              out = true;
              break;
            case '-':
              setcalib(-50);
              out = true;
              break;

            case 'h':
            case '?':
              printhelp();
              out = true;
              break;

            case 'c':
              showconf();
              out = true;
              break;

            case 'P':
              Serial.println(F("*PA=ON"));
              poweron(true);
              out = true;
              break;
            case 'p':
              Serial.println(F("*PA=OFF"));
              poweron(false);
              out = true;
              break;

            case 'S':
              digitalWrite(LED_PIN, HIGH);
              Serial.println(F("*Signal ON @ 1700"));
              si5351.output_enable(SI5351_CLK0, 1);
              si5351.set_freq((mainQRG + 1700UL) * 100UL, SI5351_CLK0);
              out = true;
              break;
            case 's':
              digitalWrite(LED_PIN, LOW);
              Serial.println(F("*Signal=OFF"));
              si5351.output_enable(SI5351_CLK0, 0);
              out = true;
              break;

            case 0x08:  // ^H, Backspace
              if (sein.length() > 0)
                sein.remove(sein.length() - 1);
              break;

            default:
              sein += c;
              break;
          }
        }
      } while (!out && c != 0x0d && c != 0x0a);
    }

    if (!out) {

      while (Serial.available()) {  // NL, CR and any accidentally typed things
        c = Serial.read();
      }

      if (sein.length() > 0) {

        sein.remove(sein.length() - 1);  // remove CR
        Serial.println();

        if ( sein.indexOf("auto") >= 0 ) {
          iauto = true;
          if ( sein.indexOf("off") < 0 )
            intervall = sein.substring(4).toInt();
          else {
            iauto = false;
          }
          if ( iauto ) {
            if ( offset == 0 ) {
              Serial.println(F("First run manual for setting start time"));
              iauto = false;
            } else {
              if ( intervall > 9 ) {
                intervall /= 2;
                intervall *= 2; // for getting start  of period
                Serial.print(F("... sending every "));
                Serial.print(intervall);
                Serial.println(F(" Minutes or until \"auto off\"."));
              } else {
                Serial.println(F("Intervall too small (at least 10 Min)"));
                iauto = false;
              }
            }
          }
          out = true;
        } else if (sein.indexOf("dbm") > 0) {
          int temp = sein.toInt();
          if (temp >= 0 && temp < 43)
            dbm = temp;
          showconf();
          out = true;
        } else if (sein.compareTo("RESET") == 0) {
          getconf();
          out = true;
        } else if (sein.indexOf("m") > 0) {
          out = true;
          if (sein.equals("6m")) {
            mainQRG = WSPR_DEFAULT_FREQ_6m;
          } else if (sein.equals("10m")) {
            mainQRG = WSPR_DEFAULT_FREQ_10m;
          } else if (sein.equals("12m")) {
            mainQRG = WSPR_DEFAULT_FREQ_12m;
          } else if (sein.equals("15m")) {
            mainQRG = WSPR_DEFAULT_FREQ_15m;
          } else if (sein.equals("17m")) {
            mainQRG = WSPR_DEFAULT_FREQ_17m;
          } else if (sein.equals("20m")) {
            mainQRG = WSPR_DEFAULT_FREQ_20m;
          } else if (sein.equals("30m")) {
            mainQRG = WSPR_DEFAULT_FREQ_30m;
          } else if (sein.equals("40m")) {
            mainQRG = WSPR_DEFAULT_FREQ_40m;
          } else if (sein.equals("60m")) {
            mainQRG = WSPR_DEFAULT_FREQ_60m;
          } else if (sein.equals("80m")) {
            mainQRG = WSPR_DEFAULT_FREQ_80m;
          } else if (sein.equals("160m")) {
            mainQRG = WSPR_DEFAULT_FREQ_160m;
          } else if (sein.equals("630m")) {
            mainQRG = WSPR_DEFAULT_FREQ_630m;
          } else if (sein.equals("2190m")) {
            mainQRG = WSPR_DEFAULT_FREQ_2190m;
          } else {
            Serial.print(F("!!! Unknown Band: "));
            Serial.println(sein);
          }
          saveconf();
          showconf();
          out = true;
        } else {
          qrgin = sein.toInt();
        }

        // normally 1400-1600, but for testing purposes greater for not disturbing others
        if (qrgin > 0 && qrgin <= 2700) {
          wsprQRG = qrgin;
        }
      }

      if (!out) {
        freq = mainQRG + wsprQRG;
        now = millis();
        if ( !iauto && offset == 0 ) {
          offset = now;
        }
        ltime = now - offset;
        lmin = ltime / 60000UL;
        lsec = (ltime / 1000) % 60L;

        if (wsprQRG > 0) {
          Serial.print(F(" ... sending now("));
          printtime(millis());  
          Serial.print(F(") on "));
          Serial.print(mainQRG);
          Serial.print(F(" + "));
          Serial.print(wsprQRG);
          Serial.print(F(" = "));
          Serial.println(freq);

          poweron(true);
          saveconf();
          encode();
          poweron(false);
        }
      }
    }
    sein = "";
    prompt();
  }
  delay(DELAY);
}
