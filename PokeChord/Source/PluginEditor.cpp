#include "PluginEditor.h"
#include <BinaryData.h>
#include <cmath>

FontHolder* FontHolder::instance = nullptr;

FontHolder::FontHolder()
{
    bold   = juce::Typeface::createSystemTypefaceFor (BinaryData::PoppinsBold_ttf, (size_t) BinaryData::PoppinsBold_ttfSize);
    medium = juce::Typeface::createSystemTypefaceFor (BinaryData::PoppinsMedium_ttf, (size_t) BinaryData::PoppinsMedium_ttfSize);
    instance = this;
}

FontHolder::~FontHolder()
{
    instance = nullptr;
}

namespace
{
    // Les textes accentués passent toujours par fromUTF8
    juce::String U (const char* s) { return juce::String::fromUTF8 (s); }

    juce::Font F (float h, bool bold = true)
    {
        if (FontHolder::instance != nullptr)
        {
            const auto& tf = bold ? FontHolder::instance->bold : FontHolder::instance->medium;
            if (tf != nullptr)
                return juce::Font (juce::FontOptions (tf).withHeight (h * 0.93f));
        }
        return juce::Font (juce::FontOptions (h, bold ? juce::Font::bold : juce::Font::plain));
    }

    const juce::Colour kInk (0xff1a1020);
    const juce::Colour kBg (0xfff3f0e8);
    const juce::Colour kTrack (0xffd8d3c6);
    const juce::Colour kRed (0xffec1824);   // rouge de la Poké Ball
    const juce::Colour kCard (0xfffbf9f4);  // fond des cartes

    struct Bin { const char* data; int size; };

    // Les 11 images, dans l'ordre des ambiances du moteur :
    // Dansant, Joyeux, Énergique, Romantique, Planant, Nostalgique,
    // Mélancolique, Triste, Sombre, Mystérieux, Tendu
    Bin getBin (int i)
    {
        const Bin bins[11] = {
            { BinaryData::mon0_png,  BinaryData::mon0_pngSize },
            { BinaryData::mon1_png,  BinaryData::mon1_pngSize },
            { BinaryData::mon2_png,  BinaryData::mon2_pngSize },
            { BinaryData::mon3_png,  BinaryData::mon3_pngSize },
            { BinaryData::mon4_png,  BinaryData::mon4_pngSize },
            { BinaryData::mon5_png,  BinaryData::mon5_pngSize },
            { BinaryData::mon6_png,  BinaryData::mon6_pngSize },
            { BinaryData::mon7_png,  BinaryData::mon7_pngSize },
            { BinaryData::mon8_png,  BinaryData::mon8_pngSize },
            { BinaryData::mon9_png,  BinaryData::mon9_pngSize },
            { BinaryData::mon10_png, BinaryData::mon10_pngSize }
        };
        return bins[juce::jlimit (0, 10, i)];
    }

    // Couleur du type de chaque Pokémon
    juce::Colour typeColour (int i)
    {
        static const juce::uint32 c[11] = {
            0xfff5922d,  // Dansant      : Salamèche (feu)
            0xffb9a98a,  // Joyeux       : Évoli (normal)
            0xfff7d02c,  // Énergique    : Pikachu (électrik)
            0xffee99ac,  // Romantique   : Rondoudou (fée)
            0xffb9a98a,  // Planant      : Ronflex (normal)
            0xff6cc24a,  // Nostalgique  : Bulbizarre (plante)
            0xff4f9be8,  // Mélancolique : Carapuce (eau)
            0xfff85888,  // Triste       : Mewtwo (psy)
            0xff735797,  // Sombre       : Ectoplasma (spectre)
            0xfff85888,  // Mystérieux   : Mew (psy)
            0xffd9382b   // Tendu        : Dracaufeu (rouge)
        };
        return juce::Colour (c[juce::jlimit (0, 10, i)]);
    }

    // Le cercle est la couleur du type, adoucie sur le fond crème
    juce::Colour circleColour (int i)
    {
        return typeColour (i).interpolatedWith (kBg, 0.68f);
    }

    const juce::Rectangle<float> kCircle (20.0f, 80.0f, 420.0f, 420.0f);
}

//==============================================================================
// Look and feel minimaliste
//==============================================================================
PokeLookAndFeel::PokeLookAndFeel()
{
    setColour (juce::PopupMenu::backgroundColourId, kBg);
    setColour (juce::PopupMenu::textColourId, kInk);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, kInk);
    setColour (juce::PopupMenu::highlightedTextColourId, kBg);
    setColour (juce::ComboBox::textColourId, kInk);
    setColour (juce::Label::textColourId, kInk);
    setColour (juce::TextButton::textColourOffId, kInk);
    setColour (juce::TextButton::textColourOnId, kInk);
}

void PokeLookAndFeel::drawLinearSlider (juce::Graphics& g, int x, int y, int width, int height,
                                        float sliderPos, float, float, juce::Slider::SliderStyle,
                                        juce::Slider&)
{
    const float cy = (float) y + (float) height * 0.5f;

    g.setColour (kTrack);
    g.fillRoundedRectangle ((float) x, cy - 2.0f, (float) width, 4.0f, 2.0f);

    g.setColour (accent.darker (0.12f));
    g.fillRoundedRectangle ((float) x, cy - 2.5f, juce::jmax (0.0f, sliderPos - (float) x), 5.0f, 2.5f);

    // le bouton du fader est une Poké Ball qui roule quand on la déplace
    if (thumb.isValid())
    {
        g.setColour (juce::Colours::black.withAlpha (0.18f));
        g.fillEllipse (sliderPos - 10.0f, cy - 7.0f, 20.0f, 20.0f);

        g.saveState();
        g.addTransform (juce::AffineTransform::rotation ((sliderPos - (float) x) / 11.0f, sliderPos, cy));
        g.drawImage (thumb, juce::Rectangle<float> (22.0f, 22.0f).withCentre ({ sliderPos, cy }));
        g.restoreState();
    }
    else
    {
        g.setColour (kRed);
        g.fillEllipse (sliderPos - 7.0f, cy - 7.0f, 14.0f, 14.0f);
    }
}

