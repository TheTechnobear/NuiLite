/*
Default Template : Mark Harris (aka TheTechnobear)
This is an example of the what you might need for your supercollider patch on the Norns
It shows recommended server options and incoming messages from norns via osc
Note: if you want to do live coding/interaction from your remote computer, look at the live coding template

usage:
- copy template to use as starting point for new your patch
- implement OSCdef that you want to use
- remove OSCdef you do not need
*/


/*
NUI OSC messages received
encoder num value
button  num value
numencoders value
numbuttons value

SK OSC messages received
*/

s.waitForBoot{
"Server Booted".postln;
"Default patch".postln;
};


// - knobs/buttons/cv are received via osc, see OSCdef below
// - audio input/output  - normal SC, use SoundIn.ar, Out.ar , as normal
// - audio files - normal SC, use SoundFile.collect("/home/we/audio/*"), then use Buffer.read
// e.g.
// ~audio_files.SoundFile.collect("/home/we/audio/*")
// z=Buffer.read(s,audio_files[0]);

//OSCdef to receive knobs/buttons from norns
//in practice, Id recommend you remove those you dont need, all are listed here for 'completeness'

(
~rPort=3000;

// messages coming from the nebulae
OSCdef( \encoder, {
	arg msg, time, addr, recvPort;
},
'/nui/encoder',recvPort:~rPort);

OSCdef( \button, {
	arg msg, time, addr, recvPort;
},
'/nui/button',recvPort:~rPort);

OSCdef( \numencoders, {
	arg msg, time, addr, recvPort;
},
'/nui/numencoders',recvPort:~rPort);

OSCdef( \numbuttons, {
	arg msg, time, addr, recvPort;
},
'/nui/numbuttons',recvPort:~rPort);

)

