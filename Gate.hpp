#pragma once
#include <stdint.h>

class Gate {

//Gate envelope with re-settable duration

 public:
    Gate() {}
    ~Gate() {}

    void Init(float sample_rate);

    void ReTrigger();
    
    void SetDuration(float dur);

    bool GetCurrentState();

    bool Process();

    float GetDuration();

    float GetElapsed();

    int GetCurrentSample();

private:

    int         sample_rate_,current_sample_;
    float       duration_,elapsed_;
    bool        current_state_;

};