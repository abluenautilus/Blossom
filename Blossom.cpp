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
Gate gate1,gate2,clockout,tuningGate;

// Knob values
float knob1, knob2, knob3, knob4, knob5;

SRButton buttons[3]; // shift register buttons

Random rng;

// Sequence data
const int maxSteps = 32;
const int minSteps = 3;
Note current_note;
Note sequence[maxSteps];
int sequenceLength = 8;

float transpose_voltage = 0.0;

int current_step = 0;
int current_page = 0;
int display_page = 0;
int current_place = 0;

float global_brightness = 0.5;
float rest_probability = 0.0;
int numScales;
bool follow = false;
float mutation_prob;

//Melody creation
int noteOptionWeights[4] = {0,0,0,10}; //REPEAT UP DOWN NEW
std::string baseKey = "C";
std::string scale = "Natural Minor";
uint8_t scaleNum = 2;
uint8_t scaleNumPrev = 5;
uint8_t baseOctave = 4;
const int midi_max = 84;
const int midi_min = 36;
const int min_octave = 2;
const int max_octave = 5;
int newnote_range = 1;

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
    int sequence[32];
    bool muted[32];
    bool operator!=(const Settings& a) {
        return a.sequenceLength != sequenceLength;
    }
};

Settings& operator* (const Settings& settings) { return *settings; }
PersistentStorage<Settings> storage(hw.seed.qspi);

void saveData() {
    
    Settings &localSettings = storage.GetSettings();
    localSettings.sequenceLength = sequenceLength;
    localSettings.global_brightness = global_brightness;
    localSettings.follow = follow;
    hw.seed.PrintLine("Saving length %d brightness %.2f",sequenceLength,global_brightness);
    for (size_t i = 0; i < 32; ++i) {
        localSettings.sequence[i] = sequence[i].noteNumMIDI;
        localSettings.muted[i] = sequence[i].muted;
        hw.seed.PrintLine("Saving step %d note %d",i,localSettings.sequence[i]);
    }
    storage.Save();
    hw.seed.PrintLine("Saved length %d",sequenceLength);
}

void loadData() {
    hw.seed.PrintLine("Before load %d",sequenceLength);
    Settings &localSettings = storage.GetSettings();
    sequenceLength = localSettings.sequenceLength;
    global_brightness = localSettings.global_brightness;
    hw.seed.PrintLine("Loading length %d brightness %.2f",sequenceLength,global_brightness);
    follow = localSettings.follow;
    for (size_t i = 0; i < 32; ++i) {
        sequence[i].changeMIDINoteNum(localSettings.sequence[i]);
        sequence[i].muted = localSettings.muted[i];
        hw.seed.PrintLine("Loading step %d note %d",i,localSettings.sequence[i]);
    }
    hw.seed.PrintLine("Loaded length %d",sequenceLength);
}


// Yeah we don't make any audio but we use the audio callback anyways
static void AudioCallback(AudioHandle::InputBuffer  in,
                          AudioHandle::OutputBuffer out,
                          size_t                    size)
{
   for(size_t i = 0; i < size; i++)
    {
        // Advance gates
        float tuninggate_val = tuningGate.Process(); 
        hw.WriteGate(GATE_1,tuninggate_val);

        float gate1_val = gate1.Process(); 

        if (!tuninggate_val) {
            hw.WriteGate(GATE_1,gate1_val);
        }


        float gate2_val = gate2.Process(); 
        hw.WriteGate(GATE_2,gate2_val);
        float clockout_val = clockout.Process(); 
        hw.WriteGate(CLOCK_OUT,clockout_val);

        // set cv out voltage
        if (!current_note.muted && !tuninggate_val)  {
            hw.SetCvOut(CV_OUT_1,current_note.voltage + transpose_voltage);
        }

    }
};

Note getRandomNote(int range) {

    //Choose a random note from the current scale

    int ourSemitone;
    //int newNoteMIDI;
    int ourChoice;

    int num_choices = validToneWeights.size();
    ourChoice = weightedRandom(&validToneWeights[0],num_choices, rng);

    ourSemitone = validTones[ourChoice];
    int oct_min = baseOctave - range;
    int oct_max = baseOctave + range;
    int ourOctave;
    if (oct_max - oct_min == 0) {
        ourOctave = baseOctave;
        hw.seed.PrintLine("ourOctave is %d",ourOctave);
    } else {
       int rn = rng.GetValue() % (oct_max - oct_min + 1);
       ourOctave = rn + oct_min;
       hw.seed.PrintLine("rn is %d ourOctave is %d",rn,ourOctave);
    }
   
    Note newNote = Note(ourSemitone, ourOctave);
    return newNote;

};

