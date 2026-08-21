# csound-for-juce

Brings the joy of Csound to JUCE developers.
Handles audio processing and parameters managment.
It hides all boilerplate code that is needed to write
VST/Clap plugins with Csound audio processing. See `examples` directory to get started.

It implements:

* solid audio processing loop
* parameter managment with smoothing and true bypass
* FX and synth plugins with side-chain

With this library you need to only write GUI code and audio processing
and managment of the plugin parameters is handled by the library.

The library is for audio-developers that which to use Csound for
audio processing but can write their own shiny UI.

### Comparison to Cabbage

So why do we need yet another Csound_JUCE tool if we have Cabbage?
Cabbage is great if you want to build a plugin entirely from CSD.
csound-for-juce is for when you want Csound's DSP power inside a JUCE plugin you control.
Use Cabbage if CSD is your whole plugin with UI produced by Cabbage. Use csound-for-juce if Csound
is one component of a larger JUCE project with customu UI.

