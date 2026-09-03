#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "juce_csd/audio/Processor.h" // Include to access Processor

namespace juce_csd {

class ErrorBanner : public juce::Component, private juce::Button::Listener {
public:
    explicit ErrorBanner(Processor& processor) : processorRef(processor) {
        // Setup the scrollable log display
        logDisplay.setMultiLine(true);
        logDisplay.setReadOnly(true);
        logDisplay.setScrollbarsShown(true);
        logDisplay.setCaretVisible(false);
        logDisplay.setPopupMenuEnabled(false);

        logDisplay.setFont(juce::FontOptions(juce::Font::getDefaultMonospacedFontName(), 18.0f, juce::Font::plain));
        logDisplay.setColour(juce::TextEditor::backgroundColourId, juce::Colours::black.withAlpha(0.9f));
        logDisplay.setColour(juce::TextEditor::textColourId, juce::Colours::white);
        logDisplay.setColour(juce::TextEditor::outlineColourId, juce::Colours::darkred);
        addAndMakeVisible(logDisplay);

        // Setup close button
        closeButton.setButtonText("X");
        closeButton.setColour(juce::TextButton::buttonColourId, juce::Colours::darkred);
        closeButton.onClick = [this]() { setVisible(false); };
        addAndMakeVisible(closeButton);

        setVisible(false); // Hidden by default

        // Check immediately in case it's added to a parent later
        checkAndShow();
    }

    void addError(const juce::String& message) {
        juce::String safeMessage = message.trim();
        if (safeMessage.isEmpty()) {
            return;
        }

        if (safeMessage.length() > 2000) {
            safeMessage = safeMessage.substring(0, 2000) + "\n... [Log truncated to prevent UI freeze] ...";
        }

        auto currentText = logDisplay.getText();
        if (currentText.length() > 4000) {
            logDisplay.setText(currentText.substring(currentText.length() - 6000));
        }

        logDisplay.moveCaretToEnd();
        logDisplay.insertTextAtCaret(safeMessage + "\n");

        setVisible(true);
        updateBounds();

        if (auto* parent = getParentComponent()) {
            parent->repaint();
        }
    }

    void clear() {
        logDisplay.clear();
        setVisible(false);
    }

    void paint(juce::Graphics& g) override {
        g.fillAll(juce::Colours::black);
        g.setColour(juce::Colours::red);
        g.drawRect(getLocalBounds(), 2);
    }

    void resized() override {
        auto bounds = getLocalBounds();
        closeButton.setBounds(bounds.removeFromRight(30).reduced(4));
        logDisplay.setBounds(bounds.reduced(4));
    }

    void parentSizeChanged() override {
        updateBounds();
    }

    // Ensure bounds are correct and state is checked when attached to a parent
    void parentHierarchyChanged() override {
        updateBounds();
        checkAndShow();
    }

private:
    Processor& processorRef;
    juce::TextEditor logDisplay;
    juce::TextButton closeButton;

    void checkAndShow() {
        if (!processorRef.is_csound_valid()) {
            juce::String errorMsg = processorRef.get_last_error();

            // Only trigger the banner if an actual error was captured
            if (errorMsg.trim().isNotEmpty()) {
                addError(errorMsg);
                return;
            }
        }
        setVisible(false);
    }

    void updateBounds() {
        if (auto* parent = getParentComponent()) {
            setBounds(parent->getLocalBounds());
        }
    }

    void buttonClicked(juce::Button*) override { setVisible(false); }
};

} // namespace juce_csd

