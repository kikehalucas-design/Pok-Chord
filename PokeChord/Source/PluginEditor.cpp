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

    // Le fond de l'interface prend une teinte très douce du type
    juce::Colour bgColour (int i)
    {
        return circleColour (i);   // fond = ancienne couleur du cercle
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

    g.setColour (whiteFader ? kInk.withAlpha (0.30f) : kTrack);
    g.fillRoundedRectangle ((float) x, cy - 2.0f, (float) width, 4.0f, 2.0f);

    // rouge, ou blanc quand l'humeur est rouge (Dracaufeu) pour rester visible
    g.setColour (whiteFader ? juce::Colours::white : kRed);
    g.fillRoundedRectangle ((float) x, cy - 2.5f, juce::jmax (0.0f, sliderPos - (float) x), 5.0f, 2.5f);

    // le bouton du fader est une Poké Ball qui roule quand on la déplace
    if (thumb.isValid())
    {
        g.saveState();
        g.addTransform (juce::AffineTransform::rotation ((sliderPos - (float) x) / 11.0f, sliderPos, cy));
        g.setOpacity (1.0f);   // drawImage reprend l'opacité de la dernière couleur : on la remet à 100 %
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

    const bool night = b.getProperties().contains ("night");
    const juce::Colour main = night ? juce::Colour (0xffeeeaff) : kInk;

    if (down || highlighted)
    {
        g.setColour (main.withAlpha (down ? 0.12f : 0.06f));
        g.fillRoundedRectangle (r, r.getHeight() * 0.5f);
    }

    g.setColour (night ? main.withAlpha (0.55f) : main);
    g.drawRoundedRectangle (r, r.getHeight() * 0.5f, 2.5f);

    g.setColour (main);
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
        const double start = i < prog.starts.size() ? prog.starts[i] : (double) i * prog.beatsPerChord;
        const double clen = i < prog.lens.size() ? prog.lens[i] : prog.beatsPerChord;
        const float x0 = xOf (start), x1 = xOf (start + clen);
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
        if (anim >= 0.0f && ! prog.lens.empty())
        {
            const int ci = prog.chordAt (e.start + 1.0e-4);
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

    const juce::Colour main = night ? juce::Colour (0xfff7d02c) : kInk;
    const juce::Colour text = night ? juce::Colour (0xffeeeaff) : kInk;

    if (pressed)
    {
        g.setColour (main.withAlpha (0.08f));
        g.fillRoundedRectangle (r, 18.0f);
    }

    g.setColour (main.withAlpha (pressed ? 1.0f : 0.65f));
    g.fillPath (dashed);

    g.setColour (text);
    g.setFont (F (night ? 13.0f : 15.0f));
    g.drawText (title, r.withHeight (r.getHeight() * 0.62f), juce::Justification::centred, false);

    g.setColour (text.withAlpha (0.5f));
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
    g.setOpacity (1.0f);   // drawImage reprend l'opacité de la dernière couleur : on la remet à 100 %
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
    sceneImage = juce::ImageCache::getFromMemory (BinaryData::forest_png, BinaryData::forest_pngSize);

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
    analyserPage.setLogo (ballImage);
    analyserPage.setInterceptsMouseClicks (false, true);
    addChildComponent (analyserPage);
    addChildComponent (fx);

    circleNow = circleFrom = circleTo = circleColour (0);
    accentNow = accentFrom = accentTo = typeColour (0);
    bgNow = bgFrom = bgTo = bgColour (0);
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
    repaint (kCircle.expanded (64.0f).toNearestInt());

    if (t > 1.5f)
    {
        animActive = false;
        ball.setAnim (0.0f, 1.0f);
        strip.setAnim (-1.0f);
        pianoRoll.setAnim (-1.0f);
        repaint (kCircle.expanded (64.0f).toNearestInt());
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
        bgFrom = bgNow;
        bgTo = bgColour (idx);
        laf.setWhiteFader (false);
        for (auto& k : sliders)
            k.slider.repaint();
        if (first)
        {
            bgNow = bgTo;
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

    card ({ 20.0f, 80.0f, 420.0f, 420.0f }, 22.0f);     // décor
    card ({ 470.0f, 150.0f, 490.0f, 170.0f }, 12.0f);   // piano roll
    card ({ 458.0f, 322.0f, 514.0f, 186.0f }, 14.0f);   // réglages
    card ({ 28.0f, 526.0f, 944.0f, 88.0f }, 16.0f);     // accords
}

void PokeChordAudioProcessorEditor::timerCallback()
{
    if (page == 1)
    {
        analyserPage.refresh();
        analyserPage.tick (juce::Time::getMillisecondCounterHiRes());
        return;
    }

    updateAnimation (juce::Time::getMillisecondCounterHiRes());
    fx.update (juce::Time::getMillisecondCounterHiRes());

    const int v = proc.getProgressionVersion();
    if (v != shownVersion)
    {
        shownVersion = v;
        refreshMood();
        if (animActive && animFired)
            fx.begin (shownMood);   // effet visuel propre au Pokémon
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
        bgNow = bgFrom.interpolatedWith (bgTo, e);
        applyAccent();
        repaint();
    }
}

//==============================================================================
void PokeChordAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (bgNow);

    if (page == 0 && cardsImage.isValid())
        g.drawImage (cardsImage, 0, 0, getWidth(), getHeight(), 0, 0, cardsImage.getWidth(), cardsImage.getHeight());

    // ---- Logo et nom
    g.setOpacity (1.0f);   // drawImage reprend l'opacité de la dernière couleur : on la remet à 100 %
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

    // ---- Décor (forêt pixel-art) derrière le Pokémon, recadré pour remplir le cadre
    const juce::Image& scene = sceneImage;
    if (scene.isValid())
    {
        const float iw = (float) scene.getWidth(), ih = (float) scene.getHeight();
        const float side = juce::jmin (iw, ih);
        const juce::Rectangle<int> src (juce::roundToInt ((iw - side) * 0.5f),
                                        0,
                                        juce::roundToInt (side), juce::roundToInt (side));

        g.saveState();
        juce::Path clip;
        clip.addRoundedRectangle (kCircle, 22.0f);
        g.reduceClipRegion (clip, juce::AffineTransform());
        g.setImageResamplingQuality (juce::Graphics::lowResamplingQuality);   // garde les pixels nets
        g.setOpacity (1.0f);   // drawImage reprend l'opacité de la dernière couleur : on la remet à 100 %
        g.drawImage (scene, (int) kCircle.getX(), (int) kCircle.getY(), (int) kCircle.getWidth(), (int) kCircle.getHeight(),
                     src.getX(), src.getY(), src.getWidth(), src.getHeight());
        g.restoreState();

        g.setColour (kInk.withAlpha (0.22f));
        g.drawRoundedRectangle (kCircle.reduced (0.5f), 22.0f, 1.5f);
    }

    if (shownMood >= 0 && shownMood < (int) mons.size() && mons[(size_t) shownMood].isValid())
    {
        float popScale = 1.0f;
        if (animActive && animSeconds >= 0.80f)
        {
            const float x = juce::jlimit (0.0f, 1.0f, (animSeconds - 0.80f) / 0.45f);
            const float c1 = 1.70158f, c3 = c1 + 1.0f;
            popScale = 1.0f + c3 * std::pow (x - 1.0f, 3.0f) + c1 * std::pow (x - 1.0f, 2.0f);
        }

        // zone réellement occupée par chaque Pokémon dans son image 480x480 (gauche, haut, droite, bas)
        static const int bb[11][4] = {
            { 105, 88, 374, 391 }, { 111, 81, 369, 398 }, { 101, 93, 379, 386 }, { 93, 101, 387, 378 },
            { 80, 113, 400, 367 }, { 92, 102, 387, 378 }, { 99, 95, 380, 385 }, { 111, 82, 369, 398 },
            { 99, 96, 381, 384 }, { 98, 96, 382, 383 }, { 81, 111, 398, 369 }
        };
        const auto& b = bb[juce::jlimit (0, 10, shownMood)];
        const float bw = (float) (b[2] - b[0]), bh = (float) (b[3] - b[1]);
        const float s = juce::jmin (320.0f / bh, 360.0f / bw);
        const float imgW = (float) mons[(size_t) shownMood].getWidth(), imgH = (float) mons[(size_t) shownMood].getHeight();
        const float k = s * 480.0f / juce::jmax (1.0f, imgW);

        // les pieds posent sur l'herbe du décor
        const float groundY = kCircle.getY() + kCircle.getHeight() * 0.925f;
        const float feetX = kCircle.getCentreX();
        const juce::Point<float> anchor (feetX, groundY);

        const float grow = popScale;   // plus de respiration au repos : l'image reste nette

        juce::Rectangle<float> r (feetX - 0.5f * (float) (b[0] + b[2]) * s, groundY - (float) b[3] * s, imgW * k, imgH * k);
        r = r.transformedBy (juce::AffineTransform::scale (grow, grow, anchor.x, anchor.y));
        g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
        g.setOpacity (1.0f);   // drawImage reprend l'opacité de la dernière couleur : on la remet à 100 %
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
            clip.addRoundedRectangle (kCircle, 22.0f);
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
    fx.setBounds (getLocalBounds());

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
    fx.stop();
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
// Effets visuels par Pokémon (dessinés par le calcul, aucune image)
//==============================================================================
namespace
{
    constexpr float kFxDur = 1.7f;

    float smooth01 (float a, float b, float x)
    {
        x = juce::jlimit (0.0f, 1.0f, (x - a) / (b - a));
        return x * x * (3.0f - 2.0f * x);
    }

    juce::Path heartPath (float x, float y, float s)
    {
        juce::Path p;
        p.startNewSubPath (x, y + s * 0.9f);
        p.cubicTo (x - s * 1.1f, y + s * 0.1f, x - s * 0.55f, y - s * 0.85f, x, y - s * 0.25f);
        p.cubicTo (x + s * 0.55f, y - s * 0.85f, x + s * 1.1f, y + s * 0.1f, x, y + s * 0.9f);
        p.closeSubPath();
        return p;
    }

    juce::Path flamePath (float x, float y, float r)
    {
        juce::Path p;
        p.startNewSubPath (x, y - r * 2.4f);
        p.quadraticTo (x + r * 1.15f, y - r * 0.2f, x, y + r);
        p.quadraticTo (x - r * 1.15f, y - r * 0.2f, x, y - r * 2.4f);
        p.closeSubPath();
        return p;
    }

    void fillRay (juce::Graphics& g, float cx, float cy, float ang, float len, float w, juce::Colour col)
    {
        juce::Path p;
        p.addTriangle (cx, cy, cx + std::cos (ang - w) * len, cy + std::sin (ang - w) * len,
                       cx + std::cos (ang + w) * len, cy + std::sin (ang + w) * len);
        g.setColour (col);
        g.fillPath (p);
    }

    // bords de la fenêtre assombris / colorés
    void fillVignette (juce::Graphics& g, float W, float H, float alpha, juce::Colour c, float inner)
    {
        if (alpha <= 0.001f)
            return;

        const float rad = std::hypot (W, H) * 0.5f;
        juce::ColourGradient gr (c.withAlpha (0.0f), W * 0.5f, H * 0.5f, c.withAlpha (alpha), W * 0.5f + rad, H * 0.5f, true);
        gr.addColour ((double) inner, c.withAlpha (0.0f));
        g.setGradientFill (gr);
        g.fillRect (0.0f, 0.0f, W, H);
    }

    void fillStar (juce::Graphics& g, float x, float y, float outer, float inner, juce::Colour col)
    {
        juce::Path p;
        p.addStar ({ x, y }, 4, inner, outer);
        g.setColour (col);
        g.fillPath (p);
    }
}

float FxOverlay::rnd (int i, int k) const
{
    juce::uint32 h = ((juce::uint32) i * 73856093u) ^ ((juce::uint32) k * 19349663u) ^ ((juce::uint32) seed * 83492791u);
    h ^= h >> 13;
    h *= 0x5bd1e995u;
    h ^= h >> 15;
    return (float) (h & 0xffffffu) / 16777215.0f;
}

void FxOverlay::begin (int moodIndex)
{
    kind = moodIndex;
    seed = juce::Random::getSystemRandom().nextInt (100000);
    startMs = juce::Time::getMillisecondCounterHiRes();
    t = 0.0f;
    active = true;
    setVisible (true);
    toFront (false);
    repaint();
}

void FxOverlay::stop()
{
    active = false;
    setVisible (false);
}

void FxOverlay::update (double nowMs)
{
    if (! active)
        return;

    t = (float) ((nowMs - startMs) / 1000.0);
    if (t > kFxDur)
    {
        stop();
        return;
    }
    repaint();
}

void FxOverlay::paint (juce::Graphics& g)
{
    if (! active)
        return;

    const float W = (float) getWidth(), H = (float) getHeight();
    const float u = t / kFxDur;
    const float env = 1.0f - smooth01 (0.65f, 1.0f, u);
    const float PX = 230.0f, PY = 290.0f;   // centre du Pokémon
    constexpr float pi = juce::MathConstants<float>::pi;
    const float pulse = std::sin (pi * juce::jmin (1.0f, u));
    auto R = [this] (int i, int k) { return rnd (i, k); };

    if (kind == 0)   // flammes : Salamèche
    {
        const bool big = false;
        const int N = 110;
        const float boost = big ? 1.35f : 1.0f;

        g.setColour (big ? juce::Colour::fromFloatRGBA (1.0f, 0.24f, 0.08f, 0.45f * juce::jmax (0.0f, 1.0f - u * 5.0f))
                         : juce::Colour::fromFloatRGBA (1.0f, 0.59f, 0.12f, 0.30f * juce::jmax (0.0f, 1.0f - u * 5.0f)));
        g.fillAll();
        fillVignette (g, W, H, (big ? 0.6f : 0.45f) * env * smooth01 (0.0f, 0.15f, u),
                      big ? juce::Colour (120, 10, 0) : juce::Colour (255, 100, 0), 0.45f);

        juce::ColourGradient glow (big ? juce::Colour::fromFloatRGBA (0.92f, 0.16f, 0.08f, 0.55f * env)
                                       : juce::Colour::fromFloatRGBA (1.0f, 0.51f, 0.08f, 0.45f * env),
                                   0.0f, H, juce::Colour::fromFloatRGBA (1.0f, 0.51f, 0.08f, 0.0f), 0.0f, H - 380.0f * boost, false);
        g.setGradientFill (glow);
        g.fillRect (0.0f, H - 380.0f * boost, W, 380.0f * boost);

        if (big)   // onde de choc du rugissement
        {
            const float a = u / 0.5f;
            if (a < 1.0f)
            {
                g.setColour (juce::Colour::fromFloatRGBA (1.0f, 0.47f, 0.12f, (1.0f - a) * 0.8f));
                g.drawEllipse (PX - a * W, PY - a * W, a * W * 2.0f, a * W * 2.0f, 30.0f * (1.0f - a) + 2.0f);
            }
        }

        for (int i = 0; i < N; ++i)
        {
            const float a = (t - R (i, 0) * 0.9f) / (0.9f + R (i, 4) * 0.5f);
            if (a < 0.0f || a > 1.0f)
                continue;

            const float x = R (i, 1) * W + std::sin (a * 6.0f + (float) i) * 14.0f;
            const float y = H + 30.0f - a * (300.0f + R (i, 2) * 520.0f) * boost;
            const float r = (16.0f + R (i, 3) * 30.0f) * (1.0f - a * 0.65f) * boost;

            const juce::Colour col = a < 0.35f ? juce::Colour (255, 200, 60) : a < 0.7f ? juce::Colour (255, 110, 20) : juce::Colour (200, 30, 10);
            g.setColour (col.withAlpha ((1.0f - a) * 0.9f));
            g.fillPath (flamePath (x, y, r));
            g.setColour (juce::Colour (255, 240, 150).withAlpha ((1.0f - a) * 0.8f));
            g.fillPath (flamePath (x, y + r * 0.3f, r * 0.55f));
        }

        for (int j = 0; j < 60; ++j)   // braises
        {
            const float a = (t - R (j, 0)) / 1.0f;
            if (a < 0.0f || a > 1.0f)
                continue;

            const float rr = 2.0f + R (j, 3) * 3.0f;
            g.setColour (juce::Colour (255, 200, 80).withAlpha (1.0f - a));
            g.fillEllipse (R (j, 1) * W + std::sin (a * 9.0f + (float) j) * 30.0f - rr, H - a * H * 0.9f * (0.6f + 0.4f * R (j, 2)) - rr, rr * 2.0f, rr * 2.0f);
        }
    }
    else if (kind == 10)   // Dracaufeu : Lance-Flammes qui balaie la fenêtre, puis incendie
    {
        const float dim = smooth01 (0.0f, 0.12f, u) * (1.0f - smooth01 (0.82f, 1.0f, u));
        const float OX = 110.0f, OY = H * 0.52f;

        g.setColour (juce::Colour::fromFloatRGBA (0.16f, 0.0f, 0.0f, 0.5f * dim));
        g.fillAll();
        fillVignette (g, W, H, 0.7f * dim, juce::Colour (60, 0, 0), 0.3f);
        g.setColour (juce::Colour::fromFloatRGBA (1.0f, 0.35f, 0.12f, 0.35f * juce::jmax (0.0f, 1.0f - u * 9.0f)));
        g.fillAll();

        for (int k = 0; k < 2; ++k)   // rugissement
        {
            const float a = (t - (float) k * 0.18f) / 0.55f;
            if (a < 0.0f || a > 1.0f)
                continue;

            const float r = a * W * 1.1f;
            g.setColour (juce::Colour::fromFloatRGBA (1.0f, 0.43f, 0.08f, (1.0f - a) * 0.85f));
            g.drawEllipse (OX + 60.0f - r, OY - r, r * 2.0f, r * 2.0f, 32.0f * (1.0f - a) + 2.0f);
        }

        // Lance-Flammes : un jet qui balaie la fenêtre de bas en haut
        for (int i = 0; i < 260; ++i)
        {
            const float te = R (i, 0) * 0.9f, a = (t - 0.12f - te) / 0.7f;
            if (a < 0.0f || a > 1.0f)
                continue;

            const float ang = 0.5f - 1.0f * (te / 0.9f) + (R (i, 1) - 0.5f) * (0.06f + a * 0.5f), d = a * W * 1.25f;
            const float x = OX + std::cos (ang) * d, y = OY + std::sin (ang) * d, r = 12.0f + a * (44.0f + R (i, 3) * 34.0f);
            const juce::Colour col = a < 0.25f ? juce::Colour (255, 240, 170) : a < 0.55f ? juce::Colour (255, 150, 30) : juce::Colour (200, 35, 10);
            const float fade = 1.0f - a * a;

            g.setColour (col.withAlpha (fade * 0.5f));
            g.fillEllipse (x - r, y - r, r * 2.0f, r * 2.0f);
            g.setColour (col.withAlpha (fade * 0.9f));
            g.fillPath (flamePath (x, y, r * 0.55f));
        }

        {
            juce::ColourGradient gl (juce::Colour::fromFloatRGBA (1.0f, 0.9f, 0.59f, 0.95f * smooth01 (0.05f, 0.2f, u) * (1.0f - smooth01 (0.62f, 0.85f, u))), OX, OY,
                                     juce::Colour::fromFloatRGBA (1.0f, 0.47f, 0.08f, 0.0f), OX + 190.0f, OY, true);
            g.setGradientFill (gl);
            g.fillRect (0.0f, 0.0f, W, H);
        }

        // incendie final : mur de flammes et flash
        for (int i = 0; i < 110; ++i)
        {
            const float a = (t - 0.75f - R (i, 0) * 0.5f) / (0.8f + R (i, 4) * 0.4f);
            if (a < 0.0f || a > 1.0f)
                continue;

            const float x = R (i, 1) * W, y = H + 30.0f - a * (300.0f + R (i, 2) * 420.0f), r = (16.0f + R (i, 3) * 30.0f) * (1.0f - a * 0.6f);
            g.setColour ((a < 0.35f ? juce::Colour (255, 200, 60) : a < 0.7f ? juce::Colour (255, 110, 20) : juce::Colour (190, 30, 10)).withAlpha ((1.0f - a) * 0.9f));
            g.fillPath (flamePath (x, y, r));
        }

        const float fu = (u - 0.5f) / 0.3f;
        if (fu > 0.0f && fu < 1.0f)
        {
            g.setColour (juce::Colour::fromFloatRGBA (1.0f, 0.92f, 0.78f, 0.55f * std::pow (1.0f - fu, 1.4f)));
            g.fillAll();
        }

        for (int j = 0; j < 70; ++j)   // braises
        {
            const float a = (t - 0.3f - R (j, 0) * 0.7f) / 1.0f;
            if (a < 0.0f || a > 1.0f)
                continue;

            const float rr = 2.0f + R (j, 3) * 3.0f;
            g.setColour (juce::Colour (255, 200, 80).withAlpha (1.0f - a));
            g.fillEllipse (R (j, 1) * W + std::sin (a * 9.0f + (float) j) * 30.0f - rr, H - a * H * 0.9f * (0.6f + 0.4f * R (j, 2)) - rr, rr * 2.0f, rr * 2.0f);
        }
    }
    else if (kind == 2)   // orage : Pikachu (gros éclairs qui frappent le sol)
    {
        const float dark = juce::jmin (1.0f, u / 0.1f) * (1.0f - smooth01 (0.6f, 0.95f, u));
        g.setColour (juce::Colour::fromFloatRGBA (0.03f, 0.03f, 0.16f, 0.38f * dark));
        g.fillAll();

        const float T[5] = { 0.0f, 0.14f, 0.34f, 0.5f, 0.78f };

        // chemin en zigzag entre deux points
        auto makeBolt = [&R] (float x0, float y0, float x1, float y1, int n, float jit, int so)
        {
            juce::Path p;
            for (int k = 0; k <= n; ++k)
            {
                const float q = (float) k / (float) n;
                const float e = (k == 0 || k == n) ? 0.0f : (std::sin (juce::MathConstants<float>::pi * q) * 0.6f + 0.4f);
                const float px = x0 + (x1 - x0) * q + (R (so + k, 6) - 0.5f) * jit * e;
                const float py = y0 + (y1 - y0) * q;
                if (k == 0) p.startNewSubPath (px, py); else p.lineTo (px, py);
            }
            return p;
        };

        for (int b = 0; b < 5; ++b)
        {
            const float lt = t - T[b];
            if (lt < 0.0f || lt > 0.3f)
                continue;

            const float fl = lt < 0.05f ? 1.0f : (std::sin (lt * 90.0f) > 0.0f ? 1.0f : 0.55f);
            const float it = (1.0f - lt / 0.3f) * fl;

            g.setColour (juce::Colour::fromFloatRGBA (1.0f, 0.99f, 0.86f, (b == 0 ? 0.55f : 0.3f) * it));
            g.fillAll();

            const float x0 = b == 0 ? PX + 40.0f : W * (0.12f + 0.76f * R (b, 5));
            const float x1 = x0 + (R (b, 7) - 0.5f) * 120.0f;
            const float yg = H * 0.92f;

            // tronc principal + 3 branches
            std::vector<juce::Point<float>> pts;
            for (int k = 0; k <= 20; ++k)
            {
                const float q = (float) k / 20.0f;
                const float e = (k == 0 || k == 20) ? 0.0f : (std::sin (juce::MathConstants<float>::pi * q) * 0.6f + 0.4f);
                pts.push_back ({ x0 + (x1 - x0) * q + (R (b * 50 + k, 6) - 0.5f) * 110.0f * e, -10.0f + (yg + 10.0f) * q });
            }

            juce::Path main;
            for (size_t k = 0; k < pts.size(); ++k)
            {
                if (k == 0) main.startNewSubPath (pts[k]); else main.lineTo (pts[k]);
            }

            juce::Path branches;
            for (int j = 0; j < 3; ++j)
            {
                const int k = 4 + (int) std::floor (R (b * 9 + j, 3) * 10.0f);
                const auto s = pts[(size_t) k];
                const float dir = R (b * 9 + j, 4) > 0.5f ? 1.0f : -1.0f;
                branches.addPath (makeBolt (s.x, s.y, s.x + dir * (90.0f + R (b * 9 + j, 2) * 140.0f),
                                            s.y + 120.0f + R (b * 9 + j, 1) * 160.0f, 6, 70.0f, b * 50 + 30 + j * 10));
            }

            const float widths[3] = { 46.0f, 20.0f, 8.0f };
            const juce::Colour cols[3] = { juce::Colour::fromFloatRGBA (1.0f, 0.84f, 0.16f, 0.28f * it),
                                           juce::Colour::fromFloatRGBA (1.0f, 0.94f, 0.47f, 0.75f * it),
                                           juce::Colours::white.withAlpha (it) };
            for (int w = 0; w < 3; ++w)
            {
                g.setColour (cols[w]);
                g.strokePath (main, juce::PathStrokeType (widths[w], juce::PathStrokeType::mitered, juce::PathStrokeType::rounded));
                g.strokePath (branches, juce::PathStrokeType (widths[w] * 0.45f, juce::PathStrokeType::mitered, juce::PathStrokeType::rounded));
            }

            // impact au sol : halo et étincelles
            juce::ColourGradient halo (juce::Colour::fromFloatRGBA (1.0f, 1.0f, 0.9f, 0.9f * it), x1, yg,
                                       juce::Colour::fromFloatRGBA (1.0f, 0.86f, 0.24f, 0.0f), x1 + 110.0f, yg, true);
            g.setGradientFill (halo);
            g.fillEllipse (x1 - 110.0f, yg - 110.0f, 220.0f, 220.0f);

            g.setColour (juce::Colour::fromFloatRGBA (1.0f, 0.94f, 0.55f, it));
            for (int k = 0; k < 10; ++k)
            {
                const float ang = -juce::MathConstants<float>::pi * R (b * 10 + k, 2), len = 40.0f + R (b * 10 + k, 3) * 70.0f;
                g.drawLine (x1, yg, x1 + std::cos (ang) * len, yg + std::sin (ang) * len * 0.6f, 3.0f);
            }
        }
    }
    else if (kind == 3)   // coeurs : Rondoudou
    {
        g.setColour (juce::Colour::fromFloatRGBA (1.0f, 0.47f, 0.67f, 0.25f * juce::jmax (0.0f, 1.0f - u * 5.0f)));
        g.fillAll();
        fillVignette (g, W, H, 0.4f * pulse, juce::Colour (255, 90, 140), 0.4f);

        const float big = u / 0.6f;   // un cœur géant qui enfle depuis le Pokémon
        if (big < 1.0f)
        {
            g.setColour (juce::Colour (255, 100, 150).withAlpha ((1.0f - big) * 0.55f));
            g.fillPath (heartPath (PX, PY, 40.0f + big * 420.0f));
        }

        for (int i = 0; i < 60; ++i)
        {
            const float a = (t - R (i, 0) * 0.7f) / 1.1f;
            if (a < 0.0f || a > 1.0f)
                continue;

            const float x = R (i, 1) * W + std::sin (a * 7.0f + (float) i) * 16.0f;
            const float y = H + 40.0f - a * (H + 100.0f) * (0.7f + 0.3f * R (i, 2));
            const float s = (20.0f + R (i, 3) * 60.0f) * 0.6f;

            g.setColour ((R (i, 5) > 0.5f ? juce::Colour (255, 105, 150) : juce::Colour (240, 60, 100)).withAlpha ((1.0f - a * a) * 0.9f));
            g.fillPath (heartPath (x, y, s));
        }
    }
    else if (kind == 1)   // explosion d'étoiles et rayons dorés : Évoli
    {
        g.setColour (juce::Colour::fromFloatRGBA (1.0f, 0.94f, 0.67f, 0.35f * juce::jmax (0.0f, 1.0f - u * 6.0f)));
        g.fillAll();

        for (int k = 0; k < 14; ++k)
            fillRay (g, PX, PY, (float) k * juce::MathConstants<float>::twoPi / 14.0f + t * 0.6f, 900.0f, 0.09f,
                     juce::Colour::fromFloatRGBA (1.0f, 0.84f, 0.35f, 0.22f * pulse));

        for (int i = 0; i < 90; ++i)
        {
            const float a = (t - R (i, 0) * 1.1f) / 0.55f;
            if (a < 0.0f || a > 1.0f)
                continue;

            const float tw = std::sin (pi * a), s = 14.0f + R (i, 3) * 40.0f;
            fillStar (g, R (i, 1) * W, R (i, 2) * H, s, s * 0.35f,
                      (R (i, 5) > 0.5f ? juce::Colour (255, 215, 110) : juce::Colours::white).withAlpha (tw));
        }
    }
    else if (kind == 9)   // galaxie d'étoiles : Mew
    {
        g.setColour (juce::Colour::fromFloatRGBA (1.0f, 0.78f, 0.94f, 0.3f * juce::jmax (0.0f, 1.0f - u * 5.0f)));
        g.fillAll();

        juce::ColourGradient gl (juce::Colour::fromFloatRGBA (1.0f, 0.71f, 0.9f, 0.5f * env), PX, PY,
                                 juce::Colour::fromFloatRGBA (1.0f, 0.71f, 0.9f, 0.0f), PX + 260.0f, PY, true);
        g.setGradientFill (gl);
        g.fillRect (0.0f, 0.0f, W, H);

        for (int k = 0; k < 3; ++k)
        {
            const float a = (t - (float) k * 0.25f) / 1.0f;
            if (a < 0.0f || a > 1.0f)
                continue;

            const float r = a * W * 0.8f;
            g.setColour (juce::Colour::fromFloatRGBA (1.0f, 0.59f, 0.86f, (1.0f - a) * 0.7f));
            g.drawEllipse (PX - r, PY - r, r * 2.0f, r * 2.0f, 8.0f * (1.0f - a) + 1.0f);
        }

        for (int i = 0; i < 110; ++i)
        {
            const float a = (t - R (i, 0) * 0.5f) / 1.15f;
            if (a < 0.0f || a > 1.0f)
                continue;

            const float ang = a * 8.0f + (float) i * 0.4f, rad = a * 420.0f * (0.35f + R (i, 2));
            const juce::Colour col = (i % 3 == 0) ? juce::Colour (255, 170, 220) : (i % 3 == 1) ? juce::Colours::white : juce::Colour (170, 220, 255);
            fillStar (g, PX + std::cos (ang) * rad * 1.2f, PY + std::sin (ang) * rad * 0.9f, 8.0f + R (i, 3) * 18.0f, 3.5f,
                      col.withAlpha (std::sin (pi * a)));
        }
    }
    else if (kind == 4)   // nuit et gros Zzz : Ronflex
    {
        g.setColour (juce::Colour::fromFloatRGBA (0.04f, 0.06f, 0.24f, 0.42f * pulse));
        g.fillAll();

        for (int i = 0; i < 40; ++i)   // petites étoiles de nuit
        {
            const float a = (t - R (i, 0) * 0.9f) / 0.6f;
            if (a < 0.0f || a > 1.0f)
                continue;

            fillStar (g, R (i, 1) * W, R (i, 2) * H * 0.8f, 4.0f + R (i, 3) * 6.0f, 1.8f, juce::Colour (255, 255, 230).withAlpha (std::sin (pi * a)));
        }

        for (int z = 0; z < 12; ++z)
        {
            const float a = (t - (float) z * 0.11f) / 1.1f;
            if (a < 0.0f || a > 1.0f)
                continue;

            const float cx = 240.0f + a * 110.0f + std::sin (a * 4.0f + (float) z) * 20.0f + (float) (z % 3) * 30.0f, cy = 210.0f - a * 200.0f;
            const auto box = juce::Rectangle<float> (180.0f, 180.0f).withCentre ({ cx, cy });

            g.setFont (F (30.0f + a * 80.0f));
            g.setColour (juce::Colour (120, 160, 255).withAlpha ((1.0f - a) * 0.4f));   // halo
            for (int o = 0; o < 4; ++o)
                g.drawText ("Z", box.translated ((o % 2 ? 3.0f : -3.0f), (o < 2 ? 3.0f : -3.0f)), juce::Justification::centred, false);
            g.setColour (juce::Colours::white.withAlpha ((1.0f - a) * 0.9f));
            g.drawText ("Z", box, juce::Justification::centred, false);
        }
    }
    else if (kind == 5)   // tempête de feuilles : Bulbizarre
    {
        g.setColour (juce::Colour::fromFloatRGBA (0.35f, 0.78f, 0.24f, 0.22f * juce::jmax (0.0f, 1.0f - u * 5.0f)));
        g.fillAll();

        const juce::Colour greens[3] = { juce::Colour (90, 180, 60), juce::Colour (130, 200, 70), juce::Colour (60, 140, 50) };
        for (int i = 0; i < 100; ++i)
        {
            const float a = (t - R (i, 0) * 0.6f) / 0.9f;
            if (a < 0.0f || a > 1.0f)
                continue;

            const float x = -40.0f + a * (W + 120.0f) * (0.8f + 0.4f * R (i, 2));
            const float y = R (i, 1) * H * 0.9f + std::sin (a * 9.0f + (float) i) * 40.0f + a * 120.0f;
            const float s = 16.0f + R (i, 3) * 18.0f;
            const auto col = greens[i % 3];

            g.setColour (col.withAlpha ((1.0f - a) * 0.4f));
            g.drawLine (x - 70.0f, y - 8.0f, x, y, 3.0f);

            juce::Path leaf;
            leaf.addEllipse (-s, -s * 0.5f, s * 2.0f, s);
            g.setColour (col.withAlpha (1.0f - a * a));
            g.fillPath (leaf, juce::AffineTransform::rotation (a * 10.0f + (float) i).translated (x, y));
        }
    }
    else if (kind == 6)   // bulles d'eau : Carapuce
    {
        g.setColour (juce::Colour::fromFloatRGBA (0.04f, 0.24f, 0.55f, 0.32f * pulse));
        g.fillAll();

        for (int k = 0; k < 6; ++k)   // rayons de lumière sous l'eau
            fillRay (g, W * (0.1f + 0.16f * (float) k) + std::sin (t * 1.5f + (float) k) * 30.0f, -40.0f,
                     pi * 0.5f + std::sin (t * 1.2f + (float) k * 2.0f) * 0.12f, H * 1.1f, 0.05f,
                     juce::Colour::fromFloatRGBA (0.75f, 0.9f, 1.0f, 0.09f * pulse));

        for (int i = 0; i < 90; ++i)
        {
            const float te = R (i, 0) * 0.8f, life = 1.0f + R (i, 4) * 0.5f, a = (t - te) / life;
            if (a < 0.0f || a > 1.0f)
                continue;

            const float r = 10.0f + R (i, 3) * R (i, 3) * 55.0f;
            const float x = R (i, 1) * W + std::sin (a * 6.0f + (float) i) * (14.0f + r * 0.3f);
            const float y = H + r - a * (H + 2.0f * r) * (0.75f + 0.35f * R (i, 2));

            if (a <= 0.85f)   // la bulle
            {
                const float wob = 1.0f + 0.05f * std::sin (t * 9.0f + (float) i);
                g.saveState();
                g.addTransform (juce::AffineTransform::scale (1.0f, wob, x, y));

                juce::ColourGradient bg (juce::Colour::fromFloatRGBA (0.78f, 0.9f, 1.0f, 0.05f), x, y,
                                         juce::Colour::fromFloatRGBA (0.59f, 0.82f, 1.0f, 0.5f), x + r, y, true);
                bg.addColour (0.8, juce::Colour::fromFloatRGBA (0.51f, 0.76f, 1.0f, 0.22f));
                g.setGradientFill (bg);
                g.fillEllipse (x - r, y - r, r * 2.0f, r * 2.0f);

                g.setColour (juce::Colour::fromFloatRGBA (0.88f, 0.95f, 1.0f, 0.85f));
                g.drawEllipse (x - r, y - r, r * 2.0f, r * 2.0f, 2.0f);

                g.setColour (juce::Colours::white.withAlpha (0.9f));   // reflet
                juce::Path hl;
                hl.addEllipse (-r * 0.2f, -r * 0.12f, r * 0.4f, r * 0.24f);
                g.fillPath (hl, juce::AffineTransform::rotation (-0.6f).translated (x - r * 0.36f, y - r * 0.4f));

                juce::Path arc;
                arc.addCentredArc (x, y, r * 0.72f, r * 0.72f, 0.0f, 0.5f + juce::MathConstants<float>::halfPi, 1.2f + juce::MathConstants<float>::halfPi, true);
                g.setColour (juce::Colours::white.withAlpha (0.5f));
                g.strokePath (arc, juce::PathStrokeType (2.0f));
                g.restoreState();
            }
            else   // la bulle éclate : anneau et gouttelettes
            {
                const float pa = (a - 0.85f) / 0.15f;
                g.setColour (juce::Colour::fromFloatRGBA (0.88f, 0.95f, 1.0f, 1.0f - pa));
                const float rr = r * (1.0f + pa * 0.8f);
                g.drawEllipse (x - rr, y - rr, rr * 2.0f, rr * 2.0f, 3.0f * (1.0f - pa) + 1.0f);

                for (int k = 0; k < 6; ++k)
                {
                    const float an = (float) k * pi / 3.0f + (float) i, l = r * (1.0f + pa * 1.4f), dr = 2.0f + r * 0.06f * (1.0f - pa);
                    g.fillEllipse (x + std::cos (an) * l - dr, y + std::sin (an) * l - dr, dr * 2.0f, dr * 2.0f);
                }
            }
        }
    }
    else if (kind == 7)   // déferlante psychique : Mewtwo
    {
        const float pu = pulse * (0.6f + 0.4f * std::sin (t * 30.0f));
        g.setColour (juce::Colour::fromFloatRGBA (1.0f, 0.39f, 0.78f, 0.24f * pu));
        g.fillAll();

        for (int k = 0; k < 10; ++k)
            fillRay (g, PX, PY, (float) k * juce::MathConstants<float>::twoPi / 10.0f + t * 1.5f, 700.0f, 0.05f,
                     juce::Colour::fromFloatRGBA (0.78f, 0.47f, 1.0f, 0.18f * pu));

        for (int k = 0; k < 6; ++k)
        {
            const float a = (t - (float) k * 0.17f) / 1.0f;
            if (a < 0.0f || a > 1.0f)
                continue;

            const float r = a * W, lw = 22.0f * (1.0f - a) + 2.0f;
            g.setColour (juce::Colour::fromFloatRGBA (1.0f, 0.47f, 0.78f, (1.0f - a) * 0.8f));
            g.drawEllipse (PX - r, PY - r, r * 2.0f, r * 2.0f, lw);
            g.setColour (juce::Colour::fromFloatRGBA (0.59f, 0.39f, 0.9f, (1.0f - a) * 0.7f));
            g.drawEllipse (PX - r * 0.92f, PY - r * 0.92f, r * 1.84f, r * 1.84f, lw * 0.6f);
        }
    }
    else if (kind == 8)   // visage fantôme : Ectoplasma
    {
        fillVignette (g, W, H, std::sin (pi * juce::jmin (u / 0.8f, 1.0f)) * 0.88f, juce::Colour (10, 0, 20), 0.3f);

        for (int i = 0; i < 26; ++i)
        {
            const float a = (t - R (i, 0) * 0.6f) / 1.1f;
            if (a < 0.0f || a > 1.0f)
                continue;

            const float r = 30.0f + R (i, 2) * 50.0f, x = R (i, 1) * W + std::sin (a * 4.0f + (float) i) * 30.0f, y = H - a * H * 0.95f;
            g.setColour (juce::Colour::fromFloatRGBA (0.43f, 0.27f, 0.67f, std::sin (pi * a) * 0.32f));
            g.fillEllipse (x - r, y - r * 1.2f, r * 2.0f, r * 2.4f);
        }

        const float fa = smooth01 (0.1f, 0.3f, u) * (1.0f - smooth01 (0.6f, 0.85f, u)) * (0.75f + 0.25f * std::sin (t * 40.0f));
        if (fa > 0.01f)
        {
            g.setColour (juce::Colour::fromFloatRGBA (0.02f, 0.0f, 0.06f, 0.5f * fa));
            g.fillAll();

            for (int side = -1; side <= 1; side += 2)   // yeux rouges
            {
                const float ex = W * 0.5f + (float) side * W * 0.12f, ey = H * 0.34f;
                g.saveState();
                g.addTransform (juce::AffineTransform::rotation (-(float) side * 0.35f, ex, ey));

                juce::ColourGradient gl (juce::Colour::fromFloatRGBA (1.0f, 0.24f, 0.24f, 0.5f * fa), ex, ey,
                                         juce::Colour::fromFloatRGBA (1.0f, 0.24f, 0.24f, 0.0f), ex + 100.0f, ey, true);
                g.setGradientFill (gl);
                g.fillEllipse (ex - 100.0f, ey - 100.0f, 200.0f, 200.0f);
                g.setColour (juce::Colour::fromFloatRGBA (1.0f, 0.27f, 0.27f, fa));
                g.fillEllipse (ex - 58.0f, ey - 28.0f, 116.0f, 56.0f);
                g.setColour (juce::Colour::fromFloatRGBA (1.0f, 0.86f, 0.78f, fa));
                g.fillEllipse (ex - 20.0f, ey - 11.0f, 40.0f, 22.0f);
                g.restoreState();
            }

            juce::Path grin;   // grand sourire
            grin.startNewSubPath (W * 0.26f, H * 0.5f);
            grin.quadraticTo (W * 0.5f, H * 0.6f, W * 0.74f, H * 0.5f);
            grin.quadraticTo (W * 0.5f, H * 1.0f, W * 0.26f, H * 0.5f);
            grin.closeSubPath();
            g.setColour (juce::Colours::white.withAlpha (fa * 0.92f));
            g.fillPath (grin);

            g.setColour (juce::Colour::fromFloatRGBA (0.08f, 0.0f, 0.16f, fa));   // dents
            for (int k = 1; k < 9; ++k)
            {
                const float q = (float) k / 9.0f, x = W * (0.26f + 0.48f * q), e = 2.0f * q * (1.0f - q);
                g.drawLine (x, H * (0.5f + 0.1f * e) + 2.0f, x, H * (0.5f + 0.5f * e) - 2.0f, 5.0f);
            }
        }
    }
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
    const juce::Colour fill = night ? juce::Colour (0xfff7d02c) : kInk;
    const juce::Colour line = night ? juce::Colour (0xffffe27a) : kInk;
    const juce::Colour txtOn = night ? juce::Colour (0xff14112c) : juce::Colours::white;
    const juce::Colour txtOff = night ? juce::Colour (0xffeeeaff) : kInk;

    if (on)
    {
        g.setColour (fill);
        g.fillRoundedRectangle (r, r.getHeight() * 0.5f);
    }
    else
    {
        g.setColour (line.withAlpha (night ? 0.85f : 1.0f));
        g.drawRoundedRectangle (r, r.getHeight() * 0.5f, 2.5f);
    }

    g.setColour (on ? txtOn : txtOff);
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
// Page ANALYSER : la nuit de Noctali
//==============================================================================
namespace
{
    const char* const kEnNotes[12] = { "C", "C#", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B" };

    // palette de nuit
    const juce::Colour kNSky0 (0xff070818), kNSky1 (0xff14153a), kNSky2 (0xff241a55);
    const juce::Colour kNGold (0xfff7d02c), kNGoldPale (0xffffe27a), kNOrange (0xffffb347);
    const juce::Colour kNLav (0xffc8c4ff);                      // texte lavande
    const juce::Colour kNText (0xffeeeaff);                     // texte principal clair
    const juce::Colour kNPanel = juce::Colour (0xff0a0a22).withAlpha (0.55f);

    // générateur déterministe : le décor est identique à chaque ouverture
    struct NRand
    {
        juce::uint32 s;
        explicit NRand (juce::uint32 seed) : s (seed) {}
        float next() { s = s * 1664525u + 1013904223u; return (float) (s >> 8) / 16777216.0f; }
    };

    // sapin stylisé en 5 étages
    void addPine (juce::Path& p, float x, float base, float h, float w)
    {
        constexpr int tiers = 5;
        p.startNewSubPath (x, base - h);
        for (int i = 1; i <= tiers; ++i)
        {
            const float y = base - h + h * (float) i / tiers * 0.95f;
            const float ww = w * (float) i / tiers * 0.5f;
            p.lineTo (x + ww, y);
            p.lineTo (x + ww * 0.55f, y);
        }
        p.lineTo (x + w * 0.08f, base);
        p.lineTo (x - w * 0.08f, base);
        for (int i = tiers; i >= 1; --i)
        {
            const float y = base - h + h * (float) i / tiers * 0.95f;
            const float ww = w * (float) i / tiers * 0.5f;
            p.lineTo (x - ww * 0.55f, y);
            p.lineTo (x - ww, y);
        }
        p.closeSubPath();
    }

    // géométrie de la page
    constexpr float kMoonX = 265.0f, kMoonY = 306.0f, kMoonR = 164.0f;
    const juce::Rectangle<float> kPanelChord (500.0f, 110.0f, 464.0f, 426.0f);
    const juce::Rectangle<float> kPanelHist (28.0f, 548.0f, 944.0f, 118.0f);
    const juce::Rectangle<float> kRollPanel (530.0f, 384.0f, 404.0f, 128.0f);
    constexpr int kRollLo = 36, kRollHi = 72;
    constexpr float kRollY = 390.0f, kRollH = 116.0f, kRollKeysX = 556.0f, kRollCol0 = 560.0f;
    constexpr float kRollColW = (404.0f - 30.0f) / 7.0f;
    constexpr int kHistShown = 7;
}

AnalyserPage::AnalyserPage (PokeChordAudioProcessor& p) : proc (p), dragZone (p)
{
    listenButton.setNight (true);
    dragZone.setNight (true);
    clearButton.getProperties().set ("night", true);

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
    listenButton.setBounds (36, 676, 210, 38);
    dragZone.setBounds (258, 676, 552, 38);
    clearButton.setBounds (822, 676, 142, 38);
    buildBackground();
}

void AnalyserPage::buildRingGlow()
{
    ringMask = juce::Image();
    ringGlow = juce::Image();
    if (! noctali.isValid())
        return;

    // 1) repère les pixels jaune vif de l'illustration : ce sont les anneaux
    const int W = noctali.getWidth(), H = noctali.getHeight();
    ringMask = juce::Image (juce::Image::ARGB, W, H, true);
    {
        juce::Image::BitmapData src (noctali, juce::Image::BitmapData::readOnly);
        juce::Image::BitmapData dst (ringMask, juce::Image::BitmapData::readWrite);
        const juce::Colour bright (0xffffeea0);
        for (int y = 0; y < H; ++y)
            for (int x = 0; x < W; ++x)
            {
                const juce::Colour c = src.getPixelColour (x, y);
                float h = 0.0f, s = 0.0f, b = 0.0f;
                c.getHSB (h, s, b);
                if (h < 0.09f || h > 0.20f)
                    continue;
                const float w = juce::jlimit (0.0f, 1.0f, (s - 0.35f) * 2.5f)
                              * juce::jlimit (0.0f, 1.0f, (b - 0.45f) * 3.0f)
                              * c.getFloatAlpha();
                if (w > 0.02f)
                    dst.setPixelColour (x, y, bright.withAlpha (w));
            }
    }

    // 2) halo : le même masque en plus petit, puis flou
    const int gw = juce::jmax (8, W / 4), gh = juce::jmax (8, H / 4);
    juce::Image small = ringMask.rescaled (gw, gh, juce::Graphics::mediumResamplingQuality);
    ringGlow = juce::Image (juce::Image::ARGB, gw, gh, true);
    juce::ImageConvolutionKernel kernel (13);
    kernel.createGaussianBlur (4.0f);
    kernel.applyToImage (ringGlow, small, ringGlow.getBounds());
}

void AnalyserPage::refresh()
{
    listenButton.setOn (proc.isAnalysing());

    const int v = proc.getAnalysisVersion();
    if (v != shownVersion)
    {
        shownVersion = v;
        dragZone.repaint();
    }
}

void AnalyserPage::tick (double nowMs)
{
    now = nowMs;
    const bool on = proc.isAnalysing();
    const float lvl = on ? proc.getInputLevel() : 0.0f;

    // enveloppes : env suit le niveau, envSlow sa moyenne ; beat réagit aux attaques
    env = juce::jmax (lvl, env * 0.92f);
    envSlow = envSlow * 0.97f + lvl * 0.03f;
    const float onset = juce::jlimit (0.0f, 1.0f, (lvl - envSlow * 1.1f) * 5.0f);
    beat = juce::jmax (beat * 0.90f, onset);

    if (on && beat > 0.5f && now - lastRipple > 420.0)
    {
        ripples.push_back (now);
        lastRipple = now;
    }
    while (! ripples.empty() && now - ripples.front() > 2400.0)
        ripples.erase (ripples.begin());

    if (on && shootStart < 0.0 && rng.nextFloat() < 0.004f)
    {
        shootStart = now;
        shootX = 560.0f + rng.nextFloat() * 300.0f;
        shootY = 40.0f + rng.nextFloat() * 80.0f;
    }
    if (shootStart >= 0.0 && now - shootStart > 700.0)
        shootStart = -1.0;

    if ((++frameCount & 1) == 0)   // 30 images par seconde suffisent
        repaint();
}

//------------------------------------------------------------------------------
// Décor fixe : ciel, forêt de sapins, panneaux de verre. Rendu une fois, en 2x.
void AnalyserPage::buildBackground()
{
    const int w = juce::jmax (1, getWidth()), h = juce::jmax (1, getHeight());
    bgCache = juce::Image (juce::Image::ARGB, w * 2, h * 2, true);
    juce::Graphics g (bgCache);
    g.addTransform (juce::AffineTransform::scale (2.0f));

    // ciel
    {
        juce::ColourGradient sky (kNSky0, 0.0f, 0.0f, kNSky2, 0.0f, (float) h, false);
        sky.addColour (0.55, kNSky1);
        g.setGradientFill (sky);
        g.fillRect (0, 0, w, h);
    }

    // trois plans de sapins, du plus clair au plus sombre
    const juce::Colour cols[3] = { juce::Colour (0xff10133a), juce::Colour (0xff0b0e2c), juce::Colour (0xff070a1f) };
    const float bases[3] = { 470.0f, 520.0f, 600.0f };
    const int counts[3] = { 26, 20, 15 };
    const float hmin[3] = { 110.0f, 150.0f, 200.0f };
    const float hmax[3] = { 200.0f, 260.0f, 330.0f };
    const float wid[3] = { 46.0f, 60.0f, 80.0f };

    for (int l = 0; l < 3; ++l)
    {
        NRand r (7919u * (juce::uint32) (l + 3));
        g.setColour (cols[l]);
        g.fillRect (0.0f, bases[l] - 2.0f, (float) w, (float) h - bases[l] + 2.0f);
        juce::Path trees;
        for (int i = 0; i < counts[l]; ++i)
        {
            const float x = (float) i * (float) w / (float) counts[l] + r.next() * 20.0f;
            const float th = hmin[l] + r.next() * (hmax[l] - hmin[l]);
            const float tw = wid[l] * (0.8f + r.next() * 0.5f);
            addPine (trees, x, bases[l], th, tw);
        }
        g.fillPath (trees);
    }

    // panneaux de verre sombre
    auto panel = [&g] (juce::Rectangle<float> r, float corner)
    {
        g.setColour (kNPanel);
        g.fillRoundedRectangle (r, corner);
        g.setColour (kNGold.withAlpha (0.22f));
        g.drawRoundedRectangle (r, corner, 1.2f);
    };
    panel (kPanelChord, 22.0f);
    panel (kPanelHist, 22.0f);

    // petit piano roll : cadre, clavier et lignes, le contenu est dessiné en direct
    g.setColour (juce::Colour (0xff06061a).withAlpha (0.55f));
    g.fillRoundedRectangle (kRollPanel, 12.0f);
    g.setColour (kNGold.withAlpha (0.18f));
    g.drawRoundedRectangle (kRollPanel, 12.0f, 1.0f);

    const float rowH = kRollH / (float) (kRollHi - kRollLo);
    for (int n = kRollLo; n < kRollHi; ++n)
    {
        const int pc = n % 12;
        const bool blk = (pc == 1 || pc == 3 || pc == 6 || pc == 8 || pc == 10);
        const float y = kRollY + kRollH - (float) (n - kRollLo + 1) * rowH;

        if (blk)
        {
            g.setColour (juce::Colours::white.withAlpha (0.035f));
            g.fillRect (kRollKeysX, y, kRollPanel.getRight() - 4.0f - kRollKeysX, rowH);
        }
        g.setColour (blk ? juce::Colour (0xff1e1c50) : kNLav.withAlpha (0.16f));
        g.fillRect (kRollPanel.getX() + 2.0f, y + 0.5f, blk ? 14.0f : 24.0f, rowH - 1.0f);

        if (pc == 0)
        {
            g.setColour (kNLav.withAlpha (0.5f));
            g.setFont (F (8.0f));
            g.drawText ("C" + juce::String (n / 12 - 1),
                        juce::Rectangle<float> (kRollPanel.getX() + 5.0f, y - 4.0f, 20.0f, 10.0f),
                        juce::Justification::centredLeft, false);
            g.setColour (juce::Colours::white.withAlpha (0.09f));
            g.fillRect (kRollKeysX, y + rowH - 0.5f, kRollPanel.getRight() - 4.0f - kRollKeysX, 1.0f);
        }
    }
    for (int i = 0; i <= kHistShown; ++i)
    {
        g.setColour (juce::Colours::white.withAlpha (0.06f));
        g.fillRect (kRollCol0 + kRollColW * (float) i - 0.5f, kRollY, 1.0f, kRollH);
    }

    // titres fixes
    g.setColour (kNLav.withAlpha (0.6f));
    g.setFont (F (11.0f));
    g.drawText ("PIANO ROLL", 530, 364, 200, 12, juce::Justification::centredLeft, false);
    g.drawText ("HISTORIQUE", 56, 560, 200, 12, juce::Justification::centredLeft, false);
}

//------------------------------------------------------------------------------
void AnalyserPage::paint (juce::Graphics& g)
{
    const bool on = proc.isAnalysing();
    const auto& live = proc.getLiveChord();
    const auto& chroma = proc.getLiveChroma();
    const auto& hist = proc.getHistory();
    const float t = (float) std::fmod (now, 1.0e7);

    if (bgCache.isValid())
    {
        g.setOpacity (1.0f);
        g.drawImage (bgCache, 0, 0, getWidth(), getHeight(), 0, 0, bgCache.getWidth(), bgCache.getHeight());
    }

    // ---- étoiles qui scintillent
    {
        NRand r (11u);
        for (int i = 0; i < 90; ++i)
        {
            const float x = r.next() * 1000.0f, y = r.next() * 430.0f;
            const float sz = r.next() * 1.5f + 0.4f, ph = r.next();
            const float a = 0.3f + 0.7f * std::fabs (std::sin (t / 900.0f * ph * 2.0f + x));
            g.setColour (juce::Colour (0xfffff4c8).withAlpha (a * 0.75f));
            g.fillEllipse (x - sz * 0.8f, y - sz * 0.8f, sz * 1.6f, sz * 1.6f);
        }
    }

    // ---- aurores pendant l'écoute
    if (on)
    {
        for (int k = 0; k < 3; ++k)
        {
            juce::Path a;
            a.startNewSubPath (0.0f, 70.0f + (float) k * 50.0f + std::sin (t / 1400.0f + (float) k * 2.0f) * 38.0f);
            for (int x = 20; x <= 1000; x += 20)
                a.lineTo ((float) x, 70.0f + (float) k * 50.0f
                                    + std::sin ((float) x / 130.0f + t / 1400.0f + (float) k * 2.0f) * 38.0f
                                    + std::sin ((float) x / 57.0f - t / 900.0f) * 12.0f);
            const juce::Colour ac = (k % 2 == 1) ? juce::Colour (0x1a6e46dc) : juce::Colour (0x1428a0c8);
            g.setColour (ac);
            g.strokePath (a, juce::PathStrokeType (46.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }
    }

    // ---- étoile filante
    if (shootStart >= 0.0)
    {
        const float k = (float) ((now - shootStart) / 700.0);
        const float x = shootX - k * 260.0f, y = shootY + k * 120.0f;
        for (int i = 0; i < 3; ++i)
        {
            const float f0 = (float) i / 3.0f, f1 = (float) (i + 1) / 3.0f;
            g.setColour (juce::Colour (0xfffff0b4).withAlpha (0.9f * (1.0f - f0) * (1.0f - k)));
            g.drawLine (x + 90.0f * f0, y - 40.0f * f0, x + 90.0f * f1, y - 40.0f * f1, 2.0f);
        }
    }

    // ---- halo de lune, qui respire avec le son
    {
        const float hr = 300.0f + 40.0f * env;
        juce::ColourGradient halo (kNGold.withAlpha (0.26f + 0.2f * beat), kMoonX, kMoonY,
                                   kNGold.withAlpha (0.0f), kMoonX + hr, kMoonY, true);
        g.setGradientFill (halo);
        g.fillEllipse (kMoonX - hr, kMoonY - hr, hr * 2.0f, hr * 2.0f);
    }

    // ---- brume qui dérive devant les sapins
    {
        NRand r (23u);
        for (int i = 0; i < 4; ++i)
        {
            const float o = r.next() * 1000.0f;
            const float sp = 0.012f + r.next() * 0.02f;
            const float y = 390.0f + (float) i * 55.0f;
            const float x = std::fmod (o + t * sp * (on ? 2.4f : 1.0f), 1600.0f) - 300.0f;
            juce::ColourGradient m (kNLav.withAlpha (on ? 0.10f : 0.05f), x, y,
                                    kNLav.withAlpha (0.0f), x + 360.0f, y, true);
            g.setGradientFill (m);
            g.fillEllipse (x - 380.0f, y - 60.0f, 760.0f, 120.0f);
        }
    }

    // ---- ondes dorées émises par la lune sur les attaques
    for (double r0 : ripples)
    {
        const float k = (float) ((now - r0) / 2400.0);
        const float rr = 170.0f + k * 520.0f;
        g.setColour (kNGold.withAlpha (0.5f * (1.0f - k)));
        g.drawEllipse (kMoonX - rr, kMoonY - rr * 0.92f, rr * 2.0f, rr * 1.84f, 3.0f * (1.0f - k) + 1.0f);
    }

    // ---- lucioles
    {
        NRand r (5u);
        for (int i = 0; i < 34; ++i)
        {
            const float bx = r.next() * 1000.0f, by = 300.0f + r.next() * 380.0f;
            const float ph = r.next() * 6.28f, sp = 0.3f + r.next() * 0.7f, rad = 1.2f + r.next() * 1.8f;
            const float x = bx + std::sin (t / 1700.0f * sp + ph) * 26.0f;
            const float y = by + std::cos (t / 1300.0f * sp + ph * 2.0f) * 18.0f
                          - (on ? 10.0f * std::sin (t / 600.0f + ph) : 0.0f);
            const float bl = on ? 0.45f + 0.55f * std::fabs (std::sin (t / (400.0f + 200.0f * sp) + ph))
                               : 0.25f + 0.25f * std::fabs (std::sin (t / 1500.0f + ph));
            const float rr = rad * (1.0f + (on ? beat * 0.6f : 0.0f));

            g.setColour (juce::Colour (0xffffe882).withAlpha (bl * 0.10f));
            g.fillEllipse (x - rr * 5.0f, y - rr * 5.0f, rr * 10.0f, rr * 10.0f);
            g.setColour (juce::Colour (0xffffe882).withAlpha (bl * 0.25f));
            g.fillEllipse (x - rr * 2.2f, y - rr * 2.2f, rr * 4.4f, rr * 4.4f);
            g.setColour (juce::Colour (0xfffffad2).withAlpha (bl));
            g.fillEllipse (x - rr * 0.7f, y - rr * 0.7f, rr * 1.4f, rr * 1.4f);
        }
    }

    // ---- la lune et Noctali
    {
        juce::ColourGradient disc (juce::Colour (0xff2e2d6c), kMoonX - 40.0f, kMoonY - 50.0f,
                                   juce::Colour (0xff15153d), kMoonX + kMoonR, kMoonY + kMoonR, true);
        g.setGradientFill (disc);
        g.fillEllipse (kMoonX - kMoonR, kMoonY - kMoonR, kMoonR * 2.0f, kMoonR * 2.0f);

        {
            juce::Graphics::ScopedSaveState ss (g);
            juce::Path clip;
            clip.addEllipse (kMoonX - kMoonR + 2.0f, kMoonY - kMoonR + 2.0f, kMoonR * 2.0f - 4.0f, kMoonR * 2.0f - 4.0f);
            g.reduceClipRegion (clip, juce::AffineTransform());
            if (noctali.isValid())
            {
                g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
                g.setOpacity (1.0f);   // drawImage reprend l'opacité de la dernière couleur : on la remet à 100 %
                g.drawImage (noctali, juce::Rectangle<float> (kMoonX - 125.0f, kMoonY - 190.0f, 214.0f, 374.0f));
            }
            // les vrais anneaux de Noctali (front, oreilles, pattes) brillent au rythme du son
            if (on && ringMask.isValid())
            {
                const juce::Rectangle<float> dst (kMoonX - 125.0f, kMoonY - 190.0f, 214.0f, 374.0f);
                const float a = juce::jlimit (0.0f, 1.0f, 0.10f + 0.90f * beat);
                if (ringGlow.isValid())
                {
                    g.setOpacity (a * 0.95f);
                    g.drawImage (ringGlow, dst.expanded (14.0f));
                }
                g.setOpacity (a);
                g.drawImage (ringMask, dst);
            }
        }

        g.setColour (kNGold.withAlpha (0.55f + 0.4f * beat));
        g.drawEllipse (kMoonX - kMoonR, kMoonY - kMoonR, kMoonR * 2.0f, kMoonR * 2.0f, 2.0f + beat * 2.0f);
    }

    // ---- les 12 notes autour de la lune
    for (int i = 0; i < 12; ++i)
    {
        const float a = -juce::MathConstants<float>::halfPi + (float) i * juce::MathConstants<float>::pi / 6.0f;
        bool inChord = false;
        if (live.valid)
            for (int pc : live.pcs)
                inChord |= (pc == i);

        const float v = on ? juce::jlimit (0.0f, 1.0f, chroma[(size_t) i]) : 0.0f;
        const float len = 8.0f + 48.0f * v;
        const float r0 = kMoonR + 22.0f;
        const float ca = std::cos (a), sa = std::sin (a);

        juce::Path bar;
        bar.startNewSubPath (kMoonX + ca * r0, kMoonY + sa * r0);
        bar.lineTo (kMoonX + ca * (r0 + len), kMoonY + sa * (r0 + len));

        const bool lit = inChord && on;
        if (lit)
        {
            g.setColour (kNGold.withAlpha (0.30f));
            g.strokePath (bar, juce::PathStrokeType (18.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }
        g.setColour (lit ? kNGold : kNLav.withAlpha (0.40f));
        g.strokePath (bar, juce::PathStrokeType (12.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        g.setColour (lit ? kNGold : kNLav.withAlpha (0.55f));
        g.setFont (F (11.0f));
        g.drawText (kEnNotes[i],
                    juce::Rectangle<float> (kMoonX + ca * (r0 + len + 16.0f) - 14.0f,
                                            kMoonY + sa * (r0 + len + 16.0f) - 7.0f, 28.0f, 14.0f),
                    juce::Justification::centred, false);
    }

    // ---- panneau de droite : l'accord entendu
    g.setColour (kNLav.withAlpha (0.65f));
    g.setFont (F (11.0f));
    g.drawText (on ? "ACCORD ENTENDU" : "EN PAUSE", 530, 132, 300, 14, juce::Justification::centredLeft, false);

    g.setColour (live.valid ? kNGoldPale : kNGoldPale.withAlpha (0.28f));
    g.setFont (F (92.0f));
    g.drawFittedText (live.valid ? PokeChordAudioProcessor::fromUtf8 (live.name) : juce::String (U ("\xe2\x80\x94")),
                      juce::Rectangle<int> (528, 150, 420, 110), juce::Justification::centredLeft, 1, 0.5f);

    juce::String info;
    if (live.valid)
    {
        for (size_t i = 0; i < live.pcs.size(); ++i)
            info << (i ? U (" \xc2\xb7 ") : juce::String()) << kEnNotes[live.pcs[i] % 12];
    }
    else
        info = on ? U ("EN ATTENTE D'AUDIO SUR LA PISTE") : U ("Appuie sur \xc3\x89" "COUTER");

    g.setColour (kNLav.withAlpha (0.8f));
    g.setFont (F (14.0f, false));
    g.drawText (info, 530, 288, 420, 18, juce::Justification::centredLeft, false);

    // jauge de confiance
    g.setColour (juce::Colours::white.withAlpha (0.1f));
    g.fillRoundedRectangle (530.0f, 322.0f, 404.0f, 7.0f, 3.5f);
    if (live.valid)
    {
        g.setColour (kNGold);
        g.fillRoundedRectangle (530.0f, 322.0f, 404.0f * juce::jlimit (0.0f, 1.0f, live.confidence), 7.0f, 3.5f);
        g.setColour (kNLav.withAlpha (0.6f));
        g.setFont (F (11.0f));
        g.drawText ("CONFIANCE " + juce::String ((int) std::lround (live.confidence * 100.0f)) + "%",
                    530, 340, 200, 14, juce::Justification::centredLeft, false);
    }

    // petit égaliseur pendant l'écoute
    if (on)
        for (int i = 0; i < 24; ++i)
        {
            const float hh = 4.0f + 22.0f * std::fabs (std::sin (t / 190.0f + (float) i * 0.7f)) * juce::jmax (env, 0.15f);
            g.setColour (kNGold.withAlpha (0.7f));
            g.fillRoundedRectangle (742.0f + (float) i * 8.0f, 354.0f - hh, 4.5f, hh, 2.0f);
        }

    // ---- contenu du petit piano roll : les derniers accords détectés
    {
        const int shown = juce::jmin (kHistShown, (int) hist.size());
        const int first = (int) hist.size() - shown;
        const float rowH = kRollH / (float) (kRollHi - kRollLo);
        const int act = on ? (int) std::fmod (t / 1000.0 * 1.4, (double) kHistShown) : -1;

        juce::Graphics::ScopedSaveState ss (g);
        juce::Path clip;
        clip.addRoundedRectangle (kRollPanel, 12.0f);
        g.reduceClipRegion (clip, juce::AffineTransform());

        for (int i = 0; i < shown; ++i)
        {
            const auto& d = hist[(size_t) (first + i)];
            const bool cur = (i == shown - 1);
            const bool hot = (i == act);
            const float x = kRollCol0 + kRollColW * (float) i + 2.0f;

            auto cell = [&] (int note, bool isBass)
            {
                if (note < kRollLo || note >= kRollHi)
                    return;
                const float y = kRollY + kRollH - (float) (note - kRollLo + 1) * rowH;
                juce::Colour c = isBass ? kNOrange : kNGold;
                if (hot) c = kNGoldPale;
                g.setColour (c.withAlpha (cur || hot ? 1.0f : 0.55f));
                g.fillRoundedRectangle (x, y + 0.6f, kRollColW - 4.0f, juce::jmax (2.0f, rowH - 1.2f), 2.0f);
            };

            cell (36 + (d.bass >= 0 ? d.bass : d.root), true);
            for (int pc : d.pcs)
                cell (60 + ((pc - d.root + 12) % 12), false);
        }

        if (on)
        {
            const float px = kRollCol0 + (float) std::fmod (t / 1000.0 * 1.4, (double) kHistShown) * kRollColW;
            g.setColour (kNGold.withAlpha (0.16f));
            g.fillRect (px - 30.0f, kRollY, 30.0f, kRollH);
            g.setColour (kNGoldPale.withAlpha (0.85f));
            g.fillRect (px, kRollY, 1.6f, kRollH);
        }
    }

    // ---- historique : un anneau de Noctali par accord
    if (hist.empty())
    {
        g.setColour (kNLav.withAlpha (0.5f));
        g.setFont (F (13.0f, false));
        g.drawText (U ("Joue de l'audio sur cette piste : les accords d\xc3\xa9tect\xc3\xa9s apparaissent ici."),
                    28, 596, 944, 40, juce::Justification::centred, false);
    }
    else
    {
        const int shown = juce::jmin (kHistShown, (int) hist.size());
        const int first = (int) hist.size() - shown;
        const float cw = 900.0f / (float) kHistShown;
        const double bpm = juce::jlimit (20.0, 300.0, proc.getBpm());
        const double t0 = hist.front().timeSec;

        for (int i = 0; i < shown; ++i)
        {
            const auto& d = hist[(size_t) (first + i)];
            const float x = 56.0f + cw * (float) i + cw * 0.5f;
            const float y = 614.0f;
            const bool last = (i == shown - 1);

            if (last)
            {
                g.setColour (kNGold.withAlpha (0.25f + 0.3f * beat));
                g.drawEllipse (x - 34.0f, y - 41.0f, 68.0f, 82.0f, 8.0f);
            }
            g.setColour (last ? kNGold : kNGold.withAlpha (0.35f));
            g.drawEllipse (x - 30.0f, y - 37.0f, 60.0f, 74.0f, last ? 4.0f : 2.5f);

            g.setColour (last ? kNGoldPale : kNText.withAlpha (0.85f));
            g.setFont (F (21.0f));
            g.drawFittedText (PokeChordAudioProcessor::fromUtf8 (d.name),
                              juce::Rectangle<int> ((int) (x - 55.0f), (int) (y - 14.0f), 110, 28),
                              juce::Justification::centred, 1, 0.6f);

            const int bar = 1 + (int) std::floor ((d.timeSec - t0) * bpm / 60.0 / 4.0);
            g.setColour (kNLav.withAlpha (0.5f));
            g.setFont (F (10.0f));
            g.drawText ("mes. " + juce::String (bar),
                        juce::Rectangle<float> (x - 40.0f, y + 40.0f, 80.0f, 12.0f),
                        juce::Justification::centred, false);
        }
    }

    // ---- en-tête : logo, titre et onglets, par-dessus la nuit
    if (logo.isValid())
    {
        g.setOpacity (1.0f);
        g.drawImage (logo, juce::Rectangle<float> (36.0f, 22.0f, 34.0f, 34.0f));
    }
    g.setColour (kNText);
    g.setFont (F (24.0f));
    g.drawText (U ("POKÉCHORD"), 80, 22, 300, 34, juce::Justification::centredLeft, false);

    {
        const char* names[2] = { "GÉNÉRER", "ANALYSER" };
        const juce::Rectangle<int> rects[2] = { { 748, 26, 108, 30 }, { 868, 26, 96, 30 } };
        g.setFont (F (13.0f));
        for (int i = 0; i < 2; ++i)
        {
            const bool onTab = (i == 1);
            g.setColour (onTab ? kNGoldPale : kNLav.withAlpha (0.55f));
            g.drawText (U (names[i]), rects[i], juce::Justification::centred, false);
            if (onTab)
            {
                g.setColour (kNGold);
                const int tw = juce::GlyphArrangement::getStringWidthInt (F (13.0f), U (names[i]));
                g.fillRoundedRectangle ((float) (rects[i].getCentreX() - tw / 2), (float) rects[i].getBottom(), (float) tw, 4.0f, 2.0f);
            }
        }
    }
}
