#include <Arduino.h>
#include <PushButton.h>

/*
** To do Add code to read 8 buttons
** ready and tap out for 2 teams
** Match start / pause / end /reset for ref
** Serial out to hacked iCruze display boards
** NF 2022/06/04

*/

// button GPIO's
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

#define MATCH_LEN 180 // match len in sec ( 3 min)
#define BLINK_DELAY 500 // in ms
#define MAIN_LOOP_DELAY 3 // in ms

PushButton Start_A(TEAM_A_START);
PushButton End_A(TEAM_A_END);
PushButton Start_B(TEAM_B_START);
PushButton End_B(TEAM_B_END);
PushButton GameStart(MATCH_START);
PushButton GameOver(MATCH_END);
PushButton GamePause(MATCH_PAUSE);
PushButton GameReset(MATCH_RESET);

uint64_t pmils;
uint64_t BRLmills;
uint64_t cmils;
uint64_t period = 50;
// Match state
enum MatchState{ all_ready, team_a_ready, team_b_ready, starting, in_progress, ending, unpaused, paused, team_a_tap, team_b_tap, time_up, ko_end };
MatchState g_match;
uint8_t g_Match_Reset;

bool GameOn;  // do we need this ?

uint64_t gMatchRunTime; //match run time ms
uint64_t gMatchStartTime; //match start time ms
uint8_t isTimerRunning;

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
void blink(bool &state, uint64_t &lastBlink, u_int8_t ledGPIO)
{
  uint8_t rState;
	if (state > 0)
	{
		digitalWrite(ledGPIO, HIGH);
		rState = 0;
	}
	else
	{
		digitalWrite(ledGPIO, LOW);
		rState = 1;
	}
	state = rState;
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
  GameOn = false;
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
  digitalWrite(HORN,HIGH);
  delay(200);
  digitalWrite(HORN,LOW);
  cmils = millis();
  BRLmills = 0;
  g_Match_Reset = false;
  isTimerRunning = false;
  gMatchRunTime = 0;
  gMatchStartTime = 0;
}

void loop() 
{
  // Button check logic
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
  // Button reading is here
  if ((cmils-BRLmills) > MAIN_LOOP_DELAY)
  {
 
    readBtns(g_match, g_Match_Reset);
  }
  // handle lights
  if ((cmils-BRLmills + 2) > MAIN_LOOP_DELAY)
  {
    // all lights off
    if((g_Match_Reset) && (g_match != starting))
    {
      digitalWrite(G_LIGHT,LOW);
      digitalWrite(Y_LIGHT,LOW);
      digitalWrite(R_LIGHT,LOW);
    }
    // match in progress
    if (g_match == in_progress)
    {
      digitalWrite(G_LIGHT,HIGH);
      digitalWrite(Y_LIGHT,LOW);
      digitalWrite(R_LIGHT,LOW);
    }
    // match is stopped
    // if ((g_match != in_progress) || (g_match != starting) || (g_match != paused) || (g_match != unpaused))
    // not sure if enum can be used this way
    if ((g_match < 3) && (g_match > 6))
    {
      digitalWrite(G_LIGHT,LOW);
      //digitalWrite(Y_LIGHT,LOW);
      digitalWrite(R_LIGHT,HIGH);
    }

    // match is starting
    // blink yellow for 3 sec, red out, green out

    // match is paused
    // red is lit yellow is blinking

    // match is ending (within 15 sec of end)
    // blink green

    // to do when everything working
    // Team A tap blink A tower red
    // Team B tap blink B tower red
    // for now just blink both towers red

  }
  // handle horn
  if ((cmils-BRLmills + 3) > MAIN_LOOP_DELAY)
  {
      // short horn on start or pause

      // long horn on match end
  }

  // update match timer
  if ((cmils-BRLmills + 4) > MAIN_LOOP_DELAY)
  {
    //start timer
    if((g_match == in_progress)&&(isTimerRunning == false))
    {
      gMatchStartTime = millis();
      isTimerRunning = true;
    }
    //restart after pause

    //stop timer
    
  }

  // update char display
  // will send info out via serial
  if ((cmils-BRLmills) > MAIN_LOOP_DELAY)
  {
    // using the 2 x 20 line iCruze display
    // refresh display every 200 ms 
    // display time remaining in min : sec ion first line
    // status display on second
    // --starting count down
    // --ending countdown
    // --match paused
    // --team A tapped
    // --team B tapped
    // --KO end
    // --Time Up end
  }
}