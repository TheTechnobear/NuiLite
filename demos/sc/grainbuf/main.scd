~rPort=3000;
~synth = nil;
~audio_files = nil;
~audio_bufs = List.new;
~dict = Dictionary.new;

s.waitForBoot{
	"Server Booted".postln;
	"GrainBuf patch".postln;

	~path="/home/we/audio/*";~audio_files= List.new;
	SoundFile.collect(~path).do { |f| ~audio_files.add(f.path);};
	~audio_files.do { | f| ~audio_bufs.add(Buffer.readChannel(s,f,channels:[0]))};

	SynthDef(\sdGrainBuf, {
		arg sndbuf, gate = 1,
		amp = 0.6, density = 10,
		dur=0.1, rate=1.0,
		pos=0, interp=2,
		pan=0, envbufnum = -1,
		maxgrains=512;

		var sig,env, gen;
		env = EnvGen.kr(
				Env([0, 1, 0], [1, 1], \sin, 1),
				gate,
				levelScale: amp,
				doneAction: 2);

		gen = Impulse.kr(density);

		sig = GrainBuf.ar(
			2,
			gen,
			dur,
			sndbuf,
			rate,
			pos,
			interp,
			pan,
			envbufnum,
			maxgrains,
		);
		Out.ar(0,sig);
	}
	).add;

	s.sync;
	2.wait;
	
        ~dict.add(\nbuf -> 0);
        ~dict.add(\sndbuf -> ~audio_bufs[0]);
	~dict.postln;
	~synth = Synth(\sdGrainBuf,~dict.asPairs);
};

