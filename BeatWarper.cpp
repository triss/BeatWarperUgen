#include "SC_PlugIn.h"
#include <stdio.h>


// Ugen argument positions
#define BUFFER 0
#define LENGTH 1
#define SLICE_OFFSET 2


static InterfaceTable *ft;


// declare struct to hold unit generator state
struct BeatWarper : public Unit
{
	uint32 warpPos;		// position in warper - at destination tempo
	uint32 bufferPos;	// position in buffer
	int prevSlice; 		// current slice number

	// buffer storage - utilised by the GET_BUF_SHARED macro
	SndBuf *m_buf;
	float m_fbufnum;
};


// declare unit generator functions
static void BeatWarper_next_a(BeatWarper *unit, int inNumSamples);
static void BeatWarper_Ctor(BeatWarper* unit);


// constructor called upon ugen startup
void BeatWarper_Ctor(BeatWarper* unit)
{
	// set the calculation function
	SETCALC(BeatWarper_next_a);

	// initialise unit generator state
	unit->warpPos = 0;
	unit->prevSlice = 0;
	unit->bufferPos = 0;

    // calculate one sample of output - every ugen needs to do this
	BeatWarper_next_a(unit, 1);
}


// calculation function for an audio rate frequency argument
void BeatWarper_next_a(BeatWarper *unit, int inNumSamples)
{
	// read in length ugen arg 
    int length = (int)(IN0(LENGTH) * SAMPLERATE); 	

	// calculate number of slices from number of ugen args
	int numSlices = unit->mNumInputs - SLICE_OFFSET; 	

	// load buffer
	GET_BUF_SHARED
	
	// check the buffer has loaded
	if (!bufData || ((bufFrames & ((unit->mWorld->mBufLength<<1) - 1)) != 0)) {
		unit->bufferPos = 0;
		ClearUnitOutputs(unit, inNumSamples);
		return;
	}

	// check num outputs is same as buffers channels
	if (unit->mNumOutputs != bufChannels) { 
		ClearUnitOutputs(unit, inNumSamples); 
		return; 
	} 

	// lookup current position from unit
	uint32 warpPos = unit->warpPos;
	int prevSlice = unit->prevSlice;

	// TODO fix wonkiness when shorter buffer loaded!!!
	if (unit->bufferPos >= bufFrames) {
		unit->bufferPos = unit->bufferPos % bufFrames;
	}

	// shift bufData pointer by current bufferPos * number of channels
	uint32 bufferPos = unit->bufferPos;
	bufData += bufferPos * bufChannels;

    // perform a loop for the number of samples in the control period
    for (int i=0; i < inNumSamples; ++i) {
		// calculate current slice number
		int slice = warpPos / length * numSlices;

		// if the slice has changed
		if(prevSlice != slice) { 	
			// jump to that slice's position
			bufferPos = IN0(slice + SLICE_OFFSET) * bufFrames;
			prevSlice = slice;
			printf("%i\n", slice);
		}

        // write the output for each channel in the buffer and shift the
		// buffer pointer
		for (uint32 c = 0; c < bufChannels; ++c) {
			// TODO Interpolate here?
			OUT(c)[i] = *bufData++;
		}

		// TODO Interpolate here alse?
		bufferPos = (bufferPos + 1) % bufFrames;
		warpPos = (warpPos + 1) % length;
    }

    // store the phase back to the struct
    unit->warpPos = warpPos;
	unit->bufferPos = bufferPos;
    unit->prevSlice = prevSlice;
}


// the entry point is called by the host when the plug-in is loaded
PluginLoad(BeatWarper) {
    ft = inTable; // store pointer to InterfaceTable

    DefineSimpleUnit(BeatWarper);
}
