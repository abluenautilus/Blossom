/** BLOSSOM
* 
* Alternative firmware for Qu-Bit Bloom
*
* Original parts by Blue Nautilus
*
*/

#include "daisy_seed.h"
#include "Bloom.h"
#include "Gate.cpp"
#include "Note.hpp"
#include "Scales.hpp"
#include "utils.hpp"
#include "ColorLists.hpp"
#include "SRButton.cpp"

#define DSY_MIN(in, mn) (in < mn ? in : mn)
#define DSY_MAX(in, mx) (in > mx ? in : mx)
#define DSY_CLAMP(in, mn, mx) (DSY_MIN(DSY_MAX(in, mn), mx))

using namespace daisy;
using namespace bloom;

using namespace seed;

enum newMelodyNoteOptions {
    NM_REPEAT,
    NM_UP,
    NM_DOWN,
    NM_NEW
};

enum srButtons {
    SRBUTTON_CHANNEL,
    SRBUTTON_RESET,
    SRBUTTON_SHIFT
};


// Declare a DaisySeed object called hardware
Bloom hw;

// Button long press time
static const float button_longpress_time = 1500;

// Output gates
Gate gate1,gate2,clockout,tuningGate,accentGate;

// Knob values
float knob1, knob2, knob3, knob4, knob5;

SRButton buttons[3]; // shift register buttons

Random rng;

//Calibration data
const float cal_global_offset = 0.519;
const float cal_points_per_volt = 0.274;

// Sequence data
const int maxSteps = 32;
const int minSteps = 3;
Note current_note;
Note sequence[maxSteps];
int sequenceLength = 8;
const float volts_per_semitone = 0.083333;

float transpose_voltage = 0.0;

int current_step = 0;
int current_page = 0;
int display_page = 0;
int current_place = 0;

//Settings
float global_brightness = 0.5;
float rest_probability = 0.0;
float gate_length = 0.1;
bool note_preview = true;

int numScales;
bool follow = false;
float mutation_prob;

float reset_prev;
float new_prev;

//Melody creation
int noteOptionWeights[4] = {0,0,0,10}; //REPEAT UP DOWN NEW
std::string baseKey = "C";
std::string scale = "Natural Minor";
uint8_t scaleNum = 1;
uint8_t scaleNumPrev = 5;
uint8_t baseOctave = 4;
const int midi_max = 84;
const int midi_min = 36;
const int min_octave = 2;
const int max_octave = 5;
const float min_velocity = 0;
const float max_velocity = 5;
int newnote_range = 1;
float accent_prob = 20;

Note rootNote = Note(baseKey,baseOctave);
std::vector<int> validTones = scaleTones.at(scale);
std::vector<int> validToneWeights = scaleToneWeights.at(scale);
int octaveOffset = 0;
int repetitionCount = 0;
Note prevNote; 
int noteKind;

float SampleRate;

// Persistence
struct Settings {
    int sequenceLength;
    float global_brightness;
    bool follow;
    bool note_preview;
    float gate_length;
    int scaleNum;
    int accent_prob;
    int sequence[32];
    bool muted[32];
    int accent[32];
    float velocity[32];
    int note_hash;
    bool operator!=(const Settings& a) {
        return a.note_hash != note_hash;
    }
};

Settings& operator* (const Settings& settings) { return *settings; }
PersistentStorage<Settings> storage(hw.seed.qspi);

void saveData() {
    
    int note_hash = 0;
    Settings &localSettings = storage.GetSettings();
    localSettings.sequenceLength = sequenceLength;
    localSettings.global_brightness = global_brightness;
    localSettings.follow = follow;
    localSettings.note_preview = note_preview;
    localSettings.gate_length = gate_length;
    localSettings.scaleNum = scaleNum;
    localSettings.accent_prob = accent_prob;
    for (size_t i = 0; i < 32; ++i) {
        localSettings.sequence[i] = sequence[i].noteNumMIDI;
        localSettings.muted[i] = sequence[i].muted;
        localSettings.accent[i]  = sequence[i].accent;
        localSettings.velocity[i] = sequence[i].velocity;
        note_hash = note_hash + sequence[i].noteNumMIDI;
    }
    localSettings.note_hash = note_hash;
    storage.Save();
}

