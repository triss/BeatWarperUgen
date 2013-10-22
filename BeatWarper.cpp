#include "SC_PlugIn.h"
#include <stdio.h>


// Ugen argument positions
#define BUFFER 0
#define LENGTH 1
#define FADE_IN 2
#define FADE_OUT 3
#define SLICE_LENGTH 4

#define SLICE_OFFSET 5


static InterfaceTable *ft;


// declare struct to hold unit generator state
struct BeatWarper : public Unit
{
	uint32 warpPos;		// position in warper - at destination tempo
	uint32 bufferPos;	// position in buffer

	int prevSlice; 		// current slice number
	int numSlices; 		// number of slices

	bool fadingIn, fadingOut; 	// whether a fade in/out is in progress
	int fadeInPos, fadeOutPos; 	// how far through fade in/out we are
	uint32 fadeOutBufferPos; 	// buffer position that last fade out started

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

	unit->numSlices = unit->mNumInputs - SLICE_OFFSET;

    // calculate one sample of output - every ugen needs to do this
	BeatWarper_next_a(unit, 1);
}


// calculation function for an audio rate frequency argument
void BeatWarper_next_a(BeatWarper *unit, int inNumSamples)
{
	// read in length ugen arg at control rate
    int length = (int)(IN0(LENGTH) * SAMPLERATE); 	

	int fadeInTime = (int)(IN0(FADE_IN) * length);
	int fadeOutTime = (int)(IN0(FADE_OUT) * length);

	// apparently taking reciprocal and multiplying is quicker than dividing 
	// so we do this for length here
	double lengthRecip = 1.0f / length; 	

	// lookup current position, number of slices from unit
	uint32 warpPos = unit->warpPos;
	int prevSlice = unit->prevSlice;
	int numSlices = unit->numSlices;

	// load buffer
	GET_BUF_SHARED
	
	// check the buffer has loaded if not return
	if (!bufData || ((bufFrames & ((unit->mWorld->mBufLength<<1) - 1)) != 0)) {
		unit->bufferPos = 0;
		ClearUnitOutputs(unit, inNumSamples);
		printf("ERROR: Buffer not available");
		return;
	}

	// check num outputs is same as buffers channels if not return
	if (unit->mNumOutputs != bufChannels) { 
		ClearUnitOutputs(unit, inNumSamples); 
		printf("ERROR: Channel outputs != Buffer outputs")
		return; 
	} 

	// TODO fix wonkiness when shorter buffer loaded!!!
	if (unit->bufferPos >= bufFrames) {
		unit->bufferPos = unit->bufferPos % bufFrames;
	}

	// shift bufData pointer by current bufferPos * number of channels
	uint32 bufferPos = unit->bufferPos;
	bufData += bufferPos * bufChannels;

	int slice = 0;

    // perform a loop for the number of samples in the control period
    for (int i=0; i < inNumSamples; ++i) {
		// calculate current slice number
		slice = warpPos * lengthRecip * numSlices;
		
		// if the slice has changed
		if(prevSlice != slice) { 	
			// jump to that slice's position
			bufferPos = IN0(slice + SLICE_OFFSET) * bufFrames;
			prevSlice = slice;
		}

        // write the output for each channel in the buffer and shift the
		// buffer pointer
		for (uint32 c = 0; c < bufChannels; ++c) {
			// TODO Interpolate here?
			OUT(c)[i] = *bufData++;
		}

		// TODO Interpolate here also?
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
