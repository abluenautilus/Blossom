#include "daisy_seed.h"
#include "Bloom.h"
#include "Gate.hpp"
#include "Note.hpp"

//HARDWARE
// In addition to the Daisy Seed, we have:
// CD4051BM Multiplexer x 2  - Texas Instruments
// PCA9685PW LED controller x 4 - NXP 
//      Connected over I2C, Seed pins 11,12
// CD4021BM Shift register x 2 - Texas Instruments
// MCP6004IST OpAmp x 3 - Microchip

using namespace daisy;
using namespace bloom;
using namespace seed;

const float max_pot_value = 65536;

// Declare a DaisySeed object called hardware
Bloom hw;

// Output gates
Gate gate1,gate2,clockout;

// Knob values
float knob1, knob2, knob3, knob4, knob5;

float SampleRate;

// static void AudioCallback(AudioHandle::InputBuffer  in,
//                           AudioHandle::OutputBuffer out,
//                           size_t                    size)
// {
//    for(size_t i = 0; i < size; i++)
//     {
        
//         // Advance gates
//         float gate1_val = gate1.Process(); 
//         hw.WriteCV(GATE_1,gate1_val);
//         float gate2_val = gate2.Process(); 
//         hw.WriteCV(GATE_1,gate2_val);
//         float clockout_val = clockout.Process(); 
//         hw.WriteCV(GATE_1,clockout_val);

//     }
// }

int main(void)
{

    // Configure and Initialize the Daisy Seed
    // These are separate to allow reconfiguration of any of the internal
    // components before initialization.
    //hardware.Configure();
    hw.Init();
    //hw.seed.StartAudio(AudioCallback);
    SampleRate = hw.seed.AudioCallbackRate();
    hw.seed.StartLog(false);

    int numLEDS = hw.ClearLeds();

    // Set up gates for output
    clockout.Init(SampleRate);
    clockout.SetDuration(0.1);
    gate1.Init(SampleRate);
    clockout.SetDuration(0.1);
    gate2.Init(SampleRate);
    clockout.SetDuration(0.1);

    // Set some LED lights on
    hw.SetLed(LED_TRUNK1,1.0,0.0,0.0); 
    hw.SetLed(LED_TRUNK2,1.0,0.5,0.0); 
    hw.SetLed(LED_TRUNK3,1.0,1.0,0.0); 
    hw.SetLed(LED_TRUNK4,0.0,1.0,0.0); 
    hw.SetLed(LED_TRUNK5,0.0,1.0,1.0);
    hw.SetLed(LED_TRUNK6,0.0,0.0,1.0);
    hw.SetLed(LED_TRUNK7,0.5,0.0,1.0);
    hw.SetLed(LED_TRUNK8,1.0,0.0,1.0); 

    hw.WriteLeds();

    int trigOut = 0;

    float values[8] = {25,25,25,25,25,25,25,25};

    hw.seed.PrintLine("Beginning loop");
    // Loop forever

    bool step_state[8] = {false,false,false,false,false,false,false,false};

    int channel = 1;
    float redval = 0.0;
    float greenval = 0.0;
    float blueval = 0.0;

    for(;;)
    {

        hw.ProcessAllControls();

        // Set colors based on channel
        if (channel == 1) { 
            redval = 1.0;
            blueval = 0.0;
        } else {
            redval = 0.0;
            blueval = 1.0;
        }

        // Process button presses
        if (hw.ButtonRisingEdge(BUTTON_CHANNEL)) {
            if (channel == 1) {
                channel = 2;
            } else {
                channel = 1;
            }
         }
        if (!hw.ButtonState(BUTTON_CHANNEL) ) {
            hw.SetLed(LED_CHANNEL,0.0,0.0,0.0);
        } else {
            hw.SetLed(LED_CHANNEL,redval,0.0,blueval);
        }

        for (int i = 0; i < ENC_LAST; ++i) {
            if (hw.ButtonRisingEdge(i)) {
                step_state[i] = !step_state[i];
            }
        }

        if (hw.ButtonState(BUTTON_SHIFT)) {
            hw.SetLed(LED_SHIFT,1.0,1.0,1.0);
        } else {
            hw.SetLed(LED_SHIFT,0.0,0.0,0.0);
        }

        if (hw.ButtonState(BUTTON_RESET)) {
            hw.SetLed(LED_RESET,1.0,1.0,1.0);
        } else {
            hw.SetLed(LED_RESET,0.0,0.0,0.0);
        }


        // Check for incoming trigger
        if (hw.ButtonRisingEdge(CLOCK_IN)) {
        
            clockout.ReTrigger();
            hw.seed.PrintLine("CLOCK --");
                       
        }

        // Check for encoder turns
        Leds lightstoset[8] = {LED_TRUNK1, LED_TRUNK2, LED_TRUNK3, LED_TRUNK4, LED_TRUNK5, LED_TRUNK6, LED_TRUNK7, LED_TRUNK8};
        for (int i = 0; i < ENC_LAST; ++i) {
            values[i] = values[i] + hw.encoders[i].Increment();
            if (values[i] > 25) {values[i] = 25;};
            if (values[i] < 0) {values[i] = 0;};
            float lightval = values[i]/25;
            if (lightval > 1.0) {lightval = 1.0;};
            if (lightval < 0.0) {lightval = 0.0;};
            if (step_state[i]) {
                hw.SetLed(lightstoset[i],redval,lightval,blueval);
            } else {
                hw.SetLed(lightstoset[i],0.0,0.0,0.0);
            }

        }
        
        // Get knob values
        knob1 = hw.GetKnobValue(KNOB_1);
        knob2 = hw.GetKnobValue(KNOB_2);
        knob3 = hw.GetKnobValue(KNOB_3);
        knob4 = hw.GetKnobValue(KNOB_4);
        knob5 = hw.GetKnobValue(KNOB_5);

        //Check incoming CV
        float cv1 = hw.GetCvValue(CV_PATH);
        hw.SetLed(LED_CENTER1,0.0,0.0,cv1);

        // Set output clock LED
        float clk = clockout.GetCurrentState();
        hw.SetLed(LED_CLOCK,0.0,clk,clk);

        // update LEDs
        hw.WriteLeds();

        // throttle update rate
        //System::Delay(100);
        
    }
}
