#include <Arduino.h>
#include <PushButton.h>

/*
** Robot Combat Arena Control Code
** Using Ardunio framework / ESP32 (maybe STM32 with GPIO changes)
** Read 8 buttons
** ready and tap out for 2 teams
** Match start / pause / end /reset for ref
** Serial out to hacked iCruze display boards
** Will control 1 or 2 "stoplight" towers with signal horns
** NF 2022/06/04

*/

// State diagram
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

// button GPIO's ESP32 / Change for STM32
#define TEAM_A_START 23
#define TEAM_A_END 22
#define TEAM_B_START 33
#define TEAM_B_END 32
#define MATCH_START 25
#define MATCH_PAUSE 26
#define MATCH_END 27
#define MATCH_RESET 14
// tower signal light GPIO's
#define R_LIGHT 2
#define Y_LIGHT 4
#define G_LIGHT 5
#define HORN 13

#define D_SER_TX 17
#define D_SER_RX 16

#define MATCH_LEN 180 // match len in sec ( 3 min)
#define MATCH_END_WARN 15 // ending warn time sec
#define BLINK_DELAY 500 // in ms
#define STARTUP_DELAY 3000 //3 sec
#define MAIN_LOOP_DELAY 5 // in ms
#define HORN_SHORT 1000 //ms
#define HORN_LONG 2000 //ms
#define DIS_DELAY 200 //ms display update rate

PushButton Start_A(TEAM_A_START);
PushButton End_A(TEAM_A_END);
PushButton Start_B(TEAM_B_START);
PushButton End_B(TEAM_B_END);
PushButton GameStart(MATCH_START);
PushButton GameOver(MATCH_END);
PushButton GamePause(MATCH_PAUSE);
PushButton GameReset(MATCH_RESET);

uint64_t Btn_timer;
uint64_t light_timer;
uint64_t Horn_timer;
uint64_t Timer_timer;
uint64_t Display_timer;

// Match state
enum MatchState{ all_ready, team_a_ready, team_b_ready, starting, in_progress, ending, unpaused, paused, team_a_tap, team_b_tap, time_up, ko_end };
MatchState g_match;
uint8_t g_Match_Reset;

uint64_t gMatchRunTime; //match run time ms
uint64_t gMatchStartTime; //match start time ms
uint64_t MatchSecRemain; // sec remaining
uint8_t isTimerRunning;
uint64_t gSDtimer; // used for start delay
uint64_t gBLtimer; // for light blinking
uint8_t gBLstate; // for light blinking
uint64_t gTootTimer; //for horn
uint8_t gHornState; // for horn
uint8_t gHornBlast;

// button reading and state setting is done here
void readBtns(MatchState &match, uint8_t &Match_Reset)
{
  // note the button.cycleCount() resets the button press counter
  uint32_t BtnCycle;

  // if match is running these buttons are active
    if (match == in_progress)
    {
      End_A.update();
      End_B.update();
      GameOver.update();
      GamePause.update();
      Match_Reset = false;
      // pause hit
      if (GamePause.isCycled())
      {
        BtnCycle = GamePause.cycleCount();
        match = paused;
        
      }
      // team tap out
      if ((End_A.isCycled() || End_B.isCycled()) && (match == in_progress))
      {
        // Team A has tapped out
        if(End_A.isCycled())
          match = team_a_tap;
        // Team B has tapped out
        if(End_B.isCycled())
          match = team_b_tap;
        BtnCycle = End_A.cycleCount();
        BtnCycle = End_B.cycleCount();
      }

      // Ref has ended the match
      if(GameOver.isCycled() && (match == in_progress)) 
      {
        BtnCycle = GameOver.cycleCount();
        match = ko_end;
      }
    }
    else
    // match is ended these are active
    // starting is the match start countdown state
    if ((match != starting)||(match != paused)||(match != unpaused))
    {
      {
        GameStart.update();
        Start_A.update();
        Start_B.update();
        GameReset.update();
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
        if (Start_A.isCycled() and Start_B.isCycled())
        {
          match = all_ready;
          BtnCycle = Start_A.cycleCount();
          BtnCycle = Start_B.cycleCount();
        }
        else
        {
          if (Start_A.isCycled())
            match = team_a_ready;
          if (Start_B.isCycled())
            match = team_b_ready;
        }
        // if everyone is ready and start is pressed
        if (GameStart.isCycled() and (match == all_ready))
        {
          BtnCycle = GameStart.cycleCount();
          BtnCycle = GameReset.cycleCount();
          match = starting;
        }
      }
    }
    // match paused
    else
    {
      if(match == paused)
      {
        GameStart.update();
        if(GameStart.isCycled())
        {
          match = unpaused;
          BtnCycle = GameStart.cycleCount();
        }
      }
    }

}

