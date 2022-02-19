# Arduino_home_heating

Arduino sketch for home automation. Every room is C++ object.

Features:
- temperature reading from various types of sensors (DS18B20, DHT22, LM35)
- Two temperature settings for day and night time
- control of thermostatic radiator valves
- boiler activation control
- reading and setting temperatures via Ethernet using the Virtuino CM library
- real time clock DS3231 with the possibility of setting via Ethernet
- temperature hysteresis setting for each room 
