/** Daisy Bloom Hardware Support File 
*
* "Reverse engineered" by Blue Nautilus
*
* Original firmware from Qu-Bit Electronix 
*
*/

#pragma once

#include "daisy_seed.h"
#include "dev/sr_4021.h"
#include "daisy_core.h"

const uint16_t unitspervolt = 819;
const float max_pot_value = 65535; // 16-bits

namespace bloom
{



/** @brief Global buffers for the LED driver
 *  Non-cached for DMA usage.
 */
static daisy::LedDriverPca9685<4, true>::DmaBuffer DMA_BUFFER_MEM_SECTION
    led_dma_buffer_a,
    led_dma_buffer_b;

/** @brief indexed accessors for Knob controls 
 *  Example usage:
 *  float val = hw.GetKnobValue(KNOB_1);
 */
enum ControlKnobs
{
    KNOB_1,
    KNOB_2,
    KNOB_3,
    KNOB_4,
    KNOB_5,
    KNOB_LAST
};

/** @brief indexed accessors for Encoders 
 *  Example usage:
 *  int increment = hw.encoders[ENC_1].Increment();
 */
enum ControlEncoders 
{
    ENC_1,
    ENC_2,
    ENC_3,
    ENC_4,
    ENC_5,
    ENC_6,
    ENC_7,
    ENC_8,
    ENC_LAST
};

/** @brief indexed accessors for CV controls 
 *  Example usage:
 *  float val = hw.GetCvValue(CV_ROOT);
 */
enum ControlCVs
{
    CV_ROOT,
    CV_PATH,
    CV_BRANCHES,
    CV_MUTATION,
    CV_RATE,
    CV_LAST
};

/** @brief indexed accessors for shift register buttons and input triggers
 *  Example usage:
 *  bool state = hw.GetRisingEdge(CLICK_ENC1);
 */
enum ShiftRegInputs
{
    CLICK_ENC1,
    CLICK_ENC2,
    CLICK_ENC3,
    CLICK_ENC4,
    CLICK_ENC5,
    CLICK_ENC6,
    CLICK_ENC7,
    CLICK_ENC8,
    BUTTON_CHANNEL,
    BUTTON_RESET,
    BUTTON_SHIFT,
    SR_BLANK1,
    SR_BLANK2,
    RESET_TRIG,
    CLOCK_IN,
    SR_LAST
};

/** @brief indexed accessors for CV outputs
 *  Example usage:
 *  WriteCV(GATE_1,val);
 */
enum GateOuts
{
    GATE_1,
    GATE_2,
    CLOCK_OUT,
    OUT_LAST
};

/** @brief Enum for addressing the CV Outputs 
 *  Example usage:
 *  SetCvOut(CV_OUT_1, volts);
 */

enum CVOuts 
{
    CV_OUT_1,
    CV_OUT_2,
    CV_OUT_BOTH
};

/** @brief indexed accessors for RGB LEDs 
 *  Example usage:
 *  hw.SetLed(LED_TRUNK1, 0.f, 0.f, 1.f);
 */
enum Leds
{
    LED_TRUNK1,
    LED_TRUNK2,
    LED_TRUNK3,
    LED_CHANNEL,
    LED_RESET,
    LED_CENTER2,
    LED_TRUNK5,
    LED_TRUNK6,
    LED_TRUNK4,
    LED_CENTER1,
    LED_SHIFT,
    LED_TRUNK8,
    LED_TRUNK7,
    LED_CENTER4,
    LED_CENTER3,
    LED_GATE1,
    LED_GATE2,
    LED_CLOCK,
    LED_OUT1,
    LED_OUT2,
    LED_LAST
};


/** 
    @brief Struct for a pair of pins that go together, as for the encoders
*/
struct pinPair {
    int a;
    int b;
};

struct Color {
    float r;
    float g;
    float b;
};

 /** outside of class static buffer(s) for DMA access */
uint16_t DMA_BUFFER_MEM_SECTION bloom_dac_buffer[2][48];

/** 
    @brief  Class for handling encoders with no switch \n 
    Made for encoders where switch is handled separately by shift register
    adapted from libDaisy encoder class
*/
class SwitchlessEncoder {

