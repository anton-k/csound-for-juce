Here is the prioritized roadmap for the library.

-----------------------------------------------------------------------------------------------

🔴 P0: Crucial for v1.0 (Release Blockers)

If these are missing, the plugin will fail in professional DAW environments, cause audio
glitches, or corrupt user projects.

1. Csound Message & Error Routing

 • Why: By default, Csound prints warnings, errors, and printks opcodes to stdout/stderr. In
   hosts like Ableton Live or Bitwig, unexpected console output from a plugin can cause crashes
   or freeze the UI.
 • Action: Implement csound->SetMessageCallback() in setup_csound(). Route messages to a
   juce::Logger or a thread-safe queue that the UI can read. Never let Csound write directly to
   standard output.

2. Smooth Bypass / DSP Fallback

 • Why: If the host bypasses the plugin, or the user clicks a bypass button, abruptly stopping
   Csound processing or passing uninitialized buffers will cause loud clicks/pops.
 • Action: Check getBypassParameter() (if using JUCE's built-in bypass) or implement a custom
   bypass parameter. Apply a short crossfade (e.g., 5-10ms) between the dry and wet signals
   when toggling.

3. Implement non thread safe alternative for faster FIFO buffer for Audio and MIDI
