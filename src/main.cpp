/*
**  Dev to use Mqtt to send the match status to the stream or other consuming application
**  changes for new areana 4-25-2023 electronics ****
*/

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

/*
** Robot Combat Arena Control Code
** Using Ardunio framework / ESP32
** Read 8 buttons (6 now)
** ready and tap out for 2 teams
** Match start / pause / end /reset for ref
** With MQTT code for stream overlay integration
** Will control 2 "stoplight" towers with signal horns
** NF 2022/06/04, updated for new electronics 2023/04/25
*/

// All #define's  at top of code to avoid issues
// inline echo for debug
#define DEBUG_ON 1
#define DEBUG_OFF 0
byte debugMode = DEBUG_ON;

#define DBG(...) debugMode == DEBUG_ON ? Serial.println(__VA_ARGS__) : NULL
// #define DEBUG

// ************* Button GPIO's ESP32 / Change for STM32 **********************************
// changed for new control
#define TEAM_A_START 23 // old GPIO 23 New GPIO 25
#define TEAM_A_END 22   // old GPIO 22 New GPIO 26
#define TEAM_B_START 33 // old GPIO 33 New GPIO 19
#define TEAM_B_END 32   // old GPIO 32 New GPIO 23
#define MATCH_START 23  // old GPIO 25 New GPIO 21
#define MATCH_PAUSE 26  // old GPIO 26 New GPIO not used
#define MATCH_END 27    // old GPIO 27 New GPIO 22
#define MATCH_RESET 14  // old GPIO 14 New GPIO not used
// tower signal light GPIO's
#define R_LIGHT 5     // old GPIO 5 New GPIO 15
#define R_LIGHT_2 4   // old GPIO 4 New GPIO 2
#define Y_LIGHT 12    // old GPIO 12 New GPIO 0
#define G_LIGHT 13    // old GPIO 13 New GPIO 4
#define HORN 2        // old GPIO 2 New GPIO 5

#define D_SER_TX 17
#define D_SER_RX 16

#define MATCH_LEN 150           // match len in sec ( 2.5 min)
#define MATCH_END_WARN 15       // ending warn time sec
#define BLINK_DELAY 500         // in ms
#define STARTUP_DELAY 5000      // 5 sec
#define MAIN_LOOP_DELAY 5       // in ms
#define HORN_SHORT 1000         // ms
#define HORN_LONG 2000          // ms
#define DIS_DELAY 200           // ms display update rate
#define PUBSUB_DELAY 200        // ms pubsub update rate
#define ADD_TIME_BTN_DELAY 2000 // ms delay to count add time btn as "down"
#define ADD_TIME_DIVISOR 200

#define AP_DELAY 2000
#define HARD_CODE_BROKER "192.168.1.140"
#define CONFIG_FILE "/svr-config.json"

//********** Wifi and MQTT stuff below ******************************************************
//******* based on Moxie board project  *****************************************************
//** Update these with values suitable for the broker used. *********************************
//** should now cave param's entered in the CP screen

IPAddress MQTTIp(192, 168, 1, 140);         // IP oF the MQTT broker if not 192.168.1.183

WiFiClient espClient;
uint64_t lastMsg = 0;
unsigned long MessID;

uint64_t msgPeriod = 10000; // Message check interval in ms (10 sec for testing)

String S_Stat_msg;
String S_Match;
String sBrokerIP;
int value = 0;
uint8_t GotMail;
uint8_t statusCode;
bool SaveConf_flag = false;
bool Use_def_IP_flag = false;

uint8_t ConnectedToAP = false;
MQTThandler MTQ(espClient, MQTTIp);
const char *outTopic = "botcontrol";
const char *inTopic = "timecontrol";

// used to get JSON config
uint8_t GetConfData(void)
{
  uint8_t retVal = 1;
  if (SPIFFS.begin(false) || SPIFFS.begin(true))
  {
    if (SPIFFS.exists(CONFIG_FILE))
    {
      File CfgFile = SPIFFS.open(CONFIG_FILE, "r");
      if (CfgFile.available())
      {
        StaticJsonDocument<512> jsn;
        DeserializationError jsErr = deserializeJson(jsn, CfgFile);
        serializeJsonPretty(jsn, Serial); // think this is just going to print to serial
        if (!jsErr)
        {
          sBrokerIP = jsn["BrokerIP"].as<String>();
          retVal = 0;
        }
      }
    }
    else
      DBG("File_not_found");
  }
  return retVal;
}

