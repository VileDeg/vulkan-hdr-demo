#include "stdafx.h"
#include "math_utils.h"

namespace math {
    int roundUpPw2(int numToRound, int multiple)
    {
        //From https://stackoverflow.com/questions/3407012/rounding-up-to-the-nearest-multiple-of-a-number
        assert(multiple && ((multiple & (multiple - 1)) == 0));
        return (numToRound + multiple - 1) & -multiple;
    }
}