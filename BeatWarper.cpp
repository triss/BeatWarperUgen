#include "SC_PlugIn.h"
#include <stdio.h>

using namespace std; // for math functions

static InterfaceTable *ft;

////////////////////////////////////////////////////////////////////////////////
// lifted from /server/plugins/DelayUGens.cpp
////////////////////////////////////////////////////////////////////////////////

// checks if buffer is loaded properly
static inline bool checkBuffer(Unit * unit, const float * bufData, uint32 bufChannels,
							   uint32 expectedChannels, int inNumSamples)
{
	if (!bufData)
		goto handle_failure;

	if (expectedChannels > bufChannels) {
		if(unit->mWorld->mVerbosity > -1 && !unit->mDone)
			Print("Buffer UGen channel mismatch: expected %i, yet buffer has %i channels\n",
				  expectedChannels, bufChannels);
		goto handle_failure;
	}
	return true;

handle_failure:
	unit->mDone = true;
	ClearUnitOutputs(unit, inNumSamples);
	return false;
}

////////////////////////////////////////////////////////////////////////////////

// Ugen argument positions
#define BUFFER 0
#define WARPED_LENGTH 1
#define RATE 2
#define SLICE_LENGTH 3
#define FADE_IN 4
#define FADE_OUT 5

#define SLICE_OFFSET 6

// handles wrapping around in buffer when we need to interpolate beginning and
// end of file - only really useful when no fade in and sliceLength = 1 
inline double wrapAround(Unit *unit, double in, double hi)
{
	if (in >= hi) {
		in -= hi;
		if (in < hi) return in;
	} else if (in < 0.) {
		in += hi;
		if (in >= 0.) return in;
	} else return in;

	return in - hi * floor(in/hi);
}

// define a struct to hold unit generator state
struct BeatWarper : public Unit
{
	uint32 warpPos;		// position in warper - at destination tempo
	double phase;		// position in buffer - double to handle interpolation 
						// nicely

	double slicePos; 			// the current frame in a slice
	int prevSlice; 				// the number of the last slice to be played
	int numSlices; 				// number of slices
	double currentSliceStart; 	// the start position of the current slice
	int fadeOutPos;

	// buffer storage - utilised by the ACQUIRE_SNDBUF_SHARED macro
	// hence the ugly m_ names
	SndBuf *m_buf;
	float m_fbufnum;
};


// declare unit generator functions
static void BeatWarper_next_a(BeatWarper *unit, int inNumSamples);
static void BeatWarper_Ctor(BeatWarper* unit);


// constructor called upon ugen startup
void BeatWarper_Ctor(BeatWarper* unit)
{
	// set the calculation function - no one really wants to Warp a control 
	// rate buffer do they? I've just done one at audio rate
	SETCALC(BeatWarper_next_a);

	// initialise unit generator state
	unit->warpPos = 0;
	unit->prevSlice = 0;
	unit->phase = 0;

	unit->numSlices = unit->mNumInputs - SLICE_OFFSET;

	unit->m_fbufnum = -1e9f;
    
	// calculate one sample of output - every ugen needs to do this
    // (I don't think) the buffer isn't likely to be ready it seems so 
	// we'll just output a sample of silence as seems to be the standard
	// with buffer using UGens
	ClearUnitOutputs(unit, 1);
}


