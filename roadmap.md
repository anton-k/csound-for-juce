# Roadmap

Here is the prioritized roadmap for the library.

-----------------------------------------------------------------------------------------------

🔴 P0: Crucial for v1.0 (Release Blockers)

## major

If these are missing, the plugin will fail in professional DAW environments, cause audio
glitches, or corrupt user projects.

1. Csound Message & Error Routing. Logger is done and hanlding Csound compilation.
   Now we need to cover edge cases with log errors or warnings.
   Validation of parameters spec with Csound actual parameters. Find out edge cases check and emit errors
  if parameter spec is inconsistent.

  Emit warnings for serialization mismatch

3. Smooth Bypass / DSP Fallback. (implement generic case on the level of csd_plugin, such that it's trivial to use on juce_csd level)

 • Why: If the host bypasses the plugin, or the user clicks a bypass button, abruptly stopping
   Csound processing or passing uninitialized buffers will cause loud clicks/pops.
 • Action: Check getBypassParameter() (if using JUCE's built-in bypass) or implement a custom
   bypass parameter. Apply a short crossfade (e.g., 5-10ms) between the dry and wet signals
   when toggling.


4. We can export to plugin with PluginProcessor add the same class for App and CLI classes that JUCE provides.
   Write convenient classes which user can inherit and define Apps and CLIs based on Csound files.

5. Memory allocation audit for audio thread

6. Add examples for plugins

   * plugin that uses sensors (metering plugin)

7. Add quickstart guide on how to define synthesizer plugin as it's very common case

7b. update quickstart guide for FX to include comments on how too use loggs and setup ErrorBanner.

8. After it's thoroughly tested on various plugins move `csd_plugin` to a separate repo.

9. Cross-compilation. Check windows compilation.

10. Packaging plugin instructions and how to add libcsound to the plugin
and distribute with the plugin

## minor

1. We have ErrorBanner to show errors from logs, should we have LogBanner/Window for all logs? Or using file dump is enough for this task?
   So far it's ok to use files for logs, just provide example how to use them in the guide.

2. Add examples for plugins

   * plugin that uses host params (metronome or delay that reads bpm)
   * plugin with sidechain (for example envelope follower or gate plugin)
   * plugin that only processes midi (for example transpose plugin)
