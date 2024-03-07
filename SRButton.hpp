#pragma once
#include "daisy_core.h"
#include "per/gpio.h"
#include "sys/system.h"

using namespace daisy;

/**
 *  This class is for buttons that are read from a shift register. 
 * 
*/

class SRButton {

    public:

        bool noticed;

        SRButton() {}
        ~SRButton() {}

        void Init(daisy::ShiftRegister4021<2> sr, int srindex, float long_press_time);

        void Debounce();

        inline bool isPressedShort() const { return pressed_short_; } 

        inline bool isPressedLong() const { return pressed_long_; } 

        inline bool isReleasedShort() const { return released_short_; } 

        inline bool isReleasedLong() const { return released_long_; } 

        inline bool isDoubleClicked() const { return double_clicked_; }

        inline bool Pressed() const { return state_;  }

        inline bool RisingEdge() const { return rising_edge_; }

        inline float GetRawVal() const {return raw_val_; }

        inline float TimeHeldMs() const
        {
            return Pressed() ? System::GetNow() - rising_edge_time_ : 0;
        }

    private:
        daisy::ShiftRegister4021<2> sr_;
        int      srindex_;
        bool     updated_, pressed_short_, 
                 pressed_long_, released_short_, released_long_,
                 double_clicked_, rising_edge_;
        float    raw_val_;
        uint8_t  last_update_, state_, prev_state_;
        float    rising_edge_time_, long_press_time_,
                 press_duration_, prev_press_duration_,
                 falling_edge_time_;
};