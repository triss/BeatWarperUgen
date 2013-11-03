BeatWarper : MultiOutUGen {
	*ar { 
		arg numChannels, bufnum, length = 1, 
		rate = 1,
		slices, sliceLength = 1, 
		fadeIn = 10, fadeOut = 10, 
		mul = 1, add = 0;

		^this.multiNew(
			'audio', numChannels, bufnum, 
			length, rate, sliceLength, 
			fadeIn, fadeOut, 

			// slices needs to come last since ugen code receives 
			// arrays as a list of arguments
			*slices 
		).madd(mul, add);
	}

	// ensure number of channels gets handled 
	// properly by buffer handling macro
	init { arg numChannels ... theInputs;
		inputs = theInputs;
		^this.initOutputs(numChannels, 'audio');
	}

	argNamesInputsOffset { ^2 }
}
