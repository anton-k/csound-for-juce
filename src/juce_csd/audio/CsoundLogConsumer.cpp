#include "juce_csd/audio/CsoundLogConsumer.h"

namespace juce_csd {

namespace {
constexpr int MAX_PENDING_LOG_CHARS = 8192;
constexpr juce::int64 MAX_LOG_FILE_BYTES = 1024 * 1024;
}

CsoundLogConsumer::CsoundLogConsumer(Processor& processor)
    : processorRef(processor)
{
}

CsoundLogConsumer::~CsoundLogConsumer() {
    stop_consuming();
    uiCallback = nullptr;
    disable_file_logging();
}

void CsoundLogConsumer::start_consuming(int hz) {
    startTimerHz(hz);
}

void CsoundLogConsumer::stop_consuming() {
    stopTimer();
}

void CsoundLogConsumer::enable_file_logging(const juce::File& logFile, const juce::String& welcomeMessage) {
    fileLogger = std::make_unique<juce::FileLogger>(logFile, welcomeMessage, MAX_LOG_FILE_BYTES);
    fileLogger->setFileSizeLimit(MAX_LOG_FILE_BYTES);
}

void CsoundLogConsumer::disable_file_logging() {
    fileLogger.reset();
}

void CsoundLogConsumer::set_ui_callback(UICallback callback) {
    uiCallback = std::move(callback);
}

void CsoundLogConsumer::timerCallback() {
    LogMessage msg;
    bool receivedAnything = false;

    // 1. Drain the RT-safe queue
    while (processorRef.pop_log(msg)) {
        receivedAnything = true;

        // If severity or source changes, flush the previous buffer first
        if (!pendingText.isEmpty() && (msg.level != pendingLevel || msg.source != pendingSource)) {
            flush_pending();
        }

        pendingLevel = msg.level;
        pendingSource = msg.source;
        pendingText += msg.text;

        // Prevent unbounded growth if Csound keeps sending fragments without newlines.
        if (pendingText.length() > MAX_PENDING_LOG_CHARS) {
            flush_pending();
        }
    }

    // 2. Flush logic:
    // Csound messages usually end with a newline. If we see one, it's complete.
    if (receivedAnything && pendingText.endsWith("\n")) {
        flush_pending();
    }
    // Fallback: If the queue is empty but we have text, Csound stopped sending.
    // Flush it now to prevent infinite latency.
    else if (!receivedAnything && !pendingText.isEmpty()) {
        flush_pending();
    }
}

void CsoundLogConsumer::flush_pending() {
    // Trim removes leading/trailing whitespace, preserving internal newlines
    juce::String cleanText = pendingText.trim();
    pendingText = juce::String();

    if (cleanText.isEmpty()) return;

    juce::String prefix;
    if (pendingSource == LogSource::Csound) {
        if (pendingLevel == csd_plugin::LogLevel::Error) prefix = "[Csound ERROR] ";
        else if (pendingLevel == csd_plugin::LogLevel::Warning) prefix = "[Csound WARN]  ";
        else prefix = "[Csound INFO]  ";
    } else {
        if (pendingLevel == csd_plugin::LogLevel::Error) prefix = "[Plugin ERROR] ";
        else if (pendingLevel == csd_plugin::LogLevel::Warning) prefix = "[Plugin WARN]  ";
        else prefix = "[Plugin INFO]  ";
    }

    juce::String fullMessage = prefix + cleanText;

    // Route to Console, File, and UI as a SINGLE unified message
    juce::Logger::writeToLog(fullMessage);
    if (fileLogger) fileLogger->logMessage(fullMessage);
    if (uiCallback) uiCallback(fullMessage, pendingLevel);
}

} // namespace juce_csd