void PokeLookAndFeel::drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox&)
{
    const auto r = juce::Rectangle<float> (0.0f, 0.0f, (float) w, (float) h).reduced (0.5f);

    g.setColour (kTrack);
    g.fillRoundedRectangle (r, 7.0f);

    const float cx = (float) w - 13.0f, cy = (float) h * 0.5f;
    juce::Path arrow;
    arrow.addTriangle (cx - 4.0f, cy - 2.0f, cx + 4.0f, cy - 2.0f, cx, cy + 3.0f);
    g.setColour (kInk);
    g.fillPath (arrow);
}

juce::Font PokeLookAndFeel::getComboBoxFont (juce::ComboBox&)
{
    return F (12.0f);
}

void PokeLookAndFeel::positionComboBoxText (juce::ComboBox& box, juce::Label& label)
{
    label.setBounds (7, 0, box.getWidth() - 24, box.getHeight());
    label.setFont (getComboBoxFont (box));
}

void PokeLookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool highlighted, bool)
{
    const auto bounds = b.getLocalBounds().toFloat();
    const float d = 16.0f;
    const juce::Rectangle<float> dot (bounds.getX() + 2.0f, bounds.getCentreY() - d * 0.5f, d, d);

    g.setColour (kInk);
    g.drawEllipse (dot, 2.0f);
    if (b.getToggleState())
        g.fillEllipse (dot.reduced (4.0f));
    juce::ignoreUnused (highlighted);

    g.setColour (kInk.withAlpha (b.getToggleState() ? 1.0f : 0.55f));
    g.setFont (F (12.0f));
    g.drawText (b.getButtonText(), bounds.withTrimmedLeft (d + 10.0f), juce::Justification::centredLeft, false);
}

void PokeLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button&, const juce::Colour&,
                                            bool highlighted, bool down)
{
    // le bouton est dessiné par drawButtonText pour rester simple
    juce::ignoreUnused (g, highlighted, down);
}

void PokeLookAndFeel::drawButtonText (juce::Graphics& g, juce::TextButton& b, bool highlighted, bool down)
{
    const auto r = b.getLocalBounds().toFloat().reduced (2.0f);

    if (down || highlighted)
    {
        g.setColour (kInk.withAlpha (down ? 0.12f : 0.06f));
        g.fillRoundedRectangle (r, r.getHeight() * 0.5f);
    }

    g.setColour (kInk);
    g.drawRoundedRectangle (r, r.getHeight() * 0.5f, 2.5f);

    g.setFont (getTextButtonFont (b, b.getHeight()));
    g.drawText (b.getButtonText(), b.getLocalBounds(), juce::Justification::centred, false);
}

juce::Font PokeLookAndFeel::getTextButtonFont (juce::TextButton&, int)
{
    return F (13.0f);
}

juce::Font PokeLookAndFeel::getPopupMenuFont()
{
    return F (14.0f, false);
}

//==============================================================================
// Accords
//==============================================================================
void ChordStrip::setActive (int index)
{
    if (index != active)
    {
        active = index;
        repaint();
    }
}

void ChordStrip::setAnim (float secondsSinceFlash)
{
    if (secondsSinceFlash != anim)
    {
        anim = secondsSinceFlash;
        repaint();
    }
}

void ChordStrip::paint (juce::Graphics& g)
{
    const auto& prog = proc.getProgression();
    const int n = (int) prog.chords.size();
    if (n == 0)
        return;

    const float cw = (float) getWidth() / (float) n;
    const float h = (float) getHeight();

    for (int i = 0; i < n; ++i)
    {
        float alpha = 1.0f, yOff = 0.0f;

        if (anim >= 0.0f)
        {
            const float local = juce::jlimit (0.0f, 1.0f, (anim - 0.08f * (float) i) / 0.30f);
            if (local <= 0.0f)
                continue;

            alpha = local;
            yOff = (1.0f - local) * 22.0f - std::sin (local * juce::MathConstants<float>::pi) * 8.0f;
        }

        const auto& c = prog.chords[(size_t) i];
        const juce::Rectangle<float> cell (cw * (float) i, yOff, cw, h);

        // pilule colorée sur l'accord qui joue
        if (i == active)
        {
            const auto pill = cell.reduced (7.0f, 3.0f);
            g.setColour (accent.withAlpha (0.28f * alpha));
            g.fillRoundedRectangle (pill, 16.0f);
            g.setColour (accent.darker (0.2f).withAlpha (alpha));
            g.drawRoundedRectangle (pill, 16.0f, 2.0f);
        }

        g.setColour (kInk.withAlpha (alpha));
        g.setFont (F (n > 6 ? 26.0f : 34.0f));
        g.drawFittedText (PokeChordAudioProcessor::fromUtf8 (c.name),
                          cell.withHeight (h * 0.62f).toNearestInt(),
                          juce::Justification::centred, 1, 0.7f);

        g.setColour (kInk.withAlpha (alpha * 0.45f));
        g.setFont (F (12.0f));
        g.drawFittedText (PokeChordAudioProcessor::fromUtf8 (c.roman),
                          cell.withTop (cell.getY() + h * 0.62f).withHeight (18.0f).toNearestInt(),
                          juce::Justification::centred, 1, 0.7f);

    }
}

//==============================================================================
// Piano roll
//==============================================================================
void PianoRoll::setAnim (float secondsSinceFlash)
{
    if (secondsSinceFlash != anim)
    {
        anim = secondsSinceFlash;
        repaint();
    }
}

void PianoRoll::setPlayhead (float beat)
{
    if (std::fabs (beat - playBeat) > 0.002f)
    {
        playBeat = beat;
        repaint();
    }
}

