# Backlog

Based on an analysis of the provided codebase, the csound-for-juce library has a
solid foundation for audio processing, and basic MIDI/IO
routing. However, to be considered complete and production-ready for commercial
or widely distributed plugin development, several critical features and
architectural improvements are needed:

## Backlog with priorities


To get csound-for-juce to a stable, production-ready v1.0, you must focus on features that
prevent DAW crashes, audio dropouts, and data loss. Features related to advanced workflows,
niche formats, and UI polish can be safely deferred.

Here is the prioritized roadmap for the library.

-----------------------------------------------------------------------------------------------

🟡 P1: Important for v1.1 (Professional Polish)

These features make the library feel professional and handle edge cases, but won't prevent a
functional v1.0 release.

 • Real-Time Safety Audit: Your architecture.md strictly enforces: "Real-time code should have
   no memory allocation". Ensure that your parameter update loops (e.g., update_on_process,
   update_smoothed_audio_params) and processBlock do not trigger hidden allocations (e.g.,
   std::string copies, std::map/std::unordered_map lookups, or std::vector resizes) on the
   audio thread.
 • State Migrations: In include/juce_csd/params/Parameters.h, there is a TODO noting that
   versioning alone isn't enough and migrations are needed. If you change parameter IDs or
   ranges between beta and v1.0, old user presets will break or crash the plugin.
 • Offline Rendering (Bounce to Disk): Ensure the Csound engine handles host offline rendering
   correctly, where sample rates or block sizes might behave differently, and play_head
   positions might jump.
 • Csound Library Distribution: The roadmap doesn't mention how libcsound is packaged. For
   v1.0, you will need a reliable CMake strategy to bundle or statically link the Csound
   binaries across Windows, macOS, and Linux so the end-user doesn't need to install Csound
   globally.
 • Csound's control channels are essentially shared memory. If the UI thread reads a sensor
   parameter while the audio thread is in the middle of a ksmps block writing to it, you could get
   torn reads (though less likely with 32-bit floats, it's still a data race). Ensure your
   SensorParam and CachedInputParam implementations are strictly thread-safe without using heavy
   mutexes on the audio thread.

1. Asynchronous Csound Compilation

 • Why: CompileCSD() blocks the thread it runs on. For complex CSD files, this can block the
   DAW's message thread during plugin instantiation, triggering "Plugin Not Responding"
   warnings.
 • Action: Move compilation to a background thread. Expose an isLoading() state so the UI can
   show a loading spinner while Csound initializes.

2. Robust Sidechain & Bus Mapping

 • Why: The current linear iteration (channel_index < csd_in_size) assumes DAWs always pack
   main and sidechain inputs sequentially. Some DAWs (like Logic Pro) handle sidechain buses
   differently.
 • Action: Use juce::AudioProcessor::getBus() to explicitly query the main and sidechain buses,
   mapping their specific channel indices to Csound's spin buffer.

3. File Path Resolution (SADIR / SSDIR)

 • Why: Csound opcodes that load files (e.g., soundin, fin) look in the DAW's working
   directory, which is almost never where the user's audio files are.
 • Action: Provide a helper to set Csound's SADIR (Audio Directory) via environment variables
   or Csound options, pointing to a user-selected folder or the plugin's bundle resources.

4. Migration to AudioProcessorValueTreeState (APVTS)

 • Why: Your custom ParameterSpec and JSON serializer work, but APVTS is the JUCE industry
   standard. It provides free Undo/Redo, parameter grouping, and easier integration with
   standard JUCE UI components.
 • Action: Refactor Parameters to wrap or utilize juce::AudioProcessorValueTreeState. (Keep
   this in v1.1 as it requires a significant refactor of your current working system).

5. JSON declarative migration: do we need it if we will migrate to APVTS?


6. The is_processing_ Spin-Lock

In src/csd_plugin/audio/Processor.cpp, you use a spin-lock to protect Csound re-initialization:


while (is_processing_.load(std::memory_order_acquire)) {
    std::this_thread::yield();
}


This is a standard and effective way to protect the Csound pointer without using a std::mutex (which
would block the audio thread). Edge Case: If the audio thread is suspended by the OS (e.g., a debugger
breakpoint, or extreme priority inversion), the message thread will spin forever and freeze the DAW UI.
Action: This is generally acceptable for v1.0, but for v1.1, you might want to add a timeout or a
fallback mechanism to prevent infinite UI hangs during debugging.

-----------------------------------------------------------------------------------------------

🟢 P2: Deferred to v1.2+ (Advanced / Niche)

These are "nice-to-have" features for specific use cases. Do not let them delay your v1.0
release.

1. Internal MIDI Learn & CC Mapping

 • Why: While useful, DAWs already provide native MIDI learn for exposed parameters. Building
   an internal MIDI mapping matrix is a heavy UI/UX undertaking.
 • Action: Defer until v1.2. For v1.0, rely on the DAW's native MIDI mapping.

2. MPE (MIDI Polyphonic Expression)

 • Why: MPE requires complex routing of per-note pitchbend, pressure, and slide data into
   Csound's internal MIDI structures.
 • Action: Defer. Standard MIDI Note On/Off and basic CC are sufficient for v1.0 synths.

3. Preset Management & UI Browsers

 • Why: Factory presets and preset browsers require building custom UI file dialogs and preset
   management logic.
 • Action: Defer. DAWs handle saving/loading plugin states natively via .fxp or their own
   preset systems.

4. CLAP Note Expressions & SysEx

 • Why: CLAP is still gaining traction, and Note Expressions are an advanced feature. SysEx is
   rarely used in modern DSP plugins outside of hardware emulation.
 • Action: Defer. Standard VST3/AU/CLAP parameter automation is enough for v1.0.