void loadData() {
    Settings &localSettings = storage.GetSettings();
    sequenceLength = localSettings.sequenceLength;
    global_brightness = localSettings.global_brightness;
    follow = localSettings.follow;
    note_preview = localSettings.note_preview;
    gate_length = localSettings.gate_length;
    scaleNum = localSettings.scaleNum;
    accent_prob = localSettings.accent_prob;
    for (size_t i = 0; i < 32; ++i) {
        sequence[i].changeMIDINoteNum(localSettings.sequence[i]);
        sequence[i].muted = localSettings.muted[i];
        sequence[i].accent = localSettings.accent[i];
        sequence[i].velocity = localSettings.velocity[i];
    }
}

// Yeah we don't make any audio but we use the audio callback anyways
static void AudioCallback(AudioHandle::InputBuffer  in,
                          AudioHandle::OutputBuffer out,
                          size_t                    size)
{
   for(size_t i = 0; i < size; i++)
    {
        // Tuning gate
        float tuninggate_val = tuningGate.Process(); 
        hw.WriteGate(GATE_1,tuninggate_val);

        //  Main gate out
        float gate1_val = gate1.Process(); 
        if (!tuninggate_val) {
            hw.WriteGate(GATE_1,gate1_val);
        }

        // Accent gate out
        float acc_val = accentGate.Process();
        hw.WriteGate(GATE_2, acc_val); 

        // Clock out
        float clockout_val = clockout.Process(); 
        hw.WriteGate(CLOCK_OUT,clockout_val);

        // CV out         
        hw.seed.dac.WriteValue(daisy::DacHandle::Channel::ONE, hw.voltsToUnits(current_note.voltage + transpose_voltage));
        hw.seed.dac.WriteValue(daisy::DacHandle::Channel::TWO, hw.voltsToUnits(current_note.velocity));

    }
};

Note getRandomNote(int range) {

    //Choose a random note from the current scale

    int ourSemitone;
    int ourChoice;

    int num_choices = validToneWeights.size();
    ourChoice = weightedRandom(&validToneWeights[0],num_choices, rng);

    ourSemitone = validTones[ourChoice];
    int oct_min = baseOctave - range;
    int oct_max = baseOctave + range;
    int ourOctave;
    if (oct_max - oct_min == 0) {
        ourOctave = baseOctave;
    } else {
       int rn = rng.GetValue() % (oct_max - oct_min + 1);
       ourOctave = rn + oct_min;
    }
   
    Note newNote = Note(ourSemitone, ourOctave);
    return newNote;

};

void changeNotes() {
    
    //substitute notes in the melody with new notes

    int noteToChange = (rng.GetValue() % sequenceLength) - 1;

    Note newNote = getRandomNote(newnote_range);

    int t = rng.GetValue() % 100;
    if (t < rest_probability) { 
        newNote.muted = true;  
    } else {
        newNote.muted = false;
    }

    t = rng.GetValue() % 100;
    if (t < accent_prob) {
        newNote.accent = true;
    } else {
        newNote.accent = false;
    }

    sequence[noteToChange] = newNote;

}

void doStep() {

    clockout.ReTrigger();

    ++current_step;
    if (current_step >= sequenceLength) {
        current_step = 0;

        //end of sequence mutations
        float noteChoice = rng.GetFloat(0,100)/2;
        if (noteChoice < mutation_prob) {
            changeNotes();
        } 
    }

    current_note = sequence[current_step];

    if (!current_note.muted) {
         gate1.ReTrigger();
    }

    if (current_note.accent) {
        accentGate.ReTrigger();
    }

    current_page = current_step / 8;
    current_place = current_step % 8;

}

void calibrationMelody() {

    sequence[0] = Note("C",4);
    sequence[0].muted = true;
    sequence[1] = Note("D",4);
    sequence[1].muted = true;
    sequence[2] = Note("E",4);
    sequence[2].muted = true;
    sequence[3] = Note("F",4);
    sequence[3].muted = true;
    sequence[4] = Note("G",4);
    sequence[4].muted = true;
    sequence[5] = Note("A",4);
    sequence[5].muted = true;
    sequence[6] = Note("B",4);
    sequence[6].muted = true;
    sequence[7] = Note("C",5);
    sequence[7].muted = true;

}