void changeNotes() {
    
    hw.seed.PrintLine("---------Note change---------");
    //substitute notes in the melody with new notes

    int noteToChange = (rng.GetValue() % sequenceLength) - 1;

    Note newNote = getRandomNote(newnote_range);

    int t = rng.GetValue() % 100;
    hw.seed.PrintLine("restprob is %.2f",rest_probability);
    hw.seed.PrintLine("t is %d",t);
    if (t < rest_probability) { 
        newNote.muted = true;  
    } else {
        newNote.muted = false;
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
            //hw.seed.PrintLine("noteChoice is %.2f mut prob %.2f",noteChoice,mutation_prob);
            changeNotes();
        } else {
            //hw.seed.PrintLine("NO CHANGE noteChoice is %.2f mut prob %.2f",noteChoice,mutation_prob);
        }
    }

    current_note = sequence[current_step];

    if (!current_note.muted) {
         gate1.ReTrigger();
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

    hw.seed.PrintLine("----NEW MELODY----");

    hw.seed.PrintLine("Getting scale %d",scaleNum);

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
            hw.seed.PrintLine("NoteKind: %d",noteKind);
        } else {
            noteKind = NM_NEW;
            hw.seed.PrintLine("Note kind: NEW");
        }
        
        if (noteKind == NM_REPEAT) {
            sequence[x] = prevNote;
            hw.seed.PrintLine("Note kind: REPEAT");

        } else if (noteKind == NM_DOWN) {

            hw.seed.PrintLine("Note kind: DOWN");
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

            hw.seed.PrintLine("Note kind: UP");
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
            hw.seed.PrintLine("Note kind: NEW");
            Note newNote = getRandomNote(newnote_range);
            sequence[x] = newNote;
            prevNote = newNote;
        }
        int t = rng.GetValue() % 100;
        if (t < rest_probability) { 
            sequence[x].muted = true;
        } else {
            sequence[x].muted = false;
        }
       

        prevNote = sequence[x];
        
    } 

    //log new seqence
    hw.seed.PrintLine("NEW MELODY:--");
    for (int i = 0; i <=31; i++){
        hw.seed.PrintLine("--: %s%d %d Muted: %d",sequence[i].noteName.c_str(),sequence[i].octave, sequence[i].noteNumMIDI,sequence[i].muted);
    }
    hw.seed.PrintLine("newnoterange %d",newnote_range);

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

            hw.seed.PrintLine("Current note is tone %d octave %d which is at index %d",currentTone,currentOctave,validToneIndex);

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

            hw.seed.PrintLine("New octave is %d new tone is %d",newOctave,newTone);

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

        hw.SetCvOut(CV_OUT_1,sequence[enc + modifier].voltage);
        tuningGate.ReTrigger();

    } else {

        hw.seed.PrintLine("shifted encoder turned.");
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

        }
    }
}

void doShiftClick(int enc) {

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
    int numLEDS = hw.ClearLeds();
    hw.seed.PrintLine("Number of LEDS: %d",numLEDS);
    hw.seed.StartAudio(AudioCallback);
    SampleRate = hw.seed.AudioSampleRate();

    numScales = scaleNames.size();

    rng.Init();

    // Set up gates for output
    clockout.Init(SampleRate);
    clockout.SetDuration(0.1);
    gate1.Init(SampleRate);
    gate1.SetDuration(0.01);
    gate2.Init(SampleRate);
    gate2.SetDuration(0.01);
    tuningGate.Init(SampleRate);
    tuningGate.SetDuration(0.25);

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
    //newMelody();
    //calibrationMelody();

    Settings defaults;
    defaults.sequenceLength = 8;
    defaults.global_brightness = 0.5;
    defaults.follow = false;
    for (size_t r = 0; r < 32; ++r) {
        defaults.sequence[r] = 60;
        defaults.muted[r] = false;
     }
    storage.Init(defaults);

    loadData();

    for(;;)
    {
        hw.ProcessAllControls();

        for (int i = 0; i < 3; ++i) {
            buttons[i].Debounce();
        }

        // Read pots
        float knob_root = hw.GetKnobValue(KNOB_1);
        transpose_voltage = knob_root * 2 - 1 ;
        
        float knob_rest = hw.GetKnobValue(KNOB_2);
        rest_probability = DSY_CLAMP(knob_rest * 100 - 10,0,90);

        float knob_branches = hw.GetKnobValue(KNOB_3);
        newnote_range = knob_branches > 0.5;

        float knob_mutation = hw.GetKnobValue(KNOB_4);
        mutation_prob = knob_mutation * 100;

        float knob_rate = hw.GetKnobValue(KNOB_5);
        

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
        if (buttons[SRBUTTON_RESET].isReleasedLong()) {
            hw.seed.PrintLine("Released Long");
        }
        if (buttons[SRBUTTON_RESET].isDoubleClicked()) {
            hw.seed.PrintLine("Double Clicked");
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
            display_page = floor(knob_rate * 4);
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

        //Check incoming CV
        float cv1 = hw.GetCvValue(CV_PATH);
        hw.SetLed(LED_CENTER1,0.0,0.0,cv1);

        // Blink clock light
        float clkval = hw.ButtonState(CLOCK_IN);
        hw.SetLed(LED_CLOCK,0.0,clkval * global_brightness,clkval* global_brightness);

        // Set gate out light
        float gate1val = gate1.GetCurrentState();
        hw.SetLed(LED_GATE1,0.0,0.0,gate1val* global_brightness);

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

        // DETECT INCOMING CLOCK
        if (hw.ButtonRisingEdge(CLOCK_IN)) {
            doStep();      
        }
        // throttle update rate
        //System::Delay(100);
        
    }
}
