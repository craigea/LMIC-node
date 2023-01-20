/*******************************************************************************
 *
 *  File:          LMIC-node.cpp
 * 
 *  Function:      LMIC-node main application file.
 * 
 *  Copyright:     Copyright (c) 2021 Leonel Lopes Parente
 *                 Copyright (c) 2018 Terry Moore, MCCI
 *                 Copyright (c) 2015 Thomas Telkamp and Matthijs Kooijman
 *
 *                 Permission is hereby granted, free of charge, to anyone 
 *                 obtaining a copy of this document and accompanying files to do, 
 *                 whatever they want with them without any restriction, including,
 *                 but not limited to, copying, modification and redistribution.
 *                 The above copyright notice and this permission notice shall be 
 *                 included in all copies or substantial portions of the Software.
 * 
 *                 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT ANY WARRANTY.
 * 
 *  License:       MIT License. See accompanying LICENSE file.
 * 
 *  Author:        Leonel Lopes Parente
 * 
 *  Description:   To get LMIC-node up and running no changes need to be made
 *                 to any source code. Only configuration is required
 *                 in platform-io.ini and lorawan-keys.h.
 * 
 *                 If you want to modify the code e.g. to add your own sensors,
 *                 that can be done in the two area's that start with
 *                 USER CODE BEGIN and end with USER CODE END. There's no need
 *                 to change code in other locations (unless you have a reason).
 *                 See README.md for documentation and how to use LMIC-node.
 * 
 *                 LMIC-node uses the concepts from the original ttn-otaa.ino 
 *                 and ttn-abp.ino examples provided with the LMIC libraries.
 *                 LMIC-node combines both OTAA and ABP support in a single example,
 *                 supports multiple LMIC libraries, contains several improvements
 *                 and enhancements like display support, support for downlinks,
 *                 separates LoRaWAN keys from source code into a separate keyfile,
 *                 provides formatted output to serial port and display
 *                 and supports many popular development boards out of the box.
 *                 To get a working node up and running only requires some configuration.
 *                 No programming or customization of source code required.
 * 
 *  Dependencies:  External libraries:
 *                 MCCI LoRaWAN LMIC library  https://github.com/mcci-catena/arduino-lmic
 *                 IBM LMIC framework         https://github.com/matthijskooijman/arduino-lmic  
 *                 U8g2                       https://github.com/olikraus/u8g2
 *                 EasyLed                    https://github.com/lnlp/EasyLed
 *
 ******************************************************************************/

#include "LMIC-node.h"


//  █ █ █▀▀ █▀▀ █▀▄   █▀▀ █▀█ █▀▄ █▀▀   █▀▄ █▀▀ █▀▀ ▀█▀ █▀█
//  █ █ ▀▀█ █▀▀ █▀▄   █   █ █ █ █ █▀▀   █▀▄ █▀▀ █ █  █  █ █
//  ▀▀▀ ▀▀▀ ▀▀▀ ▀ ▀   ▀▀▀ ▀▀▀ ▀▀  ▀▀▀   ▀▀  ▀▀▀ ▀▀▀ ▀▀▀ ▀ ▀
#include "EmonLib.h"                   // Include Emon Library
#include <ESP32_LoRaWAN.h>
#include "myWiFi.h"
#include <SimpleDHT.h>


byte pinDHT22 = 23;                     // pin 13 and 23 (GPIO23) works with DHT22
float temperature = 0;
float humidity = 0;
SimpleDHT22 dht22;

EnergyMonitor emon1;                   // Create an instance
double Irms;                           // global                     
#define CT_CLAMP_PIN 36                // pin 13 and 36 work on simple loop code
//define MQTT                            // if not defined, then wifi not used


const uint8_t payloadBufferLength = 6;    // Adjust to fit max payload length.  2 bytes mA, 2 bytes temp, 2 bytes humidity


//  █ █ █▀▀ █▀▀ █▀▄   █▀▀ █▀█ █▀▄ █▀▀   █▀▀ █▀█ █▀▄
//  █ █ ▀▀█ █▀▀ █▀▄   █   █ █ █ █ █▀▀   █▀▀ █ █ █ █
//  ▀▀▀ ▀▀▀ ▀▀▀ ▀ ▀   ▀▀▀ ▀▀▀ ▀▀  ▀▀▀   ▀▀▀ ▀ ▀ ▀▀ 


uint8_t payloadBuffer[payloadBufferLength];
static osjob_t doWorkJob;
uint32_t doWorkIntervalSeconds = DO_WORK_INTERVAL_SECONDS;  // Change value in platformio.ini

// Note: LoRa module pin mappings are defined in the Board Support Files.

// Set LoRaWAN keys defined in lorawan-keys.h.
#ifdef OTAA_ACTIVATION
    static const u1_t PROGMEM DEVEUI[8]  = { OTAA_DEVEUI } ;
    static const u1_t PROGMEM APPEUI[8]  = { OTAA_APPEUI };
    static const u1_t PROGMEM APPKEY[16] = { OTAA_APPKEY };
    // Below callbacks are used by LMIC for reading above values.
    void os_getDevEui (u1_t* buf) { memcpy_P(buf, DEVEUI, 8); }
    void os_getArtEui (u1_t* buf) { memcpy_P(buf, APPEUI, 8); }
    void os_getDevKey (u1_t* buf) { memcpy_P(buf, APPKEY, 16); }    
