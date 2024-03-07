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

    if (current_state_) {
        if (elapsed_ >= duration_) {
            current_state_ = false;
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

}
    
void Gate::SetDuration(float dur) {

    duration_ = dur;

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