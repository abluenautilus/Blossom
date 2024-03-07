#pragma once
#include <string>
#include <map>
#include <vector>
#include "Bloom.h"

const bloom::Color semitones_blue[12] = {
    {0, 0.0, 0.1}, 
    {0, 0.1, 0.1},
    {0, 0.1, 0.3},
    {0, 0.1, 0.4},
    {0, 0.2, 0.4},
    {0, 0.3, 0.5},
    {0, 0.3, 0.6},
    {0, 0.4, 0.7},
    {0, 0.4, 0.8},
    {0, 0.5, 0.8},
    {0, 0.5, 0.9},
    {0, 0.5, 1.0}
};

const bloom::Color semitones_red[12] = {
    {0.1, 0, 0}, 
    {0.2, 0, 0},
    {0.3, 0, 0},
    {0.4, 0, 0},
    {0.4, 0.2, 0},
    {0.4, 0.3, 0},
    {0.5, 0.3, 0},
    {0.6, 0.3, 0},
    {0.7, 0.4, 0},
    {0.8, 0.4, 0},
    {0.9, 0.5, 0},
    {1.0, 0.5, 0}
};

const bloom::Color red_rev[2] {
    {.2,0,0},
    {1,0.5,0}
};

const bloom::Color semitones_rainbow[12] = {
    {1, 0, 0},  //RED
    {1, 0.4, 0},
    {1, 0.8, 0},
    {1, 1, 0},  //YELLOW
    {0.5, 1, 0},
    {0, 1, 0}, //GREEN
    {0, 1, 0.5},
    {0, 0.5, 1},
    {0.0, 0.0, 1}, //BLUE
    {0.3, 0.0, 1.0},
    {0.7, 0.0, 1.0},
    {1.0, 0, 1.0} // PURPLE
};

const bloom::Color semitones_rainbow_rev[5] = {

    {0.0, 0.0, 1}, //BLUE
    {0, 1, 0}, //GREEN
    {1, 1, 0},  //YELLOW
    {1, 0, 0},  //RED
    {0.33, 0, 1.0}, // PURPLE

};

const bloom::Color steps_rainbow[8] = {
    {1, 0, 0},  //RED
    {1, 0.5, 0},
    {1, 1, 0},  //YELLOW
    {0, 1, 0}, //GREEN
    {0, 0.5, 0.5},
    {0.0, 0.0, 1}, //BLUE
    {0.5, 0.0, 1.0},
    {1.0, 0, 1.0} // PURPLE
};

float lerp(float a, float b, float f)
{
    return a * (1.0 - f) + (b * f);
}

// function to interpolate color tables
inline bloom::Color* interp(const bloom::Color colorlist[], size_t num) {

    static bloom::Color colormap[48];
    size_t eachstep = 48/(num-1);
    size_t remainder = 48 % (num-1);
    int counter = 0;

    for (size_t i = 0; i < num-1; ++i) {

        size_t limit = eachstep;
        if (i == num-2) limit += remainder;
        for (size_t step = 0; step < limit; ++step) {
            float inc = float(step)/float(eachstep);
            colormap[counter].r = lerp(colorlist[i].r, colorlist[i+1].r, inc);
            colormap[counter].g = lerp(colorlist[i].g, colorlist[i+1].g, inc);
            colormap[counter].b = lerp(colorlist[i].b, colorlist[i+1].b, inc);
            ++counter;
        }
    }
    return colormap;
}