// used to save config as JSON
uint8_t SaveConfData(void)
{
  uint8_t retVal = 1;
  // SPIFFS.format();
  if (SPIFFS.begin(true))
  {

    StaticJsonDocument<512> jsn;
    File CfgFile = SPIFFS.open(CONFIG_FILE, "w");
    if (CfgFile)
    {
      if (serializeJson(jsn, CfgFile) != 0)
        retVal = 0;
      else
        DBG("failed to write file");
    }
    CfgFile.close();
  }
  else
  {
    retVal = 1;
    DBG("failed to open file");
  }

  return retVal;
}

// Wifi captive portal setup on ESP8266
void configModeCallback(WiFiManager *myWiFiManager)
{
  Serial.println("Entered config mode");
  Serial.println(WiFi.softAPIP());
  // if you used auto generated SSID, print it
  Serial.println(myWiFiManager->getConfigPortalSSID());
  digitalWrite(G_LIGHT, HIGH); // LED_BUILTIN is the horn !!
  Use_def_IP_flag = true;
}
void saveConfigCallback()
{
  // Set save config flag
  SaveConf_flag = true;
}

// Split wifi config into 2 procs to avoid code duplication
// this does most of the heavy lifting
// Saving of Params is working now !!
void WiFiCP(WiFiManager &WFM)
{
  bool validIP;
  uint8_t loadedFile;
  uint8_t savedFile;
  uint8_t isConnected;
  bool replaceHCIP;
  String sIPaddr;
  IPAddress MQTTeIP;

  WFM.setSaveConfigCallback(saveConfigCallback);
  WFM.setAPCallback(configModeCallback);
  replaceHCIP = false;
  WiFiManagerParameter TB_brokerIP("TBbroker", "MQTT broker IP", "192.168.1.140", 30);
  WFM.setHostname("BotArena");
  WFM.addParameter(&TB_brokerIP);
  isConnected = WFM.autoConnect("BotConfigAP");
  if (isConnected)
  {
    DBG("Connected");
    loadedFile = GetConfData();
    // load from file ignore TB
    if (!Use_def_IP_flag)
    {
      DBG("loaded IP from File");
      validIP = MQTTeIP.fromString(sBrokerIP);
      if (validIP)
        replaceHCIP = true;
    }
    else
    {
      sIPaddr = TB_brokerIP.getValue();
      DBG("Used IP from TB");
      if (!sIPaddr.isEmpty())
      {
        validIP = MQTTeIP.fromString(sIPaddr);
        if (validIP)
        {
          replaceHCIP = true;
          if (SaveConf_flag == true)
          {
            sBrokerIP = sIPaddr;
            SaveConf_flag = SaveConfData();
          }
        }
      }
    }
    if (replaceHCIP == true)
    {
      MQTTIp = MQTTeIP;
      DBG("replaced default");
    }
  }
}

// called to set up wifi -- Part 1 of 2
void WiFiConf(uint8_t ResetAP)
{
  WiFiManager wifiManager;
  if (ResetAP)
  {
    wifiManager.resetSettings();
    WiFiCP(wifiManager);
  }
  else
    WiFiCP(wifiManager);
  // these are used for debug
  Serial.println("Print IP:");
  Serial.println(WiFi.localIP());
  // **************************
  GotMail = false;
  MTQ.setClientName("ESP32Client");
  MTQ.subscribeIncomming(inTopic);
  MTQ.subscribeOutgoing(outTopic);
}


// use to get ip from mDNS, return true if sucess
// not user anymore
/*
uint8_t mDNShelper(String svrName)
{
  uint8_t logflag = true;
  unsigned int mdns_qu_cnt = 0;

  if (!MDNS.begin("esp32whatever"))
  {
    Serial.println("Error setting up MDNS responder!");
    logflag = false;
  }
  else
    Serial.println("Finished intitializing the MDNS client...");
  MQTTIp = MDNS.queryHost(svrName);
  while ((MQTTIp.toString() == "0.0.0.0") && (mdns_qu_cnt < 10))
  {
    Serial.println("Trying again to resolve mDNS");
    delay(250);
    MQTTIp = MDNS.queryHost(svrName);
    mdns_qu_cnt++;
  }
  if (MQTTIp.toString() == "0.0.0.0")
    logflag = false;
  return logflag;
}
*/

