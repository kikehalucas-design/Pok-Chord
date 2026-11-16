#pragma once

#include "PluginProcessor.h"
#include <array>
#include <functional>
#include <memory>
#include <vector>

//==============================================================================
// Style minimaliste : fond crème, texte noir, traits fins
class PokeLookAndFeel : public juce::LookAndFeel_V4
{
public:
    PokeLookAndFeel();

    void drawLinearSlider (juce::Graphics&, int x, int y, int width, int height,
                           float sliderPos, float minSliderPos, float maxSliderPos,
                           juce::Slider::SliderStyle, juce::Slider&) override;

    void drawComboBox (juce::Graphics&, int width, int height, bool isButtonDown,
                       int buttonX, int buttonY, int buttonW, int buttonH, juce::ComboBox&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override;
    void positionComboBoxText (juce::ComboBox&, juce::Label&) override;

    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool highlighted, bool down) override;

    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour& backgroundColour,
                               bool highlighted, bool down) override;
    void drawButtonText (juce::Graphics&, juce::TextButton&, bool highlighted, bool down) override;
    juce::Font getTextButtonFont (juce::TextButton&, int buttonHeight) override;
    juce::Font getPopupMenuFont() override;
};

//==============================================================================
// Rangée de mots cliquables (le mot choisi est souligné)
class WordRow : public juce::Component
{
public:
    std::function<void (int)> onPick;

    void setItems (const juce::StringArray& newItems);
    void setSelected (int index);
    void setAccent (juce::Colour c);

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;

private:
    void computeLayout (std::vector<juce::Rectangle<int>>& out) const;

    juce::StringArray items;
    int selected = 0;
    juce::Colour accent { 0xfff7d02c };
};

//==============================================================================
// Rangée d'accords (apparaissent un par un après la génération)
class ChordStrip : public juce::Component
{
public:
    explicit ChordStrip (PokeChordAudioProcessor& p) : proc (p) {}

    void paint (juce::Graphics&) override;
    void setActive (int index);
    void setAnim (float secondsSinceFlash);   // < 0 : pas d'animation

private:
    PokeChordAudioProcessor& proc;
    int active = -1;
    float anim = -1.0f;
};

//==============================================================================
// Zone à attraper pour glisser le clip MIDI dans Ableton
class DragZone : public juce::Component
{
public:
    explicit DragZone (PokeChordAudioProcessor& p) : proc (p) {}

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;

private:
    PokeChordAudioProcessor& proc;
    bool dragging = false;
    bool pressed = false;
};

//==============================================================================
// La Poké Ball qui sert de bouton GÉNÉRER
class BallButton : public juce::Component
{
public:
    std::function<void()> onClick;

    void setImage (const juce::Image& img) { image = img; repaint(); }
    void setAnim (float angleRadians, float scaleFactor);

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;

private:
    juce::Image image;
    float angle = 0.0f;
    float scale = 1.0f;
};

//==============================================================================
// Petits boutons ronds : précédent / écouter
class RoundButton : public juce::Component
{
public:
    enum Kind { Back, Play };

    explicit RoundButton (Kind k) : kind (k) {}

    std::function<void()> onClick;
    void setOn (bool shouldBeOn);

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;

private:
    Kind kind;
    bool on = false;
};

//==============================================================================
class PokeChordAudioProcessorEditor : public juce::AudioProcessorEditor,
                                      private juce::Timer
{
public:
    explicit PokeChordAudioProcessorEditor (PokeChordAudioProcessor&);
    ~PokeChordAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    using SliderAttachment   = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ComboBoxAttachment = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    using ButtonAttachment   = juce::AudioProcessorValueTreeState::ButtonAttachment;

    struct Knob
    {
        juce::Slider slider;
        juce::Label label;
        std::unique_ptr<SliderAttachment> attachment;
    };

    struct Combo
    {
        juce::ComboBox box;
        juce::Label label;
        std::unique_ptr<ComboBoxAttachment> attachment;
    };

    struct Toggle
    {
        juce::ToggleButton button;
        std::unique_ptr<ButtonAttachment> attachment;
    };

    void setupSlider (Knob& k, const juce::String& paramId, const juce::String& text);
    void setupCombo (Combo& c, const juce::String& paramId, const juce::String& text);
    void setupToggle (Toggle& t, const juce::String& paramId, const juce::String& text);
    void setChoice (const char* paramId, int index);
    int readChoice (const char* paramId) const;

    void generateNew();
    void goBack();
    void setSeed (int value);
    int currentSeed() const;
    void saveMidiAs();
    void togglePlay();

    void startAnimation();
    void updateAnimation (double nowMs);
    void refreshMood();
    void timerCallback() override;

    PokeChordAudioProcessor& proc;
    PokeLookAndFeel laf;

    WordRow styleRow, moodRow;
    std::array<Knob, 6> sliders;   // 0 accords, 1 complexité, 2 audace, 3 humaniser, 4 octave, 5 volume
    std::array<Combo, 4> combos;   // 0 tonalité, 1 durée, 2 rythme, 3 disposition
    std::array<Toggle, 2> toggles; // 0 basse, 1 son d'écoute

    BallButton ball;
    RoundButton backButton { RoundButton::Back }, playButton { RoundButton::Play };
    juce::TextButton saveButton { "ENREGISTRER .MID" };
    ChordStrip strip;
    DragZone dragZone;

    std::array<juce::Image, 11> mons;
    juce::Image ballImage;

    std::vector<int> seedHistory;
    int shownVersion = -1;
    int shownMood = -1;
    juce::Colour circleFrom, circleTo, circleNow;
    float colourT = 1.0f;

    bool animActive = false;
    bool animFired = false;
    double animStart = 0.0;
    float animSeconds = 0.0f;

    std::unique_ptr<juce::FileChooser> chooser;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PokeChordAudioProcessorEditor)
};