    public:
        SwitchlessEncoder() {}
        ~SwitchlessEncoder() {}

        void Init(dsy_gpio_pin a,
                        dsy_gpio_pin b )
        {
            last_update_ = daisy::System::GetNow();
            updated_     = false;

            // Init GPIO for A, and B
            hw_a_.pin  = a;
            hw_a_.mode = DSY_GPIO_MODE_INPUT;
            hw_a_.pull = DSY_GPIO_PULLUP;
            hw_b_.pin  = b;
            hw_b_.mode = DSY_GPIO_MODE_INPUT;
            hw_b_.pull = DSY_GPIO_PULLUP;
            dsy_gpio_init(&hw_a_);
            dsy_gpio_init(&hw_b_);

            // Set initial states, etc.
            inc_ = 0;
            a_ = b_ = 0xff;
        };

        void Debounce()
        {
            // update no faster than 1kHz
            uint32_t now = daisy::System::GetNow();
            updated_     = false;

            if(now - last_update_ >= 1)
            {
                last_update_ = now;
                updated_     = true;

                // Shift Button states to debounce
                a_ = (a_ << 1) | dsy_gpio_read(&hw_a_);
                b_ = (b_ << 1) | dsy_gpio_read(&hw_b_);

                // infer increment direction
                inc_ = 0; // reset inc_ first
                if((a_ & 0x03) == 0x02 && (b_ & 0x03) == 0x00)
                {
                    inc_ = 1;
                }
                else if((b_ & 0x03) == 0x02 && (a_ & 0x03) == 0x00)
                {
                    inc_ = -1;
                }
            }

        }

        /** Returns +1 if the encoder was turned clockwise, -1 if it was turned counter-clockwise, or 0 if it was not just turned. */
        inline int32_t Increment() const { return updated_ ? inc_ : 0; }

    private:
        uint32_t last_update_;
        bool     updated_;
        dsy_gpio hw_a_, hw_b_;
        uint8_t  a_, b_;
        int32_t  inc_;
};


/** @brief Hardware support class for the Qu-Bit Bloom 
 *  This should be created, and intitialized at the beginning of any program
 *  before running anything else.
 * 
 *  If you've used the Daisy Seed, etc. before this object takes the place
 *  of the core "DaisySeed" or other board support objects.
*/
class Bloom
{
  public:
    /** @brief Empty Constructor 
     *  Call `Init` from main to initialize
    */
    Bloom() {}

    /** @brief Empty Destructor
     *  This object should span the life of the program
     */
    ~Bloom() {
        
    }

    static Bloom hw;

    /** @brief Initialize the hardware. 
     *  Call this function at the start of main()
     * 
     *  @param boost true sets the processor to run at the maximum 480MHz, 
     *               false sets the processor to run at 400MHz.
     *               Defaults to true (480MHz).
     */
    void Init(bool boost = true)
    {

        seed.Init(boost);
        ConfigureControls();
        InitDac();
        ConfigureLeds();
        seed.adc.Start();
    }


    void InitDac()
    {
        daisy::DacHandle::Config cfg;
        cfg.bitdepth   = daisy::DacHandle::BitDepth::BITS_12;
        cfg.buff_state = daisy::DacHandle::BufferState::ENABLED;
        cfg.mode       = daisy::DacHandle::Mode::POLLING;
        cfg.chn        = daisy::DacHandle::Channel::BOTH;
        seed.dac.Init(cfg);
        seed.dac.WriteValue(daisy::DacHandle::Channel::BOTH, 0);
    }


    /** @brief Convert volts to raw output units, based on calibration */
    uint16_t voltsToUnits(float volts) {

        return volts * unitspervolt;
      
    }

    /** @brief sets the state of the LED on the daisy itself */
    void SetTestLed(bool state) { seed.SetLed(state); }

    /** @brief sets all RGB LEDs to off state 
     *  This can be called from the top of wherever LEDs are periodically set
     *  or within the UI framework's Canvas descriptor via the 
     *  clearFunction_ function.
     */
    int ClearLeds()
    {
        for(int i = 0; i < LED_LAST; i++)
        {
            SetLed((Leds)i, 0.0f, 0.0f, 0.0f);
        }

       return led_driver_.GetNumLeds();
    }

