BeatWarper : MultiOutUGen {
	*ar { 
		arg numChannels, bufnum, length = 1, 
		slices, sliceLength = 1, 
		fadeIn = 10, fadeOut = 10, 
		mul = 1, add = 0;

		^this.multiNew(
			'audio', numChannels, bufnum, 
			length, sliceLength, 
			fadeIn, fadeOut, 
			*slices
		).madd(mul, add);
	}

	init { arg numChannels ... theInputs;
		inputs = theInputs;
		^this.initOutputs(numChannels, 'audio');
	}

	argNamesInputsOffset { ^2 }
}
