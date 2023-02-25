//
// output.h
// 


#ifndef __OUTPUT_H__
#define __OUTPUT_H__

#define SAMPLE_WINDOW 7 // must be odd number

class Output
{
public:
    Output(u_int8_t pinNr);
    Output(u_int8_t pinNr, u_int8_t mode, bool onState);
    void setOnDelay(u_int16_t seconds);
    void setOffDelay(u_int16_t seconds);
    void off(void);
    void on(void);
    void setValue(bool onOff);
    bool getValue(void);

    void tickSecond(void);

private:
    u_int8_t  _pinNr;
    bool _onState;
    bool _state;
    u_int16_t _onDelay;
    u_int16_t _offDelay;
    u_int16_t _onDelayCount;
    u_int16_t _offDelayCount;
    u_int8_t _instanceNr;
    static u_int8_t _instanceCnt;
};

#endif