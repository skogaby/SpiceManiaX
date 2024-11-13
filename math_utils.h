#pragma once

// Map a value from one numberspace to another, while maintaining proportionality between the input
// numberspace and the output value with respect to the output numberspace.
int MapValue(int x, int in_min, int in_max, int out_min, int out_max) {
    return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}