// calculation function for audio rate 
void BeatWarper_next_a(BeatWarper *unit, int inNumSamples)
{
	// lookup current position in warper, slice and buffer as well as number 
	// of slices from unit
	uint32 warpPos = unit->warpPos;
	int prevSlice = unit->prevSlice;
	int numSlices = unit->numSlices;
	double slicePos = unit->slicePos;
	double currentSliceStart = unit->currentSliceStart;
	int fadeOutPos = unit->fadeOutPos;

	// get our position in the buffer
	double phase = unit->phase;

	// read in warped length ugen arg at control rate
    int warpedLength = (int)(IN0(WARPED_LENGTH) * SAMPLERATE); 	

	// apparently taking reciprocal and multiplying is quicker than dividing 
	// over and over so we do this for length here
	double warpedLengthRecip = 1.0f / warpedLength; 	

	float rate = IN0(RATE);

	double sliceLengthSamples = 
		IN0(SLICE_LENGTH) * (warpedLength / numSlices);

	float fadeInSamples = IN0(FADE_IN);
	float fadeOutSamples = IN0(FADE_OUT);

	// load in the buffer if its changed
	float fbufnum  = ZIN0(0);
	if (fbufnum != unit->m_fbufnum) {
		uint32 bufnum = (int)fbufnum;
		World *world = unit->mWorld;
		if (bufnum >= world->mNumSndBufs) bufnum = 0;
		unit->m_fbufnum = fbufnum;
		unit->m_buf = world->mSndBufs + bufnum;
	}

	// acquire the buffer for reading
	const SndBuf *buf = unit->m_buf;
	ACQUIRE_SNDBUF_SHARED(buf);
	const float *bufData __attribute__((__unused__)) = buf->data;
	uint32 bufChannels __attribute__((__unused__)) = buf->channels;
	uint32 bufSamples __attribute__((__unused__)) = buf->samples;
	uint32 bufFrames = buf->frames;
	int mask __attribute__((__unused__)) = buf->mask;
	int guardFrame __attribute__((__unused__)) = bufFrames - 2;

	// check buffer has been acquired
	int numOutputs = unit->mNumOutputs;
	if (!checkBuffer(unit, bufData, bufChannels, numOutputs, inNumSamples))
		return;

	// if we've reached the end of the buffer go back to the beginning of it
	if (phase >= bufFrames) {
		phase = 0;
	}

	// probably best not to keep setting aside this space!
	// we'll just update the value sin here
	int currentSlice;
	double fadeAmt = 1.;

    // perform a loop for the number of samples in the control period
    for (int i=0; i < inNumSamples; ++i) {
		// calculate current slice number
		currentSlice = warpPos * warpedLengthRecip * numSlices;
		
		// if the slice has changed
		if(prevSlice != currentSlice) { 	
			// jump to that slice's position
			currentSliceStart = IN0(currentSlice + SLICE_OFFSET) * bufFrames;
			phase = currentSliceStart;
			prevSlice = currentSlice;
			slicePos = 0.0;
			fadeOutPos = 0;
		}

		// calculate fade in/out amount
		
		// if we're fading in
		if(slicePos < fadeInSamples) {
			fadeAmt = pow(slicePos / fadeInSamples, 4);
		}

		// if we're fading out
		if(slicePos >= sliceLengthSamples - fadeOutSamples) {
			fadeAmt	= 1.0 - pow(fadeOutPos / fadeOutSamples, 4);

			if(fadeAmt < 0.0) {
				fadeAmt = 0.0;
			}

			fadeOutPos += 1;
		}

		// look up positions of 4 samples to interpolate
		phase = wrapAround((Unit*)unit, phase, bufFrames); 
		int32 iphase = (int32)phase; 
		const float* bPos = bufData + iphase * bufChannels; 
		const float* aPos = bPos - bufChannels; 
		const float* cPos = bPos + bufChannels; 
		const float* dPos = cPos + bufChannels; 

		// change positions if we're about to loop around
		// only really useful if no fade times are set
		if (iphase == 0) { 
			aPos += bufSamples; 
		} else if (iphase >= guardFrame) { 
			if (iphase == guardFrame) { 
				dPos -= bufSamples; 
			} else { 
				cPos -= bufSamples; 
				dPos -= bufSamples; 
			} 
		} 

		// work out how far between samples we'd like to be
		float fracphase = phase - (double)iphase; 

		// interpolate each channel and output
		for (int32 channel=0; channel<numOutputs; ++channel) { 
			// look up the samples we're going to interpolate
			float a = aPos[channel]; 
			float b = bPos[channel]; 
			float c = cPos[channel]; 
			float d = dPos[channel]; 

			// interpolate them, apply fade and output them
			OUT(channel)[i] = cubicinterp(fracphase, a, b, c, d) * fadeAmt; 
		}

		phase += rate;

		slicePos = phase - currentSliceStart;
		if(slicePos < 0.) { 
			slicePos = 0.;
		}

		warpPos = (warpPos + 1) % warpedLength;
    }

	RELEASE_SNDBUF_SHARED(buf);

    // store everything back to the struct so we can look it up next cycle
    unit->warpPos = warpPos;
	unit->phase = phase;
	unit->slicePos = slicePos;
    unit->prevSlice = prevSlice;
	unit->currentSliceStart = currentSliceStart;
	unit->fadeOutPos = fadeOutPos;
}


// the entry point is called by the host when the plug-in is loaded
PluginLoad(BeatWarper) {
    ft = inTable; // store pointer to InterfaceTable

    DefineSimpleUnit(BeatWarper);
}