    /** @brief Write to one of the gate outputs (digital) 
     * @param output enum referencing the output channel
     * @param val output value 0-1
    */
    void WriteGate(int output, float val) {

        dsy_gpio_write(&outs[output], val);
    }

    /** @brief Write to one or both of the two analog CV outputs 
     *  @param channel enum of the channel to write to
     *  @param volts value in volts to write
     */

    void SetCvOut(CVOuts channel, float volts)
    {
        uint16_t val = voltsToUnits(volts);
        daisy::DacHandle::Channel ourChannel;
        switch(channel) {
            case CV_OUT_1:
                ourChannel = daisy::DacHandle::Channel::ONE;
            case CV_OUT_2:
                ourChannel = daisy::DacHandle::Channel::TWO;
            case CV_OUT_BOTH:
                ourChannel = daisy::DacHandle::Channel::BOTH; 
        }
        seed.dac.WriteValue(ourChannel, val);
    }

    /** @brief Writes the state of all LEDs to the hardware 
     *  This can be called from a fixed interval in the main loop,
     *  or within the UI framework's Canvas descriptor via the 
     *  flushFunction_ function.
     */
    void WriteLeds() { led_driver_.SwapBuffersAndTransmit(); }

    /** @brief Sets the RGB value of a given LED 
     *  @param idx LED index (one of Leds enum above)
     *  @param r 0- 1 red value 
     *  @param g 0- 1 green value 
     *  @param b 0- 1 blue value 
    */
    void SetLed(Leds idx, float r, float g, float b)
    {
        LedIdx led = LedMap[idx];
        if(led.r != -1)
            led_driver_.SetLed(led.r, r);
        led_driver_.SetLed(led.g, g);
        led_driver_.SetLed(led.b, b);
    }

    void SetLedColor(Leds idx, Color color)
    {
        LedIdx led = LedMap[idx];
        if(led.r != -1)
            led_driver_.SetLed(led.r, color.r);
        led_driver_.SetLed(led.g, color.g);
        led_driver_.SetLed(led.b, color.b);
    }

    /** @brief Sets the Color value of an LED
     *  @param idx LED index (on of Leds enum above)
     *  @param c Color object containing desired RGB values
     */
    void SetLed(Leds idx, daisy::Color c)
    {
        SetLed(idx, c.Red(), c.Green(), c.Blue());
    }

    /** @brief filters and debounces all control 
     *  This should be run once per audio callback.
     */
    void ProcessAllControls()
    {
        ProcessAnalogControls();
        ProcessDigitalControls();
    }

    /** @brief filters and debounces digital controls (switches)
     *  This is called from ProcessAllControls, and should be run once per audio callback
     */
    void ProcessDigitalControls()
    {

        // shift register (encoder clicks)
        shiftreg.Update();
  
        for(size_t i = 0; i < 16; i++)
        {
            shiftreg_state[i] = shiftreg.State(i) | (shiftreg_state[i] << 1);
        }

        for (size_t i = 0; i < ENC_LAST; ++i) {

            encoders[i].Debounce();

        }
    }

    /** @brief filters all analog controls (knobs and CVs)
     *  This is called from ProcessAllControls
     */
    void ProcessAnalogControls()
    {
        for(int i = 0; i < KNOB_LAST; i++)
        {
            controls[i].Process();
        }

        for(int i = 0; i < CV_LAST; i++)
        {
            cv[i].Process();
        }

    }
    /** @brief returns a 0-1 value for the given knob control
     *  @param ctrl knob index to read from. Should be one of ControlKnob (i.e. KNOB_1)
     */
    inline float GetKnobValue(int ctrl) const { return controls[ctrl].Value(); }

    /** @brief  gets calibrated offset-adjusted CV Value from the hardware.
     *  @param  cv_idx index of the CV to read from.
     *  @return input value
     */
    inline float GetCvValue(int cv_idx)
    {
        return cv[cv_idx].Value();
    }

    /** @brief  gets the state of one of the switches or encoder clicks
     *  @param  idx index of the switch to read from
     *  @return state of button
     */
    inline bool ButtonState(size_t idx) const
    {
        return shiftreg_state[idx] == 0x00;
    }

