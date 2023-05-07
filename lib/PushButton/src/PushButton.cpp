// NPF 2022-11-18
// Pushbutton Library
// Button handling class with debounce

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
	longPressdelay = DEF_PRESSDELAY;
	longPressValue = 0;
	longPressInitMS = 0;
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
	longPressdelay = DEF_PRESSDELAY;
	longPressValue = 0;
	longPressInitMS = 0;
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
		// set long press counter here
		// note: this will get reset on button cycle, i.e this is always the latest long press
		if(bPressed)
		{
			if (longPressInitMS == 0)
				longPressInitMS = millis();
			longPressValue = millis() - longPressInitMS;
		}
		if (PrevPress && !(bPressed))// count cycle on rising
		{
			isCycle = true;
			cycles++;
			longPressInitMS = 0;// reset long press
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
// set long press delay millis
void PushButton::setLongPressMS(unsigned int delay)
{
	longPressdelay = delay;
}
// return the long press value if it excedes the delay
uint64_t PushButton::getLongPressMS()
{
	uint64_t tempValue;
	if (longPressValue > longPressdelay)
		tempValue = longPressValue;
	else
		tempValue = 0;
	return tempValue;
}