#else
    // ABP activation
    static const u4_t DEVADDR = ABP_DEVADDR ;
    static const PROGMEM u1_t NWKSKEY[16] = { ABP_NWKSKEY };
    static const u1_t PROGMEM APPSKEY[16] = { ABP_APPSKEY };
    // Below callbacks are not used be they must be defined.
    void os_getDevEui (u1_t* buf) { }
    void os_getArtEui (u1_t* buf) { }
    void os_getDevKey (u1_t* buf) { }
#endif


int16_t getSnrTenfold()
{
    // Returns ten times the SNR (dB) value of the last received packet.
    // Ten times to prevent the use of float but keep 1 decimal digit accuracy.
    // Calculation per SX1276 datasheet rev.7 §6.4, SX1276 datasheet rev.4 §6.4.
    // LMIC.snr contains value of PacketSnr, which is 4 times the actual SNR value.
    return (LMIC.snr * 10) / 4;
}


int16_t getRssi(int8_t snr)
{
    // Returns correct RSSI (dBm) value of the last received packet.
    // Calculation per SX1276 datasheet rev.7 §5.5.5, SX1272 datasheet rev.4 §5.5.5.

    #define RSSI_OFFSET            64
    #define SX1276_FREQ_LF_MAX     525000000     // per datasheet 6.3
    #define SX1272_RSSI_ADJUST     -139
    #define SX1276_RSSI_ADJUST_LF  -164
    #define SX1276_RSSI_ADJUST_HF  -157

    int16_t rssi;

    #ifdef MCCI_LMIC

        rssi = LMIC.rssi - RSSI_OFFSET;

    #else
        int16_t rssiAdjust;
        #ifdef CFG_sx1276_radio
            if (LMIC.freq > SX1276_FREQ_LF_MAX)
            {
                rssiAdjust = SX1276_RSSI_ADJUST_HF;
            }
            else
            {
                rssiAdjust = SX1276_RSSI_ADJUST_LF;   
            }
        #else
            // CFG_sx1272_radio    
            rssiAdjust = SX1272_RSSI_ADJUST;
        #endif    
        
        // Revert modification (applied in lmic/radio.c) to get PacketRssi.
        int16_t packetRssi = LMIC.rssi + 125 - RSSI_OFFSET;
        if (snr < 0)
        {
            rssi = rssiAdjust + packetRssi + snr;
        }
        else
        {
            rssi = rssiAdjust + (16 * packetRssi) / 15;
        }
    #endif

    return rssi;
}


void printEvent(ostime_t timestamp, 
                const char * const message, 
                PrintTarget target = PrintTarget::All,
                bool clearDisplayStatusRow = true,
                bool eventLabel = false)
{
    #ifdef USE_DISPLAY 
        if (target == PrintTarget::All || target == PrintTarget::Display)
        {
            display.clearLine(TIME_ROW);
            display.setCursor(COL_0, TIME_ROW);
            display.print(F("Time:"));                 
            display.print(timestamp); 
            display.clearLine(EVENT_ROW);
            if (clearDisplayStatusRow)
            {
                display.clearLine(STATUS_ROW);    
            }
            display.setCursor(COL_0, EVENT_ROW);               
            display.print(message);
        }
    #endif  
    
    #ifdef USE_SERIAL
        // Create padded/indented output without using printf().
        // printf() is not default supported/enabled in each Arduino core. 
        // Not using printf() will save memory for memory constrainted devices.
        String timeString(timestamp);
        uint8_t len = timeString.length();
        uint8_t zerosCount = TIMESTAMP_WIDTH > len ? TIMESTAMP_WIDTH - len : 0;

        if (target == PrintTarget::All || target == PrintTarget::Serial)
        {
            printChars(serial, '0', zerosCount);
            serial.print(timeString);
            serial.print(":  ");
            if (eventLabel)
            {
                serial.print(F("Event: "));
            }
            serial.println(message);
        }
    #endif   
}           

void printEvent(ostime_t timestamp, 
                ev_t ev, 
                PrintTarget target = PrintTarget::All, 
                bool clearDisplayStatusRow = true)
{
    #if defined(USE_DISPLAY) || defined(USE_SERIAL)
        printEvent(timestamp, lmicEventNames[ev], target, clearDisplayStatusRow, true);
    #endif
}


void printFrameCounters(PrintTarget target = PrintTarget::All)
{
    #ifdef USE_DISPLAY
        if (target == PrintTarget::Display || target == PrintTarget::All)
        {
            display.clearLine(FRMCNTRS_ROW);
            display.setCursor(COL_0, FRMCNTRS_ROW);
            display.print(F("Up:"));
            display.print(LMIC.seqnoUp);
            display.print(F(" Dn:"));
            display.print(LMIC.seqnoDn);        
        }
    #endif

    #ifdef USE_SERIAL
        if (target == PrintTarget::Serial || target == PrintTarget::All)
        {
            printSpaces(serial, MESSAGE_INDENT);
            serial.print(F("Up: "));
            serial.print(LMIC.seqnoUp);
            serial.print(F(",  Down: "));
            serial.println(LMIC.seqnoDn);        
        }
    #endif        
}      