     /** @brief  detects rising edge of one of the switches or encoder clicks
     *  @param  idx index of the switch to read from
     *  @return true if rising edge detected otherwise false
     */
    inline bool ButtonRisingEdge(size_t idx) const
    {
        return shiftreg_state[idx] == 0x80;
    }

     /** @brief  detects falling edge of one of the switches or encoder clicks
     *  @param  idx index of the switch to read from
     *  @return true if false edge detected otherwise false
     */
    inline bool ButtonFallingEdge(size_t idx) const
    {
        return shiftreg_state[idx] == 0x7F;
    }

    /** LED arrays */ 
    Leds trunk_leds[8] = {LED_TRUNK1, LED_TRUNK2, LED_TRUNK3, LED_TRUNK4, LED_TRUNK5, LED_TRUNK6, LED_TRUNK7, LED_TRUNK8};
    Leds center_leds[4] = {LED_CENTER1, LED_CENTER2, LED_CENTER3, LED_CENTER4};

    /** Array of CV inputs */
    daisy::AnalogControl cv[CV_LAST];

    /** Array of encoders */
    bloom::SwitchlessEncoder encoders[ENC_LAST];

    /** Array of outputs */
    dsy_gpio outs[OUT_LAST];

    /** Array of knob controls */
    daisy::AnalogControl controls[KNOB_LAST];

    /** Shift register to collect switches and triggers */
    /**< Two 4021s daisy-chained. */
    uint8_t shiftreg_state[16];
    daisy::ShiftRegister4021<2> shiftreg; 

    //encoder stuff (temporary)
    dsy_gpio hw_a_, hw_b_;

    /** Daisy Seed base object */
    daisy::DaisySeed seed;

  private:

    /** @brief Internal struct for managing encoder pin pairs */
    const pinPair encPins[16] = {
        {26, 27}, // ENC 1
        {24, 25}, // ENC 2
        {14, 13}, // ENC 3
        {20, 21}, // ENC 4
        {18, 19}, // ENC 5
        {1, 0},   // ENC 6
        {3, 2},   // ENC 7
        {5, 4}    // ENC 8
    };

    /** @brief Internal struct for managing LED indices */
    struct LedIdx
    {
        int8_t r;
        int8_t g;
        int8_t b;
    };

    /** @brief internal mapping for LED routing on hardware */
    const LedIdx LedMap[LED_LAST] = {{0, 1, 2}, //LED_STEP1
                                     {3, 4, 5}, //LED_STEP2
                                     {6, 7, 8}, //LED_STEP3
                                     {9, 10, 11}, //LED_CHANNEL
                                     {12, 13, 14}, //LED_RESET
                                     {16, 17, 18}, //LED_BANK2
                                     {19, 20, 21}, //LED_STEP5
                                     {22, 23, 24}, //LED_STEP6
                                     {25, 26, 27}, //LED_STEP4
                                     {28, 29, 30}, //LED_BANK1
                                     {32, 33, 34}, //LED_SHIFT
                                     {35, 36, 37}, //LED_STEP8
                                     {38, 39, 40}, //LED_STEP7
                                     {41, 42, 43}, //LED_BANK4
                                     {44, 45, 46}, //LED_BANK3
                                     {48, 49, 50}, //GATE 1
                                     {51, 52, 53}, //GATE 2
                                     {54, 55, 56}, //CLOCK
                                     {57, 58, 59}, //OUT 1
                                     {60, 61, 62}, //OUT 2
                                     };

    /** @brief LED driver object for internal use */
    daisy::LedDriverPca9685<4, true> led_driver_;
    int numleds = led_driver_.GetNumLeds();

    daisy::I2CHandle   i2c;