// **********************************************************************************************
// ****************** End Wifi Config code ******************************************************

// ********************* State diagram for Arena control ***************************************
/* no match states:
    Reset and ready for start ( solid yellow )
      team a ready
      team b ready
      both teams ready (blinking yellow)
    match pause (blinking yellow or blinking red / match timer paused)
    match tapout (blinking red)
    match end by KO (blinking red)
    match end by time (solid red)
  Match states
    Start pressed (3 secs of blinking yellow followed by green.  Match timer starts then)
    start pressed after pause (same as above with timer resuming)
    active match (solid green)
    10 sec count down (blinking green)
  Horn will sound at match end, time up or team tapout
*/
// ***********************************************************************************************
// ***********************************************************************************************
//  set for pull up inputs
PushButton Start_A(TEAM_A_START);
PushButton End_A(TEAM_A_END);
PushButton Start_B(TEAM_B_START);
PushButton End_B(TEAM_B_END);
PushButton GameStart(MATCH_START);
PushButton GameOver(MATCH_END);
PushButton GamePause(MATCH_PAUSE);
PushButton GameReset(MATCH_RESET);

// timer var for stuff
uint64_t Btn_timer;
uint64_t light_timer;
uint64_t Horn_timer;
uint64_t Timer_timer;
uint64_t Display_timer;
uint64_t PubSub_timer; // use for pubsub
uint64_t Dis_min;
uint64_t Dis_sec;

// Match state
enum MatchState
{
  all_ready,
  team_a_ready,
  team_b_ready,
  starting,
  in_progress,
  ending,
  unpaused,
  paused,
  team_a_tap,
  team_b_tap,
  time_up,
  ko_end,
  sysint
};
enum MatchState g_match;
enum MatchState last_match_state;
enum MatchState debug_lastmatch;
bool g_Match_Reset;

uint64_t gMatchRunTime;   // match run time ms
uint64_t gMatchStartTime; // match start time ms
uint64_t gTempRunTime;    // time ms
uint64_t MatchSecRemain;  // sec remaining
int64_t CountDownMSec;    // count down
int secToAdd;             //
bool isTimerRunning;
uint64_t gSDtimer;      // used for start delay
uint64_t gBLtimer;      // for light blinking
uint8_t gBLstate;       // for light blinking
uint64_t gBLtimer_2;    // for light blinking so 2 can blink at the same time
uint8_t gBLstate_2;     // for light blinking so 2 can blink at the same time
uint64_t gTootTimer;    // for horn
uint8_t gHornBlast;     // for horn
uint8_t gHornSounded;   // for horn
uint64_t gAddSecTimer;  // for the "add sec" function
uint8_t gMatchOverFlag; // Set for any state thats "game over"
uint8_t MQstatcode;     // for MQTT pubsub
String Msgcontents;

uint64_t ResetSec; // time to set clock to (only during pause)