void printSessionKeys()
{    
    #if defined(USE_SERIAL) && defined(MCCI_LMIC)
        u4_t networkId = 0;
        devaddr_t deviceAddress = 0;
        u1_t networkSessionKey[16];
        u1_t applicationSessionKey[16];
        LMIC_getSessionKeys(&networkId, &deviceAddress, 
                            networkSessionKey, applicationSessionKey);

        printSpaces(serial, MESSAGE_INDENT);    
        serial.print(F("Network Id: "));
        serial.println(networkId, DEC);

        printSpaces(serial, MESSAGE_INDENT);    
        serial.print(F("Device Address: "));
        serial.println(deviceAddress, HEX);

        printSpaces(serial, MESSAGE_INDENT);    
        serial.print(F("Application Session Key: "));
        printHex(serial, applicationSessionKey, 16, true, '-');

        printSpaces(serial, MESSAGE_INDENT);    
        serial.print(F("Network Session Key:     "));
        printHex(serial, networkSessionKey, 16, true, '-');
    #endif
}


void printDownlinkInfo(void)
{
    #if defined(USE_SERIAL) || defined(USE_DISPLAY)

        uint8_t dataLength = LMIC.dataLen;
        // bool ackReceived = LMIC.txrxFlags & TXRX_ACK;

        int16_t snrTenfold = getSnrTenfold();
        int8_t snr = snrTenfold / 10;
        int8_t snrDecimalFraction = snrTenfold % 10;
        int16_t rssi = getRssi(snr);

        uint8_t fPort = 0;        
        if (LMIC.txrxFlags & TXRX_PORT)
        {
            fPort = LMIC.frame[LMIC.dataBeg -1];
        }        

        #ifdef USE_DISPLAY
            display.clearLine(EVENT_ROW);        
            display.setCursor(COL_0, EVENT_ROW);
            display.print(F("RX P:"));
            display.print(fPort);
            if (dataLength != 0)
            {
                display.print(" Len:");
                display.print(LMIC.dataLen);                       
            }
            display.clearLine(STATUS_ROW);        
            display.setCursor(COL_0, STATUS_ROW);
            display.print(F("RSSI"));
            display.print(rssi);
            display.print(F(" SNR"));
            display.print(snr);                
            display.print(".");                
            display.print(snrDecimalFraction);                      
        #endif

        #ifdef USE_SERIAL
            printSpaces(serial, MESSAGE_INDENT);    
            serial.println(F("Downlink received"));

            printSpaces(serial, MESSAGE_INDENT);
            serial.print(F("RSSI: "));
            serial.print(rssi);
            serial.print(F(" dBm,  SNR: "));
            serial.print(snr);                        
            serial.print(".");                        
            serial.print(snrDecimalFraction);                        
            serial.println(F(" dB"));

            printSpaces(serial, MESSAGE_INDENT);    
            serial.print(F("Port: "));
            serial.println(fPort);
   
            if (dataLength != 0)
            {
                printSpaces(serial, MESSAGE_INDENT);
                serial.print(F("Length: "));
                serial.println(LMIC.dataLen);                   
                printSpaces(serial, MESSAGE_INDENT);    
                serial.print(F("Data: "));
                printHex(serial, LMIC.frame+LMIC.dataBeg, LMIC.dataLen, true, ' ');
            }
        #endif
    #endif
} 


void printHeader(void)
{
    #ifdef USE_DISPLAY
        display.clear();
        display.setCursor(COL_0, HEADER_ROW);
        display.print(F("LMIC-node"));
        #ifdef ABP_ACTIVATION
            display.drawString(ABPMODE_COL, HEADER_ROW, "ABP");
        #endif
        #ifdef CLASSIC_LMIC
            display.drawString(CLMICSYMBOL_COL, HEADER_ROW, "*");
        #endif
        display.drawString(COL_0, DEVICEID_ROW, deviceId);
        display.setCursor(COL_0, INTERVAL_ROW);
        display.print(F("Interval:"));
        display.print(doWorkIntervalSeconds);
        display.print("s");
    #endif

    #ifdef USE_SERIAL
        serial.println(F("\n\nLMIC-node\n"));
        serial.print(F("Device-id:     "));
        serial.println(deviceId);            
        serial.print(F("LMIC library:  "));
        #ifdef MCCI_LMIC  
            serial.println(F("MCCI"));
        #else
            serial.println(F("Classic [Deprecated]")); 
        #endif
        serial.print(F("Activation:    "));
        #ifdef OTAA_ACTIVATION  
            serial.println(F("OTAA"));
        #else
            serial.println(F("ABP")); 
        #endif
        #if defined(LMIC_DEBUG_LEVEL) && LMIC_DEBUG_LEVEL > 0
            serial.print(F("LMIC debug:    "));  
            serial.println(LMIC_DEBUG_LEVEL);
        #endif
        serial.print(F("Interval:      "));
        serial.print(doWorkIntervalSeconds);
        serial.println(F(" seconds"));
        if (activationMode == ActivationMode::OTAA)
        {
            serial.println();
        }
    #endif
}     


