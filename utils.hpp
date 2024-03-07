#pragma once
#include <random>
#include <assert.h>
#include "daisy_seed.h"

using namespace daisy;


//Generates a random number from a list of weighted options
int weightedRandom(int weights[],int num_choices, Random rng) {


    int sum_of_weight = 0;

    for(int i=0; i<num_choices; i++) {
        sum_of_weight += weights[i];
    }

    int rnd = floor(rng.GetFloat(sum_of_weight));
    //int rnd = std::rand() % sum_of_weight;

    for(int i=0; i<num_choices; i++) {
        if(rnd < weights[i]) {
            return i;
        }
        rnd -= weights[i];
    }
    assert(!"we should not ever get to this place");
}