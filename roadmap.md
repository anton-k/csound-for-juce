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

3. Graceful Csound Compilation Failure Handling

 • Why: In your current setup_csound() method, you call csound->CompileCSD() and csound->Start(). If the
   user provides a CSD with a syntax error, Csound will fail to compile, return a non-zero error code,
   and output the error to stdout. Your plugin will then set ready_to_play = true (implicitly, or crash
   when PerformKsmps is called on an unstarted instance).
 • Action:
    1 Check the return value of CompileCSD() and Start().
    2 If they fail, set ready_to_play = false so processBlock safely clears the buffer and passes dry
      audio.
    3 Expose a bool is_csound_valid() or a std::string get_last_error() method so the UI can display a
      red "Compilation Error" banner to the user instead of just outputting silence.
