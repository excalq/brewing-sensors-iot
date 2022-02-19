# Brewing Sensors via ESP32 to MQTT and ELK

_**Author**: Arthur Kepler for Oort Cloud Brewing (our brew collective)_

_**Date**_: Published October 11, 2020

## Description

This set of two sensors, which share a breadboard with an ESP32 devboard, captures
temperature, humidity, and gas pressure, reporting it back to an MQTT and ELK stack server.

The pressure sensor is connected to a tee fitting between the CO2 tank and the keg.

## Requirements

### Hardware
* A small breadboard and about 60cm (2 ft) of wiring
* ESP32 dev board (PIN comments are for NodeMCU32 dev board) (https://www.amazon.com/dp/B0718T232Z)
* DHT11 Temperature sensor (https://www.adafruit.com/product/386)
* 60psi 5v-in/4.5v-out pressure transducer. _60 is a good upper bound for brewing with nitro, 1/4" is good for gas line fittings. Amazon does not have these, most US OEMs charge > $120. I recommend eBay or Google Shopping._ (https://www.google.com/search?q=pressure+transducer+60+psi+5v&tbm=shop)

### Software
* Arduino IDE (or your favorite IoT IDE, etc.)
* MQTT broker on a host server (Tested with Mosquitto)
* Python3 (Tested on v3.5) on host server
* ELK Stack (Optional) (index export needs 6.5+, Kibana Viz export needs 7.3+)
_It's easy to recreate the index and visualization without importing my `.ndjson` export._

### Arduino Libraries
* Wifi by Ardino
* PubSub Client by Nick O'Leary (Arduino Client for MQTT) (https://pubsubclient.knolleary.net/)
* DHT-sensor-library by Adafruit

### Pip Modules

```bash
pip3 install paho-mqtt
pip3 install elasticsearch
```

## Components

1. `arduino-ide-code/brewing-sensors.ino`: Arduino C++ code, tested and uploaded via Arudino IDE
2. `ELK/mqtt-to-elasticearch.py`: Python script to read from MQTT and publish to Elasticsearch
3. `ELK/kibana-chart-export.ndjson`: Export of Kibana saved-objects for the oort_sensors-* 
 
## Install Notes

Update `brewing-sensors.ino` with your WIFI and MQTT server config.

Optionally change references of `oort_sensors-*` to an ELK index name of your choosing.