void PianoRoll::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();

    g.setColour (kCard);
    g.fillRoundedRectangle (bounds, 12.0f);
    g.setColour (kTrack.withAlpha (0.35f));
    g.fillRoundedRectangle (bounds.reduced (1.0f), 12.0f);
    g.setColour (kInk.withAlpha (0.14f));
    g.drawRoundedRectangle (bounds.reduced (0.5f), 12.0f, 1.0f);

    const auto& prog = proc.getProgression();
    if (prog.events.empty() || prog.lengthBeats <= 0.0)
        return;

    const float left = 30.0f, top = 20.0f, right = 10.0f, bottom = 8.0f;
    const juce::Rectangle<float> area (left, top, bounds.getWidth() - left - right, bounds.getHeight() - top - bottom);

    // étendue des notes (au moins 16 demi-tons pour que les accords restent lisibles)
    int lo = 127, hi = 0;
    for (const auto& e : prog.events)
    {
        lo = std::min (lo, e.note);
        hi = std::max (hi, e.note);
    }
    lo -= 1;
    hi += 1;
    while (hi - lo < 15)
    {
        --lo;
        if (hi - lo < 15)
            ++hi;
    }
    const int rows = hi - lo + 1;
    const float rowH = area.getHeight() / (float) rows;
    const float len = (float) prog.lengthBeats;
    auto xOf = [&] (double beat) { return area.getX() + (float) (beat / len) * area.getWidth(); };
    auto yOf = [&] (int note) { return area.getBottom() - (float) (note - lo + 1) * rowH; };

    // lignes des touches noires et des do
    for (int n = lo; n <= hi; ++n)
    {
        const int pc = ((n % 12) + 12) % 12;
        const float y = yOf (n);
        if (pc == 1 || pc == 3 || pc == 6 || pc == 8 || pc == 10)
        {
            g.setColour (kInk.withAlpha (0.05f));
            g.fillRect (area.getX(), y, area.getWidth(), rowH);
        }
        if (pc == 0)
        {
            g.setColour (kInk.withAlpha (0.18f));
            g.fillRect (area.getX(), y + rowH - 0.5f, area.getWidth(), 1.0f);
            g.setColour (kInk.withAlpha (0.45f));
            g.setFont (F (9.0f));
            g.drawText ("C" + juce::String (n / 12 - 1), juce::Rectangle<float> (2.0f, y + rowH - 6.0f, left - 4.0f, 12.0f),
                        juce::Justification::centredRight, false);
        }
    }

    // temps et mesures
    const int beats = (int) std::floor (len);
    for (int b = 0; b <= beats; ++b)
    {
        const bool bar = (b % 4 == 0);
        g.setColour (kInk.withAlpha (bar ? 0.20f : 0.07f));
        g.fillRect (xOf ((double) b) - 0.5f, area.getY(), 1.0f, area.getHeight());
    }

    // noms des accords en haut
    g.setFont (F (10.0f));
    for (size_t i = 0; i < prog.chords.size(); ++i)
    {
        const double start = (double) i * prog.beatsPerChord;
        const float x0 = xOf (start), x1 = xOf (start + prog.beatsPerChord);
        float a = 0.6f;
        if (anim >= 0.0f)
            a *= juce::jlimit (0.0f, 1.0f, (anim - 0.08f * (float) i) / 0.30f);
        g.setColour (kInk.withAlpha (a));
        g.drawText (PokeChordAudioProcessor::fromUtf8 (prog.chords[i].name),
                    juce::Rectangle<float> (x0 + 2.0f, 3.0f, x1 - x0 - 2.0f, 14.0f),
                    juce::Justification::centredLeft, true);
    }

    // notes
    for (const auto& e : prog.events)
    {
        float visible = 1.0f;
        if (anim >= 0.0f && prog.beatsPerChord > 0.0)
        {
            const int ci = (int) (e.start / prog.beatsPerChord);
            visible = juce::jlimit (0.0f, 1.0f, (anim - 0.08f * (float) ci) / 0.30f);
            if (visible <= 0.0f)
                continue;
        }

        const float x0 = xOf (e.start);
        const float w = std::max (2.0f, xOf (e.start + e.length) - x0 - 1.5f);
        const float y = yOf (e.note);
        const bool isBass = e.note < 48;
        const float vel = 0.50f + 0.50f * (float) e.velocity / 127.0f;

        const float nh = std::max (3.0f, rowH - 1.0f);
        const bool lit = playBeat >= 0.0f && playBeat >= (float) e.start && playBeat < (float) (e.start + e.length);

        if (lit)
        {
            g.setColour (accent.withAlpha (0.35f * visible));
            g.fillRoundedRectangle (x0 - 2.0f, y - 1.5f, w + 4.0f, nh + 4.0f, 4.0f);
            g.setColour ((isBass ? accent.brighter (0.2f) : accent).withAlpha (visible));
            g.fillRoundedRectangle (x0, y + 0.5f, w, nh, 2.0f);
            g.setColour (kInk.withAlpha (0.7f * visible));
            g.drawRoundedRectangle (x0, y + 0.5f, w, nh, 2.0f, 1.0f);
        }
        else
        {
            g.setColour ((isBass ? accent.darker (0.25f) : kInk).withAlpha (vel * visible));
            g.fillRoundedRectangle (x0, y + 0.5f, w, nh, 2.0f);
        }
    }

    // tête de lecture
    if (playBeat >= 0.0f)
    {
        g.setColour (accent.withAlpha (0.30f));
        g.fillRect (xOf ((double) playBeat) - 3.0f, area.getY() - 4.0f, 6.0f, area.getHeight() + 6.0f);
        g.setColour (accent.darker (0.35f));
        g.fillRect (xOf ((double) playBeat) - 1.0f, area.getY() - 4.0f, 2.0f, area.getHeight() + 6.0f);
    }
}

//==============================================================================
// Zone de glisser-déposer MIDI
//==============================================================================
void DragZone::paint (juce::Graphics& g)
{
    const auto r = getLocalBounds().toFloat().reduced (3.0f);

    juce::Path outline;
    outline.addRoundedRectangle (r, 18.0f);

    juce::Path dashed;
    const float dashes[] = { 7.0f, 5.0f };
    juce::PathStrokeType (2.5f).createDashedStroke (dashed, outline, dashes, 2);

    if (pressed)
    {
        g.setColour (kInk.withAlpha (0.08f));
        g.fillRoundedRectangle (r, 18.0f);
    }

    g.setColour (kInk.withAlpha (pressed ? 1.0f : 0.75f));
    g.fillPath (dashed);

    g.setColour (kInk);
    g.setFont (F (15.0f));
    g.drawText (title, r.withHeight (r.getHeight() * 0.62f), juce::Justification::centred, false);

    g.setColour (kInk.withAlpha (0.5f));
    g.setFont (F (11.0f, false));
    g.drawText (subtitle ? subtitle() : juce::String(), r.withTop (r.getY() + r.getHeight() * 0.56f),
                juce::Justification::centred, false);
}

void DragZone::mouseDown (const juce::MouseEvent&)
{
    pressed = true;
    dragging = false;
    repaint();
}