void newMelody() {

    //Generate a new melody sequence
    scale = scaleNames.at(scaleNum);
    validTones = scaleTones.at(scale);
    validToneWeights = scaleToneWeights.at(scale);
    octaveOffset = 0;
    repetitionCount = 0;

    for (int x = 0; x < maxSteps; x++ ){

        //Decide what kind of note
        //First note is always a new random note
        
        if (x>0){
            noteKind = weightedRandom(noteOptionWeights,4, rng);
        } else {
            noteKind = NM_NEW;
        }
        
        if (noteKind == NM_REPEAT) {
            sequence[x] = prevNote;

        } else if (noteKind == NM_DOWN) {

            //find tone of previous note in the scale, find index of toneNum in validTones
            std::vector<int>::iterator it = std::find(validTones.begin(),validTones.end(),prevNote.toneNum);
            int toneIndex = std::distance(validTones.begin(), it);

            toneIndex--;
            int newOctave = prevNote.octave;
            if (toneIndex < 0) {
                toneIndex = validTones.size()-1;
                newOctave--;
            }
            int newTone = validTones[toneIndex];
        
            std::string newNoteName = prevNote.getNoteNameFromNum(newTone);

            Note aNewNote = Note(newNoteName,newOctave);
            sequence[x] = aNewNote;

        } else if (noteKind == NM_UP) {

            //find tone of previous note in the scale, find index of toneNum in validTones
            std::vector<int>::iterator it = std::find(validTones.begin(),validTones.end(),prevNote.toneNum);
            int toneIndex = std::distance(validTones.begin(), it);

            toneIndex++;
            int newOctave = prevNote.octave;
            if (toneIndex >= int(validTones.size())) {
                toneIndex = 0;
                newOctave++;
            }
            int newTone = validTones[toneIndex];
        
            std::string newNoteName = prevNote.getNoteNameFromNum(newTone);

            Note aNewNote = Note(newNoteName,newOctave);
            sequence[x] = aNewNote;


        } else if (noteKind == NM_NEW) {
            Note newNote = getRandomNote(newnote_range);
            sequence[x] = newNote;
            prevNote = newNote;
        }

        // Decide if note is muted
        int t = rng.GetValue() % 100;
        if (t < rest_probability) { 
            sequence[x].muted = true;
        } else {
            sequence[x].muted = false;
        }
       
        // Decide if note is accented
        t = rng.GetValue() % 100;
        if (t < accent_prob) { 
            sequence[x].accent = true;
        } else {
            sequence[x].accent = false;
        }

        // Decide velocity value
        t = rng.GetValue() % 100;
        sequence[x].velocity = (float)t/100 * (max_velocity - min_velocity);

        prevNote = sequence[x];
        
    } 


}


int find(std::vector<int> arr, int len, int seek)
{
    for (int i = 0; i < len; ++i)
    {
        if (arr[i] == seek) return i;
    }
    return -1;
}

