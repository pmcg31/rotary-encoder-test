#ifndef __H_ROTARYENCODERLISTENER__
#define __H_ROTARYENCODERLISTENER__

class RotaryEncoder;

class RotaryEncoderListener
{
public:
    virtual void turned(RotaryEncoder *source, int deltaClicks, int rpm) = 0;
    virtual void clicked(RotaryEncoder *source) = 0;
};

#endif