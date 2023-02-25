//
// output
//

#include <Arduino.h>
#include "output.h"



u_int8_t Output::_instanceCnt = 0;

// Constructor
Output::Output(u_int8_t pinNr)
{
  printf("[OUTPUT] constructor 1 (%d)\n",pinNr);

  Output(pinNr, OUTPUT, HIGH);
}

// Constructor
Output::Output(u_int8_t pinNr, u_int8_t mode, bool onState)
{
  _pinNr = pinNr;
  _onState = onState;
  _state = false;
  _onDelay = 0;
  _offDelay = 0;
  _onDelayCount = 0;
  _offDelayCount = 0;

  _instanceCnt++;
  _instanceNr = _instanceCnt;

  printf("[OUTPUT] constructor 2 (%d,%d)\n",_pinNr,_onState);

  pinMode(pinNr, mode);
  digitalWrite(pinNr, !onState);
}

void Output::setOnDelay(u_int16_t seconds)
{
  _onDelay = seconds;
  _onDelayCount = 0;
}

void Output::setOffDelay(u_int16_t seconds)
{
  _offDelay = seconds;
  _offDelayCount = 0;
}


void Output::on(void)
{
  u_int8_t  pinState;

  pinState = _onState ? HIGH : LOW;

  printf("[OUTPUT] ON  %d (%d,%d)\n",_instanceNr, _pinNr, pinState);  
  digitalWrite(_pinNr, pinState);

  if (_onDelayCount == 0)
  {
    _state = true;
  }
  else
  {
    _onDelayCount = _onDelay;    
  }
}

void Output::off(void)
{
  u_int8_t  pinState;

  pinState = !_onState ? LOW : HIGH;

  if (_offDelayCount == 0)
  {
    printf("[OUTPUT] OFF\n");  
    digitalWrite(_pinNr, pinState);
    _state = false;
  }
  else
  {
    _offDelayCount = _offDelay;    
  }
}

void Output::setValue(bool onOff)
{
  if (onOff == true)
  {
    on();
  }
  else
  {
    off();
  }
}

bool Output::getValue()
{
  return _state;
}


void Output::tickSecond(void)
{
  static bool onArmed = false;
  static bool offArmed = false;

  if (_onDelayCount != 0)
  {
    _onDelayCount--;
    onArmed = (_onDelayCount == 0);
  }
  
  if (onArmed)
  {
    digitalWrite(_pinNr, _onState);
    _state = true;
    onArmed = false;
  }


  if (_offDelayCount != 0)
  {
    _offDelayCount--;
    offArmed = (_offDelayCount == 0);
  }
  
  if (offArmed)
  {
    digitalWrite(_pinNr, !_onState);
    _state = false;
    offArmed = false;
  }
}


// end of file

