Arduino Nano Program for sending WSPR

Place a PushButton between D3 and GND as PTT or enter just the Frequency-Offset (1400-1600) via Serial Line to start.

Needs Etherkit JTEncode Library

Before sending, have a look into the Example si5351_calibration!

i am using two calculators for the collins-filter and they seem to be very usable:
- (for first orientation) http://www.elektronik-bastler.info/stn/lc_filter.html 
- (for fine tuning of parts) https://www.leobaumann.de/usacollins_filter.htm

i got a hint for a simulation programm from DK3BA: QUCS, needs Qt5, on github

