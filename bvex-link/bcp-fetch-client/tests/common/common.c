#include <math.h>
#include "common.h"

// Function to generate a sine wave value
double generate_sinusoid(double amplitude, double freq, double shift, double x)
{
    return amplitude * sin(freq * x + shift);
}