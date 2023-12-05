#include "ButtonPanel.h"
//#include <config.h>

ButtonPanel::ButtonPanel(void)
{
    Start_A.setPin(TEAM_A_START,HI_LO);
    Start_B.setPin(xTEAM_B_START,HI_LO);
    End_A.setPin(xTEAM_A_END,HI_LO);
    End_B.setPin(xTEAM_B_END,HI_LO);
    GameStart.setPin(xMATCH_START,HI_LO);
    GameOver.setPin(xMATCH_END, HI_LO);
    GamePause.setPin(xMATCH_PAUSE, HI_LO);
    GameReset.setPin(xMATCH_RESET,HI_LO);
    //Cnt = MatchState::count;
}
int ButtonPanel::setMatchstate(unsigned int imatch)
{
    int retVal = 0;
    if(imatch < e_len)
    {
        match = static_cast<MatchState>(imatch);
        retVal = 1
    }
    else
        retVal = 0;
    return retVal;
}
int ButtonPanel::readBtns(void)
{   
    // note the button.cycleCount() resets the button press counter
  uint8_t BtnCycle;

  // if match is running these buttons are active
  if ((match == MatchState::in_progress) || (match == MatchState::ending))
  {
    End_A.update();
    End_B.update();
    GameOver.update();
    GameStart.update();
    // GamePause.update();
    // BtnCycle =GameReset.cycleCount();
    mReset = false;

    // team tap out
    if ((End_A.isCycled() || End_B.isCycled()) && (match == MatchState::in_progress))
    {
      // Team A has tapped out
      if (End_A.isCycled())
        match = MatchState::team_a_tap;
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

        // this discards the reads on A, B and main start if not reset
        if (mReset == false)
        {
          BtnCycle = Start_A.cycleCount();
          BtnCycle = Start_B.cycleCount();
          BtnCycle = GameStart.cycleCount();
        }

        // check to see if both teams are ready
        if ((Start_A.isCycled()) && (match == MatchState::team_b_ready))
        {
          match = MatchState::all_ready;
          BtnCycle = Start_A.cycleCount();
          BtnCycle = Start_B.cycleCount();
          BtnCycle = GameStart.cycleCount();
        }
        if ((Start_B.isCycled()) && (match == MatchState::team_a_ready))
        {
          match = MatchState::all_ready;
          BtnCycle = Start_A.cycleCount();
          BtnCycle = Start_B.cycleCount();
          BtnCycle = GameStart.cycleCount();
        }
        if ((Start_A.isCycled()) && (match != MatchState::all_ready))
        {
          match = MatchState::team_a_ready;
          BtnCycle = Start_A.cycleCount();
        }
        if ((Start_B.isCycled()) && (match != MatchState::all_ready))
        {
          match = MatchState::team_b_ready;
          BtnCycle = Start_B.cycleCount();
        }

        // if everyone is ready and start is pressed
        if ((GameStart.isCycled()) && (match == MatchState::all_ready))
        {
          BtnCycle = GameStart.cycleCount();
          BtnCycle = Start_A.cycleCount();
          BtnCycle = Start_B.cycleCount();
          // throw out any presses to the end match buttons
          BtnCycle = End_A.cycleCount();
          BtnCycle = End_B.cycleCount();
          BtnCycle = GameOver.cycleCount();

          // BtnCycle = GameReset.cycleCount();
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
        GameOver.update();
        if (GameStart.isCycled())
        {
          match = MatchState::unpaused;
          BtnCycle = GameStart.cycleCount();
          BtnCycle = GameOver.cycleCount();
        }
      }
    }
  } 
  return 1;
}