#ifdef ABP_ACTIVATION
    void setAbpParameters(dr_t dataRate = DefaultABPDataRate, s1_t txPower = DefaultABPTxPower) 
    {
        // Set static session parameters. Instead of dynamically establishing a session
        // by joining the network, precomputed session parameters are be provided.
        #ifdef PROGMEM
            // On AVR, these values are stored in flash and only copied to RAM
            // once. Copy them to a temporary buffer here, LMIC_setSession will
            // copy them into a buffer of its own again.
            uint8_t appskey[sizeof(APPSKEY)];
            uint8_t nwkskey[sizeof(NWKSKEY)];
            memcpy_P(appskey, APPSKEY, sizeof(APPSKEY));
            memcpy_P(nwkskey, NWKSKEY, sizeof(NWKSKEY));
            LMIC_setSession (0x1, DEVADDR, nwkskey, appskey);
        #else
            // If not running an AVR with PROGMEM, just use the arrays directly
            LMIC_setSession (0x1, DEVADDR, NWKSKEY, APPSKEY);
        #endif

        #if defined(CFG_eu868)
            // Set up the channels used by the Things Network, which corresponds
            // to the defaults of most gateways. Without this, only three base
            // channels from the LoRaWAN specification are used, which certainly
            // works, so it is good for debugging, but can overload those
            // frequencies, so be sure to configure the full frequency range of
            // your network here (unless your network autoconfigures them).
            // Setting up channels should happen after LMIC_setSession, as that
            // configures the minimal channel set. The LMIC doesn't let you change
            // the three basic settings, but we show them here.
            LMIC_setupChannel(0, 868100000, DR_RANGE_MAP(DR_SF12, DR_SF7),  BAND_CENTI);      // g-band
            LMIC_setupChannel(1, 868300000, DR_RANGE_MAP(DR_SF12, DR_SF7B), BAND_CENTI);      // g-band
            LMIC_setupChannel(2, 868500000, DR_RANGE_MAP(DR_SF12, DR_SF7),  BAND_CENTI);      // g-band
            LMIC_setupChannel(3, 867100000, DR_RANGE_MAP(DR_SF12, DR_SF7),  BAND_CENTI);      // g-band
            LMIC_setupChannel(4, 867300000, DR_RANGE_MAP(DR_SF12, DR_SF7),  BAND_CENTI);      // g-band
            LMIC_setupChannel(5, 867500000, DR_RANGE_MAP(DR_SF12, DR_SF7),  BAND_CENTI);      // g-band
            LMIC_setupChannel(6, 867700000, DR_RANGE_MAP(DR_SF12, DR_SF7),  BAND_CENTI);      // g-band
            LMIC_setupChannel(7, 867900000, DR_RANGE_MAP(DR_SF12, DR_SF7),  BAND_CENTI);      // g-band
            LMIC_setupChannel(8, 868800000, DR_RANGE_MAP(DR_FSK,  DR_FSK),  BAND_MILLI);      // g2-band
            // TTN defines an additional channel at 869.525Mhz using SF9 for class B
            // devices' ping slots. LMIC does not have an easy way to define set this
            // frequency and support for class B is spotty and untested, so this
            // frequency is not configured here.
        #elif defined(CFG_us915) || defined(CFG_au915)
            // NA-US and AU channels 0-71 are configured automatically
            // but only one group of 8 should (a subband) should be active
            // TTN recommends the second sub band, 1 in a zero based count.
            // https://github.com/TheThingsNetwork/gateway-conf/blob/master/US-global_conf.json
            LMIC_selectSubBand(1);
        #elif defined(CFG_as923)
            // Set up the channels used in your country. Only two are defined by default,
            // and they cannot be changed.  Use BAND_CENTI to indicate 1% duty cycle.
            // LMIC_setupChannel(0, 923200000, DR_RANGE_MAP(DR_SF12, DR_SF7),  BAND_CENTI);
            // LMIC_setupChannel(1, 923400000, DR_RANGE_MAP(DR_SF12, DR_SF7),  BAND_CENTI);

            // ... extra definitions for channels 2..n here
        #elif defined(CFG_kr920)
            // Set up the channels used in your country. Three are defined by default,
            // and they cannot be changed. Duty cycle doesn't matter, but is conventionally
            // BAND_MILLI.
            // LMIC_setupChannel(0, 922100000, DR_RANGE_MAP(DR_SF12, DR_SF7),  BAND_MILLI);
            // LMIC_setupChannel(1, 922300000, DR_RANGE_MAP(DR_SF12, DR_SF7),  BAND_MILLI);
            // LMIC_setupChannel(2, 922500000, DR_RANGE_MAP(DR_SF12, DR_SF7),  BAND_MILLI);

            // ... extra definitions for channels 3..n here.
        #elif defined(CFG_in866)
            // Set up the channels used in your country. Three are defined by default,
            // and they cannot be changed. Duty cycle doesn't matter, but is conventionally
            // BAND_MILLI.
            // LMIC_setupChannel(0, 865062500, DR_RANGE_MAP(DR_SF12, DR_SF7),  BAND_MILLI);
            // LMIC_setupChannel(1, 865402500, DR_RANGE_MAP(DR_SF12, DR_SF7),  BAND_MILLI);
            // LMIC_setupChannel(2, 865985000, DR_RANGE_MAP(DR_SF12, DR_SF7),  BAND_MILLI);

            // ... extra definitions for channels 3..n here.
        #endif

        // Disable link check validation
        LMIC_setLinkCheckMode(0);

        // TTN uses SF9 for its RX2 window.
        LMIC.dn2Dr = DR_SF9;

        // Set data rate and transmit power (note: txpow is possibly ignored by the library)
        LMIC_setDrTxpow(dataRate, txPower);    
    }