// button reading and state setting is done here
void readBtns(MatchState &match, bool &Match_Reset)
{
  // note the button.cycleCount() resets the button press counter
  uint8_t BtnCycle;

  // if match is running these buttons are active
  if ((match == MatchState::in_progress) || (match == MatchState::ending))
  {
    End_A.update();
    End_B.update();
    GameOver.update();
    // Start is used for pause -- change for new electronics
    GameStart.update();
    // GamePause.update();
    BtnCycle =GameReset.cycleCount();
    Match_Reset = false;
    /*
          if (GamePause.isCycled())
          {
            BtnCycle = GamePause.cycleCount();
            match = MatchState::paused;
          }
    */
    // team tap out
    if ((End_A.isCycled() || End_B.isCycled()) && (match == MatchState::in_progress))
    {
      // Team A has tapped out
      if (End_A.isCycled())
        match = MatchState::team_a_tap;
      // Team B has tapped out
      if (End_B.isCycled())
        match = MatchState::team_b_tap;
      BtnCycle = End_A.cycleCount();
      BtnCycle = End_B.cycleCount();
    }

    // Ref has ended the match
    if (GameOver.isCycled() && (match == MatchState::in_progress))
    {
      BtnCycle = GameOver.cycleCount();
      match = MatchState::ko_end;
    }
    // start is used for pause if match is running -- Change for new electronics
    if (GameStart.isCycled() && (match == MatchState::in_progress))
    {
      BtnCycle = GameStart.cycleCount();
      match = MatchState::paused;
    }
  }
  else
  // match is not running these are active
  {

    // starting is the match start countdown state
    if ((match != MatchState::starting) && (match != MatchState::paused) && (match != MatchState::unpaused))
    {
      {
        GameStart.update();
        Start_A.update();
        Start_B.update();
        // GameReset.update();

        // this discards the reads on A, B and main start if not reset
        if (Match_Reset == false)
        {
          BtnCycle = Start_A.cycleCount();
          BtnCycle = Start_B.cycleCount();
          BtnCycle = GameStart.cycleCount();
        }
        if (GameReset.isCycled())
        {
          Match_Reset = true;
          BtnCycle =GameReset.cycleCount();
        }
      

        // check to see if both teams are ready
        if ((Start_A.isCycled()) && (Start_B.isCycled()))
        {
          match = MatchState::all_ready;
          BtnCycle = Start_A.cycleCount();
          BtnCycle = Start_B.cycleCount();
        }
        else
        {
          if ((Start_A.isCycled()) && (match != MatchState::all_ready))
          {
            match = MatchState::team_a_ready;
          }
          if ((Start_B.isCycled()) && (match != MatchState::all_ready))
          {
            match = MatchState::team_b_ready;
          }
        }
        // if everyone is ready and start is pressed
        if ((GameStart.isCycled()) && (match == MatchState::all_ready))
        {
          BtnCycle = GameStart.cycleCount();
          BtnCycle = GameReset.cycleCount();
          match = MatchState::starting;
        }
      }
    }
    // match paused
    else
    {
      if (match == MatchState::paused)
      {
        GameStart.update();
        // GamePause.update();
        // GameReset.update();
        GameOver.update();
        if (GameStart.isCycled())
        {
          match = MatchState::unpaused;
          BtnCycle = GameStart.cycleCount();
          BtnCycle = GameOver.cycleCount();
          // BtnCycle = GamePause.cycleCount();
          // BtnCycle = GameReset.cycleCount();
        }
      }
    }
  }
}

// new to handle msg from MQTT
void MQTThandleIncoming(String Msg, uint64_t &addTime, bool &Match_Reset, MatchState &_match)
{

  char CMD;
  String sResetSecs;

  CMD = Msgcontents.charAt(1);
  if (Msgcontents.length() > 1)
  {
    // parse the input will be 1 letter and an integer
    // 'R' for reset
    // 'A' for add time
    if (CMD == 'A')
    {
      sResetSecs = Msgcontents.substring(2, Msgcontents.length());
      ResetSec = Msgcontents.toInt();
    }
  }
  else
  {
    if ((_match != MatchState::starting) && (_match != MatchState::paused) && (_match != MatchState::unpaused))
    {
      DBG("Hit Reset 1");
      
      if (CMD == 'R')
        Match_Reset = true;
        DBG("Hit Reset 2");
    }
  }
}

// used to blink the light tower
void blink(u_int8_t &state, uint64_t &lastBlink, u_int8_t ledGPIO)
{
  if ((millis() - lastBlink) > BLINK_DELAY)
  {
    if (state > 0)
    {
      digitalWrite(ledGPIO, HIGH);
      state = 0;
    }
    else
    {
      digitalWrite(ledGPIO, LOW);
      state = 1;
    }
    lastBlink = millis();
  }
}

