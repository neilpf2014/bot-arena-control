#include <Arduino.h>
#include <PushButton.h>

/*
** To do Add code to read 8 buttons
** ready and tap out for 2 teams
** Match start / pause / end /reset for ref
** Serial out to hacked iCruze display boards
** 

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
enum MatchState{ all_ready, team_a_ready, team_b_ready, starting, in_progress, paused, time_up, ko_end, team_a_tap, team_b_tap };
MatchState match;
uint8_t Match_Reset;

bool GameOn;
uint32_t BtnCycle;

// used to blink the light tower
bool blink(bool state, u_int8_t ledGPIO)
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
	return rState;
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
  Match_Reset = false;
}

void loop() 
{
  // Button reading is here
  if ((cmils-BRLmills) > MAIN_LOOP_DELAY)
  {
  // button reading and state setting is done here
  // if match is running these buttons are active
    if (match == in_progress)
    {
      End_A.update();
      End_B.update();
      GameOver.update();
      GamePause.update();
      Match_Reset = false;
    }
    else
    // match is not running these are active
    if (match != starting)
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
        if (GameStart.isCycled() and (match == all_ready))
        {
          BtnCycle = GameStart.cycleCount();
          BtnCycle = GameReset.cycleCount();
          match = starting;
        }
      }
    }
  
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
  }
  // handle lights / horn here outside of button loop

  // loop to deal with match timer


// This is just test code
if ((cmils-pmils) > period)
  {
     // This is just test code
     if(Start_A.isCycled())
     {
       Serial.print("Button 1 pressed ");
       Serial.print(Start_A.cycleCount());
       Serial.println(" times!");
     }    
      if(Start_B.isCycled())
     {
       Serial.print("Button 2 pressed ");
       Serial.print(Start_B.cycleCount());
       Serial.println(" times!");
     }
     pmils = cmils;
  }
}