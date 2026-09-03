
#pragma once

#include <juce_core/juce_core.h>
#include <juce_csd/audio/Processor.h>
#include <csd_plugin/audio/Logger.h>
#include <functional>

namespace juce_csd {

/// A helper class that safely consumes the RT-safe Csound log queue on the main thread.
/// It routes messages to the JUCE console, an optional file, and a UI callback.
class CsoundLogConsumer : private juce::Timer {
public:
    using UICallback = std::function<void(const juce::String& message, csd_plugin::LogLevel
level)>;

    explicit CsoundLogConsumer(Processor& processor);
    ~CsoundLogConsumer() override;

    /// Start/stop polling the queue (default 30Hz)
    void start_consuming(int hz = 30);
    void stop_consuming();

    /// Optional: Route logs to a file. Max file size defaults to 1MB to prevent disk bloat.
    void enable_file_logging(const juce::File& logFile, const juce::String& welcomeMessage =
{});
    void disable_file_logging();

    /// Optional: Register a lambda to receive logs for UI updates (e.g., error banners)
    void set_ui_callback(UICallback callback);

private:
    void timerCallback() override;
    void flush_pending(); // <-- Added

    Processor& processorRef;
    std::unique_ptr<juce::FileLogger> fileLogger;
    UICallback uiCallback;

    // Buffer to accumulate fragmented Csound messages
    juce::String pendingText;
    csd_plugin::LogLevel pendingLevel = csd_plugin::LogLevel::Info;
    LogSource pendingSource = LogSource::Csound;
};

} // namespace juce_csd