// handle lights here
void setLights(MatchState &match, bool Match_Reset)
{

  // all lights off
  if ((Match_Reset) && (match != MatchState::starting))
  {
    digitalWrite(G_LIGHT, LOW);
    digitalWrite(Y_LIGHT, LOW);
    digitalWrite(R_LIGHT, LOW);
    digitalWrite(R_LIGHT_2, LOW);
  }
  // match in progress
  if (match == MatchState::in_progress)
  {
    digitalWrite(G_LIGHT, HIGH);
    digitalWrite(Y_LIGHT, LOW);
    digitalWrite(R_LIGHT, LOW);
    digitalWrite(R_LIGHT_2, LOW);
  }
  // match is stopped
  if ((match == MatchState::time_up) && (!Match_Reset))
  {
    digitalWrite(G_LIGHT, LOW);
    // digitalWrite(Y_LIGHT,LOW);
    digitalWrite(R_LIGHT, HIGH);
    digitalWrite(R_LIGHT_2, HIGH);
  }
  // match is starting
  // blink yellow for x sec, red out, green out
  if (match == MatchState::starting)
  {
    digitalWrite(G_LIGHT, LOW);
    blink(gBLstate, gBLtimer, Y_LIGHT);
    digitalWrite(R_LIGHT, LOW);
    digitalWrite(R_LIGHT_2, LOW);
  }
  // match is paused
  // red is lit yellow is blinking
  if (match == MatchState::paused)
  {
    blink(gBLstate, gBLtimer, Y_LIGHT);
    digitalWrite(R_LIGHT, HIGH);
    digitalWrite(R_LIGHT_2, HIGH);
    digitalWrite(G_LIGHT, LOW);
  }
  // match is ending (within 15 sec of end)
  // blink green
  if (match == MatchState::ending)
  {
    blink(gBLstate, gBLtimer, G_LIGHT);
  }
  // to do when everything working
  // for now just blink both towers red
  if ((match == MatchState::ko_end) && (!Match_Reset))
  {
    digitalWrite(G_LIGHT, LOW);
    blink(gBLstate, gBLtimer, R_LIGHT);
    blink(gBLstate_2, gBLtimer_2, R_LIGHT_2);
  }
  // Team A tap blink A tower red
  if ((match == MatchState::team_a_tap) && (!Match_Reset))
  {
    digitalWrite(G_LIGHT, LOW);
    blink(gBLstate, gBLtimer, R_LIGHT);
    digitalWrite(R_LIGHT_2, LOW);
  }
  // Team B tap blink B tower red
  if ((match == MatchState::team_b_tap) && (!Match_Reset))
  {
    digitalWrite(G_LIGHT, LOW);
    blink(gBLstate, gBLtimer, R_LIGHT_2);
    digitalWrite(R_LIGHT, LOW);
  }
}

// was debug now setting Match string for MQTT
String MstateSetMQTT(MatchState match, u_int8_t Match_Reset, u_int8_t printState)
{
  String S_temp;
  switch (match)
  {
  case MatchState::all_ready:
  {
    S_temp = "all_ready";
  }
  break;
  case MatchState::sysint:
  {
    S_temp = "system_startup";
  }
  break;
  case MatchState::team_a_ready:
  {
    S_temp = "team_a_ready";
  }
  break;
  case MatchState::team_b_ready:
  {
    S_temp = "team_b_ready";
  }
  break;
  case MatchState::starting:
  {
    S_temp = "starting";
  }
  break;
  case MatchState::ending:
  {
    S_temp = "ending";
  }
  break;
  case MatchState::unpaused:
  {
    S_temp = "unpaused";
  }
  break;
  case MatchState::paused:
  {
    S_temp = "paused";
  }
  break;
  case MatchState::team_a_tap:
  {
    S_temp = "team_a_tap";
  }
  break;
  case MatchState::team_b_tap:
  {
    S_temp = "team_b_tap";
  }
  break;
  case MatchState::in_progress:
  {
    S_temp = "Match Running";
  }
  break;
  case MatchState::time_up:
  {
    S_temp = "time_up";
  }
  break;
  case MatchState::ko_end:
  {
    S_temp = "ko_end";
  }
  default:
    break;
  }
  if ((match == MatchState::ko_end) || (match == MatchState::team_a_tap) || (match == MatchState::team_b_tap) || (match == MatchState::time_up) || (match == MatchState::sysint))
  {
    if (Match_Reset > 0)
      S_temp = "reset_ready";
  }
  if (printState > 0)
    Serial.println(S_temp);
  return S_temp;
}