void DragZone::mouseDrag (const juce::MouseEvent& e)
{
    if (dragging || e.getDistanceFromDragStart() < 6)
        return;

    if (! fileProvider)
        return;

    const auto file = fileProvider();
    if (! file.existsAsFile())
        return;

    dragging = true;

    juce::DragAndDropContainer::performExternalDragDropOfFiles (juce::StringArray { file.getFullPathName() },
                                                                false, this,
                                                                [this] { dragging = false; });
}

void DragZone::mouseUp (const juce::MouseEvent&)
{
    pressed = false;
    dragging = false;
    repaint();
}

//==============================================================================
// Poké Ball
//==============================================================================
void BallButton::setAnim (float angleRadians, float scaleFactor)
{
    if (angleRadians != angle || scaleFactor != scale)
    {
        angle = angleRadians;
        scale = scaleFactor;
        repaint();
    }
}

void BallButton::paint (juce::Graphics& g)
{
    if (image.isNull())
        return;

    const auto b = getLocalBounds().toFloat();
    const auto c = b.getCentre();
    const float d = juce::jmin (b.getWidth(), b.getHeight()) * 0.66f;

    g.addTransform (juce::AffineTransform::scale (scale, scale, c.x, c.y)
                        .followedBy (juce::AffineTransform::rotation (angle, c.x, c.y)));
    g.drawImage (image, juce::Rectangle<float> (d, d).withCentre (c));
}

void BallButton::mouseDown (const juce::MouseEvent&)
{
    if (onClick)
        onClick();
}

//==============================================================================
// Boutons ronds
//==============================================================================
void RoundButton::setOn (bool shouldBeOn)
{
    if (shouldBeOn != on)
    {
        on = shouldBeOn;
        repaint();
    }
}

void RoundButton::paint (juce::Graphics& g)
{
    const auto r = getLocalBounds().toFloat().reduced (3.0f);

    if (on)
    {
        g.setColour (kInk.withAlpha (0.1f));
        g.fillEllipse (r);
    }

    g.setColour (kInk);
    g.drawEllipse (r, 2.5f);

    const auto c = r.getCentre();
    juce::Path p;

    if (kind == Back)
    {
        p.addTriangle (c.x + 7.0f, c.y - 9.0f, c.x + 7.0f, c.y + 9.0f, c.x - 8.0f, c.y);
    }
    else if (on)
    {
        p.addRectangle (c.x - 8.0f, c.y - 9.0f, 5.0f, 18.0f);
        p.addRectangle (c.x + 3.0f, c.y - 9.0f, 5.0f, 18.0f);
    }
    else
    {
        p.addTriangle (c.x - 6.0f, c.y - 10.0f, c.x - 6.0f, c.y + 10.0f, c.x + 9.0f, c.y);
    }

    g.fillPath (p);
}

void RoundButton::mouseDown (const juce::MouseEvent&)
{
    if (onClick)
        onClick();
}

//==============================================================================
// Editeur
//==============================================================================
PokeChordAudioProcessorEditor::PokeChordAudioProcessorEditor (PokeChordAudioProcessor& p)
    : juce::AudioProcessorEditor (&p), proc (p), strip (p), dragZone (p), pianoRoll (p), analyserPage (p)
{
    setLookAndFeel (&laf);

    for (int i = 0; i < 11; ++i)
    {
        const auto b = getBin (i);
        mons[(size_t) i] = juce::ImageCache::getFromMemory (b.data, b.size);
    }
    ballImage = juce::ImageCache::getFromMemory (BinaryData::ball_png, BinaryData::ball_pngSize);
    laf.setThumbImage (ballImage);
    noctaliImage = juce::ImageCache::getFromMemory (BinaryData::noctali_png, BinaryData::noctali_pngSize);

    // ---- Poké Ball
    ball.setImage (ballImage);
    ball.setMouseCursor (juce::MouseCursor::PointingHandCursor);
    ball.onClick = [this] { startAnimation(); };
    addAndMakeVisible (ball);

    // ---- Mots : style et humeur
    setupCombo (styleCombo, "style", "STYLE");
    setupCombo (moodCombo,  "mood",  "HUMEUR");

    addAndMakeVisible (pianoRoll);

    // ---- Curseurs
    setupSlider (sliders[0], "numChords",  "ACCORDS");
    setupSlider (sliders[1], "complexity", U ("COMPLEXITÉ"));
    setupSlider (sliders[2], "boldness",   "AUDACE");
    setupSlider (sliders[3], "humanize",   "HUMANISER");
    setupSlider (sliders[4], "octave",     "OCTAVE");
    setupSlider (sliders[5], "velRandom",  U ("VÉLO RANDOM"));

    // ---- Menus
    setupCombo (combos[0], "key",      U ("TONALITÉ"));
    setupCombo (combos[1], "mode",     "MODE");
    setupCombo (combos[2], "duration", U ("DURÉE"));
    setupCombo (combos[3], "rhythm",   "RYTHME");
    setupCombo (combos[4], "voicing",  "DISPOSITION");

    setupCombo (presetCombo, "preset", "");
    presetCombo.label.setVisible (false);

    setupToggle (toggles[0], "bass",  "BASSE");
    setupToggle (toggles[1], "sound", U ("SON D'ÉCOUTE"));

    // ---- Boutons
    backButton.onClick = [this] { goBack(); };
    playButton.onClick = [this] { togglePlay(); };
    addAndMakeVisible (backButton);
    addAndMakeVisible (playButton);

    saveButton.onClick = [this] { saveMidiAs(); };
    addAndMakeVisible (saveButton);

    addAndMakeVisible (strip);

    dragZone.title = U ("GLISSE LE MIDI DANS ABLETON");
    dragZone.fileProvider = [this] { return proc.writeMidiFile(); };
    dragZone.subtitle = [this] { return proc.describeProgression(); };
    addAndMakeVisible (dragZone);

    analyserPage.setNoctali (noctaliImage);
    analyserPage.setInterceptsMouseClicks (false, true);
    addChildComponent (analyserPage);

    circleNow = circleFrom = circleTo = circleColour (0);
    accentNow = accentFrom = accentTo = typeColour (0);
    applyAccent();

    setSize (1000, 720);
    setPage (0);
    startTimerHz (60);
}

PokeChordAudioProcessorEditor::~PokeChordAudioProcessorEditor()
{
    stopTimer();
    proc.setAnalysing (false);
    setLookAndFeel (nullptr);
}

