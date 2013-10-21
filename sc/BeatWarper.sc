MyCounter : MultiOutUGen {
	*ar { arg numChannels, bufnum, length = 1, slices, mul = 1, add = 0;
		^this.multiNew('audio', numChannels, bufnum, length, *slices).madd(mul, add);
	}

	init { arg numChannels ... theInputs;
		inputs = theInputs;
		^this.initOutputs(numChannels, 'audio');
	}

	argNamesInputsOffset { ^2 }
}
