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

    void SetRatchets(int ratchets);

    int GetRatchets();


private:

    int         sample_rate_,current_sample_,ratchets_,num_sections_;
    float       duration_,elapsed_,part_duration_;
    bool        current_state_;

};