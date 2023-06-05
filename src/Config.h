#include <stdlib.h>
#include <Arduino.h>
#include <PushButton.h>
#include <MQTThandler.h>
#include <FS.h>
#include <SPIFFS.h>
#include <ArduinoJson.h>
#include <WiFi.h>
#include <DNSServer.h>
#include <WebServer.h>
#include <WiFiManager.h> //https://github.com/tzapu/WiFiManager
#include <ESPmDNS.h>

#define DEBUG_ON 1

/* PINOUTS FOR CONTROL BOARD 
VALID AS OF 2023/06/05 -K
*/
#define TEAM_A_START 26 // old GPIO 23 New GPIO 25  26
#define TEAM_A_END 25   // old GPIO 22 New GPIO 26  25
#define TEAM_B_START 19 // old GPIO 33 New GPIO 19
#define TEAM_B_END 23   // old GPIO 32 New GPIO 23
#define MATCH_START 22  // old GPIO 25 New GPIO 21 22
#define MATCH_PAUSE 14  // old GPIO 26 New GPIO not used 14
#define MATCH_END 21    // old GPIO 27 New GPIO 22 21
#define MATCH_RESET 27  // old GPIO 14 New GPIO not used 27

// tower signal light GPIO's
#define R_LIGHT 13  // old GPIO 5 New GPIO 13
#define R_LIGHT_2 2 // old GPIO 4 New GPIO 2
#define Y_LIGHT 18  // old GPIO 12 New GPIO 18
#define G_LIGHT 4   // old GPIO 13 New GPIO 4
#define HORN 33     // old GPIO 2 New GPIO 33

/* Arena Timing Configuration */

#define MATCH_LEN ((2 * 60) + 30) // match len in sec ( 2.5 min)
#define MATCH_END_WARN 10         // ending warn time sec
#define BLINK_DELAY 500           // in ms
#define STARTUP_DELAY (3 * 1000)  // 3 2 1 go!
#define MAIN_LOOP_DELAY 5         // in ms
#define HORN_SHORT 1000           // ms
#define HORN_LONG 2000            // ms
#define PUBSUB_DELAY 200          // ms pubsub update rate
#define ADD_TIME_BTN_DELAY 2000   // ms delay to count add time btn as "down"

#define AP_DELAY 2000
#define HARD_CODE_BROKER "192.168.1.140"
#define CONFIG_FILE "/svr-config.json"