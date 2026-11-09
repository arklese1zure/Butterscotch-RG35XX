#ifndef _SW_INLINED_H
#define _SW_INLINED_H

#include "defines.h"

FORCE_INLINE int swrMin(int a, int b) { return a < b ? a : b; }
FORCE_INLINE int swrMax(int a, int b) { return a > b ? a : b; }
FORCE_INLINE int swrAbs(int x) { return x < 0 ? -x : x; }

FORCE_INLINE int swrSgn(float x)
{
    if (x < 0) return -1;
    return 1;
}

FORCE_INLINE int swrFloor(float x)
{
    int i = (int) x;
    return i - (x < (float) i);
}

FORCE_INLINE int swrCeiling(float x)
{
    int i = (int) x;
    return i + (x > (float) i);
}

// How many quarter turns (0 to 3) this angle is, or -1 if it isn't a whole number of them.
FORCE_INLINE int swrQuarterTurns(float angleDeg)
{
    float turns = angleDeg / 90.0f;
    float nearest = floorf(turns + 0.5f);
    if (fabsf(turns - nearest) > 0.0001f) return -1;
    int result = ((int) nearest) % 4;
    return result < 0 ? result + 4 : result;
}

FORCE_INLINE bool swrIsWhole(float value)
{
    return fabsf(value - floorf(value + 0.5f)) < 0.0001f;
}

// Checks if the "rotate" version of the sprite drawing routine should be used.
FORCE_INLINE bool swrMustRotate(float angleDeg)
{
    int angleDegInt = (int)(angleDeg * 4);
    angleDegInt %= 360*4;
    
    if (angleDegInt > 180*4)
        angleDegInt -= 360*4;
    
    return swrAbs(angleDegInt) >= 1; // 0.25 degrees
}

// Checks if the "rotate" version of the sprite drawing routine should be used.
// This is a more sensitive version.
FORCE_INLINE bool swrMustRotateSensitive(float angleDeg)
{
    int angleDegInt = (int)(angleDeg * 16);
    angleDegInt %= 360*16;
    
    if (angleDegInt > 180*16)
        angleDegInt -= 360*16;
    
    return swrAbs(angleDegInt) >= 1; // 1/16 of a degree
}

#endif//_SW_INLINED_H