void PokeChordAudioProcessorEditor::setupSlider (Knob& k, const juce::String& paramId, const juce::String& text)
{
    k.slider.setSliderStyle (juce::Slider::LinearHorizontal);
    k.slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    k.slider.setPopupDisplayEnabled (true, true, this);
    addAndMakeVisible (k.slider);

    k.label.setText (text, juce::dontSendNotification);
    k.label.setFont (F (11.0f));
    k.label.setColour (juce::Label::textColourId, kInk.withAlpha (0.6f));
    k.label.setJustificationType (juce::Justification::centredLeft);
    k.label.setInterceptsMouseClicks (false, false);
    addAndMakeVisible (k.label);

    k.attachment = std::make_unique<SliderAttachment> (proc.apvts, paramId, k.slider);
}

void PokeChordAudioProcessorEditor::setupCombo (Combo& c, const juce::String& paramId, const juce::String& text)
{
    if (auto* choiceParam = dynamic_cast<juce::AudioParameterChoice*> (proc.apvts.getParameter (paramId)))
        c.box.addItemList (choiceParam->choices, 1);

    addAndMakeVisible (c.box);

    c.label.setText (text, juce::dontSendNotification);
    c.label.setFont (F (10.0f));
    c.label.setColour (juce::Label::textColourId, kInk.withAlpha (0.5f));
    c.label.setJustificationType (juce::Justification::centredLeft);
    c.label.setInterceptsMouseClicks (false, false);
    addAndMakeVisible (c.label);

    c.attachment = std::make_unique<ComboBoxAttachment> (proc.apvts, paramId, c.box);
}

void PokeChordAudioProcessorEditor::setupToggle (Toggle& t, const juce::String& paramId, const juce::String& text)
{
    t.button.setButtonText (text);
    addAndMakeVisible (t.button);
    t.attachment = std::make_unique<ButtonAttachment> (proc.apvts, paramId, t.button);
}

//==============================================================================
void PokeChordAudioProcessorEditor::setChoice (const char* paramId, int index)
{
    if (auto* p = proc.apvts.getParameter (paramId))
    {
        p->beginChangeGesture();
        p->setValueNotifyingHost (p->convertTo0to1 ((float) index));
        p->endChangeGesture();
    }
}

int PokeChordAudioProcessorEditor::readChoice (const char* paramId) const
{
    return (int) std::lround (proc.apvts.getRawParameterValue (paramId)->load());
}

int PokeChordAudioProcessorEditor::currentSeed() const
{
    return (int) std::lround (proc.apvts.getRawParameterValue ("seed")->load());
}

void PokeChordAudioProcessorEditor::setSeed (int value)
{
    if (auto* p = proc.apvts.getParameter ("seed"))
    {
        p->beginChangeGesture();
        p->setValueNotifyingHost (p->convertTo0to1 ((float) value));
        p->endChangeGesture();
    }
}

void PokeChordAudioProcessorEditor::generateNew()
{
    const int old = currentSeed();
    seedHistory.push_back (old);
    if (seedHistory.size() > 200)
        seedHistory.erase (seedHistory.begin());

    int next = old;
    while (next == old)
        next = juce::Random::getSystemRandom().nextInt (100000);

    setSeed (next);
    proc.startAudition();
}

void PokeChordAudioProcessorEditor::goBack()
{
    if (seedHistory.empty())
        return;

    const int previous = seedHistory.back();
    seedHistory.pop_back();
    setSeed (previous);
    proc.startAudition();
}

void PokeChordAudioProcessorEditor::togglePlay()
{
    if (proc.auditioning.load())
        proc.stopAudition();
    else
        proc.startAudition();
}

void PokeChordAudioProcessorEditor::saveMidiAs()
{
    const auto src = proc.writeMidiFile();
    const auto start = juce::File::getSpecialLocation (juce::File::userDesktopDirectory).getChildFile (src.getFileName());

    chooser = std::make_unique<juce::FileChooser> (U ("Enregistrer le clip MIDI"), start, "*.mid");
    chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles
                              | juce::FileBrowserComponent::warnAboutOverwriting,
                          [src] (const juce::FileChooser& fc)
                          {
                              const auto dest = fc.getResult();
                              if (dest != juce::File())
                                  src.copyFileTo (dest.withFileExtension ("mid"));
                          });
}

//==============================================================================
// Animation de la Poké Ball (≈ 1,2 s)
//   0.00 - 0.10  la balle s'enfonce
//   0.10 - 0.70  elle tremble trois fois
//   0.70 - 0.80  elle gonfle
//   0.80         flash : nouvelle génération, le Pokémon jaillit, les accords arrivent un par un
//==============================================================================
void PokeChordAudioProcessorEditor::startAnimation()
{
    if (animActive)
        return;

    animActive = true;
    animFired = false;
    animStart = juce::Time::getMillisecondCounterHiRes();
    animSeconds = 0.0f;
}

void PokeChordAudioProcessorEditor::updateAnimation (double nowMs)
{
    if (! animActive)
        return;

    const float t = (float) ((nowMs - animStart) / 1000.0);
    animSeconds = t;

    float angle = 0.0f, sc = 1.0f;

    if (t < 0.10f)
    {
        sc = 1.0f - 0.12f * (t / 0.10f);
    }
    else if (t < 0.70f)
    {
        const float u = (t - 0.10f) / 0.60f;
        sc = 0.88f + 0.17f * juce::jmin (1.0f, (t - 0.10f) / 0.05f);
        angle = std::sin (u * 6.0f * juce::MathConstants<float>::pi) * 0.42f * (1.0f - 0.4f * u);
    }
    else if (t < 0.80f)
    {
        sc = 1.05f + 0.20f * ((t - 0.70f) / 0.10f);
    }
    else if (t < 1.00f)
    {
        sc = 1.25f - 0.25f * ((t - 0.80f) / 0.20f);
    }

    ball.setAnim (angle, sc);

    if (t >= 0.80f && ! animFired)
    {
        animFired = true;
        generateNew();
    }

    strip.setAnim (animFired ? t - 0.80f : -1.0f);
    pianoRoll.setAnim (animFired ? t - 0.80f : -1.0f);
    repaint (kCircle.expanded (4.0f).toNearestInt());

    if (t > 1.5f)
    {
        animActive = false;
        ball.setAnim (0.0f, 1.0f);
        strip.setAnim (-1.0f);
        pianoRoll.setAnim (-1.0f);
        repaint (kCircle.expanded (4.0f).toNearestInt());
    }
}