#endif //ABP_ACTIVATION


void initLmic(bit_t adrEnabled = 1,
              dr_t abpDataRate = DefaultABPDataRate, 
              s1_t abpTxPower = DefaultABPTxPower) 
{
    // ostime_t timestamp = os_getTime();

    // Initialize LMIC runtime environment
    os_init();
    // Reset MAC state
    LMIC_reset();

    #ifdef ABP_ACTIVATION
        setAbpParameters(abpDataRate, abpTxPower);
    #endif

    // Enable or disable ADR (data rate adaptation). 
    // Should be turned off if the device is not stationary (mobile).
    // 1 is on, 0 is off.
    LMIC_setAdrMode(adrEnabled);

    if (activationMode == ActivationMode::OTAA)
    {
        #if defined(CFG_us915) || defined(CFG_au915)
            // NA-US and AU channels 0-71 are configured automatically
            // but only one group of 8 should (a subband) should be active
            // TTN recommends the second sub band, 1 in a zero based count.
            // https://github.com/TheThingsNetwork/gateway-conf/blob/master/US-global_conf.json
            LMIC_selectSubBand(1); 
        #endif
    }

    // Relax LMIC timing if defined
    #if defined(LMIC_CLOCK_ERROR_PPM)
        uint32_t clockError = 0;
        #if LMIC_CLOCK_ERROR_PPM > 0
            #if defined(MCCI_LMIC) && LMIC_CLOCK_ERROR_PPM > 4000
                // Allow clock error percentage to be > 0.4%
                #define LMIC_ENABLE_arbitrary_clock_error 1
            #endif    
            clockError = (LMIC_CLOCK_ERROR_PPM / 100) * (MAX_CLOCK_ERROR / 100) / 100;
            LMIC_setClockError(clockError);
        #endif

        #ifdef USE_SERIAL
            serial.print(F("Clock Error:   "));
            serial.print(LMIC_CLOCK_ERROR_PPM);
            serial.print(" ppm (");
            serial.print(clockError);
            serial.println(")");            
        #endif
    #endif

    #ifdef MCCI_LMIC
        // Register a custom eventhandler and don't use default onEvent() to enable
        // additional features (e.g. make EV_RXSTART available). User data pointer is omitted.
        LMIC_registerEventCb(&onLmicEvent, nullptr);
    #endif
}


#ifdef MCCI_LMIC 
void onLmicEvent(void *pUserData, ev_t ev)
#else
void onEvent(ev_t ev) 
#endif
{
    // LMIC event handler
    ostime_t timestamp = os_getTime(); 

    switch (ev) 
    {
#ifdef MCCI_LMIC
        // Only supported in MCCI LMIC library:
        case EV_RXSTART:
            // Do not print anything for this event or it will mess up timing.
            break;

        case EV_TXSTART:
            setTxIndicatorsOn();
            printEvent(timestamp, ev);            
            break;               

        case EV_JOIN_TXCOMPLETE:
        case EV_TXCANCELED:
            setTxIndicatorsOn(false);
            printEvent(timestamp, ev);
            break;               
#endif
        case EV_JOINED:
            setTxIndicatorsOn(false);
            printEvent(timestamp, ev);
            printSessionKeys();

            // Disable link check validation.
            // Link check validation is automatically enabled
            // during join, but because slow data rates change
            // max TX size, it is not used in this example.                    
            LMIC_setLinkCheckMode(0);

            // The doWork job has probably run already (while
            // the node was still joining) and have rescheduled itself.
            // Cancel the next scheduled doWork job and re-schedule
            // for immediate execution to prevent that any uplink will
            // have to wait until the current doWork interval ends.
            os_clearCallback(&doWorkJob);
            os_setCallback(&doWorkJob, doWorkCallback);
            break;

        case EV_TXCOMPLETE:
            // Transmit completed, includes waiting for RX windows.
            setTxIndicatorsOn(false);   
            printEvent(timestamp, ev);
            printFrameCounters();

            // Check if downlink was received
            if (LMIC.dataLen != 0 || LMIC.dataBeg != 0)
            {
                uint8_t fPort = 0;
                if (LMIC.txrxFlags & TXRX_PORT)
                {
                    fPort = LMIC.frame[LMIC.dataBeg -1];
                }
                printDownlinkInfo();
                processDownlink(timestamp, fPort, LMIC.frame + LMIC.dataBeg, LMIC.dataLen);                
            }
            break;     
          
        // Below events are printed only.
        case EV_SCAN_TIMEOUT:
        case EV_BEACON_FOUND:
        case EV_BEACON_MISSED:
        case EV_BEACON_TRACKED:
        case EV_RFU1:                    // This event is defined but not used in code
        case EV_JOINING:        
        case EV_JOIN_FAILED:           
        case EV_REJOIN_FAILED:
        case EV_LOST_TSYNC:
        case EV_RESET:
        case EV_RXCOMPLETE:
        case EV_LINK_DEAD:
        case EV_LINK_ALIVE:
#ifdef MCCI_LMIC
        // Only supported in MCCI LMIC library:
        case EV_SCAN_FOUND:              // This event is defined but not used in code 
#endif
            printEvent(timestamp, ev);    
            break;

        default: 
            printEvent(timestamp, "Unknown Event");    
            break;
    }
}


