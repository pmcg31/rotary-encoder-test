#include <Arduino.h>
#include <FunctionalInterrupt.h>
#include "RotaryEncoder.hpp"

const int RotaryEncoder::g_debounceTime_us = 250;

RotaryEncoder::RotaryEncoder(RotaryEncoderListener *listener,
                             int aPin, int bPin, int buttonPin,
                             int detents /*= 24*/)
    : _listener(listener),
      _aPin(aPin),
      _bPin(bPin),
      _zPin(buttonPin),
      _detents(detents),
      _lastRotaryTick_ms(0),
      _aPinPending(false),
      _zPinPending(false),
      _aPinDelta(0),
      _aPinTotalTriggers(0),
      _bPinValue(0),
      _zPinTriggered(false)
{
}

void RotaryEncoder::init()
{
    pinMode(_aPin, INPUT_PULLUP);
    pinMode(_bPin, INPUT_PULLUP);
    pinMode(_zPin, INPUT_PULLUP);

    attachInterrupt(_aPin, std::bind(&RotaryEncoder::_aPinHandler, this), FALLING);
    attachInterrupt(_zPin, std::bind(&RotaryEncoder::_zPinHandler, this), FALLING);

    _aPinTimerArgs.callback = &RotaryEncoder::g_aPinDebounce;
    _aPinTimerArgs.arg = this;
    esp_timer_create(&_aPinTimerArgs, &_aPinTimer);

    _zPinTimerArgs.callback = &RotaryEncoder::g_zPinDebounce;
    _zPinTimerArgs.arg = this;
    esp_timer_create(&_zPinTimerArgs, &_zPinTimer);
}

void RotaryEncoder::loop()
{
    unsigned long now = millis();

    if (_aPinTotalTriggers != 0)
    {
        int tempTotalTriggers = _aPinTotalTriggers;
        int tempDelta = _aPinDelta;
        _aPinTotalTriggers = 0;
        _aPinDelta = 0;
        int tickTime_ms = now > _lastRotaryTick_ms ? (now - _lastRotaryTick_ms) / tempTotalTriggers : 50;
        _lastRotaryTick_ms = now;

        int rpm = int(60000.0 / (double(tickTime_ms) * 24.0));

        _listener->turned(this, tempDelta, rpm);
    }

    if (_zPinTriggered)
    {
        _zPinTriggered = false;

        _listener->clicked(this);
    }
}

void IRAM_ATTR RotaryEncoder::_aPinHandler()
{
    if (!_aPinPending)
    {
        _aPinPending = true;
        _bPinValue = digitalRead(_bPin) == LOW;
        esp_timer_start_once(_aPinTimer, g_debounceTime_us);
    }
}

void IRAM_ATTR RotaryEncoder::_zPinHandler()
{
    if (!_zPinPending)
    {
        _zPinPending = true;
        esp_timer_start_once(_zPinTimer, g_debounceTime_us);
    }
}

void IRAM_ATTR RotaryEncoder::_aPinDebounce()
{
    _aPinPending = false;
    if (digitalRead(_aPin) == LOW)
    {
        _aPinTotalTriggers++;
        if (_bPinValue)
        {
            _aPinDelta++;
        }
        else
        {
            _aPinDelta--;
        }
    }
}

void IRAM_ATTR RotaryEncoder::_zPinDebounce()
{
    _zPinPending = false;
    if (digitalRead(_zPin) == LOW)
    {
        _zPinTriggered = true;
    }
}

void IRAM_ATTR RotaryEncoder::g_aPinDebounce(void *arg)
{
    RotaryEncoder *obj = (RotaryEncoder *)arg;
    obj->_aPinDebounce();
}

void IRAM_ATTR RotaryEncoder::g_zPinDebounce(void *arg)
{
    RotaryEncoder *obj = (RotaryEncoder *)arg;
    obj->_zPinDebounce();
}