// used to sound horn (yet another timer)
void soundHorn(u_int8_t &hornOn, uint64_t &hornTime, u_int32_t tootLen, u_int8_t GPIO)
{
  // horn is on when true, horn time stores start of sound
  if (hornOn == 1)
  {
    hornTime = millis();
    hornOn = 2;
  }
  if (((millis() - hornTime) < tootLen) && (hornOn == 2))
  {
    digitalWrite(GPIO, HIGH);
  }
  else
  {
    digitalWrite(GPIO, LOW);
    hornOn = 0;
  }
}

// Match time control
// timervalue is the match runtime in Milsec
void match_timer(MatchState &l_match, u_int64_t &StartTime, u_int64_t &timerValue, bool &running, bool reset)
{
  // start timer
  if ((l_match == MatchState::in_progress) && (running == false))
  {
    // reset timer at beginning of match
    StartTime = millis();
    timerValue = 0;
    running = true;
  }
  // restart after pause
  if ((l_match == MatchState::unpaused) && (running == false))
  {
    running = true;
    // need to adjust start time to account for pause
    StartTime = millis() - timerValue;
    l_match = MatchState::in_progress;
  }
  // stop timer
  if ((l_match != MatchState::in_progress) && (l_match != MatchState::ending))
  {
    // done for debug only
    if (running)
    {
      running = false;
    }
  }
  // update timer
  if ((l_match == MatchState::in_progress) || (l_match == MatchState::ending))
  {
    if ((running) && (timerValue < (MATCH_LEN * 1000)))
    {
      timerValue = millis() - StartTime;
    }
    else
    {
      running = false;
    }
    if (running == false)
    {
      l_match = MatchState::time_up;
    }
  }
  // set match end warning
  if ((l_match == MatchState::in_progress) && (((MATCH_LEN * 1000) - timerValue) < (MATCH_END_WARN * 1000)))
    l_match = MatchState::ending;
}

// separate the Wifi / MQTT init from other setup stuff
void IOTsetup()
{
  bool testIP;
  uint64_t APmodeCKtimer;
  uint8_t Btnstate;
  uint8_t tempint;
  APmodeCKtimer = millis();
  Btnstate = 0;

  // Will wait 2 sec and check for reset to be held down / pressed
  while ((APmodeCKtimer + AP_DELAY) > millis())
  {
    if (End_A.isCycled())
      Btnstate = 1;
    End_A.update();
  }
  tempint = End_A.cycleCount();
  String TempIP = MQTTIp.toString();
  // these lines set up the access point, mqtt & other internet stuff
  pinMode(G_LIGHT, OUTPUT); // Initialize the Green for Wifi
  WiFiConf(Btnstate);
  // comment this out so we can enter broker IP on setup
  /*
  testIP = mDNShelper();
  if (!testIP){
    MQTTIp.fromString(TempIP);
  }
  Serial.print("IP address of server: ");
  */
  Serial.print("IP address of broker: ");
  Serial.println(MQTTIp.toString());
  MTQ.setServerIP(MQTTIp);
  digitalWrite(G_LIGHT, LOW); // turn off the light if on from config
  // **********************************************************
}

void setup()
{

  Serial.begin(115200);
  IOTsetup();

  pinMode(R_LIGHT, OUTPUT);
  pinMode(R_LIGHT_2, OUTPUT);
  pinMode(Y_LIGHT, OUTPUT);
  pinMode(G_LIGHT, OUTPUT);
  pinMode(HORN, OUTPUT);
  // check lights
  digitalWrite(G_LIGHT, HIGH);
  digitalWrite(Y_LIGHT, HIGH);
  digitalWrite(R_LIGHT, HIGH);
  digitalWrite(R_LIGHT_2, HIGH);
  Serial.println("program starting");
  delay(1000);
  digitalWrite(G_LIGHT, LOW);
  digitalWrite(Y_LIGHT, LOW);
  digitalWrite(R_LIGHT, LOW);
  digitalWrite(R_LIGHT_2, LOW);
  // check horn
  digitalWrite(HORN, HIGH);
  delay(1000);
  digitalWrite(HORN, LOW);
  // initalize everything else
  g_Match_Reset = false;
  isTimerRunning = false;
  gMatchRunTime = 0;
  gMatchStartTime = 0;
  gTempRunTime = 0;
  gSDtimer = 0;
  CountDownMSec = 0;
  gHornBlast = 0;
  gHornSounded = 0;
  gMatchOverFlag = 0;
  MatchSecRemain = 0;
  ResetSec = 0;
  gBLtimer = millis();
  gBLtimer_2 = millis();
  Btn_timer = millis();
  PubSub_timer = millis();
  g_match = MatchState::sysint;
  debug_lastmatch = MatchState::sysint;
  S_Match = "init";
}