void doEncoder(int enc, int val) {

    // Handle encoder turns

    if (!hw.ButtonState(BUTTON_SHIFT)) {
        //
        // Note change encoder
        //

        //quantized if encoder not pushed

        int modifier = 0;
        if (follow) {
            modifier = current_page * 8;
        } else {
            modifier = display_page * 8;
        }

        if (!hw.ButtonState(enc)) {

            validTones = scaleTones.at(scale);
            int numValidTones = validTones.size();
            int currentTone = sequence[enc + modifier].toneNum;
            int currentOctave = sequence[enc + modifier].octave;
            int validToneIndex = find(validTones, numValidTones,currentTone);
            int newval = validToneIndex + val;
            int newTone, newOctave;
            newOctave = 4;

            if (newval < 0)  {
                newOctave = currentOctave - 1;
                if (newOctave < min_octave) {
                    newOctave = min_octave;
                    newTone = validTones[0];
                } else {
                    newTone = validTones[numValidTones-1];
                }
            } else if (newval > numValidTones -1) {
                newOctave = currentOctave + 1;
                if (newOctave > max_octave) {
                    newOctave = max_octave;
                    newTone = validTones[numValidTones-1];
                } else {
                    newTone = validTones[0];
                }

            } else {
                newOctave = currentOctave;
                newTone = validTones[newval];
            }

            std::string newNoteNum = sequence[enc + modifier].numToNote[newTone];
            bool m = sequence[enc + modifier].muted;
            sequence[enc + modifier] = Note(newNoteNum,newOctave);
            sequence[enc + modifier].muted = m;

        } else {

            int oldval = sequence[enc + modifier].noteNumMIDI;
            int newval = oldval + val;

            if (newval > midi_max) newval = midi_max;
            if (newval < midi_min) newval = midi_min;

            sequence[enc + modifier].changeMIDINoteNum(newval);

        }

        if (note_preview) {
            hw.seed.dac.WriteValue(daisy::DacHandle::Channel::ONE, hw.voltsToUnits(sequence[enc + modifier].voltage + transpose_voltage));
            tuningGate.ReTrigger();
        }

    } else {

        if (enc == 0) {
            //
            //Length encoder
            //
            
            int newLength = sequenceLength + val;
            if (newLength > maxSteps)  newLength = maxSteps;
            if (newLength < minSteps) newLength = minSteps;
            sequenceLength = newLength;
            
            int numPages = (sequenceLength / 8);
            int numLights = sequenceLength % 8;
            if (numLights == 0) {
                numLights = 8;
                numPages = numPages - 1;
                if (numPages < 0) numPages = 0;
            }
            
            for (int x = 0; x < 8; ++x) {
                if (x < numLights) {
                    hw.SetLed(hw.trunk_leds[x],steps_rainbow[1].r * global_brightness, steps_rainbow[1].g * global_brightness, steps_rainbow[1].b * global_brightness);
                } else {
                    hw.SetLed(hw.trunk_leds[x],0,0,0);
                }
  
            }
            for (int x = 0; x < 4; ++x) {
                if (x <= numPages) {
                    hw.SetLed(hw.center_leds[x],steps_rainbow[1].r * global_brightness, steps_rainbow[1].g * global_brightness, steps_rainbow[1].b * global_brightness);
                } else {
                    hw.SetLed(hw.center_leds[x],0,0,0);
                }
            }

            hw.WriteLeds();
        } else if (enc == 1) {
            //
            // Scale encoder
            //
        
            int newScale = scaleNum + val;
            if (newScale < 1) newScale = 1;
            if (newScale > numScales) newScale = numScales;
            scaleNum = newScale;

            int numPages = (scaleNum / 8);
            int numLights = scaleNum % 8;
            if (numLights == 0) {
                numLights = 8;
                numPages = numPages - 1;
                if (numPages < 0) numPages = 0;
            }

            for (int x = 0; x < 8; ++x) {
                if (x < numLights) {
                    hw.SetLed(hw.trunk_leds[x],steps_rainbow[3].r * global_brightness,steps_rainbow[3].g * global_brightness,steps_rainbow[3].b * global_brightness);
                } else {
                    hw.SetLed(hw.trunk_leds[x],0,0,0);
                }
  
            }
            for (int x = 0; x < 4; ++x) {
                if (x <= numPages) {
                    hw.SetLed(hw.center_leds[x],steps_rainbow[3].r * global_brightness,steps_rainbow[3].g * global_brightness,steps_rainbow[3].b * global_brightness);
                } else {
                    hw.SetLed(hw.center_leds[x],0,0,0);
                }
            }

            hw.WriteLeds();
        
        } else if (enc == 7) {

            global_brightness = global_brightness + (val * 0.05);
            hw.WriteLeds();

        } else if (enc == 6) {

            gate_length = DSY_CLAMP(gate_length + (val * 0.05),0.05,1);
            gate1.SetDuration(gate_length);

            
        } else if (enc == 5) {

            accent_prob = DSY_CLAMP(accent_prob + val,0,100);

        }
    }
}

void doShiftClick(int enc) {


    if (enc == 6) {
        note_preview = !note_preview;
    }

    if (enc == 7) {
        if (follow) {
            follow = false;
        } else {
            follow = true;
        }
    }

}

