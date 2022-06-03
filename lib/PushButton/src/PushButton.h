// pushbutton.h

#ifndef _PUSHBUTTON_h
#define _PUSHBUTTON_h

#if defined(ARDUINO) && ARDUINO >= 100
#include "Arduino.h"
#else
#include "WProgram.h"
#endif
// very simple state machine to read / debounce a GPIO push button
class PushButton
{
#define DOWN 1
#define UP 0
protected:
	uint8_t btnState;
	uint8_t isCycle;
	uint8_t bPressed;
	uint8_t wasReleased = 0;
	unsigned int cycles;
	int btnPin;
	int _hilo;
	unsigned long Cmils;
	unsigned long Pmils;
	const unsigned long debounce = 40; // 20 milsec debounce

public:
	// Digital pin
	PushButton(unsigned int pin);
	// pin and pull high / low
	PushButton(unsigned int pin, unsigned int hilo);
	//update button status
	void update();
	// was button cycled ? (T/F)
	uint8_t isCycled();
	// retun # presses & reset cycles
	uint8_t cycleCount();
	// is button currently pressed ? (T/F)
	uint8_t down();
};
#endif