//==============================================================================
void PokeChordAudioProcessorEditor::refreshMood()
{
    const auto& names = chords::moodNames();
    const auto& name = proc.getProgression().moodName;

    int idx = 0;
    for (size_t i = 0; i < names.size(); ++i)
        if (names[i] == name)
        {
            idx = (int) i;
            break;
        }

    if (idx != shownMood)
    {
        const bool first = (shownMood < 0);
        shownMood = idx;

        circleFrom = circleNow;
        circleTo = circleColour (idx);
        accentFrom = accentNow;
        accentTo = typeColour (idx);
        if (first)
        {
            circleNow = circleTo;
            accentNow = accentTo;
            applyAccent();
            colourT = 1.0f;
        }
        else
        {
            colourT = 0.0f;
        }

        repaint();
    }
}

void PokeChordAudioProcessorEditor::applyAccent()
{
    laf.setAccent (accentNow);
    pianoRoll.setAccent (accentNow);
    strip.setAccent (accentNow);
    for (auto& k : sliders)
        k.slider.repaint();
    repaint (748, 20, 220, 42);
}

void PokeChordAudioProcessorEditor::rebuildCards()
{
    float s = 1.0f;
    if (auto* d = juce::Desktop::getInstance().getDisplays().getPrimaryDisplay())
        s = juce::jlimit (1.0f, 3.0f, (float) d->scale);

    cardsImage = juce::Image (juce::Image::ARGB, juce::jmax (1, juce::roundToInt ((float) getWidth() * s)),
                              juce::jmax (1, juce::roundToInt ((float) getHeight() * s)), true);
    juce::Graphics g (cardsImage);
    g.addTransform (juce::AffineTransform::scale (s));

    auto card = [&g] (juce::Rectangle<float> r, float radius)
    {
        juce::Path p;
        p.addRoundedRectangle (r, radius);
        juce::DropShadow (juce::Colours::black.withAlpha (0.13f), 10, { 0, 4 }).drawForPath (g, p);
        g.setColour (kCard);
        g.fillPath (p);
        g.setColour (kInk.withAlpha (0.12f));
        g.drawRoundedRectangle (r.reduced (0.5f), radius, 1.0f);
    };

    card ({ 470.0f, 150.0f, 490.0f, 170.0f }, 12.0f);   // piano roll
    card ({ 458.0f, 322.0f, 514.0f, 186.0f }, 14.0f);   // réglages
    card ({ 28.0f, 526.0f, 944.0f, 88.0f }, 16.0f);     // accords
}

void PokeChordAudioProcessorEditor::timerCallback()
{
    if (page == 1)
    {
        analyserPage.refresh();
        return;
    }

    updateAnimation (juce::Time::getMillisecondCounterHiRes());

    const int v = proc.getProgressionVersion();
    if (v != shownVersion)
    {
        shownVersion = v;
        refreshMood();
        strip.repaint();
        pianoRoll.repaint();
        dragZone.repaint();
    }

    pianoRoll.setPlayhead (proc.getPlayBeat());
    strip.setActive (proc.playingChord.load());
    playButton.setOn (proc.auditioning.load());

    if (colourT < 1.0f)
    {
        colourT = juce::jmin (1.0f, colourT + 1.0f / (60.0f * 0.35f));
        const float e = 1.0f - std::pow (1.0f - colourT, 3.0f);
        circleNow = circleFrom.interpolatedWith (circleTo, e);
        accentNow = accentFrom.interpolatedWith (accentTo, e);
        applyAccent();
        repaint (kCircle.expanded (4.0f).toNearestInt());
    }
    else if (! animActive && (++breatheTick & 1) == 0)
    {
        // le Pokémon respire doucement au repos
        repaint (kCircle.expanded (4.0f).toNearestInt());
    }
}

//==============================================================================
void PokeChordAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (kBg);

    if (page == 0 && cardsImage.isValid())
        g.drawImage (cardsImage, 0, 0, getWidth(), getHeight(), 0, 0, cardsImage.getWidth(), cardsImage.getHeight());

    // ---- Logo et nom
    g.drawImage (ballImage, juce::Rectangle<float> (36.0f, 22.0f, 34.0f, 34.0f));
    g.setColour (kInk);
    g.setFont (F (24.0f));
    g.drawText (U ("POKÉCHORD"), 80, 22, 300, 34, juce::Justification::centredLeft, false);

    // ---- Onglets
    {
        const char* names[2] = { "GÉNÉRER", "ANALYSER" };
        const juce::Rectangle<int> rects[2] = { { 748, 26, 108, 30 }, { 868, 26, 96, 30 } };
        g.setFont (F (13.0f));
        for (int i = 0; i < 2; ++i)
        {
            const bool on = (i == page);
            g.setColour (kInk.withAlpha (on ? 1.0f : 0.4f));
            g.drawText (U (names[i]), rects[i], juce::Justification::centred, false);
            if (on)
            {
                g.setColour (accentNow.darker (0.1f));
                const int tw = juce::GlyphArrangement::getStringWidthInt (F (13.0f), U (names[i]));
                g.fillRoundedRectangle ((float) (rects[i].getCentreX() - tw / 2), (float) rects[i].getBottom(), (float) tw, 4.0f, 2.0f);
            }
        }
    }

    if (page == 1)
        return;

    // ---- Cercle de la couleur du type + Pokémon
    g.setColour (circleNow);
    g.fillEllipse (kCircle);

    if (shownMood >= 0 && shownMood < (int) mons.size() && mons[(size_t) shownMood].isValid())
    {
        float popScale = 1.0f;
        if (animActive && animSeconds >= 0.80f)
        {
            const float x = juce::jlimit (0.0f, 1.0f, (animSeconds - 0.80f) / 0.45f);
            const float c1 = 1.70158f, c3 = c1 + 1.0f;
            popScale = 1.0f + c3 * std::pow (x - 1.0f, 3.0f) + c1 * std::pow (x - 1.0f, 2.0f);
        }

        auto r = kCircle.reduced (14.0f);
        r = r.withSizeKeepingCentre (r.getWidth() * popScale, r.getHeight() * popScale);

        if (! animActive)
        {
            // respiration : léger gonflement ancré aux pieds + petit balancement
            const float ph = (float) (juce::Time::getMillisecondCounterHiRes() * 0.001) * juce::MathConstants<float>::twoPi / 3.4f;
            const float sc = 1.0f + 0.022f * std::sin (ph);
            const float nh = r.getHeight() * sc;
            r = juce::Rectangle<float> (r.getWidth() * sc, nh).withCentre ({ r.getCentreX(), r.getBottom() - nh * 0.5f });
        }
        g.drawImage (mons[(size_t) shownMood], r);
    }

    // ---- Flash blanc et anneau
    if (animActive && animSeconds >= 0.80f)
    {
        const float ft = (animSeconds - 0.80f) / 0.30f;
        if (ft < 1.0f)
        {
            g.saveState();
            juce::Path clip;
            clip.addEllipse (kCircle);
            g.reduceClipRegion (clip, juce::AffineTransform());

            const float rad = 30.0f + 400.0f * ft;
            const auto area = juce::Rectangle<float> (rad * 2.0f, rad * 2.0f).withCentre (kCircle.getCentre());

            g.setColour (juce::Colours::white.withAlpha (0.9f * (1.0f - ft)));
            g.fillEllipse (area);
            g.setColour (juce::Colours::white.withAlpha (1.0f - ft));
            g.drawEllipse (area, 5.0f);
            g.restoreState();
        }
    }

    // ---- Petits titres
    g.setColour (kInk.withAlpha (0.5f));
    g.setFont (F (10.0f));

    // ---- Ligne au-dessus des accords
}