int main(void)
{
    // Configure and Initialize the Bloom
    hw.Init();
    hw.seed.StartLog(false);
    hw.ClearLeds();
    hw.seed.StartAudio(AudioCallback);
    SampleRate = hw.seed.AudioSampleRate();

    numScales = scaleNames.size();

    rng.Init();

    // Set up gates for output
    clockout.Init(SampleRate);
    clockout.SetDuration(0.1);
    gate1.Init(SampleRate);
    gate1.SetDuration(gate_length);
    gate2.Init(SampleRate);
    gate2.SetDuration(0.01);
    tuningGate.Init(SampleRate);
    tuningGate.SetDuration(0.25);
    accentGate.Init(SampleRate);
    accentGate.SetDuration(0.1);

    // Set up buttons as shift reg buttons
    buttons[SRBUTTON_CHANNEL].Init(hw.shiftreg, BUTTON_CHANNEL, button_longpress_time);
    buttons[SRBUTTON_RESET].Init(hw.shiftreg, BUTTON_RESET, button_longpress_time);
    buttons[SRBUTTON_SHIFT].Init(hw.shiftreg, BUTTON_SHIFT, button_longpress_time);

    //set up color tables
    bloom::Color* colormap_seq;
    colormap_seq = interp(semitones_rainbow_rev, 5);

    //Set some LED lights on
    hw.SetLedColor(LED_TRUNK1,steps_rainbow[0]); 
    hw.SetLedColor(LED_TRUNK2,steps_rainbow[1]); 
    hw.SetLedColor(LED_TRUNK3,steps_rainbow[2]); 
    hw.SetLedColor(LED_TRUNK4,steps_rainbow[3]); 
    hw.SetLedColor(LED_TRUNK5,steps_rainbow[4]);
    hw.SetLedColor(LED_TRUNK6,steps_rainbow[5]);
    hw.SetLedColor(LED_TRUNK7,steps_rainbow[6]);
    hw.SetLedColor(LED_TRUNK8,steps_rainbow[7]); 
    hw.WriteLeds();

    scale = scaleNames.at(scaleNum);
    validTones = scaleTones.at(scale);
    validToneWeights = scaleToneWeights.at(scale);

    int note_hash = 0;
    Settings defaults;
    defaults.sequenceLength = 8;
    defaults.global_brightness = 0.5;
    defaults.follow = false;
    defaults.note_preview = true;
    defaults.gate_length = 0.1;
    defaults.scaleNum = 1;
    defaults.accent_prob = 20;
    for (size_t r = 0; r < 32; ++r) {
        defaults.sequence[r] = 60;
        defaults.muted[r] = false;
        defaults.accent[r] = 0;
        defaults.velocity[r] = 0;
        note_hash = note_hash + defaults.sequence[r];
     }
     defaults.note_hash = note_hash;
    storage.Init(defaults);

    loadData();

    //If saved data are not there, we should restore the defaults
    if (sequenceLength < 1 || scaleNum < 1 || scaleNum >= numScales) { 
        storage.RestoreDefaults();  
        loadData();
    }

    for(;;)
    {
        hw.ProcessAllControls();

        for (int i = 0; i < 3; ++i) {
            buttons[i].Debounce();
        }

        // Read pots
        float knob_root = hw.GetKnobValue(KNOB_1);
        float knob_rest = hw.GetKnobValue(KNOB_2);
        float knob_range = hw.GetKnobValue(KNOB_3); 
        float knob_mutation = hw.GetKnobValue(KNOB_4);
        float knob_page = hw.GetKnobValue(KNOB_5);

        // Read incoming cv
        float cv_root = hw.GetCvValue(CV_ROOT);
        float cv_rest = hw.GetCvValue(CV_PATH);
        float cv_range = hw.GetCvValue(CV_BRANCHES);
        float cv_mutation = hw.GetCvValue(CV_MUTATION);
        float cv_new = hw.GetCvValue(CV_RATE);

        float root_volts = (cv_root + cal_global_offset)/cal_points_per_volt;
        transpose_voltage = (knob_root * 2 - 1) + root_volts;
        
        rest_probability = DSY_CLAMP((knob_rest * 100 - 10) + (cv_rest * 100 -10 ),0,90);
        newnote_range = (knob_range + cv_range) > 0.5;
        mutation_prob = DSY_CLAMP((knob_mutation + cv_mutation) * 100,0,100); 

        if (cv_new > 0.1 && new_prev < 0.1) {
            newMelody();
        }
        new_prev = cv_new;

        // Read buttons
        if (buttons[SRBUTTON_SHIFT].Pressed()) {
            hw.SetLed(LED_SHIFT,1.0 * global_brightness,1.0 * global_brightness,1.0 * global_brightness);
        } else {
            hw.SetLed(LED_SHIFT,0.0,0.0,0.0);
        }

        if (buttons[SRBUTTON_RESET].Pressed()) {
            hw.SetLed(LED_RESET,1.0 * global_brightness,1.0 * global_brightness,1.0 * global_brightness);
        } else {
            hw.SetLed(LED_RESET,0.0,0.0,0.0);
        }

        if (buttons[SRBUTTON_CHANNEL].RisingEdge()) {
            saveData();
        }
        if (buttons[SRBUTTON_RESET].isPressedLong()) {
            loadData();
        }

        if (!buttons[SRBUTTON_CHANNEL].Pressed() ) {
            hw.SetLed(LED_CHANNEL,0.0,0.0,0.0);
        } else {
            hw.SetLed(LED_CHANNEL,1,0.0,1);
        }

        if (buttons[SRBUTTON_RESET].isReleasedShort()) {
            newMelody();
        }

        hw.WriteLeds();
  
        int step_to_compare = current_step % 8;
        
        if (follow) {
            display_page = current_page;
        } else {
            display_page = floor(knob_page * 4);
        }
        int modifier = display_page * 8;
        
        // Check encoder clicks
        for (int i = 0; i < ENC_LAST; ++i) {
            if (hw.ButtonRisingEdge(i)) {
                if (hw.ButtonState(BUTTON_SHIFT)) {
                    doShiftClick(i);
                } else {
                    sequence[i + modifier].muted = !sequence[i + modifier].muted;
                }
            }
        }


        // Set individual step LEDs
        for (size_t i = 0; i < 8; ++i) {

            if ((int)i == step_to_compare && current_page == display_page) {
                if (!buttons[SRBUTTON_SHIFT].Pressed()) hw.SetLed(hw.trunk_leds[i],1.0 * global_brightness,1.0 * global_brightness,1.0* global_brightness);
            } else {
                if (!sequence[i + modifier].muted) {
                    if (!buttons[SRBUTTON_SHIFT].Pressed()){
                        int noteval = sequence[i + modifier].noteNumMIDI-midi_min;
                        hw.SetLed(hw.trunk_leds[i],colormap_seq[noteval].r * global_brightness,colormap_seq[noteval].g * global_brightness,colormap_seq[noteval].b * global_brightness);
                    }
                } else {
                    if (!buttons[SRBUTTON_SHIFT].Pressed()){
                        hw.SetLed(hw.trunk_leds[i],0.0,0.0,0.0);
                    }
                }
                int enc = hw.encoders[i].Increment();
                if (enc) {
                    doEncoder(i,enc);
                } 
            }
        }

        // Blink clock light
        float clkval = hw.ButtonState(CLOCK_IN);
        hw.SetLed(LED_CLOCK,0.0,clkval * global_brightness,clkval* global_brightness);

        // Set gate out light
        float gate1val = gate1.GetCurrentState();
        hw.SetLed(LED_GATE1,0.0,0.0,gate1val* global_brightness);

        // Set accent light
        float accval = accentGate.GetCurrentState();
        hw.SetLed(LED_GATE2,0.0, 0.0, accval * global_brightness);

        // set page LEDs
        for (int x =0; x < 4; ++x) {
            if (x == display_page) {
                hw.SetLed(hw.center_leds[x],steps_rainbow[7].r * global_brightness, steps_rainbow[7].g * global_brightness, steps_rainbow[7].b * global_brightness);
            } else if (x == current_page) {
                hw.SetLed(hw.center_leds[x],steps_rainbow[5].r * global_brightness, steps_rainbow[5].g * global_brightness, steps_rainbow[5].b * global_brightness);
            } else {
                hw.SetLed(hw.center_leds[x],0,0,0);
            }
        }

        // update LEDs
        if (!buttons[SRBUTTON_SHIFT].Pressed()) hw.WriteLeds();

        // Reset to defaults
        if (buttons[SRBUTTON_CHANNEL].isPressedLong()) {

            storage.RestoreDefaults();
            loadData();
            hw.SetLed(LED_CHANNEL,1,0,0);
            hw.SetLed(LED_RESET,1,0,0);
            hw.SetLed(LED_SHIFT,1,0,0);
            hw.WriteLeds();
            hw.SetLed(LED_CHANNEL,0,0,0);
            hw.SetLed(LED_RESET,0,0,0);
            hw.SetLed(LED_SHIFT,0,0,0);
            hw.WriteLeds();
        }
        if (hw.ButtonRisingEdge(RESET_TRIG)) {
            current_step = 0;
        }

        // DETECT INCOMING CLOCK
        if (hw.ButtonRisingEdge(CLOCK_IN)) {
            doStep();      
        }
        
    }
}
