#include "Gate.hpp"

void Gate::Init(float sample_rate) {

    sample_rate_ = sample_rate;
    current_sample_ = 0;
    current_state_ = false;
    duration_ = 0.250f;
    elapsed_ = float(current_sample_) * (1/float(sample_rate_));
}

bool Gate::Process() {

    ++current_sample_;

    elapsed_ = float(current_sample_) * (1/float(sample_rate_));

    if (elapsed_ >= duration_) {
        current_state_ = false;
    } else {
        if (ratchets_ > 0){
         
            float part = elapsed_/part_duration_;
            int prefix = (double)((int)part*100)/100;
       
            if (prefix % 2) {
                current_state_ = false;
            } else {
                current_state_ = true;
            }
        } else {
            current_state_ = true;
        }
    }
    

    return current_state_;

}

int Gate::GetCurrentSample() {

    return current_sample_;

}

void Gate::ReTrigger() {

    current_sample_ = 0;
    current_state_ = true;
    elapsed_ = 0.f;
    
    daisy::DaisySeed::PrintLine("ratchets %d parts %d",ratchets_,num_sections_);

}
    
void Gate::SetDuration(float dur) {

    duration_ = dur;
    part_duration_ = duration_/(float)num_sections_;

}

void Gate::SetRatchets(int ratchets) {

    ratchets_ = ratchets;
    if (ratchets_ == 0) {
        num_sections_ = 1;
    } else {
        num_sections_ = (ratchets + 1) * 2;
    }
    part_duration_ = duration_/(float)num_sections_;
}

int Gate::GetRatchets() {
    return ratchets_;
}

float Gate::GetDuration() {

    return duration_;

}

float Gate::GetElapsed() {

    return elapsed_;

}

bool Gate::GetCurrentState() {

    return current_state_;

}