void PokeChordAudioProcessorEditor::resized()
{
    rebuildCards();

    styleCombo.label.setBounds (470, 90, 235, 14);
    styleCombo.box.setBounds (470, 106, 235, 30);
    moodCombo.label.setBounds (725, 90, 235, 14);
    moodCombo.box.setBounds (725, 106, 235, 30);

    pianoRoll.setBounds (470, 150, 490, 170);

    const int colX[2] = { 470, 725 };
    const int colW = 235;
    for (int i = 0; i < 6; ++i)
    {
        const int col = i / 3, row = i % 3;
        const int x = colX[col], y = 330 + row * 30;
        sliders[(size_t) i].label.setBounds (x, y, 84, 24);
        sliders[(size_t) i].slider.setBounds (x + 88, y, colW - 88, 24);
    }

    const int cx[5] = { 470, 567, 644, 739, 870 };
    const int cw[5] = { 92, 72, 90, 126, 90 };
    for (int i = 0; i < 5; ++i)
    {
        combos[(size_t) i].label.setBounds (cx[i], 420, cw[i], 14);
        combos[(size_t) i].box.setBounds (cx[i], 436, cw[i], 28);
    }

    toggles[0].button.setBounds (470, 474, 110, 26);
    toggles[1].button.setBounds (600, 474, 150, 26);
    presetCombo.box.setBounds (770, 472, 190, 30);

    strip.setBounds (36, 530, 928, 80);

    analyserPage.setBounds (getLocalBounds());

    ball.setBounds (21, 615, 90, 90);
    backButton.setBounds (118, 633, 54, 54);
    playButton.setBounds (180, 633, 54, 54);
    dragZone.setBounds (250, 632, 510, 56);
    saveButton.setBounds (776, 634, 188, 52);
}

//==============================================================================
// Onglets
//==============================================================================
void PokeChordAudioProcessorEditor::setPage (int newPage)
{
    page = newPage;
    const bool gen = (page == 0);

    for (juce::Component* comp : std::initializer_list<juce::Component*> { &styleCombo.box, &styleCombo.label, &moodCombo.box, &moodCombo.label, &pianoRoll, &ball, &backButton,
                                                                            &playButton, &saveButton, &strip, &dragZone })
        comp->setVisible (gen);

    for (auto& k : sliders) { k.slider.setVisible (gen); k.label.setVisible (gen); }
    for (auto& cb : combos) { cb.box.setVisible (gen); cb.label.setVisible (gen); }
    for (auto& t : toggles) t.button.setVisible (gen);
    presetCombo.box.setVisible (gen);

    analyserPage.setVisible (! gen);
    proc.setAnalysing (! gen);
    if (! gen)
    {
        proc.stopAudition();
        analyserPage.refresh();
    }

    repaint();
}

void PokeChordAudioProcessorEditor::mouseDown (const juce::MouseEvent& e)
{
    if (juce::Rectangle<int> (748, 20, 108, 42).contains (e.getPosition()))
        setPage (0);
    else if (juce::Rectangle<int> (868, 20, 96, 42).contains (e.getPosition()))
        setPage (1);
}

//==============================================================================
// Pilule écouter / pause
//==============================================================================
void PillButton::setOn (bool shouldBeOn)
{
    if (shouldBeOn != on)
    {
        on = shouldBeOn;
        repaint();
    }
}

void PillButton::paint (juce::Graphics& g)
{
    const auto r = getLocalBounds().toFloat().reduced (2.0f);

    if (on)
    {
        g.setColour (kInk);
        g.fillRoundedRectangle (r, r.getHeight() * 0.5f);
    }
    else
    {
        g.setColour (kInk);
        g.drawRoundedRectangle (r, r.getHeight() * 0.5f, 2.5f);
    }

    g.setColour (on ? juce::Colours::white : kInk);
    g.setFont (F (17.0f));
    g.drawText (U (on ? "\xe2\x96\xa0  PAUSE" : "\xe2\x97\x8f  \xc3\x89" "COUTER"), getLocalBounds(),
                juce::Justification::centred, false);
}

void PillButton::mouseDown (const juce::MouseEvent&)
{
    if (onClick)
        onClick();
}

