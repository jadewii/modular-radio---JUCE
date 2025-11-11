/**
 * SoundTouch Library Wrapper
 * Includes all SoundTouch source files for compilation
 */

// Disable warnings for third-party code
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Weverything"

// Core SoundTouch files
#include "SoundTouch/AAFilter.cpp"
#include "SoundTouch/FIRFilter.cpp"
#include "SoundTouch/FIFOSampleBuffer.cpp"
#include "SoundTouch/RateTransposer.cpp"
#include "SoundTouch/SoundTouch.cpp"  // Main SoundTouch implementation
#include "SoundTouch/TDStretch.cpp"
#include "SoundTouch/BPMDetect.cpp"
#include "SoundTouch/PeakFinder.cpp"

// Interpolation files
#include "SoundTouch/InterpolateCubic.cpp"
#include "SoundTouch/InterpolateLinear.cpp"
#include "SoundTouch/InterpolateShannon.cpp"

// CPU detection (for optimization)
#include "SoundTouch/cpu_detect_x86.cpp"

// CPU-specific optimizations
#include "SoundTouch/sse_optimized.cpp"
#include "SoundTouch/mmx_optimized.cpp"

#pragma clang diagnostic pop
