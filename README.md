# Arduino (Nano) Program for sending WSPR with SI5351

Place a PushButton between D3 and GND as PTT or <br>

Serial Commands:
    QRG (1400-1600) to send
    <xx>m for Band (sends@1700), ie 6m..15m..2160m
    <xx>dbm
    c   to see current config
    +/- for changing Calibration +/- 1.46Hz
    S/s to send Signal@1700(for calib)
    P/p to switch on/off PA
    ESC to cancel

All this settings will be saved in the EEPROM of your Arduino and can be resetted by holding D3 LOW while Arduino starts (after reset).

D4 acts high-active as activator for the PA. (see circuit).

Needs Etherkit JTEncode Library, Etherkit SI5351 Library

Before sending, have a look into the Example si5351_calibration!

i am using two calculators for the collins-filter and they seem to be very usable:
- (for first orientation) http://www.elektronik-bastler.info/stn/lc_filter.html 
- (for fine tuning of parts) https://www.leobaumann.de/usacollins_filter.htm

i got a hint for a simulation programm from DK3BA: QUCS, needs Qt5, on github

also a good hint is to use DL1JWD Tools: 06(Pi and T Coupler)

