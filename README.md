# Instantaneous Power LoRaWAN Sensor on Heltec Wifi Lora 32 v2 <!-- omit from toc --> 

Forked from https://github.com/lnlp/LMIC-node, this repository supports a production IoT sensor providing 

	- instantaneous power measurements using a CT Clamp
	- temperature 
	- humidity
	
The device, Heltec WiFi LoRa 32 V2, is communicating via LoraWAN on the Helium Network (HNT) in the SF Bay Area, USA.  The sensor was also working on HNT and The Things Networks in Munich, Germany.  The impetus for this project was to confirm that several pumps were working at a site 9 time zones away with no access to WLAN and unreliable personnel.  Energy usage was not the goal, but the project could be expanded to include this in the future (a bit tricky due to low frequency of uplinks on LoRaWAN).  

<img src="docs/img/HVAC.png" width="200">

*Heltec Lora 32 monitoring HVAC blower motor*

## Contents <!-- omit from toc --> 

- [1. Background](#1-background)
- [2. Sensor hardware](#2-sensor-hardware)
- [3. LoRaWAN software](#3-lorawan-software)
- [4. Open Energy Monitor and CT Clamp](#4-open-energy-monitor-and-ct-clamp)
	- [4.1. Calibration](#41-calibration)
- [5. LoRaWAN Service Provider](#5-lorawan-service-provider)
- [6. Sensor Data](#6-sensor-data)
	- [6.1. Google sheets](#61-google-sheets)
- [7. Troubleshooting and notes](#7-troubleshooting-and-notes)
	- [7.1. CT Clamp](#71-ct-clamp)
	- [7.2. LoRaWAN](#72-lorawan)
		- [7.2.1. Antenna](#721-antenna)
		- [7.2.2. Helium Network - Staging vs Production](#722-helium-network---staging-vs-production)
		- [7.2.3. Other](#723-other)
	- [7.3. Off the shelf products](#73-off-the-shelf-products)
- [8. Known Issues](#8-known-issues)

## 1. Background

The primary goal of this project is to monitor a remote pump to insure it is functioning.  Access to AC power electrical wiring is possible, which led to the exploration of a using a CT Clamp to monitor instantaneous power.  
	
After exploring the possible communication alternatives for monitoring a remote sensor, LoRaWAN was decided on for testing.   WLAN was not accessible and GPRS/GSM/LTE/4G/5G would be comparitively expensive.  Helium uses data credits (DC), 1 DC = 24 Byte Packet = $0.00001 USD, which equates to about $0.25 per year. Other methods probably exist, but were not explored.   

## 2. Sensor hardware

Familiarity with the ESP8266 - NodeMCU led me to look for an ESP type sensor.  In fact the project started with the ESP8266 and later migrated to LoRaWAN on the Heltec device.  The Heltec WiFi LoRa 32 V2 is ESP32 based.   The integrated LoRaWAN hardware turned out to be an excellent choice to reduce development time, since the LoRaWAN radio and ESP are built into the development board (no wiring required).  Onboard display is also nice for troubleshooting.

## 3. LoRaWAN software

Software that easily supported existing LoRaWAN services was a bit tricky.  I first explored Heltec's proprietary code (requires a license key).  Point to point examples exist, but the sensor needs to communicate with a service provider.  The Things Network forum which led me to the base code for the project LMIC-node.
	
https://www.thethingsnetwork.org/forum/t/example-tutorial-of-connecting-a-heltec-v2-to-ttn/58930
	
Later I found this excellent repository, forked from the LMIC-node code:  
	
https://github.com/Chiumanfu/LMIC-node_Sensor-for-Helium-Network
	
Full of tips and tricks for deploying on the Helium Network.  Especially useful is the example byte order for DEVEUI and APPEUI within HNT, as well as, the TagoIO integration examples.

## 4. Open Energy Monitor and CT Clamp 

Nice write up on how to monitor energy usage.   This became the starting point for this project.
		
https://savjee.be/blog/Home-Energy-Monitor-ESP32-CT-Sensor-Emonlib/
	
### 4.1. Calibration 

Calibration of the CT clamp is required.  Good background information here:
	
https://community.openenergymonitor.org/t/calibration-and-parameters-in-emonlib/6855
			
Trial and error seemed to be the fastest way to calibrate using a volt meter with CT clamp and the sensor side by side.

<img src="docs/img/calibrate.png" width="200">

*CT clamp calibration*

## 5. LoRaWAN Service Provider

The final location of this device is the SF Bay Area.  As of late 2022, The Things Network (TTN) has very little coverage in the Bay Area.  Although TTN was tested in Munich, it was quickly abandoned in favor of HNT, which is supported widely in the Bay Area.
	
<img src="docs/img/TTN-BayArea-Coverage.png" width="400">

*TTN Coverage Map*
	
	
<img src="docs/img/HNT-BayArea-Coverage.png" width="400">

*HNT Coverage Map*
[Explorer] (https://explorer.helium.com/)

	
## 6. Sensor Data
	
Many ways to display the uplink data.  The HNT website has good information for various 3rd party integrations. Both Google Sheets and TagoIO integrations were implemented for this project.  TagoIO gives quick access to raw payload, hotspot name, RSSI, SNR and SF.  Tagoio.js in the payload-formatters directory works for this device.
	
### 6.1. Google sheets
	
Most flexible way to show data.  HNT has a good tutorial here : [Google Sheets Tutorial](https://docs.helium.com/use-the-network/console/integrations/google-sheets/).  Live sensor data here showing pump iRMS:  [Pump iRMS](https://docs.google.com/spreadsheets/d/e/2PACX-1vQj1dn7G-eRcmEB5HmqzgXQ_rtUPW68QRESnKMKsQ1TotCIrERJMUct81eylsNTOlBHo3MNF0EYcU-y/pubchart?oid=1019867124&amp;format=interactive).
	
I was also curious how many LoRaWAN uplinks were dropped, so I'm tracking time between uplinks here: [Uplink Intervals](https://docs.google.com/spreadsheets/d/e/2PACX-1vQj1dn7G-eRcmEB5HmqzgXQ_rtUPW68QRESnKMKsQ1TotCIrERJMUct81eylsNTOlBHo3MNF0EYcU-y/pubhtml?gid=776933052&amp;single=true&amp;widget=true&amp;headers=false).   In January 2023 this chart shows two power outages (one lasting 505 minutes) and 4 lost uplinks at 15 minute intervals.  On January 18th I sent a downlink command and changed to uplink interval to 30 minutes and 1 uplink was lost for the remainder of January.

	
<img src="docs/img/Lora-tx-interval.png" width="400">

*Lost uplinks and power outages*

## 7. Troubleshooting and notes
	
	
### 7.1. CT Clamp

- Best to start with known power, ie 1500 watt hair dryer, 800 watt microwave
- Calibration with emonlib is critical; The SCT 013-030 has a built in burden resistor measured to be 58 ohm
- Only clamp one AC leg

### 7.2. LoRaWAN

#### 7.2.1. Antenna 
Different antenna required for EU and US.   Best position for LoRaWAN is veritical.
- EU - 868 Mhz
- US - 915 Mhz

When device is behind barriers it will take longer to join network; assume LMIC code starts at SF7 and adjusts toward SF12.  In Munich the breaker panel was located in the middle of the building resulting in a SF11 at join after a long time (30+ minutes).  When testing near a window the join was immediate at SF7.

#### 7.2.2. Helium Network - Staging vs Production

Staging worked flawlessly in Munich for 2 months and several days in US.  Within 3 hours of final placement in the US, staging stop working for several days. Recommend purchasing DC credits (minimal cost) and get on production console early to avoid headaches.

- [Staging] (https://staging-console.helium.wtf/)

- [Production] https://console.helium.com/)

#### 7.2.3. Other 
- [TTN LoRaWAN intro] (https://www.thethingsnetwork.org/docs/lorawan/)
- [LoRaWAN Utilization] (https://www.r-bloggers.com/2022/03/data-science-on-blockchain-with-r-part-iii-helium-based-iot-is-taking-the-world/)

### 7.3. Off the shelf products
	
- [Netvox] (http://www.netvox.com.tw/product.asp?pro=R718N17#) (120 euro)
- [Watteco] (https://www.watteco.com/product/intenso-sensor-lorawan/)
- [Sense] (https://sense.com/) (power monitor; not LoRaWAN)

## 8. Known Issues
- [Issues] (https://github.com/craigea/lorawan-heltec-sensor/issues)

	