static void doWorkCallback(osjob_t* job)
{
    // Event hander for doWorkJob. Gets called by the LMIC scheduler.
    // The actual work is performed in function processWork() which is called below.

    ostime_t timestamp = os_getTime();
    #ifdef USE_SERIAL
        serial.println();
        printEvent(timestamp, "doWork job started", PrintTarget::Serial);
    #endif    

    // Do the work that needs to be performed.
    processWork(timestamp);

    // This job must explicitly reschedule itself for the next run.
    ostime_t startAt = timestamp + sec2osticks((int64_t)doWorkIntervalSeconds);
    os_setTimedCallback(&doWorkJob, startAt, doWorkCallback);    
}


lmic_tx_error_t scheduleUplink(uint8_t fPort, uint8_t* data, uint8_t dataLength, bool confirmed = false)
{
    // This function is called from the processWork() function to schedule
    // transmission of an uplink message that was prepared by processWork().
    // Transmission will be performed at the next possible time

    ostime_t timestamp = os_getTime();
    printEvent(timestamp, "Packet queued");

    lmic_tx_error_t retval = LMIC_setTxData2(fPort, data, dataLength, confirmed ? 1 : 0);
    timestamp = os_getTime();

    if (retval == LMIC_ERROR_SUCCESS)
    {
        #ifdef CLASSIC_LMIC
            // For MCCI_LMIC this will be handled in EV_TXSTART        
            setTxIndicatorsOn();  
        #endif        
    }
    else
    {
        String errmsg; 
        #ifdef USE_SERIAL
            errmsg = "LMIC Error: ";
            #ifdef MCCI_LMIC
                errmsg.concat(lmicErrorNames[abs(retval)]);
            #else
                errmsg.concat(retval);
            #endif
            printEvent(timestamp, errmsg.c_str(), PrintTarget::Serial);
        #endif
        #ifdef USE_DISPLAY
            errmsg = "LMIC Err: ";
            errmsg.concat(retval);
            printEvent(timestamp, errmsg.c_str(), PrintTarget::Display);
        #endif         
    }
    return retval;    
}


//  █ █ █▀▀ █▀▀ █▀▄   █▀▀ █▀█ █▀▄ █▀▀   █▀▄ █▀▀ █▀▀ ▀█▀ █▀█
//  █ █ ▀▀█ █▀▀ █▀▄   █   █ █ █ █ █▀▀   █▀▄ █▀▀ █ █  █  █ █
//  ▀▀▀ ▀▀▀ ▀▀▀ ▀ ▀   ▀▀▀ ▀▀▀ ▀▀  ▀▀▀   ▀▀  ▀▀▀ ▀▀▀ ▀▀▀ ▀ ▀


static volatile uint16_t counter_ = 0;

uint16_t getCounterValue()
{
    // Increments counter and returns the new value.
    delay(50);         // Fake this takes some time
    return ++counter_;
}

void resetCounter()
{
    // Reset counter to 0
    counter_ = 0;
}


