#pragma once

#include "PluginProcessor.h"
#include <array>
#include <functional>
#include <memory>
#include <vector>

//==============================================================================
// Polices intégrées (Poppins), partagées entre toutes les fenêtres du plugin
struct FontHolder
{
    FontHolder();
    ~FontHolder();

    juce::Typeface::Ptr bold, medium;
    static FontHolder* instance;
};

//==============================================================================
// Style minimaliste : fond crème, texte noir, traits fins
class PokeLookAndFeel : public juce::LookAndFeel_V4
{
public:
    PokeLookAndFeel();

    // image de la Poké Ball utilisée comme bouton des faders
    void setThumbImage (const juce::Image& img) { thumb = img; }
    void setAccent (juce::Colour c) { accent = c; }
    void setWhiteFader (bool w) { whiteFader = w; }

    void drawLinearSlider (juce::Graphics&, int x, int y, int width, int height,
                           float sliderPos, float minSliderPos, float maxSliderPos,
                           juce::Slider::SliderStyle, juce::Slider&) override;

    int getSliderThumbRadius (juce::Slider&) override { return 11; }

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

private:
    juce::Image thumb;
    juce::Colour accent { 0xffec1824 };
    bool whiteFader = false;
};

//==============================================================================
// Rangée d'accords (apparaissent un par un après la génération)
class ChordStrip : public juce::Component
{
public:
    explicit ChordStrip (PokeChordAudioProcessor& p) : proc (p) {}

    void paint (juce::Graphics&) override;
    void setActive (int index);
    void setAccent (juce::Colour c) { accent = c; repaint(); }
    void setAnim (float secondsSinceFlash);   // < 0 : pas d'animation

private:
    PokeChordAudioProcessor& proc;
    juce::Colour accent { 0xfff7d02c };
    int active = -1;
    float anim = -1.0f;
};

//==============================================================================
// Piano roll : les notes générées (la basse est colorée), avec la tête de lecture
class PianoRoll : public juce::Component
{
public:
    explicit PianoRoll (PokeChordAudioProcessor& p) : proc (p) {}

    void setAccent (juce::Colour c) { accent = c; repaint(); }
    void setAnim (float secondsSinceFlash);   // < 0 : pas d'animation
    void setPlayhead (float beat);            // < 0 : pas de lecture

    void paint (juce::Graphics&) override;

private:
    PokeChordAudioProcessor& proc;
    juce::Colour accent { 0xfff7d02c };
    float anim = -1.0f;
    float playBeat = -1.0f;
};

//==============================================================================
// Zone à attraper pour glisser le clip MIDI dans Ableton
class DragZone : public juce::Component
{
public:
    explicit DragZone (PokeChordAudioProcessor& p) : proc (p) {}

    void setNight (bool n) { night = n; repaint(); }
    bool isNight() const { return night; }

    juce::String title;
    std::function<juce::File()> fileProvider;     // crée le fichier .mid au moment du glisser
    std::function<juce::String()> subtitle;       // petite ligne sous le titre

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;

private:
    PokeChordAudioProcessor& proc;
    bool night = false;
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
// Gros bouton en forme de pilule (écouter / pause)
class PillButton : public juce::Component
{
public:
    std::function<void()> onClick;
    void setNight (bool n) { night = n; repaint(); }
    void setOn (bool shouldBeOn);

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;

private:
    bool night = false;
    bool on = false;
};

//==============================================================================
// Effet visuel plein écran, différent pour chaque Pokémon, lancé à la génération
class FxOverlay : public juce::Component
{
public:
    FxOverlay() { setInterceptsMouseClicks (false, false); setOpaque (false); }

    void begin (int moodIndex);
    void update (double nowMs);
    void stop();

    void paint (juce::Graphics&) override;

private:
    float rnd (int i, int k) const;

    int kind = -1;
    int seed = 1;
    double startMs = 0.0;
    float t = 0.0f;
    bool active = false;
};

//==============================================================================
// Page ANALYSER : accord entendu, notes, historique, MIDI à glisser
class AnalyserPage : public juce::Component
{
public:
    explicit AnalyserPage (PokeChordAudioProcessor&);

    void setNoctali (const juce::Image& img) { noctali = img; buildRingGlow(); repaint(); }
    void setLogo (const juce::Image& img) { logo = img; }
    void refresh();
    void tick (double nowMs);   // appelé ~60 fois par seconde quand la page est visible

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void buildBackground();     // décor fixe (ciel, forêt, panneaux), mis en cache
    void buildRingGlow();       // repère les anneaux jaunes de Noctali dans son image, pour les faire briller

    PokeChordAudioProcessor& proc;
    juce::Image noctali, logo;
    juce::Image ringMask, ringGlow;   // anneaux de Noctali isolés, et leur halo flou
    PillButton listenButton;
    juce::TextButton clearButton { "EFFACER" };
    DragZone dragZone;
    int shownVersion = -1;

    juce::Image bgCache;
    double now = 0.0;
    int frameCount = 0;
    float env = 0.0f, envSlow = 0.0f, beat = 0.0f;
    std::vector<double> ripples;
    double lastRipple = 0.0;
    double shootStart = -1.0;
    float shootX = 0.0f, shootY = 0.0f;
    juce::Random rng;
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
    void mouseDown (const juce::MouseEvent&) override;

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

    void setPage (int newPage);
    void startAnimation();
    void updateAnimation (double nowMs);
    void refreshMood();
    void timerCallback() override;

    juce::SharedResourcePointer<FontHolder> fonts;   // doit rester le premier membre
    PokeChordAudioProcessor& proc;
    PokeLookAndFeel laf;

    Combo styleCombo, moodCombo;
    std::array<Knob, 6> sliders;   // 0 accords, 1 complexité, 2 audace, 3 humaniser, 4 octave, 5 volume
    std::array<Combo, 5> combos;   // 0 tonalité, 1 mode, 2 durée, 3 rythme, 4 disposition
    std::array<Toggle, 2> toggles; // 0 basse, 1 son d'écoute
    Combo presetCombo;             // preset du son d'écoute

    BallButton ball;
    RoundButton backButton { RoundButton::Back }, playButton { RoundButton::Play };
    juce::TextButton saveButton { "ENREGISTRER .MID" };
    ChordStrip strip;
    DragZone dragZone;
    PianoRoll pianoRoll;
    AnalyserPage analyserPage;
    FxOverlay fx;
    int page = 0;   // 0 = générer, 1 = analyser

    std::array<juce::Image, 11> mons;
    juce::Image ballImage;
    juce::Image noctaliImage;
    juce::Image sceneImage;   // décor derrière les Pokémon (forêt)

    std::vector<int> seedHistory;
    int shownVersion = -1;
    int shownMood = -1;
    juce::Colour circleFrom, circleTo, circleNow;
    juce::Colour accentFrom, accentTo, accentNow;
    juce::Colour bgFrom, bgTo, bgNow;
    float colourT = 1.0f;
    int breatheTick = 0;
    juce::Image cardsImage;   // fond : cartes avec ombre douce (mis en cache)
    void applyAccent();
    void rebuildCards();

    bool animActive = false;
    bool animFired = false;
    double animStart = 0.0;
    float animSeconds = 0.0f;

    std::unique_ptr<juce::FileChooser> chooser;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PokeChordAudioProcessorEditor)
};
