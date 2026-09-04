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

## 1. High-priority bugs / likely functional defects

### 1.1 Choice parameters have inconsistent 0-based / 1-based semantics

**Files:**

- `include/juce_csd/params/Parameters.h`
- `src/juce_csd/params/Parameters.cpp`
- `src/juce_csd/params/JsonSerializer.cpp`
- QuickStart guide / examples

The documentation says choice values sent to Csound are **1-based**, with `0` reserved as “nothing selected”.

But the code uses JUCE’s `AudioParameterChoice`, which is normally **0-based**.

Example:

```cpp
auto* param = new juce::AudioParameterChoice(
    spec.id,
    spec.name,
    spec.choices,
    spec.default_value
);
```

If `spec.default_value` is intended to be 1-based, this is wrong because JUCE expects a 0-based default index.

Also:

```cpp
audio_parameter.cached.set_value(static_cast<double>(param->getIndex()));
```

This sends `0` for the first choice, but the docs say Csound expects `1`.

Serialization also stores:

```cpp
json_data["audio"][pair.first] = pair.second->getIndex();
```

Again, this is 0-based.

**Impact:**

- First menu item may map to Csound value `0` instead of `1`.
- Default choice may be wrong.
- Saved state may restore the wrong choice.
- UI combo box IDs, JUCE index, and Csound channel values can disagree.

**Recommended convention:**

Keep JUCE internally 0-based, but send Csound 1-based values.

For example:

```cpp
int defaultIndex = juce::jlimit(0, spec.choices.size() - 1, spec.default_value - 1);

auto* param = new juce::AudioParameterChoice(
    spec.id,
    spec.name,
    spec.choices,
    defaultIndex
);
```

Then when writing to Csound:

```cpp
audio_parameter.cached.set_value(static_cast<double>(param->getIndex() + 1));
```

Serialization should probably store the 1-based Csound value too:

```cpp
json_data["audio"][pair.first] = pair.second->getIndex() + 1;
```

And deserialize by converting back:

```cpp
int csoundValue = audio_json[id].get<int>();
int juceIndex = juce::jlimit(0, param_ptr->choices.size() - 1, csoundValue - 1);
```

This needs to be made explicit in the docs.

---

### 1.2 MIDI input timestamps use the wrong coordinate system

**Files:**

- `src/juce_csd/audio/Processor.cpp`
- `src/csd_plugin/audio/Processor.cpp`

In `juce_csd::Processor::processBlock`:

```cpp
const int block_start_global_sample = csound.get_current_sample();
...
read_midi_from_host(host_midi_buffer);
```

But `read_midi_from_host` pushes events using the host-relative sample position:

```cpp
csound.get_midi_buffers().in().push(
    csd_plugin::RawMidiEvent(
        metadata.samplePosition,
        msg.getRawData(),
        msg.getRawDataSize()
    )
);
```

However, inside `csd_plugin::Processor::midi_read`, events are compared against a global cycle position:

```cpp
int cycle_end_sample = proc->current_cycle_end_sample;

if (next_event.samplePosition < cycle_end_sample) {
    ...
}
```

`current_cycle_end_sample` is based on `current_sample`, which is a running global counter.
But `metadata.samplePosition` is relative to the current host block, usually `0..blockSize - 1`.

**Impact:**

MIDI event timing is likely wrong. For example, after the plugin has been running for a while, all host MIDI events may have timestamps near `0`, while `current_cycle_end_sample` may be millions. They may all be consumed immediately, losing sample-accurate timing.

**Fix direction:**

Convert host MIDI positions to the processor’s global time before pushing them:

```cpp
read_midi_from_host(host_midi_buffer, block_start_global_sample);
```

Then:

```cpp
int32_t globalPos = blockStartGlobalSample + metadata.samplePosition;

csound.get_midi_buffers().in().push(
    csd_plugin::RawMidiEvent(globalPos, msg.getRawData(), msg.getRawDataSize())
);
```

You should also decide whether `current_sample` should be `int64_t`, because `int` can overflow in long sessions.

---

### 1.3 MIDI output scheduling may also be incorrect

**File:** `src/juce_csd/audio/Processor.cpp`

`write_midi_to_host` reads all output MIDI events and clamps them into the current block:

```cpp
int relative_pos = csd_midi_event.samplePosition - block_start_sample;
relative_pos = std::clamp(relative_pos, 0, block_size - 1);
host_midi_messages.addEvent(juce_midi_event, relative_pos);
```

