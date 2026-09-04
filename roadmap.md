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


Bugs/inconsistencies:

### 2.6 Last-error buffer can be read while being modified

**File:** `include/csd_plugin/audio/Processor.h`

`set_last_error` is RT-safe, which is good.

But `get_last_error()` reads `last_error_buffer_` from the UI thread while the audio thread may concurrently call `set_last_error`.

The atomic `has_error_` flag helps, but if an error is already set and a new error arrives, the UI may read a partially overwritten buffer.

**Possible fixes:**

- use a small fixed-size ring of error messages;
- use a sequence number and double buffering;
- only allow UI to read a snapshot copied atomically;
- or accept truncation/torn messages but document it.

---

### 2.7 Logging can flood the queue/file

The Csound message callback routes messages into a queue. If Csound code uses opcodes like `printk`, `printks`, `printf`, etc., it can generate messages at k-rate.

The example CSD in the QuickStart guide contains:

```csound
printk2 kfeedback
printk2 kcutOff
printk2 kmix
```

That can spam logs heavily.

**Recommendations:**

- remove `printk2` from example plugin code;
- add log throttling;
- optionally suppress Csound console messages in release builds;
- provide a debug switch.

---

### 2.9 `juce::Logger::writeToLog` and `fileLogger` may duplicate logs

**File:** `src/juce_csd/audio/CsoundLogConsumer.cpp`

```cpp
juce::Logger::writeToLog(fullMessage);
if (fileLogger) fileLogger->logMessage(fullMessage);
```

If the global JUCE logger is already writing to a file, this may duplicate messages.

Consider choosing one routing strategy:

- console/global logger only;
- file logger only;
- UI callback only;
- or make it configurable.

---

### 2.10 `getTailLengthSeconds()` returns infinity

**File:** `src/juce_csd/plugin/PluginProcessor.cpp`

```cpp
return std::numeric_limits<double>::infinity();
```

This is safe for tails, but some hosts may handle infinite tails poorly during offline bounce or freeze.

Consider making this configurable:

```cpp
IOLayout::tail_time_seconds
```

or returning a large finite value.

---

### 2.13 `PluginProcessor::make_buses_properties` only supports mono/stereo

**File:** `src/juce_csd/plugin/PluginProcessor.cpp`

```cpp
switch (io_layout.in_size) {
    case 1: ...
    case 2: ...
}
```

There is no default case.

If someone creates an `IOLayout` with 4 inputs, no bus is added and the plugin will probably fail confusingly.

**Options:**

- explicitly support only mono/stereo and `static_assert`/validate;
- add multichannel support;
- log an error.

---



