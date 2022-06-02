// 
// 
// 

#include "PushButton.h"
// digital pin, assume pull low
PushButton::PushButton(unsigned int pin)
{
	btnPin = pin;
	pinMode(btnPin, INPUT_PULLDOWN);
	btnState = UP;
	Cmils = millis();
	Pmils = Cmils;
	_hilo = 0;
	bPressed = 0;
	cycles = 0;
	isCycle = 0;
}

// pin and 0 for pull low / 1 pull high
PushButton::PushButton(unsigned int pin, unsigned int hilo)
{
	btnPin = pin;
	if (hilo < 1)
		pinMode(pin, INPUT_PULLDOWN);
	else
		pinMode(pin, INPUT_PULLUP);
	btnState = UP;
	Cmils = millis();
	Pmils = Cmils;
	_hilo = hilo;
	bPressed = 0;
	cycles = 0;
	isCycle = 0;
}

// call in loop to update state
void PushButton::update()
{
	uint8_t dr = 0;
	uint8_t BS;
	uint8_t PrevPress;
	dr = digitalRead(btnPin);
	Cmils = millis();
	if (_hilo == 0)
		if (dr == HIGH)
			BS = DOWN;
		else
			BS = UP;
	else
		if (dr == LOW)
			BS = DOWN;
		else
			BS = UP;
	PrevPress = bPressed;
	if ((Cmils - Pmils) > debounce)
	{
		if ((BS == DOWN) && (btnState == DOWN))
			bPressed = true;
		else
			bPressed = false;
		if (PrevPress && !(bPressed))// count cycle on rising
		{
			isCycle = true;
			cycles++;
		}
		btnState = BS;
		Pmils = Cmils;
	}
}
// true if cycled from last call to cycle count
uint8_t PushButton::isCycled()
{
	return isCycle;
}
// return number of cycles from last call
// will reset counter
uint8_t PushButton::cycleCount()
{
	unsigned int retval;
	retval = cycles;
	cycles = 0;
	isCycle = false;
	return retval;
}

// is button currently pressed
uint8_t PushButton::down()
{
	return bPressed;
}


