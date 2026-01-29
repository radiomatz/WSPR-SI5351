# Arduino (Nano) Program for sending WSPR

Place a PushButton between D3 and GND as PTT or <br>
enter just the Frequency-Offset (1400-1600) via Serial Line to start.<br>
enter <BandNr>m for changing Bands, i.E: "20m"<br>
enter <xx>dbm for the dBm Information Field<br>
enter "+" or "-" to calibrate Frequency down(+) or up(-).

All this settings will be saved in the EEPROM of your Arduino and can be resetted by holding D3 LOW while Arduino starts (after reset).

D4 acts high-active as activator for the PA. (see circuit).

Needs Etherkit JTEncode Library

Before sending, have a look into the Example si5351_calibration!

i am using two calculators for the collins-filter and they seem to be very usable:
- (for first orientation) http://www.elektronik-bastler.info/stn/lc_filter.html 
- (for fine tuning of parts) https://www.leobaumann.de/usacollins_filter.htm

i got a hint for a simulation programm from DK3BA: QUCS, needs Qt5, on github

also a good hint is to use DL1JWD Tools: 06(Pi and T Coupler)