    /** @brief  Sets up all contols */
    void ConfigureControls()
    {
        // Set up ADC
        // We have two multiplexers and 11 individual channels
        daisy::AdcChannelConfig cfg[13];

        cfg[0].InitMux(daisy::seed::D15,
                        8, 
                       daisy::seed::D30,
                       daisy::seed::D29,
                       daisy::seed::D28);

        cfg[1].InitMux(daisy::seed::D16,
                        8,
                       daisy::seed::D30,
                       daisy::seed::D29,
                       daisy::seed::D28);
        cfg[2].InitSingle(daisy::seed::D26);
        cfg[3].InitSingle(daisy::seed::D27);
        cfg[4].InitSingle(daisy::seed::D31);
        cfg[5].InitSingle(daisy::seed::D32);
        cfg[6].InitSingle(daisy::seed::D20);
        cfg[7].InitSingle(daisy::seed::D21);
        cfg[8].InitSingle(daisy::seed::D22);
        cfg[9].InitSingle(daisy::seed::D23);
        cfg[10].InitSingle(daisy::seed::D24);
        cfg[11].InitSingle(daisy::seed::D25);
            
        // Initialize the ADC with the 13 channels
        seed.adc.Init(cfg, 12);

        // Shift register gets encoder clicks, switches, and input triggers
        daisy::ShiftRegister4021<2>::Config shiftreg_cfg;
        shiftreg_cfg.clk     = seed.GetPin(8);
        shiftreg_cfg.latch   = seed.GetPin(7);
        shiftreg_cfg.data[0] = seed.GetPin(9);
        shiftreg.Init(shiftreg_cfg);

        // Initialize switchless encoders
        for (size_t i = 0; i < ENC_LAST; ++i) {
            encoders[i].Init(seed.GetPin(encPins[i].a),seed.GetPin(encPins[i].b));
        }

        // Initialize gate outputs
        int output_pins[OUT_LAST] = {6, 10, 17};
        for (size_t i = 0; i < OUT_LAST; ++i) {
            outs[i].pin  = seed.GetPin(output_pins[i]);
            outs[i].mode = DSY_GPIO_MODE_OUTPUT_PP;
            outs[i].pull = DSY_GPIO_NOPULL;
            dsy_gpio_init(&outs[i]);
        }

        // Initialize pots
        controls[KNOB_1].Init(seed.adc.GetMuxPtr(0, 0),seed.AudioCallbackRate());
        controls[KNOB_2].Init(seed.adc.GetMuxPtr(0, 1),seed.AudioCallbackRate());
        controls[KNOB_3].Init(seed.adc.GetMuxPtr(0, 2),seed.AudioCallbackRate());
        controls[KNOB_4].Init(seed.adc.GetMuxPtr(0, 3),seed.AudioCallbackRate());
        controls[KNOB_5].Init(seed.adc.GetMuxPtr(1, 0),seed.AudioCallbackRate());

        // Initialize CV inputs
        cv[CV_ROOT].InitBipolarCv(seed.adc.GetMuxPtr(0, 4),seed.AudioCallbackRate());
        cv[CV_PATH].InitBipolarCv(seed.adc.GetMuxPtr(0, 5),seed.AudioCallbackRate());
        cv[CV_BRANCHES].InitBipolarCv(seed.adc.GetMuxPtr(0, 6),seed.AudioCallbackRate());
        cv[CV_MUTATION].InitBipolarCv(seed.adc.GetMuxPtr(0, 7),seed.AudioCallbackRate());
        cv[CV_RATE].InitBipolarCv(seed.adc.GetMuxPtr(1, 1),seed.AudioCallbackRate());
        
    }

    /** @brief  Set up LEDs */
    void ConfigureLeds()
    {
        // reinit i2c at 1MHz
        daisy::I2CHandle::Config codec_i2c_config;
        codec_i2c_config.periph = daisy::I2CHandle::Config::Peripheral::I2C_1;
        codec_i2c_config.pin_config = {seed.GetPin(11), seed.GetPin(12)};
        codec_i2c_config.speed      = daisy::I2CHandle::Config::Speed::I2C_1MHZ;
        codec_i2c_config.mode = daisy::I2CHandle::Config::Mode::I2C_MASTER;
        i2c.Init(codec_i2c_config);

        // init driver
        led_driver_.Init(i2c, {0x00, 0x01, 0x02, 0x03}, led_dma_buffer_a, led_dma_buffer_b);

        ClearLeds();
        WriteLeds();
    }

};


} // namespace bloom

