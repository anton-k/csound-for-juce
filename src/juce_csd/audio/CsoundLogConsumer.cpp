#include "juce_csd/audio/CsoundLogConsumer.h"

namespace juce_csd {

CsoundLogConsumer::CsoundLogConsumer(Processor& processor)
    : processorRef(processor)
{
}

CsoundLogConsumer::~CsoundLogConsumer() {
    stop_consuming();
    disable_file_logging();
}

void CsoundLogConsumer::start_consuming(int hz) {
    startTimerHz(hz);
}

void CsoundLogConsumer::stop_consuming() {
    stopTimer();
}

void CsoundLogConsumer::enable_file_logging(const juce::File& logFile, const juce::String&
welcomeMessage) {
    // Create a file logger (1MB max size to prevent infinite disk growth)
    fileLogger = std::make_unique<juce::FileLogger>(logFile, welcomeMessage, 1024 * 1024);
}

void CsoundLogConsumer::disable_file_logging() {
    fileLogger.reset();
}

void CsoundLogConsumer::set_ui_callback(UICallback callback) {
    uiCallback = std::move(callback);
}

void CsoundLogConsumer::timerCallback() {
    LogMessage msg;

    while (processorRef.pop_log(msg)) {
        juce::String text(msg.text);
        juce::String prefix;

        // Format prefix based on Source and Level
        if (msg.source == LogSource::Csound) {
            if (msg.level == csd_plugin::LogLevel::Error) prefix = "[Csound ERROR] ";
            else if (msg.level == csd_plugin::LogLevel::Warning) prefix = "[Csound WARN]  ";
            else prefix = "[Csound INFO]  ";
        } else {
            if (msg.level == csd_plugin::LogLevel::Error) prefix = "[Plugin ERROR] ";
            else if (msg.level == csd_plugin::LogLevel::Warning) prefix = "[Plugin WARN]  ";
            else prefix = "[Plugin INFO]  ";
        }

        juce::String fullMessage = prefix + text;

        // Route to Console, File, and UI...
        juce::Logger::writeToLog(fullMessage);
        if (fileLogger) fileLogger->logMessage(fullMessage);
        if (uiCallback) uiCallback(fullMessage, msg.level);
    }
}

} // namespace juce_csd

