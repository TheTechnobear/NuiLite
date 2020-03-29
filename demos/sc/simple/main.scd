s.waitForBoot{
"Server Booted".postln;
"Simple Test Synth".postln;

~pitch = 440;


SynthDef(\testSynth, {
        arg freq =440;
        var sig;
        sig = Saw.ar(freq);
        Out.ar(0,sig!2);
}).add;

s.sync;

~synth=Synth.new(\testSynth);

};

(
OSCdef( \oscencoder, {
    arg msg, time, addr, recvPort;
    ~pitch = ~pitch + (msg[2] * 100);
    ~synth.set(\freq, ~pitch);
},
"/nui/encoder",
recvPort:3000
);


)