void processWork(ostime_t doWorkJobTimeStamp)
{
    // This function is called from the doWorkCallback() 
    // callback function when the doWork job is executed.

    // Uses globals: payloadBuffer and LMIC data structure.

    // This is where the main work is performed like
    // reading sensor and GPS data and schedule uplink
    // messages if anything needs to be transmitted.

    // Skip processWork if using OTAA and still joining.
    if (LMIC.devaddr != 0)
    {
        // Collect input data.
        // For simplicity LMIC-node uses a counter to simulate a sensor. 
        // The counter is increased automatically by getCounterValue()
        // and can be reset with a 'reset counter' command downlink message.

        uint16_t counterValue = getCounterValue();
        ostime_t timestamp = os_getTime();
        
        // Added this throttle, so MQTT via wifi can be updated everytime and fair use policy of lorawan observed

        bool allowuplink = true;
        if (0)                                    //disabled mqtt, so allow all uplinks
//        if (counterValue % 10)                  //allow uplink every 10 loops, do it hear before we stuff countervalue
        {
            allowuplink = false;
        }

//      Bad Irms readings at startup, for now strip this out.        
//        int analogValue = analogRead(37);
//        int analogVolts = analogReadMilliVolts(37);

//      Open issue: on protoboard with heltec_wifi_lora_32_v2.  
//      First several readings (12 @ 15 minute intervals) of Irms after powerup are high and fall
//      nonlinear to the correct readings.
//      Troubleshooting:
//          1. removed above analogRead above with, no affect
//          2. moved CT Clamp from pin 13 to 36, no affect
//      Since device is running for months, my application can live with this; YMMV

//        double Irms = emon1.calcIrms(1480);   // Calculate Irms only
        Irms = emon1.calcIrms(1480);            // use the global Irms, so wifi module can access

        dht22.read2(pinDHT22, &temperature, &humidity, NULL);
            
        #ifdef MQTT
        sendmqtt();
        #endif

        //  when USE_DISPLAY defined, then LMIC code will write to display; not defined write sensor data to display        
        #ifndef USE_DISPLAY
        Display.clear();
        Display.drawString(0, 0,  "mA: " + String(Irms * 1000));      //show mA
        Display.drawString(0, 16, "CV: " + String(counterValue));   //what does CV look like 
        Display.drawString(0, 32, "T: " + String(temperature));
        Display.drawString(0, 48, "H: " + String(humidity));    
        Display.display();
        #endif

        #if 0               //better for 24 font
        Display.clear();
        Display.drawString(0, 0,  "mA: " + String(Irms * 1000));      //show mA
        Display.drawString(0, 20, "CV: " + String(counterValue));   //what does CV look like 
        Display.drawString(0, 40, "T: " + String(temperature) + "H: " + String(humidity));    
        Display.display();
        #endif


        //  This is lazy, should be its own variable.  Quick and dirty hack.
        counterValue = Irms * 1000;  // keep counter going, but stuff for uplink; A no go, so try *1000 for mA

        // Display.drawString(0, 30, "C mA: " + String(counterValue));   //what does CV look like 
        // Display.drawString(0, 45, "Uplink: " + String(allowuplink));   //Uplink this loop?
        // Display.display();

        // This works
        //        counterValue = analogVolts;  //  keep counter going, but stuff for uplink


        #ifdef USE_DISPLAY
            // Interval and Counter values are combined on a single row.
            // This allows to keep the 3rd row empty which makes the
            // information better readable on the small display.
            display.clearLine(INTERVAL_ROW);
            display.setCursor(COL_0, INTERVAL_ROW);
            display.print("I:");
            display.print(doWorkIntervalSeconds);
            display.print("s");        
            display.print(" Ctr:");
            display.print(counterValue);

            display.clearLine(ROW_3);
            // display.setCursor(COL_0, ROW_3);
            // display.print(" Irms:");
            // display.print(Irms);

         //   display.print("  mV:");
         //   display.print(analogVolts);

        #endif
        #ifdef USE_SERIAL
            printEvent(timestamp, "Input data collected", PrintTarget::Serial);
            printSpaces(serial, MESSAGE_INDENT);
            serial.print(F("COUNTER value: "));
            serial.println(counterValue);
  
         // print out the values you read:
            printSpaces(serial, MESSAGE_INDENT);
//            Serial.printf("ADC analog value = %d\n",analogValue);
//            Serial.printf("ADC millivolts value = %d\n",analogVolts);

            printSpaces(serial, MESSAGE_INDENT);
            serial.print(F("Irms values: "));
            Serial.print(Irms*230.0);	      // Apparent power
            Serial.print(" ");
            Serial.println(Irms);		       // Irms
        #endif    

        // For simplicity LMIC-node will try to send an uplink
        // message every time processWork() is executed.

        // Schedule uplink message if possible
        if (LMIC.opmode & OP_TXRXPEND)
        {
            // TxRx is currently pending, do not send.
            #ifdef USE_SERIAL
                printEvent(timestamp, "Uplink not scheduled because TxRx pending", PrintTarget::Serial);
            #endif    
            #ifdef USE_DISPLAY
                printEvent(timestamp, "UL not scheduled", PrintTarget::Display);
            #endif
        }
        else
        {
            // Prepare uplink payload.
            uint8_t fPort = 10;
            payloadBuffer[0] = counterValue >> 8;               //  counterValue was stuffed with mA
            payloadBuffer[1] = counterValue & 0xFF;

            uint16_t itemp = temperature * 100;                 //manipulate for uplink...ugly and will fail minus degree
            payloadBuffer[2] = itemp >> 8;
            payloadBuffer[3] = itemp & 0xFF;

            uint16_t ihum = humidity * 100;                     //manipulate for uplink...ugly
            payloadBuffer[4] = ihum >> 8;
            payloadBuffer[5] = ihum & 0xFF;


            uint8_t payloadLength = 6;                          // LMIC set to 2 and ignores buffer length

            // throttle uplinks, so we can push irms via wifi; probably a better way, but this works.
            if (allowuplink)
            {
               scheduleUplink(fPort, payloadBuffer, payloadLength);
            }
            

        }
    }
}    
 

void processDownlink(ostime_t txCompleteTimestamp, uint8_t fPort, uint8_t* data, uint8_t dataLength)
{
    // This function is called from the onEvent() event handler
    // on EV_TXCOMPLETE when a downlink message was received.

    // Implements a 'reset counter' command that can be sent via a downlink message.
    // To send the reset counter command to the node, send a downlink message
    // (e.g. from the TTN Console) with single byte value resetCmd on port cmdPort.

    const uint8_t cmdPort = 100;
//    const uint8_t resetCmd= 0xC0;
    const uint8_t resetCmd= 0x52;     //Capital R
    const uint8_t Uplink01Cmd= 0x4F;  //Capital O - One
    const uint8_t Uplink30Cmd= 0x54;  //Capital T - Thirty
    const uint8_t Uplink15Cmd= 0x46;  //Capital F - Fifteen
    const uint8_t Uplink60Cmd= 0x53;  //Capital S - Sixty

    if (fPort == cmdPort && dataLength == 1 && data[0] )
    {
        #ifdef USE_SERIAL
            printSpaces(serial, MESSAGE_INDENT);
            serial.println(F("Downlink cmd received"));
        #endif
        ostime_t timestamp = os_getTime();

        switch (data[0]) 
        {
        case resetCmd:
            doWorkIntervalSeconds = DO_WORK_INTERVAL_SECONDS;
            resetCounter();
            printEvent(timestamp, "Counter reset", PrintTarget::All, false);
            break;

        case Uplink01Cmd:
            doWorkIntervalSeconds = 60;
            break;        
            
        case Uplink30Cmd:
            doWorkIntervalSeconds = 1800;
            break;           

        case Uplink15Cmd:
            doWorkIntervalSeconds = 900;
            break;        
            
        case Uplink60Cmd:
            doWorkIntervalSeconds = 3600;
            break;           

        default: 
            printEvent(timestamp, "Unknown Event");    
            break;
        }

    }          
}