//==============================================================================
// Page ANALYSER
//==============================================================================
namespace
{
    const char* const kFrNotes[12] = { "DO", "DO#", "R\xc3\x89", "MIB", "MI", "FA", "FA#", "SOL", "LAB", "LA", "SIB", "SI" };
    const char* const kEnNotes[12] = { "C", "C#", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B" };
    const juce::Colour kNoctaliBg (0xfff4d77a);
}

AnalyserPage::AnalyserPage (PokeChordAudioProcessor& p) : proc (p), dragZone (p)
{
    listenButton.setMouseCursor (juce::MouseCursor::PointingHandCursor);
    listenButton.onClick = [this]
    {
        proc.setAnalysing (! proc.isAnalysing());
        listenButton.setOn (proc.isAnalysing());
        repaint();
    };
    addAndMakeVisible (listenButton);

    clearButton.onClick = [this] { proc.clearHistory(); repaint(); };
    addAndMakeVisible (clearButton);

    dragZone.title = U ("GLISSE LES ACCORDS EN MIDI DANS ABLETON");
    dragZone.fileProvider = [this] { return proc.writeDetectedMidiFile(); };
    dragZone.subtitle = [this]
    {
        const int n = (int) proc.getHistory().size();
        return juce::String (n) + (n > 1 ? U (" accords d\xc3\xa9tect\xc3\xa9s") : U (" accord d\xc3\xa9tect\xc3\xa9"));
    };
    addAndMakeVisible (dragZone);
}

void AnalyserPage::resized()
{
    listenButton.setBounds (36, 640, 230, 54);
    dragZone.setBounds (278, 640, 544, 54);
    clearButton.setBounds (834, 640, 130, 54);
}

void AnalyserPage::refresh()
{
    listenButton.setOn (proc.isAnalysing());

    const int v = proc.getAnalysisVersion();
    if (v != shownVersion)
    {
        shownVersion = v;
        repaint();
        dragZone.repaint();
    }
}

void AnalyserPage::paint (juce::Graphics& g)
{
    const auto& live = proc.getLiveChord();
    const auto& chroma = proc.getLiveChroma();
    const auto& hist = proc.getHistory();

    // ---- Noctali dans son cercle jaune
    const juce::Rectangle<float> circle (60.0f, 120.0f, 340.0f, 340.0f);
    g.setColour (kNoctaliBg);
    g.fillEllipse (circle);

    if (noctali.isValid())
    {
        g.saveState();
        juce::Path clip;
        clip.addEllipse (circle);
        g.reduceClipRegion (clip, juce::AffineTransform());
        g.drawImage (noctali, juce::Rectangle<float> (90.0f, 112.0f, 200.0f, 350.0f));
        g.restoreState();
    }

    // ---- Accord actuel
    g.setColour (kInk.withAlpha (0.5f));
    g.setFont (F (10.0f));
    g.drawText ("ACCORD ACTUEL", 440, 118, 300, 14, juce::Justification::centredLeft, false);

    g.setColour (kInk.withAlpha (live.valid ? 1.0f : 0.25f));
    g.setFont (F (84.0f));
    g.drawFittedText (live.valid ? PokeChordAudioProcessor::fromUtf8 (live.name) : juce::String (U ("\xe2\x80\x94")),
                      juce::Rectangle<int> (440, 134, 524, 96), juce::Justification::centredLeft, 1, 0.6f);

    juce::String info;
    if (live.valid)
    {
        for (size_t i = 0; i < live.pcs.size(); ++i)
            info << (i ? U (" \xc2\xb7 ") : juce::String()) << U (kFrNotes[live.pcs[i] % 12]);
        info << U (" \xe2\x80\x94 CONFIANCE ") << juce::String ((int) std::lround (live.confidence * 100.0f)) << "%";
    }
    else
    {
        info = proc.isAnalysing() ? U ("EN ATTENTE D'AUDIO SUR LA PISTE") : U ("\xc3\x89" "COUTE EN PAUSE");
    }

    g.setColour (kInk.withAlpha (0.5f));
    g.setFont (F (12.0f));
    g.drawText (info, 440, 234, 524, 18, juce::Justification::centredLeft, false);

    // ---- Notes entendues
    g.setFont (F (10.0f));
    g.drawText ("NOTES ENTENDUES", 440, 282, 300, 14, juce::Justification::centredLeft, false);

    const float colW = 524.0f / 12.0f;
    for (int i = 0; i < 12; ++i)
    {
        bool inChord = false;
        if (live.valid)
            for (int pc : live.pcs)
                inChord |= (pc == i);

        const float h = 6.0f + 94.0f * juce::jlimit (0.0f, 1.0f, chroma[(size_t) i]);
        const float x = 440.0f + colW * (float) i + 3.0f;

        g.setColour (inChord ? kInk : kTrack);
        g.fillRoundedRectangle (x, 420.0f - h, colW - 6.0f, h, 3.0f);

        g.setColour (kInk.withAlpha (0.5f));
        g.setFont (F (10.0f));
        g.drawText (kEnNotes[i], juce::Rectangle<float> (x, 424.0f, colW - 6.0f, 14.0f),
                    juce::Justification::centred, false);
    }

    // ---- Historique des accords
    g.setColour (kInk);
    g.fillRect (36, 521, 928, 2);

    if (hist.empty())
    {
        g.setColour (kInk.withAlpha (0.4f));
        g.setFont (F (14.0f, false));
        g.drawText (U ("Joue de l'audio sur cette piste : les accords d\xc3\xa9tect\xc3\xa9s apparaissent ici."),
                    36, 530, 928, 80, juce::Justification::centred, false);
        return;
    }

    const int shown = juce::jmin (8, (int) hist.size());
    const int first = (int) hist.size() - shown;
    const float cw = 928.0f / (float) shown;
    const double bpm = juce::jlimit (20.0, 300.0, proc.getBpm());
    const double t0 = hist.front().timeSec;

    for (int i = 0; i < shown; ++i)
    {
        const auto& d = hist[(size_t) (first + i)];
        const juce::Rectangle<float> cell (36.0f + cw * (float) i, 530.0f, cw, 80.0f);

        g.setColour (kInk);
        g.setFont (F (shown > 6 ? 26.0f : 34.0f));
        g.drawFittedText (PokeChordAudioProcessor::fromUtf8 (d.name), cell.withHeight (50.0f).toNearestInt(),
                          juce::Justification::centred, 1, 0.7f);

        const int bar = 1 + (int) std::floor ((d.timeSec - t0) * bpm / 60.0 / 4.0);
        g.setColour (kInk.withAlpha (0.45f));
        g.setFont (F (11.0f));
        g.drawText ("mes. " + juce::String (bar), cell.withTop (cell.getY() + 52.0f).withHeight (16.0f),
                    juce::Justification::centred, false);

        if (i == shown - 1)
        {
            g.setColour (kInk);
            g.fillRoundedRectangle (cell.getCentreX() - 14.0f, cell.getBottom() - 6.0f, 28.0f, 3.0f, 1.5f);
        }
    }
}
