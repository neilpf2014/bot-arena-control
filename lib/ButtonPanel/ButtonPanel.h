// pushbutton.h
// NPF 2023-06-26

#ifndef _BUTTONPANEL_h
#define _BUTTONPANEL_h

#if defined(ARDUINO) && ARDUINO >= 100
#include "Arduino.h"
#else
#include "WProgram.h"
#endif

#include <PushButton.h>
//#include <Config.h>


// abstact the button array
class ButtonPanel
{
  #define TEAM_A_START 26 // old GPIO 23 New GPIO 25  26
  #define xTEAM_A_END 25   // old GPIO 22 New GPIO 26  25
  #define xTEAM_B_START 19 // old GPIO 33 New GPIO 19
  #define xTEAM_B_END 23   // old GPIO 32 New GPIO 23
  #define xMATCH_START 22  // old GPIO 25 New GPIO 21 22
  #define xMATCH_PAUSE 14  // old GPIO 26 New GPIO not used 14
  #define xMATCH_END 21    // old GPIO 27 New GPIO 22 21
  #define xMATCH_RESET 27  // old GPIO 14 New GPIO not used 27
  #define HI_LO 1

 
protected:
    PushButton Start_A;
    PushButton End_A;
    PushButton Start_B;
    PushButton End_B;
    PushButton GameStart;
    PushButton GameOver;
    PushButton GamePause;
    PushButton GameReset;

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
        sysint,
        count
    };
    static unsigned int e_len = 14;
    enum MatchState match;
    enum MatchState Cnt;
    bool mReset;
    void readBtns(void);

public:
    ButtonPanel(void);
    int setMatchstate(unsigned int imatch);
    int updateBtns(void);

};
#endif