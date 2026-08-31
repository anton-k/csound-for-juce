# csound-for-juce brings the joy of Csound to JUCE developers

csound-for-juce handles the audio processing loop and parameter management
so you don't have to. It hides all the boilerplate needed to
build VST, AU, and CLAP plugins powered by Csound.

Csound has been the gold standard of computer music research since
1985 — over 1,800 opcodes, from granular synthesis to spectral
processing to physical modelling. It's a lifetime of accumulated
DSP knowledge you can use today.
csound-for-juce brings that power into JUCE plugins without
the boilerplate. No C API wrangling. No thread management.
No channel mapping. Just a clean C++ library that handles
the audio loop and parameters, while you focus on sound design and your UI.

**What it gives you**:

* Solid audio processing loop, ready for production
* Parameter management with smoothing and true bypass
* Support for both FX and synth plugins, including side-chain input
* Seamless CSD embedding and lifecycle management
* Full access to Csound's vast opcode library — use any algorithm Csound offers, directly in your plugin

**What you write**: just the GUI and Csound file for audio core. Audio processing, parameter routing, and plugin lifecycle are handled by the library.

**Who it's for**: audio developers who want to harness Csound's decades of DSP innovation inside a JUCE plugin they fully control — and who'd rather write their own shiny UI than fight with C's threading and channel APIs.

Get started:

* [`examples/`](https://github.com/anton-k/csound-for-juce/tree/main/examples) - example plugins built with the library
* [Quick start guide](https://github.com/anton-k/csound-for-juce/blob/main/tutorial/QuickStartGuide.md)
* check out the repo [`reverb-csd-juce`](https://github.com/anton-k/reverb-csd-juce)
   for an example on how to use the library with Cmake in your own project.

## Comparison to Cabbage

So why do we need yet another Csound_JUCE tool if we have Cabbage?
Cabbage is great if you want to build a plugin entirely from CSD.
csound-for-juce is for when you want Csound's DSP power inside a JUCE plugin you control.
Use Cabbage if CSD is your whole plugin with UI produced by Cabbage. Use csound-for-juce if Csound
is one component of a larger JUCE project with custom UI.

Cabbage is a complete plugin authoring environment. You write your CSD file with
embedded GUI annotations, and Cabbage builds the entire plugin for you — DSP, UI,
and all. It's fantastic if CSD is your whole plugin and you're happy with Cabbage's
 built-in widget system.

csound-for-juce is a library, not a framework. It gives you Csound's audio
engine inside a JUCE plugin that you architect and control.
You write your own GUI with JUCE's component system, your own parameter layout,
your own plugin logic — and Csound handles the DSP underneath.

Use Cabbage if you want to go from CSD to plugin with zero C++ code.
Use csound-for-juce if you want Csound's DSP power but need full control
over your plugin's architecture, UI, and integration with the JUCE ecosystem.
