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
