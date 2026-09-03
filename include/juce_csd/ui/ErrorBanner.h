
#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <algorithm>

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

        logDisplay.setFont(juce::FontOptions(juce::Font::getDefaultMonospacedFontName(), 16.0f, juce::Font::plain));

        logDisplay.setColour(juce::TextEditor::backgroundColourId, juce::Colours::black.withAlpha(0.9f));
        logDisplay.setColour(juce::TextEditor::textColourId, juce::Colours::white);
        logDisplay.setColour(juce::TextEditor::outlineColourId, juce::Colours::darkred);
        addAndMakeVisible(logDisplay);

        // Setup close button
        closeButton.setColour(juce::TextButton::buttonColourId, juce::Colours::darkred);
        closeButton.onClick = [this]() { setVisible(false); };
        addAndMakeVisible(closeButton);

        setVisible(false); // Hidden by default
    }

    void addError(const juce::String& message) {
        // Prevent UI hangs by limiting the maximum number of characters.
        // juce::TextEditor layout performance degrades significantly with large text blocks.
       juce::String safeMessage = message;
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

        // Force layout and repaint so it shows up immediately
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

    // Automatically resize when the parent window resizes
    void parentSizeChanged() override {
        updateBounds();
    }

    // Ensure bounds are correct when first attached to a parent
    void parentHierarchyChanged() override {
        updateBounds();
    }

private:
    juce::TextEditor logDisplay;
    juce::TextButton closeButton {"X"};

    void updateBounds() {
        if (auto* parent = getParentComponent()) {
            auto parentBounds = parent->getLocalBounds();
            // Occupy 25% of the parent window's height, with a minimum of 100 pixels
            int bannerHeight = std::max(100, parentBounds.getHeight() / 4);
            setBounds(parentBounds.removeFromTop(bannerHeight));
        }
    }

    void buttonClicked(juce::Button*) override { setVisible(false); }
};

} // namespace juce_csd