// used to blink the light tower
void blink(u_int8_t &state, uint64_t &lastBlink, u_int8_t ledGPIO)
{
  if((millis() - lastBlink) > BLINK_DELAY)
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
void setLights(MatchState &match, u_int8_t Match_Reset, u_int64_t &SDtimer)
{
  
// all lights off
  if((Match_Reset) && (match != starting))
  {
    digitalWrite(G_LIGHT,LOW);
    digitalWrite(Y_LIGHT,LOW);
    digitalWrite(R_LIGHT,LOW);
  }
  // match in progress
  if (match == in_progress)
  {
    digitalWrite(G_LIGHT,HIGH);
    digitalWrite(Y_LIGHT,LOW);
    digitalWrite(R_LIGHT,LOW);
  }
  // match is stopped
  // if ((g_match != in_progress) || (g_match != starting) || (g_match != paused) || (g_match != unpaused))
  // not sure if enum can be used this way
  if ((match < 3) && (match > 6))
  {
    digitalWrite(G_LIGHT,LOW);
    //digitalWrite(Y_LIGHT,LOW);
    digitalWrite(R_LIGHT,HIGH);
  }
  // match is starting
  // blink yellow for 3 sec, red out, green out
  if(match == starting)
  {
    if((millis()-SDtimer) < STARTUP_DELAY)
      blink(gBLstate, gBLtimer, Y_LIGHT);
    else
      match=in_progress;
  }
  // match is paused
  // red is lit yellow is blinking
  if(match == paused)
  {
    blink(gBLstate, gBLtimer, Y_LIGHT);
    digitalWrite(R_LIGHT,HIGH);
  }
  // match is ending (within 15 sec of end)
  // blink green
  if(match == ending)
  {
    blink(gBLstate, gBLtimer, G_LIGHT);
  }

  // to do when everything working
  // Team A tap blink A tower red
  // Team B tap blink B tower red
  // for now just blink both towers red
}

// used to sound horn (yet another timer)
void soundHorn(u_int8_t &hornBlast, uint64_t &hornTime, u_int32_t tootLen, u_int8_t GPIO)
{
  u_int8_t state  = hornBlast % 2;
  if((millis() - hornTime) > tootLen)
  {
    if (state > 0)
    {
      digitalWrite(GPIO, HIGH);
    }
    else
    {
      digitalWrite(GPIO, LOW);
    }
    hornTime = millis();
    hornBlast++;
  }
}
// update and display match time
void match_timer(u_int64_t StartTime, u_int64_t &timerValue, u_int8_t &running)
{
  if ((running) && (timerValue < MATCH_LEN * 1000))
    timerValue = millis() - StartTime;
  else
    running = false;
}

void setup() {
  Serial.begin(115200);
  Serial1.setPins(D_SER_RX, D_SER_TX);
  Serial1.begin(9600);
  pinMode(R_LIGHT,OUTPUT);
  pinMode (Y_LIGHT,OUTPUT);
  pinMode (G_LIGHT, OUTPUT);
  // check lights
  digitalWrite(G_LIGHT,HIGH);
  digitalWrite(Y_LIGHT,HIGH);
  digitalWrite(R_LIGHT,HIGH);
  Serial.println("test");
  delay(1000);
  digitalWrite(G_LIGHT,LOW);
  digitalWrite(Y_LIGHT,LOW);
  digitalWrite(R_LIGHT,LOW);
  // check horn
  digitalWrite(HORN,HIGH);
  delay(200);
  digitalWrite(HORN,LOW);
  g_Match_Reset = false;
  isTimerRunning = false;
  gMatchRunTime = 0;
  gMatchStartTime = 0;
  gBLtimer = millis();
}

void loop() 
{
  
  // Button reading is here
  if ((millis()-Btn_timer) > MAIN_LOOP_DELAY)
  {
    readBtns(g_match, g_Match_Reset);
    // set startup delay - match will be inprogress after this is done
    if (g_match == starting)
      gSDtimer = millis();
    Btn_timer = millis();
  }
  // handle lights
  if ((millis()-light_timer) > MAIN_LOOP_DELAY + 1)
  {
    setLights(g_match, g_Match_Reset, gSDtimer);
    light_timer = millis();
  }
  // handle horn
  if ((millis()- Horn_timer) > MAIN_LOOP_DELAY * 2 )
  {
      // short horn on start or pause
      if(g_match ==starting)
      {
        while (gHornBlast < 6)
        {
          soundHorn(gHornBlast,gTootTimer,HORN_SHORT,HORN);
        }
      }
      else
        if(g_match < 8)
          gHornBlast = 0;
      // long horn on match end
      if(g_match > 7)
      {
        while (gHornBlast < 2)
        {
          soundHorn(gHornBlast,gTootTimer,HORN_LONG,HORN);
        }
      }
      else
        if(g_match!=starting)
          gHornBlast = 0;
      Horn_timer = millis();
  }

  // update match timer
  if ((millis() + Timer_timer) > MAIN_LOOP_DELAY)
  {
    //start timer
    if((g_match == in_progress)&&(isTimerRunning == false))
    {
      gMatchStartTime = millis();
      isTimerRunning = true;
    }
    //restart after pause
    if((g_match == unpaused)&&(isTimerRunning == false))
    {
      isTimerRunning = true;
      g_match = in_progress;
    }
    //stop timer
    if(g_match != in_progress)
      isTimerRunning = false;

    //update timer
    if(((g_match == in_progress)||(g_match == ending))&&(isTimerRunning == true))
    {
      match_timer(gMatchStartTime, gMatchRunTime, isTimerRunning);
      if(isTimerRunning == false)
        g_match = time_up;
    }
    // check for match ending
    MatchSecRemain = MATCH_LEN - ((gMatchRunTime - gMatchStartTime)/1000);
    if (MatchSecRemain < MATCH_END_WARN)
      g_match  = ending;
    Timer_timer = millis();
  }

  // update char display
  // will send info out via serial
  if ((millis()-Display_timer) > DIS_DELAY)
  {
    // using the 2 x 20 line iCruze display
    // refresh display every 200 ms 
    // display time remaining in min : sec on first line
    // status display on second
    if(g_match == in_progress)
    {
      Serial1.println((MatchSecRemain/60) + ':' + (MatchSecRemain % 60) + " Remaining");
      Serial1.println("Match in Progress");
    }
    if(g_match == paused)
    {
      Serial1.println((MatchSecRemain/60) + ':' + (MatchSecRemain % 60) + " Remaining");
      Serial1.println("--Paused --");
    }
    if(g_match == starting)
    {
      Serial1.println("Starting in: " + (gSDtimer/1000));
      Serial1.println("Teams Ready-Starting");
    }
    if(g_match == time_up)
    {
      Serial1.println("--- Time's Up ---");
      Serial1.println("-- MATCH OVER ---");
    }
    if(g_match == team_a_tap)
    {
      Serial1.println((MatchSecRemain/60) + ':' + (MatchSecRemain % 60) + " -MATCH OVER-");
      Serial1.println("--TEAM A TAPOUT--");
    }
    if(g_match == team_b_tap)
    {
      Serial1.println((MatchSecRemain/60) + ':' + (MatchSecRemain % 60) + " -MATCH OVER-");
      Serial1.println("--TEAM B TAPOUT--");
    }
    if(g_match == ko_end)
    {
      Serial1.println((MatchSecRemain/60) + ':' + (MatchSecRemain % 60) + " -MATCH OVER-");
      Serial1.println("-- BY KNOCKOUT --");
    }
    if(g_match == all_ready)
    {
      Serial1.println("-- 3 min clock ---");
      Serial1.println("--- ALL READY ---");
    }
    if((g_match != all_ready)&&(g_Match_Reset == true))
    {
      Serial1.println("-- WAITING FOR ---");
      Serial1.println("----- READY -----");
    }
    Display_timer = millis();
  }
}