If Csound generates events ahead of time because the internal FIFO runs ahead of the host block, those events may belong to a future host block. Clamping them to the current block can destroy timing.

**Impact:**

- MIDI events may be emitted too early.
- Multiple future events may be collapsed onto the end of the current block.
- Synth timing may become unstable for block sizes that are not multiples of `ksmps`.

**Better approach:**

Maintain a global MIDI timestamp model and only emit events whose timestamp falls inside the current host block:

```cpp
if (event.globalPosition >= blockStart && event.globalPosition < blockStart + blockSize) {
    emit event;
} else if (event.globalPosition >= blockStart + blockSize) {
    leave event in queue for next block;
}
```

This requires changing `RawMidiEvent::samplePosition` to a larger integer type, probably `int64_t`.

---

### 1.4 Audio FIFO cycle calculation can request more input than available

**File:** `src/csd_plugin/audio/Processor.cpp`

This function decides how many Csound cycles to run:

```cpp
int target_frames = block_size + ksmps;
```

Then it may run enough cycles to produce `block_size + ksmps` output frames.

For effects with audio input, each Csound cycle also consumes `ksmps * in_size` input samples.

If `block_size` is not a multiple of `ksmps`, or if `block_size < ksmps`, the processor may try to consume more input samples than the host provided in the current block.

Example:

- `ksmps = 64`
- `block_size = 100`
- output prefill = `64`
- target = `164`
- cycles needed = `2`
- input consumed = `128`
- input written by host = `100`

So the second Csound cycle underflows the input FIFO.

The code currently does this:

```cpp
bool read_success = audio_buffers.in().read_block(spin, ksmps * in_size);

if (!read_success) {
    std::memset(spin, 0, ksmps * in_size * sizeof(double));
}
```

But `FastFifo::read_block` fails atomically: if there are not enough samples, it consumes nothing.

So if 36 samples are available and 64 are requested, the 36 samples remain in the FIFO, the current Csound cycle is filled with silence, and those 36 samples are used later, potentially causing time misalignment.

**Impact:**

- Silence or glitches when block size is not a multiple of `ksmps`.
- Possible latency drift.
- Input samples may be delayed by one or more Csound cycles.
- Behavior may depend heavily on host block size.

**Suggested fixes:**

You need a deliberate FIFO/latency strategy.

At minimum:

1. Do not silently discard or strand partial input data.
2. Either limit Csound cycles by available input, or implement partial reads.
3. Rethink the `block_size + ksmps` target margin.

A safer read pattern would be:

```cpp
int needed = ksmps * in_size;
int available = audio_buffers.in().get_size();
int to_read = std::min(needed, available);

audio_buffers.in().read_block(spin, to_read);
std::fill(spin + to_read, spin + needed, 0.0);
```

But this is only a partial mitigation. The real issue is that the number of Csound cycles, input consumption, output prefill, latency reporting, and MIDI timestamps need to be coordinated as one timing model.

---

### 1.6 `prepareToPlay` does not check whether Csound initialization succeeded

**File:** `src/juce_csd/audio/Processor.cpp`

Current code:

```cpp
void Processor::prepareToPlay(double sample_rate, int max_block_size)
{
    csound.prepare_to_play(static_cast<int>(std::round(sample_rate)), max_block_size);
    parameters.prepare(csound.get_csound(), sample_rate, max_block_size);

    int ksmps = csound.get_csound_settings().ksmps;
    csound.set_krate_callback([this, ksmps]() {
        parameters.update_krate_params(ksmps);
    });
}
```

If Csound compilation fails, `csound.is_ready_to_play()` will be false, but this function still calls:

```cpp
parameters.prepare(csound.get_csound(), sample_rate, max_block_size);
```

Depending on Csound’s internal state, this may be unsafe or may simply produce useless channel pointers. More importantly, the plugin continues as if preparation succeeded.

**Fix:**

```cpp
csound.prepare_to_play(...);

if (!csound.is_ready_to_play()) {
    return;
}

parameters.prepare(...);
csound.set_krate_callback(...);
```

Also consider propagating the error to JUCE via logging or `ErrorBanner`.

---

### 1.7 Bypass behavior is too abrupt

**File:** `src/juce_csd/audio/Processor.cpp`

Current bypass handling:

```cpp
if (is_bypassed || !csound.is_ready_to_play()) {
    buffer.clear();
    host_midi_buffer.clear();
    return;
}
```

For an effect, clearing the buffer is usually not true bypass. It will silence the output and can cause clicks.

Also, when bypassed, Csound processing stops, so `current_sample` stops advancing. When bypass is disabled again, Csound time may no longer match host time.

**Impact:**

- Clicks/pops on bypass toggle.
- Loss of dry signal.
- MIDI/timing desync after bypass.
- Envelopes or Csound score time may stop.

**Better behavior:**

For effect plugins:

- crossfade between processed and dry signal;
- or pass dry audio through while keeping Csound state alive;
- or reset/resync Csound when leaving bypass.

For synth plugins:

- bypass may mean silence, but parameter smoothing and voice release still need consideration.

This matches the roadmap item about smooth bypass.

---

### 1.8 Bus layout validation ignores sidechain buses

**File:** `src/juce_csd/plugin/PluginProcessor.cpp`

Current:

```cpp
return (
    layouts.getMainOutputChannelSet().size() == layout.get_out_size()
 && (
        layouts.getMainInputChannelSet().size() == layout.get_total_in_size()
     || layouts.getMainInputChannelSet().size() == layout.in_size
    )
);
```

This does not explicitly validate the sidechain bus.

If `IOLayout` has a sidechain, JUCE may have a separate sidechain input bus. The current check may accept layouts where the sidechain is missing, or where sidechain channels are incorrectly treated as main input channels.

**Better logic:**

```cpp
const auto mainIn = layouts.getMainInputChannelSet().size();
const auto scIn = layouts.getSidechainInputChannelSet().size();
const auto out = layouts.getMainOutputChannelSet().size();

if (layout.sidechain_size > 0) {
    return out == layout.out_size
        && mainIn == layout.in_size
        && scIn == layout.sidechain_size;
}

return out == layout.out_size
    && mainIn == layout.in_size
    && scIn == 0;
```

Standalone behavior may need special handling because standalone apps often do not have sidechain inputs.

---

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

### 2.19 `Parameters` getters throw exceptions

**File:** `src/juce_csd/params/Parameters.cpp`

```cpp
throw std::runtime_error("Float parameter not found: " + id);
```

This is okay on the UI thread if exceptions are enabled, but plugin code often prefers to avoid exceptions.

Also, `ParameterAttachments::add_slider` calls these getters. If a parameter ID is misspelled, the editor constructor may throw and crash the plugin.

**Better options:**

- return `std::optional<std::reference_wrapper<...>>`;
- return pointer and assert/log;
- use `juce::Result`;
- validate parameter specs before creating UI.

---


### 2.20 State deserialization errors are only printed with `DBG`

**File:** `src/juce_csd/params/Parameters.cpp`

```cpp
if (result.failed()) {
    DBG(result.getErrorMessage());
}
```

For production, this should probably go through the plugin logger or error reporting system.

---

### 2.21 Parameter spec version is not validated

`ParameterSpecMap` stores `version`, and JSON serialization writes it, but deserialization does not currently validate or migrate versions.

This is noted in your TODO, but it is important for v1.0.

Potential issues:

- renamed parameter IDs;
- removed parameters;
- changed choice order;
- changed float range;
- changed UI parameter semantics.

---

### 2.23 `Parameters` initializer list order may not match member declaration order

In `Parameters::Parameters`:

```cpp
Parameters::Parameters(juce::AudioProcessor& processor, const ParameterSpec& spec):
    audio_parameters(),
    ui_parameters(),
    sensor_parameters(),
    parameter_spec_map(spec)
```

But member initialization order is determined by declaration order, not initializer list order.

If `parameter_spec_map` is declared after other members, this can produce `-Wreorder` warnings.

Make initializer list order match declaration order.

---

### 2.24 `UiParameterList` uses `std::atomic<float>` directly in a map

This works because you construct elements in place, but it is fragile.

`std::atomic` is not copyable or movable. If someone later writes code that copies the map, it will break.

Options:

```cpp
std::map<std::string, std::unique_ptr<std::atomic<float>>>
```

or a small wrapper:

```cpp
struct AtomicFloat {
    std::atomic<float> value;
};
```

with explicit constructors.

---


Real-time audit for audio thread

### 3.1 `std::function` in the audio path

`csd_plugin::Processor` uses:

