#include "SRButton.hpp"
#include "daisy_patch_sm.h"
//#include "daisysp.h"

using namespace daisy;

const float double_click_time = 300;

void SRButton::Init(daisy::ShiftRegister4021<2> sr, 
                    int srindex, 
                    float long_press_time
                )
{

    last_update_ = System::GetNow();
    updated_     = false;
    state_       = 0x00;
    sr_          = sr;
    srindex_     = srindex;
    long_press_time_ = long_press_time;
    rising_edge_ = false;

}


void SRButton::Debounce()
{
    // update no faster than 1kHz
    uint32_t now = System::GetNow();
    updated_     = false;

    if(now - last_update_ >= 1)
    {
        last_update_ = now;
        updated_     = true;
        prev_state_ = state_;
        prev_press_duration_ = press_duration_;

        sr_.Update();
        state_ = !sr_.State(srindex_);
        
        state_ ? press_duration_ = now - rising_edge_time_ : press_duration_ = 0;

        if (state_) {
            // BUTTON IS DOWN
            if (!prev_state_) {
                // RISING EDGE
                rising_edge_time_ = now;
                rising_edge_ = true;
                pressed_short_ = true;
                pressed_long_ = false;
                released_long_ = false;
                released_short_ = false;
                if (now-falling_edge_time_ < double_click_time) {
                    double_clicked_ = true;
                } else {
                    double_clicked_ = false;
                }
            } else {
                // HELD PRESS
                rising_edge_ = false;
                if (press_duration_ >= long_press_time_ && prev_press_duration_ < long_press_time_) {
                    pressed_short_ = false;
                    pressed_long_ = true;
                    released_long_ = false;
                    released_short_ = false;
                } if (press_duration_ >= long_press_time_ && prev_press_duration_ > long_press_time_) {
                    pressed_short_ = false;
                    pressed_long_ = false;
                    released_long_ = false;
                    released_short_ = false;
                } 
                double_clicked_ = false;
            }
        } else {
            // BUTTON IS UP
            rising_edge_ = false;
            if (prev_state_) {
                // RELEASE DETECTED
                falling_edge_time_ = now;
                if (prev_press_duration_ < long_press_time_) {
                    pressed_short_ = false;
                    pressed_long_ = false;
                    released_long_ = false;
                    released_short_ = true;
                    double_clicked_ = false;  
                } else if (prev_press_duration_ >= long_press_time_) {
                    pressed_short_ = false;
                    pressed_long_ = false;
                    released_long_ = true;
                    released_short_ = false;
                    double_clicked_ = false;
                }
            } else {
                // BUTTON REMAINS UP
                pressed_short_ = false;
                pressed_long_ = false;
                released_long_ = false;
                released_short_ = false;
                double_clicked_ = false;
            }
        }

        
    }
}