//  █ █ █▀▀ █▀▀ █▀▄   █▀▀ █▀█ █▀▄ █▀▀   █▀▀ █▀█ █▀▄
//  █ █ ▀▀█ █▀▀ █▀▄   █   █ █ █ █ █▀▀   █▀▀ █ █ █ █
//  ▀▀▀ ▀▀▀ ▀▀▀ ▀ ▀   ▀▀▀ ▀▀▀ ▀▀  ▀▀▀   ▀▀▀ ▀ ▀ ▀▀ 


void setup() 
{
    // boardInit(InitType::Hardware) must be called at start of setup() before anything else.
    bool hardwareInitSucceeded = boardInit(InitType::Hardware);

    #ifdef USE_DISPLAY 
        initDisplay();
    #endif

    #ifdef USE_SERIAL
        initSerial(MONITOR_SPEED, WAITFOR_SERIAL_S);
    #endif    

    boardInit(InitType::PostInitSerial);

    #if defined(USE_SERIAL) || defined(USE_DISPLAY)
        printHeader();
    #endif

    if (!hardwareInitSucceeded)
    {   
        #ifdef USE_SERIAL
            serial.println(F("Error: hardware init failed."));
            serial.flush();            
        #endif
        #ifdef USE_DISPLAY
            // Following mesage shown only if failure was unrelated to I2C.
            display.setCursor(COL_0, FRMCNTRS_ROW);
            display.print(F("HW init failed"));
        #endif
        abort();
    }

    initLmic();

//  █ █ █▀▀ █▀▀ █▀▄   █▀▀ █▀█ █▀▄ █▀▀   █▀▄ █▀▀ █▀▀ ▀█▀ █▀█
//  █ █ ▀▀█ █▀▀ █▀▄   █   █ █ █ █ █▀▀   █▀▄ █▀▀ █ █  █  █ █
//  ▀▀▀ ▀▀▀ ▀▀▀ ▀ ▀   ▀▀▀ ▀▀▀ ▀▀  ▀▀▀   ▀▀  ▀▀▀ ▀▀▀ ▀▀▀ ▀ ▀

    // Place code for initializing sensors etc. here.

    resetCounter();
    analogSetPinAttenuation(CT_CLAMP_PIN, ADC_11db);  //does this make a difference?
    adcAttachPin(CT_CLAMP_PIN);                  // initialize pin CT PIN as ADC - maybe the below takes care of this?
    adcAttachPin(37);                           // we can read 37 mvolts fine.

//    Burden Resistor	Current Calibration Coefficient
//15 Ω ±1% - old	133.3
//18 Ω ±1% - shipped as standard in emonTx V2 kit	111.1
//22 Ω ±1% - standard in emonTx V3 for CTs 1-3	90.9
//120 Ω ±1% - standard in emonTx V3 for CT 4	16.67
//33 Ω ±1% - shipped as standard in emonTx Shield kit	60.6

    emon1.current(CT_CLAMP_PIN, 19);             // calibration with meter; 19 is right number; measured 57 ohm burden in CT013 30A/1V.


// if LMIC not using display, then output sensor data; USE_DISPLAY=LMIC info
    #ifndef USE_DISPLAY
    Display.init();
//    Display.setFont(ArialMT_Plain_24);
    Display.setFont(ArialMT_Plain_16);
    Display.drawString(0, 30, "Joining...");
    Display.display();
    #endif

    #ifdef MQTT
        setup_wifi();
        client.setServer(mqtt_server, 1883);
        client.setCallback(callback);
    #endif



//  █ █ █▀▀ █▀▀ █▀▄   █▀▀ █▀█ █▀▄ █▀▀   █▀▀ █▀█ █▀▄
//  █ █ ▀▀█ █▀▀ █▀▄   █   █ █ █ █ █▀▀   █▀▀ █ █ █ █
//  ▀▀▀ ▀▀▀ ▀▀▀ ▀ ▀   ▀▀▀ ▀▀▀ ▀▀  ▀▀▀   ▀▀▀ ▀ ▀ ▀▀ 

    if (activationMode == ActivationMode::OTAA)
    {
        LMIC_startJoining();
    }

    // Schedule initial doWork job for immediate execution.
    os_setCallback(&doWorkJob, doWorkCallback);
}


void loop() 
{
    os_runloop_once();
}