```cpp
std::function<void()> krate_callback;
LogCallback log_callback;
```

`std::function` can be fine if the target is small and does not allocate, but it introduces type erasure overhead and can be surprising.

For strict RT code, consider:

```cpp
void* callbackUserData;
void (*krateCallback)(void* userData);
```

or a fixed functor type.

At minimum, document that callbacks must not allocate.

---

### 3.2 JUCE MIDI iteration may not be fully allocation-free

In:

```cpp
for (const auto metadata : host_midi_messages) {
    auto msg = metadata.getMessage();
    ...
}
```

Depending on JUCE version and message type, this may involve temporary objects. Short MIDI messages are usually fine, but SysEx or unusual messages may allocate.

You already filter SysEx, which is good.

For maximum safety, use the lowest-level JUCE MIDI iteration API available and avoid constructing large `MidiMessage` objects.

---

### 3.3 Adding MIDI events to the host buffer may allocate

```cpp
host_midi_messages.addEvent(juce_midi_event, relative_pos);
```

JUCE may need to grow its internal buffer. This can allocate.

This is often unavoidable in JUCE plugin `processBlock`, but it is worth noting for the RT audit.

Possible mitigations:

- reserve MIDI buffer capacity if JUCE exposes a way;
- limit number of output MIDI events per block;
- avoid producing large MIDI output.

---

## 4. API/design improvements

### 4.1 Introduce a single timing model

The current code has several related concepts:

- host block sample position;
- Csound global sample position;
- MIDI input relative position;
- MIDI output global position;
- output FIFO fill level;
- `ksmps` cycle boundaries;
- reported latency.

These should be unified into a small timing/scheduling module.

A possible design:

```cpp
struct ProcessTiming {
    int64_t hostBlockStartSample;
    int64_t csoundSampleTime;
    int blockSize;
    int ksmps;
};
```

Then MIDI input/output and audio FIFO management can use one source of truth.

---

### 4.3 Add explicit validation results

Instead of only returning `bool` from setup functions, consider:

```cpp
juce::Result prepareToPlay(...);
```

or:

```cpp
std::expected<void, std::string> setupCsound(...);
```

This would make error handling cleaner.

---


### 4.4 Make bypass behavior configurable

Different plugin types need different bypass behavior.

Possible modes:

```cpp
enum class BypassMode {
    HardBypass,
    DryThrough,
    Crossfade,
    TailOnly
};
```

For FX plugins, crossfade/dry-through is usually best.

---


### 4.5 Add parameter validation against Csound channels

You already retrieve channel names:

```cpp
void CsoundSettings::set_channel_names(Csound* csound);
```

Use this to validate the `ParameterSpec`.

For example:

```cpp
std::vector<std::string> validateParameters(
    const ParameterSpec& spec,
    const CsoundSettings& settings
);
```

Return warnings/errors such as:

```text
Audio parameter 'mix' has no matching Csound input control channel.
Sensor parameter 'level' has no matching Csound output control channel.
Choice parameter 'mode' has empty choice list.
```

This directly addresses the roadmap item about parameter spec validation.

---



## 6. Suggested refactoring priorities

If I were prioritizing fixes for v1.0, I would do them in this order:

### Phase 1: correctness blockers

1. Initialize host parameters.
2. Fix choice parameter 0-based/1-based convention.
3. Fix MIDI timestamp coordinate system.
4. Fix latency double-counting.
5. Fix `MemoryOutputStream` append mode.
6. Fix `ErrorBanner` truncation bug.
7. Fix MIDI output size cast bug.
8. Guard `prepareToPlay` against failed Csound compilation.

### Phase 2: audio/MIDI timing stability

1. Redesign audio FIFO cycle calculation.
2. Implement partial input reads or cycle limiting.
3. Make MIDI output scheduling block-accurate.
4. Use 64-bit sample positions.
5. Define bypass behavior with crossfade/dry-through.

### Phase 3: robustness

1. Validate parameter specs against Csound channels.
2. Emit warnings/errors for missing channels.
3. Improve state version handling.
4. Add log throttling and dropped-message counters.
5. Make `FastFifo` thread-safety expectations explicit.

### Phase 4: style/architecture

1. Remove JUCE includes from `csd_plugin`.
2. Standardize naming.
3. Add `.clang-format`.
4. Remove unused members/includes.
5. Add const/noexcept/nodiscard.
6. Split parameter update into input/output phases.

---



