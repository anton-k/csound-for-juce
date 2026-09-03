#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

namespace juce_csd {

class ErrorBanner : public juce::Component, private juce::Button::Listener {
public:
    ErrorBanner() {
        // Setup the scrollable log display
        logDisplay.setMultiLine(true);
        logDisplay.setReadOnly(true);
        logDisplay.setScrollbarsShown(true);
        logDisplay.setCaretVisible(false);
        logDisplay.setPopupMenuEnabled(false);
        logDisplay.setFont(juce::FontOptions(juce::Font::getDefaultMonospacedFontName(),
14.0f, juce::Font::plain));
        logDisplay.setColour(juce::TextEditor::backgroundColourId,
juce::Colours::black.withAlpha(0.9f));
        logDisplay.setColour(juce::TextEditor::textColourId, juce::Colours::red);
        logDisplay.setColour(juce::TextEditor::outlineColourId, juce::Colours::darkred);
        addAndMakeVisible(logDisplay);

        // Setup close button
        closeButton.setColour(juce::TextButton::buttonColourId, juce::Colours::darkred);
        closeButton.onClick = [this]() { setVisible(false); };
        addAndMakeVisible(closeButton);

        setVisible(false); // Hidden by default
    }

    void addError(const juce::String& message) {
        logDisplay.moveCaretToEnd();
        logDisplay.insertTextAtCaret(message + "\n");
        setVisible(true);

        // Force layout and repaint so it shows up immediately
        if (auto* parent = getParentComponent()) {
            setBounds(parent->getLocalBounds().removeFromTop(150));
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

private:
    juce::TextEditor logDisplay;
    juce::TextButton closeButton {"X"};

    void buttonClicked(juce::Button*) override { setVisible(false); }
};

} // namespace juce_csd

