#ifndef __H_ROTARYENCODER__
#define __H_ROTARYENCODER__

#include <Arduino.h>
#include <esp_timer.h>
#include "RotaryEncoderListener.hpp"

class RotaryEncoder
{
public:
    RotaryEncoder(RotaryEncoderListener *listener,
                  int aPin, int bPin, int buttonPin,
                  int detents = 24);

    void init();

    void loop();

private:
    void IRAM_ATTR _aPinHandler();
    void IRAM_ATTR _zPinHandler();
    void IRAM_ATTR _aPinDebounce();
    void IRAM_ATTR _zPinDebounce();

private:
    static void IRAM_ATTR g_aPinDebounce(void *);
    static void IRAM_ATTR g_zPinDebounce(void *);

private:
    static const int g_debounceTime_us;

private:
    RotaryEncoderListener *_listener;
    int _aPin;
    int _bPin;
    int _zPin;
    int _detents;
    unsigned long _lastRotaryTick_ms;

    esp_timer_handle_t _aPinTimer;
    esp_timer_handle_t _zPinTimer;

    esp_timer_create_args_t _aPinTimerArgs;
    esp_timer_create_args_t _zPinTimerArgs;

    volatile bool _aPinPending;
    volatile bool _zPinPending;
    volatile int _aPinDelta;
    volatile int _aPinTotalTriggers;
    volatile bool _bPinValue;
    volatile bool _zPinTriggered;
};

#endif