// Main Loop
void loop()
{
  // Button reading is done here
  if ((millis() - Btn_timer) > MAIN_LOOP_DELAY)
  {
    readBtns(g_match, g_Match_Reset);

    // set startup delay - match will be in-progress after this
    if ((g_match == MatchState::starting) && (gSDtimer == 0))
    {
      gSDtimer = millis();
    }
    else if (g_match == MatchState::starting)
    {
      CountDownMSec = STARTUP_DELAY - (millis() - gSDtimer);
      if (CountDownMSec < 5)
      {
        // start match / reset count down timer
        g_match = MatchState::in_progress;
        CountDownMSec = 0;
        gSDtimer = 0;
      }
    }
    Btn_timer = millis();
  }

  // handle lights

  if ((millis() - light_timer) > MAIN_LOOP_DELAY + 1)
  {
    setLights(g_match, g_Match_Reset);
    // this ensures that we get the echo debug to the console if MQTT fails
    if (debug_lastmatch != g_match)
    {
      S_Match = MstateSetMQTT(g_match, g_Match_Reset, 1);
      debug_lastmatch = g_match;
    }
    light_timer = millis();
  }

  // handle horn

  if ((millis() - Horn_timer) > MAIN_LOOP_DELAY + 2)
  {
    if (g_match == MatchState::starting)
      gHornBlast = 1;
    // short horn on start or pause
    if (g_match == MatchState::in_progress)
    {
      while (gHornBlast > 0)
      {
        soundHorn(gHornBlast, gTootTimer, HORN_SHORT, HORN);
      }
      gHornSounded = 0;
    }
    // long horn on match end
    if ((g_match == MatchState::ko_end) || (g_match == MatchState::time_up) || (g_match == MatchState::team_a_tap) || (g_match == MatchState::team_b_tap))
    {
      if (gHornSounded != 1)
        gHornBlast = 1;
      while (gHornBlast > 0)
      {
        soundHorn(gHornBlast, gTootTimer, HORN_LONG, HORN);
      }
      gHornSounded = 1;
    }
    Horn_timer = millis();
  }

  // update match timer

  if ((millis() + Timer_timer) > MAIN_LOOP_DELAY)
  {
    match_timer(g_match, gMatchStartTime, gMatchRunTime, isTimerRunning, g_Match_Reset);
    // count down timer display
    MatchSecRemain = MATCH_LEN - (gMatchRunTime / 1000);
    if ((g_match == MatchState::paused) && (ResetSec > 0))
    {
      // get this from a number sent in via MQTT
      if (ResetSec < MATCH_LEN)
      {
        gMatchRunTime = (MATCH_LEN - ResetSec) * 1000;
        ResetSec = 0;
      }
    }
    Timer_timer = millis();
  }

  // Deal with MQTT Pubsub
  if ((millis() - PubSub_timer) > PUBSUB_DELAY)
  {
      // send / recieve status via MQTT
    S_Match = MstateSetMQTT(g_match, g_Match_Reset, 0);
    // this should be sending cur mills,state_code,state string, match sec remain as the message
    S_Stat_msg = String(millis()) + "," + String(g_match) + "," + S_Match + "," + String(MatchSecRemain) + "," + String(MATCH_LEN);
    MQstatcode = MTQ.publish(S_Stat_msg);
    GotMail = MTQ.update();
    if (GotMail == true)
    {
      //*********Incoming Msg *****************************
      Serial.print("message is: ");
      Msgcontents = MTQ.GetMsg();
      Serial.println(Msgcontents);
      MQTThandleIncoming(Msgcontents, ResetSec, g_Match_Reset, g_match);
      //********************************************************
      GotMail = false;
    }
    PubSub_timer = millis();
  }
}