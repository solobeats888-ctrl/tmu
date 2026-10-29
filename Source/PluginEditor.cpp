#include "PluginEditor.h"
#include <BinaryData.h>

namespace
{
// ---- paleta: violeta, purpura y negro (+ lavanda clara para teclas y etiquetas)
const juce::Colour outerBg    (0xff050406);
const juce::Colour black      (0xff0b090d);
const juce::Colour bodyTop    (0xff0b090d);   // todo negro
const juce::Colour violetHi   (0xffc98cff);
const juce::Colour violet     (0xff8b3dff);
const juce::Colour purple     (0xff5b1a9e);
const juce::Colour purpleDark (0xff2a0a4a);
const juce::Colour screenTxt  (0xffb77dff);
const juce::Colour screenDim  (0xff5a3a86);
const juce::Colour glassBase  (0xff0d0514);
const juce::Colour lavender   (0xffb98cff);   // texto: violeta

constexpr int W = 1000, H = 658;
constexpr int M = 70;            // margen izquierdo / derecho de todo el contenido
constexpr int R = W - M;         // borde derecho
constexpr int HEADER = 76;       // alto de la franja violeta

// limites exactos del dibujo de un texto (sin el espacio extra de la fuente), para alinear al pixel
juce::Rectangle<float> tightBounds (const juce::Font& f, const juce::String& t)
{
    juce::GlyphArrangement ga;
    ga.addLineOfText (f, t, 0.0f, 0.0f);
    return ga.getBoundingBox (0, -1, false);
}

void drawTight (juce::Graphics& g, const juce::Font& f, const juce::String& t, float left, float top)
{
    const auto b = tightBounds (f, t);
    g.setFont (f);
    g.drawSingleLineText (t, juce::roundToInt (left - b.getX()), juce::roundToInt (top - b.getY()));
}

const juce::StringArray& installedFonts()
{
    static const juce::StringArray names = juce::Font::findAllTypefaceNames();
    return names;
}

// firma "viszeversa" en cursiva, igual que rmx_
juce::Font scriptFont (float h)
{
    for (auto* n : { "Snell Roundhand", "Apple Chancery", "Zapfino" })
        if (installedFonts().contains (n))
            return juce::Font (juce::FontOptions (n, h * (juce::String (n) == "Snell Roundhand" ? 1.3f : 1.1f), juce::Font::plain));
    return juce::Font (juce::FontOptions (juce::Font::getDefaultSerifFontName(), h, juce::Font::italic));
}

void glowText (juce::Graphics& g, const juce::String& s, juce::Rectangle<int> r, juce::Justification j, juce::Colour c, float glow = 0.0f)
{
    if (glow > 0)
    {
        g.setColour (c.withAlpha (glow));
        for (auto d : { juce::Point<int> (1, 0), juce::Point<int> (-1, 0), juce::Point<int> (0, 1), juce::Point<int> (0, -1) })
            g.drawText (s, r.translated (d.x, d.y), j, true);
    }
    g.setColour (c);
    g.drawText (s, r, j, true);
}

juce::Path starPath (juce::Point<float> c, float r)
{
    juce::Path p;
    p.addStar (c, 5, r * 0.45f, r, 0.0f);
    return p;
}

void drawStars (juce::Graphics& g, juce::Rectangle<int> area, int stars, juce::Colour on, juce::Colour off, float size = 6.0f)
{
    for (int k = 0; k < 3; ++k)
    {
        const auto c = juce::Point<float> ((float) area.getX() + 7.0f + (float) k * 14.0f, (float) area.getCentreY());
        g.setColour (k < stars ? on : off);
        g.fillPath (starPath (c, size));
    }
}

void drawLed (juce::Graphics& g, juce::Point<float> c, float radius)
{
    juce::ColourGradient glow (violetHi.withAlpha (0.6f), c, violetHi.withAlpha (0.0f), c.translated (radius * 3.5f, 0), true);
    g.setGradientFill (glow);
    g.fillEllipse (juce::Rectangle<float> (radius * 7, radius * 7).withCentre (c));
    g.setColour (juce::Colours::black);
    g.fillEllipse (juce::Rectangle<float> (radius * 2 + 2, radius * 2 + 2).withCentre (c));
    g.setColour (violetHi.brighter (0.3f));
    g.fillEllipse (juce::Rectangle<float> (radius * 2, radius * 2).withCentre (c));
}
} // namespace

//==============================================================================
TMULook::TMULook()
{
    accent = violet;
    // Fuentes de Resources/fonts (van adentro del plugin). Se buscan por nombre de archivo,
    // asi alcanza con poner el .ttf/.otf en la carpeta y volver a compilar.
    auto findFont = [] (const char* namePart) -> juce::Typeface::Ptr
    {
        for (int i = 0; i < BinaryData::namedResourceListSize; ++i)
        {
            const juce::String original (BinaryData::getNamedResourceOriginalFilename (BinaryData::namedResourceList[i]));
            if (original.containsIgnoreCase (namePart) && (original.endsWithIgnoreCase (".ttf") || original.endsWithIgnoreCase (".otf")))
            {
                int size = 0;
                if (auto* data = BinaryData::getNamedResource (BinaryData::namedResourceList[i], size))
                    return juce::Typeface::createSystemTypefaceFor (data, (size_t) size);
            }
        }
        return nullptr;
    };
    press = findFont ("JetBrainsMono-Bold");
    pressHeavy = findFont ("JetBrainsMono-ExtraBold");
    if (pressHeavy == nullptr) pressHeavy = press;
    mainScale = 1.55f;   // JetBrains Mono es mas angosta que la fuente pixel anterior
    sig = findFont ("Kihim");

    setColour (juce::ListBox::backgroundColourId, juce::Colours::transparentBlack);
    setColour (juce::ListBox::outlineColourId, juce::Colours::transparentBlack);
    setColour (juce::TextButton::textColourOffId, lavender);
    setColour (juce::TextButton::textColourOnId, lavender);
    setColour (juce::MidiKeyboardComponent::shadowColourId, juce::Colours::transparentBlack);
    setColour (juce::MidiKeyboardComponent::keySeparatorLineColourId, juce::Colours::transparentBlack);
    setColour (juce::MidiKeyboardComponent::whiteNoteColourId, juce::Colours::transparentBlack);
    setColour (juce::BubbleComponent::backgroundColourId, black);
    setColour (juce::BubbleComponent::outlineColourId, violet);
    setColour (juce::TooltipWindow::textColourId, lavender);
    setColour (juce::TextEditor::textColourId, violetHi);
    setColour (juce::TextEditor::highlightColourId, violet.withAlpha (0.4f));
    setColour (juce::TextEditor::highlightedTextColourId, juce::Colours::white);
    setColour (juce::CaretComponent::caretColourId, violetHi);
}

juce::Font TMULook::pixel (float h) const { return juce::Font (juce::FontOptions (press).withHeight (h * mainScale)); }

juce::Font TMULook::heavy (float h) const { return juce::Font (juce::FontOptions (pressHeavy).withHeight (h)); }

juce::Font TMULook::signature (float h) const
{
    if (sig != nullptr) return juce::Font (juce::FontOptions (sig).withHeight (h));
    return scriptFont (h);
}

void TMULook::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float start, float end, juce::Slider& slider)
{
    auto area = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h);
    const float size = juce::jmin (area.getWidth(), area.getHeight());
    const auto c = area.getCentre();
    const float rOuter = size * 0.5f;
    const bool bipolar = slider.getProperties()["bipolar"];
    const float angle = start + pos * (end - start);

    // aro violeta (arco del valor)
    juce::Path track, value;
    track.addCentredArc (c.x, c.y, rOuter - 3, rOuter - 3, 0, start, end, true);
    const float from = bipolar ? (start + end) * 0.5f : start;
    value.addCentredArc (c.x, c.y, rOuter - 3, rOuter - 3, 0, juce::jmin (from, angle), juce::jmax (from, angle), true);
    g.setColour (purpleDark);
    g.strokePath (track, juce::PathStrokeType (4.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour (violetHi);
    g.strokePath (value, juce::PathStrokeType (4.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    const float rKnob = rOuter - 9.0f;
    auto kr = juce::Rectangle<float> (rKnob * 2, rKnob * 2).withCentre (c);
    g.setColour (juce::Colours::black.withAlpha (0.6f));
    g.fillEllipse (kr.translated (0, 4));
    juce::ColourGradient body (juce::Colour (0xff4a4250), kr.getX(), kr.getY(), black, kr.getRight(), kr.getBottom(), false);
    g.setGradientFill (body);
    g.fillEllipse (kr);
    g.setColour (juce::Colours::white.withAlpha (0.10f));
    g.drawEllipse (kr.reduced (1.0f), 1.0f);
    const juce::Point<float> p0 (c.x + (rKnob * 0.3f) * std::sin (angle), c.y - (rKnob * 0.3f) * std::cos (angle));
    const juce::Point<float> p1 (c.x + (rKnob - 5) * std::sin (angle), c.y - (rKnob - 5) * std::cos (angle));
    g.setColour (violetHi);
    g.drawLine ({ p0, p1 }, 3.0f);
}

void TMULook::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool over, bool down)
{
    const auto style = b.getProperties()["style"].toString();
    auto r = b.getLocalBounds().toFloat().reduced (1.0f);
    const float rad = r.getHeight() * 0.5f;
    const bool on = b.getToggleState();

    if (style == "stripe")   // pill de la barra de arriba: negra con borde violeta; la activa se ilumina
    {
        auto f = r.reduced (0.5f).translated (0, down ? 1.0f : 0.0f);
        g.setColour (on ? violet.withAlpha (0.22f) : (over ? juce::Colour (0xff1a1420) : juce::Colour (0xff110e14)));
        g.fillRoundedRectangle (f, rad);
        g.setColour (on ? violetHi : violet.withAlpha (over ? 0.6f : 0.35f));
        g.drawRoundedRectangle (f, rad, on ? 1.5f : 1.0f);
        return;
    }
    // "body": pill negra con borde violeta, igual que las de arriba
    auto f = r.reduced (0.5f).translated (0, down ? 1.0f : 0.0f);
    g.setColour (over ? juce::Colour (0xff1a1420) : juce::Colour (0xff110e14));
    g.fillRoundedRectangle (f, rad);
    g.setColour (violet.withAlpha (over ? 0.6f : 0.35f));
    g.drawRoundedRectangle (f, rad, 1.0f);
}

void TMULook::drawButtonText (juce::Graphics& g, juce::TextButton& b, bool over, bool down)
{
    const auto style = b.getProperties()["style"].toString();
    auto r = b.getLocalBounds().translated (0, down ? 1 : 0);
    const bool on = b.getToggleState();
    const auto col = style == "stripe" ? (on ? violetHi : (over ? lavender : violet.brighter (0.15f)))
                                       : (over ? violetHi : lavender);
    if (b.getProperties()["star"])
    {
        g.setColour (col);
        g.fillPath (starPath ({ (float) r.getCentreX() - 18.0f, (float) r.getCentreY() }, 6.0f));
        r.removeFromLeft (r.getWidth() / 2 - 8);
        g.setFont (pixel (10.0f));
        g.drawText (b.getButtonText(), r, juce::Justification::centredLeft);
        return;
    }
    g.setFont (pixel (10.0f));
    g.setColour (col);
    g.drawText (b.getButtonText(), r, juce::Justification::centred);
}

void TMULook::drawScrollbar (juce::Graphics& g, juce::ScrollBar&, int x, int y, int w, int h, bool vertical,
                             int thumbStart, int thumbSize, bool, bool)
{
    juce::Rectangle<int> thumb = vertical ? juce::Rectangle<int> (x + 2, thumbStart, w - 4, thumbSize)
                                          : juce::Rectangle<int> (thumbStart, y + 2, thumbSize, h - 4);
    g.setColour (violet.withAlpha (0.55f));
    for (int yy = thumb.getY(); yy < thumb.getBottom(); yy += 4)
        g.fillRect (thumb.getX(), yy, thumb.getWidth(), 2);
}

//==============================================================================
void RetroKeyboard::drawWhiteNote (int note, juce::Graphics& g, juce::Rectangle<float> area, bool isDown, bool isOver, juce::Colour, juce::Colour)
{
    auto r = area.reduced (1.0f, 0.0f);
    juce::Path key;
    key.addRoundedRectangle (r.getX(), r.getY() - 6.0f, r.getWidth(), r.getHeight() + 6.0f, 4.0f);
    g.saveState();
    g.reduceClipRegion (area.toNearestInt());
    // teclas negras: las "blancas" en negro grafito con separacion violeta
    juce::ColourGradient kg (isDown ? violetHi : (isOver ? juce::Colour (0xff2e2636) : juce::Colour (0xff221c28)), 0, r.getY(),
                             isDown ? violet : juce::Colour (0xff120f15), 0, r.getBottom(), false);
    g.setGradientFill (kg);
    g.fillPath (key);
    g.setColour (violet.withAlpha (isDown ? 0.9f : 0.35f));
    g.drawRoundedRectangle (r.getX() + 0.5f, r.getY() - 6.0f, r.getWidth() - 1.0f, r.getHeight() + 5.5f, 4.0f, 1.0f);
    g.restoreState();
    if (note % 12 == 0)
    {
        g.setFont (look.pixel (7.0f));
        g.setColour (isDown ? juce::Colours::white : violetHi.withAlpha (0.7f));
        g.drawText ("C" + juce::String (note / 12), area.withTrimmedBottom (5).removeFromBottom (10).toNearestInt(), juce::Justification::centred);
    }
}

void RetroKeyboard::drawBlackNote (int, juce::Graphics& g, juce::Rectangle<float> area, bool isDown, bool isOver, juce::Colour)
{
    auto r = area.reduced (1.5f, 0.0f).withTrimmedTop (-4.0f);
    g.setColour (juce::Colours::black.withAlpha (0.6f));
    g.fillRoundedRectangle (r.translated (1.5f, 1.5f), 2.5f);
    g.setColour (isDown ? violet : (isOver ? juce::Colour (0xff2a1a3a) : juce::Colour (0xff050406)));
    g.fillRoundedRectangle (r, 2.5f);
    g.setColour (violet.withAlpha (isDown ? 1.0f : 0.5f));
    g.drawRoundedRectangle (r.reduced (0.5f), 2.5f, 1.0f);
}

void RetroKeyboard::drawUpDownButton (juce::Graphics&, int, int, bool, bool, bool) {}

//==============================================================================
juce::Rectangle<int> TutorialOverlay::boxArea() const
{
    if (steps.empty()) return {};
    const auto spot = steps[index].spot;
    const int bw = 480, bh = 220;
    juce::Rectangle<int> box (0, 0, bw, bh);
    if (spot.isEmpty()) return box.withCentre (getLocalBounds().getCentre());
    // debajo de la zona iluminada si entra, si no arriba
    if (spot.getBottom() + 20 + bh < getHeight()) box.setPosition (spot.getCentreX() - bw / 2, spot.getBottom() + 20);
    else box.setPosition (spot.getCentreX() - bw / 2, spot.getY() - 20 - bh);
    return box.constrainedWithin (getLocalBounds().reduced (16));
}

juce::Rectangle<int> TutorialOverlay::nextArea() const { auto b = boxArea(); return { b.getRight() - 150, b.getBottom() - 44, 134, 30 }; }
juce::Rectangle<int> TutorialOverlay::skipArea() const { auto b = boxArea(); return { b.getX() + 16, b.getBottom() - 44, 110, 30 }; }

void TutorialOverlay::paint (juce::Graphics& g)
{
    if (steps.empty()) return;
    const auto& st = steps[index];

    // oscurecer todo menos la zona explicada
    juce::Path dim;
    dim.addRectangle (getLocalBounds().toFloat());
    if (! st.spot.isEmpty())
    {
        dim.addRoundedRectangle (st.spot.toFloat().expanded (6), 14.0f);
        dim.setUsingNonZeroWinding (false);
    }
    g.setColour (juce::Colours::black.withAlpha (0.72f));
    g.fillPath (dim);
    if (! st.spot.isEmpty())
    {
        g.setColour (violetHi);
        for (float i = 0; i < st.spot.getWidth() + 12; i += 10)   // borde punteado
        {
            g.fillRect ((float) st.spot.getX() - 6 + i, (float) st.spot.getY() - 7, 5.0f, 2.0f);
            g.fillRect ((float) st.spot.getX() - 6 + i, (float) st.spot.getBottom() + 5, 5.0f, 2.0f);
        }
    }

    auto box = boxArea();
    g.setColour (juce::Colours::black.withAlpha (0.6f));
    g.fillRoundedRectangle (box.toFloat().translated (0, 5), 14.0f);
    g.setColour (glassBase);
    g.fillRoundedRectangle (box.toFloat(), 14.0f);
    g.setColour (violet);
    g.drawRoundedRectangle (box.toFloat().reduced (1), 14.0f, 2.0f);

    auto inner = box.reduced (18, 16);
    g.setFont (look.pixel (8.0f));
    g.setColour (screenDim.brighter (0.6f));
    g.drawText (juce::String ((int) index + 1) + "/" + juce::String ((int) steps.size()), inner.removeFromTop (12), juce::Justification::centredRight);
    g.setFont (look.pixel (13.0f));
    glowText (g, st.title, inner.removeFromTop (22), juce::Justification::centredLeft, violetHi);
    inner.removeFromTop (8);
    juce::AttributedString as;
    as.append (st.text, look.pixel (9.0f), lavender);
    as.setLineSpacing (7.0f);
    as.setWordWrap (juce::AttributedString::byWord);
    juce::TextLayout tl;
    tl.createLayout (as, (float) inner.getWidth());
    tl.draw (g, inner.withTrimmedBottom (40).toFloat());

    const bool last = index + 1 >= steps.size();
    g.setColour (violet);
    g.fillRoundedRectangle (nextArea().toFloat(), 15.0f);
    g.setFont (look.pixel (9.0f));
    g.setColour (juce::Colours::white);
    g.drawText (last ? "A JUGAR!" : "SIGUIENTE >", nextArea(), juce::Justification::centred);
    if (! last)
    {
        g.setColour (lavender.withAlpha (0.6f));
        g.drawText ("SALTAR", skipArea(), juce::Justification::centredLeft);
    }
}

void TutorialOverlay::mouseUp (const juce::MouseEvent& e)
{
    const bool last = index + 1 >= steps.size();
    if (nextArea().contains (e.getPosition()) || (! boxArea().contains (e.getPosition())))
    {
        if (last) { setVisible (false); if (onFinished) onFinished(); }
        else { ++index; repaint(); }
    }
    else if (! last && skipArea().contains (e.getPosition()))
    {
        setVisible (false);
        if (onFinished) onFinished();
    }
}

//==============================================================================
TMUEditor::TMUEditor (TMUProcessor& p)
    : juce::AudioProcessorEditor (&p), proc (p),
      volumeAtt (p.params, "volume", volume),
      modAtt (p.params, "mod", modKnob),
      filterAtt (p.params, "filter", filterKnob),
      transposeAtt (p.params, "transpose", transpose),
      rateAtt (p.params, "rate", rateKnob),
      swingAtt (p.params, "swing", swingKnob),
      humanAtt (p.params, "humanize", humanKnob),
      keyboard (p.keyboardState, look)
{
    setLookAndFeel (&look);

    for (auto* b : { &viewType, &viewSource, &viewAZ, &dupButton, &favButton, &helpButton })
    {
        b->getProperties().set ("style", "stripe");
        addAndMakeVisible (b);
    }
    favButton.getProperties().set ("star", true);
    viewType.setTooltip ("Ordenar por tipo de sonido");
    viewSource.setTooltip ("Ordenar por soundfont");
    viewAZ.setTooltip ("Ordenar de la A a la Z");
    favButton.setTooltip ("Estrellas del sonido elegido: 1, 2, 3 y sin estrellas");
    helpButton.setTooltip ("Ver el tutorial");
    viewType.onClick   = [this] { setView ("type"); };
    viewSource.onClick = [this] { setView ("source"); };
    viewAZ.onClick     = [this] { setView ("az"); };
    helpButton.onClick = [this] { showTutorial(); };
    favButton.onClick  = [this]
    {
        const int row = presetList.getSelectedRow();
        if (entries == nullptr || row < 0 || row >= (int) filtered.size()) return;
        setStarsForRow (row, (prefs->getStars ((*entries)[(size_t) filtered[(size_t) row]].key) + 1) % 4);
    };

    search.setFont (look.pixel (13.0f));
    search.setIndents (4, 3);
    search.setColour (juce::TextEditor::textColourId, violetHi);
    search.setTextToShowWhenEmpty ("nombre, tipo o soundfont", screenDim);
    search.setText (proc.uiSearch, false);
    search.applyFontToAllText (look.pixel (13.0f));
    search.onTextChange = [this] { proc.uiSearch = search.getText(); rebuildFiltered(); repaint (lcdArea); };
    search.onEscapeKey  = [this] { search.clear(); proc.uiSearch = {}; rebuildFiltered(); };
    addAndMakeVisible (search);

    showDuplicates = prefs->getShowDuplicates();
    prefsVersion = prefs->getVersion();
    dupButton.setToggleState (! showDuplicates, juce::dontSendNotification);
    dupButton.setTooltip ("Sin repetidos: oculta los sonidos iguales a otro y los que no suenan");
    dupButton.onClick = [this]
    {
        showDuplicates = ! showDuplicates;
        prefs->setShowDuplicates (showDuplicates);
        prefsVersion = prefs->getVersion();
        dupButton.setToggleState (! showDuplicates, juce::dontSendNotification);
        rebuildGroups();
        repaint();
    };

    for (auto* b : { &rescanButton, &folderButton, &prevButton, &nextButton })
    {
        b->getProperties().set ("style", "body");
        addAndMakeVisible (b);
    }
    rescanButton.onClick = [this] { proc.library->rescan(); };
    rescanButton.setTooltip ("Volver a buscar soundfonts");
    folderButton.onClick = [] { auto f = PresetLibrary::getUserFolder(); f.createDirectory(); f.startAsProcess(); };
    folderButton.setTooltip ("Abre la carpeta donde podes agregar mas .sf2");
    prevButton.onClick = [this] { step (-1); };
    nextButton.onClick = [this] { step (1); };
    prevButton.setTooltip ("Sonido anterior");
    nextButton.setTooltip ("Sonido siguiente");

    for (auto* lb : { &groupList, &presetList })
    {
        lb->setRowHeight (24);
        lb->setOutlineThickness (0);
        addAndMakeVisible (lb);
    }
    presetList.setWantsKeyboardFocus (true);

    velButton.getProperties().set ("style", "body");
    velButton.setTooltip ("Curva de velocity: SUAVE (suena fuerte aunque toques despacio), NORMAL, DURA (hay que pegarle) o FIJA");
    velButton.onClick = [this]
    {
        if (auto* prm = proc.params.getParameter ("velcurve"))
        {
            const int n = ((int) proc.params.getRawParameterValue ("velcurve")->load() + 1) % 4;
            prm->setValueNotifyingHost (prm->convertTo0to1 ((float) n));
        }
        updateVelButton();
    };
    addAndMakeVisible (velButton);
    updateVelButton();

    for (auto* k : allKnobs())
    {
        k->setDoubleClickReturnValue (true, k == &rateKnob ? 5.5 : 0.0);
        k->setPopupDisplayEnabled (true, true, this);
        k->onRightClick = [this] (ParamKnob& knob) { showKnobMenu (knob); };
        k->setTooltip ("Click derecho: asignar a un control de tu teclado MIDI");
        k->getProperties().set ("bipolar", k == &filterKnob || k == &transpose);
        addAndMakeVisible (k);
    }

    keyboard.setAvailableRange (48, 84);   // C4 a C7 (como FL)
    keyboard.setOctaveForMiddleC (5);
    keyboard.setBlackNoteLengthProportion (0.6f);
    keyboard.setWantsKeyboardFocus (false);
    keyboard.setScrollButtonsVisible (false);
    keyboard.setOpaque (false);
    addAndMakeVisible (keyboard);

    addChildComponent (tutorial);
    tutorial.onFinished = [this] { prefs->setTutorialSeen (true); prefsVersion = prefs->getVersion(); };

    setSize (W, H);

    currentGroup = proc.uiGroup;
    setView (proc.uiView.isEmpty() ? juce::String ("type") : proc.uiView);
    startTimerHz (15);

    if (! prefs->getTutorialSeen())
        showTutorial();
}

TMUEditor::~TMUEditor()
{
    setLookAndFeel (nullptr);
}

void TMUEditor::showTutorial()
{
    const auto pills = viewType.getBounds().getUnion (helpButton.getBounds());
    const auto knobs = knobsArea;
    tutorial.start ({
        { "BIENVENIDO A TMU",  "Todos tus soundfonts en una consola. Son miles de sonidos ordenados para que encuentres rapido lo que buscas.\n\nEn 6 pasos te muestro como se usa.", {} },
        { "LA PANTALLA",       "Elegi un sonido con un click o con las flechas arriba/abajo. A la izquierda estan los grupos. Arriba escribis para buscar: bass, pad, mario...", lcdArea },
        { "COMO SE ORDENA",    "TIPO: pianos, bajos, pads...\nSF2: por soundfont.\nA-Z: alfabetico.\nSIN REP: oculta los sonidos repetidos y los que no suenan.", pills },
        { "FAVORITOS",         "Toca FAV (o las teclas 1, 2, 3) para darle estrellas al sonido elegido. Los vas a encontrar en el grupo FAVORITOS de la pantalla.", favButton.getBounds() },
        { "PERILLAS",          "MOD: vibrato y filtro que se mueven estilo Roland (arranca despues de un ratito, tambien con la rueda de tu teclado). RATE: velocidad. FILTRO, ST, SWING (atrasa los 16avos), HUMAN (velocity y tiempo humanos) y VEL (curva de velocity). Click derecho: asignar a tu MIDI.", knobs },
        { "PROBA Y SUMA MAS",  "Toca el mini piano con el mouse. < > pasan de sonido. CARPETA abre la carpeta para sumar tus .sf2 y RESCAN los carga.", keySlot.getUnion (folderButton.getBounds()) },
        { "LISTO!",            "Si te olvidas de algo, toca ? arriba a la derecha y vuelve este tutorial.", helpButton.getBounds() },
    });
    tutorial.setBounds (getLocalBounds());
}

//==============================================================================
void TMUEditor::buildChassis()
{
    const float scale = chassisScale;
    chassis = juce::Image (juce::Image::ARGB, (int) std::ceil (W * scale), (int) std::ceil (H * scale), true);
    juce::Graphics g (chassis);
    g.addTransform (juce::AffineTransform::scale (scale));

    g.fillAll (outerBg);
    juce::ColourGradient floor (juce::Colour (0xff160c1e), W * 0.5f, H * 0.55f, outerBg, 0, 0, true);
    g.setGradientFill (floor);
    g.fillAll();

    // ---- cuerpo negro: ocupa toda la ventana, sin bordes
    auto body = juce::Rectangle<float> (0, 0, (float) W, (float) H);
    juce::ColourGradient bg (bodyTop, 0, body.getY(), black, 0, body.getY() + body.getHeight() * 0.6f, false);
    g.setGradientFill (bg);
    g.fillRect (body);

    // ---- franja violeta superior
    {
        juce::Path p;
        p.addRectangle (body.getX(), body.getY(), body.getWidth(), (float) HEADER);
        g.setColour (black);
        g.fillPath (p);
        g.setColour (violet.withAlpha (0.45f));   // linea fina que separa la barra
        g.fillRect (body.getX(), body.getY() + (float) HEADER - 1.0f, body.getWidth(), 1.0f);
    }
    // ---- logo: TMU y SOUND FONT PLAYER alineados a la izquierda, como bloque centrado en la franja
    {
        const auto logoFont = look.heavy (52.0f);
        const auto subFont = look.pixel (8.0f);
        const auto lb = tightBounds (logoFont, "TMU");
        const auto sb = tightBounds (subFont, "SOUND FONT PLAYER");
        const float gap = 7.0f;
        const float top = ((float) HEADER - (lb.getHeight() + gap + sb.getHeight())) * 0.5f;
        g.setColour (purpleDark);
        drawTight (g, logoFont, "TMU", (float) M + 2.0f, top + 3.0f);
        g.setColour (violetHi);
        drawTight (g, logoFont, "TMU", (float) M, top);
        g.setColour (violet);
        drawTight (g, subFont, "SOUND FONT PLAYER", (float) M, top + lb.getHeight() + gap);

        // firma "viszeversa": imagen hecha con la fuente Kihim, del alto de las letras de TMU
        auto logo = juce::ImageCache::getFromMemory (BinaryData::viszeversa_png, BinaryData::viszeversa_pngSize);
        const float h = std::round (lb.getHeight() * 0.62f);
        const float w = h * (float) logo.getWidth() / (float) juce::jmax (1, logo.getHeight());
        g.setColour (violet);
        // centrada en el alto de las letras de TMU
        g.drawImage (logo, juce::Rectangle<float> ((float) M + lb.getWidth() + 24.0f, top + (lb.getHeight() - h) * 0.5f, w, h),
                     juce::RectanglePlacement::stretchToFit, true);
    }

    // ---- ranura del cartucho (pantalla)
    auto slot = lcdArea.toFloat();
    g.setColour (juce::Colours::white.withAlpha (0.07f));
    g.fillRoundedRectangle (slot.translated (0, 1.5f).expanded (1), 16.0f);
    g.setColour (juce::Colour (0xff040305));
    g.fillRoundedRectangle (slot, 16.0f);

    // ---- rejilla de ventilacion
    for (int i = 0; i < 26; ++i)
    {
        const float x = (float) keySlot.getX() + (float) i * ((float) keySlot.getWidth() - 8.0f) / 25.0f;
        const float y = (float) folderButton.getBottom() - 24.0f;   // termina a la misma altura que CARPETA
        g.setColour (juce::Colours::black);
        g.fillRoundedRectangle (x, y, 8.0f, 24.0f, 3.0f);
        g.setColour (juce::Colours::white.withAlpha (0.05f));
        g.fillRect (x, y + 24.0f, 8.0f, 1.0f);
    }

    // ---- etiquetas
    g.setFont (look.pixel (8.0f));
    g.setColour (lavender.withAlpha (0.8f));
    auto knobLabel = [&] (juce::Rectangle<int> a, const char* t) { g.drawText (t, a.withY (a.getBottom() + 6).withHeight (10).expanded (20, 0), juce::Justification::centred); };
    knobLabel (volArea, "VOL");
    knobLabel (modArea, "MOD");
    knobLabel (filterArea, "FILTRO");
    knobLabel (rateArea, "RATE");
    knobLabel (swingArea, "SWING");
    knobLabel (humanArea, "HUMAN");
    knobLabel (velArea, "VEL");
    g.drawText ("POWER", juce::Rectangle<int> (prevButton.getX(), keySlot.getY(), 80, 16), juce::Justification::centredLeft);
    g.drawText ("SONIDO", prevButton.getBounds().getUnion (nextButton.getBounds()).withY (prevButton.getY() - 14).withHeight (10), juce::Justification::centred);

    // ---- vidrio de la pantalla y LED (fijos)
    auto glass = lcdArea.toFloat().reduced (14.0f);
    {
        juce::ColourGradient crt (juce::Colour (0xff1c0b2c), glass.getCentreX(), glass.getCentreY(), glassBase, glass.getX(), glass.getY(), true);
        g.setGradientFill (crt);
        g.fillRoundedRectangle (glass, 10.0f);
        g.saveState();
        g.reduceClipRegion (glass.toNearestInt());
        g.setColour (juce::Colours::black.withAlpha (0.3f));
        for (float y = glass.getY(); y < glass.getBottom(); y += 3.0f) g.drawHorizontalLine ((int) y, glass.getX(), glass.getRight());
        g.restoreState();
        juce::ColourGradient refl (juce::Colours::white.withAlpha (0.05f), glass.getX(), glass.getY(), juce::Colours::transparentWhite, glass.getX(), glass.getY() + 90, false);
        g.setGradientFill (refl);
        g.fillRoundedRectangle (glass.reduced (4.0f), 8.0f);
    }
    drawLed (g, { (float) R - 6.0f, (float) keySlot.getY() + 8.0f }, 5.0f);
}

void TMUEditor::paint (juce::Graphics& g)
{
    // fondo fijo pre-dibujado a la resolucion real de la pantalla: se copia 1 a 1 (muy liviano)
    const float sc = juce::jmax (1.0f, g.getInternalContext().getPhysicalPixelScaleFactor());
    if (chassis.isNull() || std::abs (sc - chassisScale) > 0.01f)
    {
        chassisScale = sc;
        buildChassis();
    }
    g.setImageResamplingQuality (juce::Graphics::lowResamplingQuality);
    g.drawImageTransformed (chassis, juce::AffineTransform::scale (1.0f / chassisScale));

    auto glass = lcdArea.toFloat().reduced (14.0f);

    const auto info = proc.getLoadedInfo();
    auto gi = glass.toNearestInt().reduced (18, 10);
    g.setFont (look.pixel (13.0f));
    glowText (g, "BUSCAR>", gi.withHeight (28).withWidth (110), juce::Justification::centredLeft, violetHi);
    juce::String status;
    if (proc.library->isScanning()) status = "CARGANDO...";
    else if (entries != nullptr) status = juce::String (filtered.size()) + "/" + juce::String (groups.empty() ? (int) entries->size() : groups[0].count);
    g.setFont (look.pixel (11.0f));
    glowText (g, status, gi.withHeight (28).withTrimmedLeft (gi.getWidth() - 220), juce::Justification::centredRight, screenTxt);

    g.setColour (violet.withAlpha (0.45f));
    for (int x = gi.getX(); x < gi.getRight(); x += 6) g.fillRect (x, gi.getY() + 34, 3, 2);
    for (int x = gi.getX(); x < gi.getRight(); x += 6) g.fillRect (x, miniLcdArea.getY() - 6, 3, 2);
    for (int y = gi.getY() + 42; y < miniLcdArea.getY() - 10; y += 6) g.fillRect (groupList.getRight() + 6, y, 2, 3);

    if (entries != nullptr && entries->empty() && ! proc.library->isScanning())
    {
        g.setFont (look.pixel (12.0f));
        glowText (g, "NO HAY SOUNDFONTS", presetList.getBounds().withHeight (30).translated (0, 40), juce::Justification::centred, violetHi);
        g.setFont (look.pixel (8.0f));
        glowText (g, "Pone .sf2 en Musica/TMU SoundFonts y toca RESCAN", presetList.getBounds().withHeight (20).translated (0, 76), juce::Justification::centred, screenTxt);
    }

    // ---- sonando (con pista de ayuda)
    auto mi = miniLcdArea;
    juce::String top = "SONANDO";
    if (info.loading) top = "CARGANDO...";
    else if (info.error.isNotEmpty()) top = "ERROR: " + info.error;
    auto topRow = mi.removeFromTop (14);
    g.setFont (look.pixel (8.0f));
    glowText (g, top, topRow.removeFromLeft (90), juce::Justification::centredLeft, screenDim.brighter (0.7f), 0.0f);
    // velocity de la ultima nota (ya con la curva VEL y el HUMAN): numero + barrita
    if (const int lv = proc.getLastVelocity(); lv > 0)
    {
        auto vr = topRow.removeFromLeft (150);
        glowText (g, "VEL " + juce::String (lv).paddedLeft ('0', 3), vr.removeFromLeft (52), juce::Justification::centredLeft, screenTxt, 0.0f);
        auto bar = vr.withSizeKeepingCentre (vr.getWidth() - 8, 4).toFloat();
        g.setColour (violet.withAlpha (0.25f));
        g.fillRect (bar);
        g.setColour (violetHi);
        g.fillRect (bar.withWidth (bar.getWidth() * (float) lv / 127.0f));
    }
    if (info.name.isNotEmpty() && ! info.loading)
    {
        const int st = prefs->getStars (PresetEntry::makeKey (juce::File (info.file).getFileName(), info.phdrIndex, info.name));
        drawStars (g, topRow.removeFromRight (44), st, violetHi, violetHi.withAlpha (0.15f), 5.0f);
        topRow.removeFromRight (8);
        glowText (g, juce::String (tmu::categoryName (info.category)).toUpperCase() + " / " + info.source.toUpperCase(), topRow,
                  juce::Justification::centredRight, screenTxt.withAlpha (0.8f), 0.0f);
    }
    auto nameRow = mi;
    g.setFont (look.pixel (8.0f));
    g.setColour (screenDim.brighter (0.5f));
    g.drawText ("? = AYUDA", nameRow.removeFromRight (90), juce::Justification::centredRight);
    g.setFont (look.pixel (16.0f));
    glowText (g, info.name.isNotEmpty() ? info.name.toUpperCase() : juce::String ("---"), nameRow, juce::Justification::centredLeft, violetHi, 0.22f);

    // ---- transporte y CC asignados
    const int tr = (int) transpose.getValue();
    g.setFont (look.pixel (8.0f));
    g.setColour (lavender.withAlpha (0.8f));
    g.drawText ("ST " + juce::String (tr > 0 ? "+" : "") + juce::String (tr), trArea.withY (trArea.getBottom() + 6).withHeight (10).expanded (20, 0), juce::Justification::centred);
    const auto learning = proc.getLearningParam();
    for (auto* k : allKnobs())
    {
        const int cc = proc.getAssignedCC (k->paramId);
        const bool isLearning = learning == k->paramId;
        if (cc < 0 && ! isLearning) continue;
        g.setColour (isLearning ? violetHi : lavender.withAlpha (0.6f));
        g.drawText (isLearning ? juce::String ("MOVE...") : "CC" + juce::String (cc), k->getBounds().withY (k->getY() - 14).withHeight (10).expanded (20, 0),
                    juce::Justification::centred);
    }
}

void TMUEditor::resized()
{
    // Grilla: todo arranca en x = M y termina en x = R; 24 px entre bloques.
    lcdArea = { M, HEADER + 30, R - M, 340 };
    auto glass = lcdArea.reduced (14).reduced (18, 10);
    miniLcdArea = { glass.getX(), glass.getBottom() - 36, glass.getWidth(), 36 };

    const int rowTop = lcdArea.getBottom() + 24;
    keySlot = { M, rowTop, 420, 126 };

    // columna derecha (POWER, < >, RESCAN, CARPETA), alineada al borde derecho
    const int colW = 108, colX = R - colW;
    prevButton.setBounds   (colX, rowTop + 42, 52, 30);
    nextButton.setBounds   (R - 52, rowTop + 42, 52, 30);
    rescanButton.setBounds (colX, rowTop + 88, colW, 30);
    folderButton.setBounds (colX, rowTop + 134, colW, 30);

    // 2 filas de 4 perillas con el mismo espacio entre el piano y la columna derecha:
    // arriba el sonido (VOL MOD RATE FILTRO), abajo el groove (ST SWING HUMAN VEL)
    const int knob = 46, freeW = colX - keySlot.getRight(), gapK = (freeW - 4 * knob) / 5;
    const int rowY[2] = { rowTop + 4, rowTop + 4 + 82 };
    juce::Rectangle<int>* rows[2][4] = { { &volArea, &modArea, &rateArea, &filterArea }, { &trArea, &swingArea, &humanArea, &velArea } };
    for (int r = 0; r < 2; ++r)
    {
        int kx = keySlot.getRight() + gapK + (freeW - 4 * knob - 5 * gapK) / 2;
        for (auto* a : rows[r])
        {
            *a = { kx, rowY[r], knob, knob };
            kx += knob + gapK;
        }
    }
    knobsArea = volArea.getUnion (velArea).expanded (12, 16);

    // botones de la franja violeta: mismo ancho, mismo espacio, centrados en la franja y alineados a la derecha
    int x = R;
    const int bh = 32, by = (HEADER - bh) / 2;
    for (auto* b : { &helpButton, &favButton, &dupButton, &viewAZ, &viewSource, &viewType })
    {
        const int bw = b == &helpButton ? bh : 80;
        x -= bw;
        b->setBounds (x, by, bw, bh);
        x -= 8;
    }

    search.setBounds (glass.getX() + 112, glass.getY(), 340, 28);
    auto body = glass.withTrimmedTop (42).withTrimmedBottom (48);
    groupList.setBounds (body.removeFromLeft (250));
    body.removeFromLeft (14);
    presetList.setBounds (body);

    volume.setBounds (volArea);
    modKnob.setBounds (modArea);
    filterKnob.setBounds (filterArea);
    transpose.setBounds (trArea);
    rateKnob.setBounds (rateArea);
    swingKnob.setBounds (swingArea);
    humanKnob.setBounds (humanArea);
    velButton.setBounds (velArea.withSizeKeepingCentre (64, 26));


    keyboard.setBounds (keySlot);
    keyboard.setKeyWidth ((float) keySlot.getWidth() / 22.0f);   // C4..C7 = 22 teclas blancas

    tutorial.setBounds (getLocalBounds());
    chassis = {};   // se vuelve a dibujar en el proximo paint
}

bool TMUEditor::keyPressed (const juce::KeyPress& k)
{
    if (k == juce::KeyPress::upKey)   { step (-1); return true; }
    if (k == juce::KeyPress::downKey) { step (1);  return true; }
    const auto ch = k.getTextCharacter();
    if (ch >= '0' && ch <= '3' && ! search.hasKeyboardFocus (true))
    {
        setStarsForRow (presetList.getSelectedRow(), ch - '0');
        return true;
    }
    return false;
}

//==============================================================================
void TMUEditor::timerCallback()
{
    auto now = proc.library->getEntries();
    if (now != entries)
    {
        entries = now;
        rebuildGroups();
        repaint();
    }

    const auto info = proc.getLoadedInfo();
    if (info.name != lastInfo.name || info.file != lastInfo.file || info.loading != lastInfo.loading
        || info.error != lastInfo.error || info.phdrIndex != lastInfo.phdrIndex)
    {
        const bool presetChanged = info.file != lastInfo.file || info.phdrIndex != lastInfo.phdrIndex;
        lastInfo = info;
        if (presetChanged) selectLoadedRow();
        presetList.repaint();
        repaint (miniLcdArea);
    }

    if (prefs->getVersion() != prefsVersion)
    {
        prefsVersion = prefs->getVersion();
        showDuplicates = prefs->getShowDuplicates();
        dupButton.setToggleState (! showDuplicates, juce::dontSendNotification);
        rebuildGroups();
        repaint();
    }

    // repintar solo si algo cambio (con la ventana abierta y quieta casi no gasta CPU)
    juce::String sig;
    if (velButton.getButtonText() != velText()) updateVelButton();
    sig << (int) proc.library->isScanning() << "|" << (int) filtered.size() << "|" << (int) transpose.getValue() << "|" << proc.getLearningParam() << "|" << proc.getNoteCount();
    for (auto* k : allKnobs()) sig << "|" << proc.getAssignedCC (k->paramId);
    if (sig != lastPaintSig)
    {
        lastPaintSig = sig;
        repaint (miniLcdArea.expanded (4));
        repaint (knobsArea.expanded (20, 6));
        repaint (lcdArea.withHeight (70));
    }

    const int row = presetList.getSelectedRow();
    const bool selFav = entries != nullptr && row >= 0 && row < (int) filtered.size() && prefs->getStars ((*entries)[(size_t) filtered[(size_t) row]].key) > 0;
    if (favButton.getToggleState() != selFav) favButton.setToggleState (selFav, juce::dontSendNotification);
}

juce::String TMUEditor::velText() const
{
    static const char* names[] = { "SUAVE", "NORMAL", "DURA", "FIJA" };
    return names[juce::jlimit (0, 3, (int) proc.params.getRawParameterValue ("velcurve")->load())];
}

void TMUEditor::updateVelButton()
{
    velButton.setButtonText (velText());
    velButton.setToggleState (velButton.getButtonText() != "NORMAL", juce::dontSendNotification);
}

void TMUEditor::showKnobMenu (ParamKnob& knob)
{
    juce::PopupMenu menu;
    const int cc = proc.getAssignedCC (knob.paramId);
    menu.addSectionHeader ("Controlador MIDI");
    menu.addItem (proc.getLearningParam() == knob.paramId ? "Esperando... (move una perilla)" : "Aprender controlador (MIDI learn)",
                  [this, id = knob.paramId] { proc.startLearning (id); repaint(); });
    menu.addItem (cc >= 0 ? "Quitar controlador (CC " + juce::String (cc) + ")" : "Quitar controlador", cc >= 0,
                  false, [this, id = knob.paramId] { proc.clearLearning (id); repaint(); });

    // menu del DAW para este parametro (en FL Studio incluye "Link to controller")
    if (auto* host = getHostContext())
        if (auto* prm = proc.params.getParameter (knob.paramId))
            if (auto hostMenu = host->getContextMenuForParameter (prm))
            {
                menu.addSeparator();
                menu.addSubMenu ("Opciones del DAW (Link to controller...)", hostMenu->getEquivalentPopupMenu());
            }

    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&knob));
}

void TMUEditor::setView (const juce::String& v)
{
    if (proc.uiView != v) currentGroup = {};
    proc.uiView = v;
    viewType.setToggleState (v == "type", juce::dontSendNotification);
    viewSource.setToggleState (v == "source", juce::dontSendNotification);
    viewAZ.setToggleState (v == "az", juce::dontSendNotification);
    rebuildGroups();
}

bool TMUEditor::entryInGroup (const PresetEntry& e, const juce::String& key) const
{
    if (key.isEmpty() || key == "*") return true;
    if (key == "fav") return prefs->getStars (e.key) > 0;
    if (proc.uiView == "type")   return juce::String (e.category) == key;
    if (proc.uiView == "source") return e.source == key;
    // A-Z
    const auto c = juce::CharacterFunctions::toUpperCase (e.name[0]);
    if (key == "#") return ! (c >= 'A' && c <= 'Z');
    return juce::String::charToString (c) == key;
}

void TMUEditor::rebuildGroups()
{
    groups.clear();
    if (entries == nullptr) { groupList.updateContent(); rebuildFiltered(); return; }

    int visible = 0, favs = 0;
    for (auto& e : *entries)
        if (isVisible (e)) { ++visible; if (prefs->getStars (e.key) > 0) ++favs; }
    groups.push_back ({ "*", "Todos", visible });
    groups.push_back ({ "fav", juce::String (juce::CharPointer_UTF8 ("\xe2\x98\x85 Favoritos")), favs });

    if (proc.uiView == "type")
    {
        std::array<int, tmu::NumCategories> counts {};
        for (auto& e : *entries) if (isVisible (e)) counts[(size_t) e.category]++;
        for (int c = 0; c < tmu::NumCategories; ++c)
            if (counts[(size_t) c] > 0) groups.push_back ({ juce::String (c), tmu::categoryName (c), counts[(size_t) c] });
    }
    else if (proc.uiView == "source")
    {
        std::map<juce::String, int> counts;
        for (auto& e : *entries) if (isVisible (e)) counts[e.source]++;
        std::vector<std::pair<juce::String, int>> v (counts.begin(), counts.end());
        std::sort (v.begin(), v.end(), [] (auto& a, auto& b) { return a.first.compareNatural (b.first) < 0; });
        for (auto& [name, n] : v) groups.push_back ({ name, name, n });
    }
    else
    {
        std::map<juce::String, int> counts;
        for (auto& e : *entries)
        {
            if (! isVisible (e)) continue;
            const auto c = juce::CharacterFunctions::toUpperCase (e.name[0]);
            counts[(c >= 'A' && c <= 'Z') ? juce::String::charToString (c) : juce::String ("#")]++;
        }
        for (auto& [k, n] : counts) groups.push_back ({ k, k, n });
    }

    int sel = 0;
    for (size_t i = 0; i < groups.size(); ++i) if (groups[i].key == currentGroup) sel = (int) i;
    currentGroup = groups[(size_t) sel].key;
    proc.uiGroup = currentGroup;

    groupList.updateContent();
    suppressSelection = true;
    groupList.selectRow (sel, true, true);
    suppressSelection = false;
    groupList.repaint();
    rebuildFiltered();
}

void TMUEditor::rebuildFiltered()
{
    filtered.clear();
    if (entries != nullptr)
    {
        juce::StringArray words;
        words.addTokens (search.getText().trim(), " ", "");
        words.removeEmptyStrings();

        for (int i = 0; i < (int) entries->size(); ++i)
        {
            const auto& e = (*entries)[(size_t) i];
            if (! isVisible (e) || ! entryInGroup (e, currentGroup)) continue;
            bool ok = true;
            for (auto& w : words)
                if (! e.name.containsIgnoreCase (w) && ! e.source.containsIgnoreCase (w)
                    && ! juce::String (tmu::categoryName (e.category)).containsIgnoreCase (w)) { ok = false; break; }
            if (ok) filtered.push_back (i);
        }
        if (currentGroup == "fav")   // favoritos: primero los de 3 estrellas
            std::stable_sort (filtered.begin(), filtered.end(), [this] (int a, int b)
            { return prefs->getStars ((*entries)[(size_t) a].key) > prefs->getStars ((*entries)[(size_t) b].key); });
    }
    presetList.updateContent();
    selectLoadedRow();
    presetList.repaint();
    repaint (lcdArea.withHeight (60));
}

void TMUEditor::selectLoadedRow()
{
    suppressSelection = true;
    presetList.deselectAllRows();
    if (entries != nullptr)
        for (size_t r = 0; r < filtered.size(); ++r)
        {
            const auto& e = (*entries)[(size_t) filtered[r]];
            if (e.phdrIndex == lastInfo.phdrIndex && e.file == lastInfo.file)
            {
                presetList.selectRow ((int) r, false, true);
                break;
            }
        }
    suppressSelection = false;
}

void TMUEditor::choosePreset (int row)
{
    if (entries == nullptr || row < 0 || row >= (int) filtered.size()) return;
    proc.loadPreset ((*entries)[(size_t) filtered[(size_t) row]]);
}

void TMUEditor::step (int delta)
{
    if (filtered.empty()) return;
    int row = presetList.getSelectedRow();
    row = row < 0 ? 0 : (row + delta + (int) filtered.size()) % (int) filtered.size();
    presetList.selectRow (row);   // dispara selectedRowsChanged -> carga
}

//==============================================================================
//==============================================================================
void TMUEditor::GroupModel::paintListBoxItem (int row, juce::Graphics& g, int w, int h, bool selected)
{
    if (row < 0 || row >= (int) ed.groups.size()) return;
    const auto& gr = ed.groups[(size_t) row];
    auto r = juce::Rectangle<int> (0, 0, w, h).reduced (2, 1);
    if (selected)
    {
        g.setColour (violet.withAlpha (0.3f));
        g.fillRect (r);
    }
    r.removeFromLeft (6);
    const auto col = selected ? violetHi : screenTxt.withAlpha (0.75f);
    g.setFont (ed.look.pixel (10.0f));
    glowText (g, selected ? ">" : "", r.removeFromLeft (16), juce::Justification::centredLeft, violetHi);
    if (gr.key == "fav")
    {
        g.setColour (col);
        g.fillPath (starPath ({ (float) r.getX() + 6.0f, (float) r.getCentreY() }, 6.0f));
        r.removeFromLeft (16);
    }
    if (selected)
    {
        g.setColour (screenTxt.withAlpha (0.7f));
        g.drawText (juce::String (gr.count), r.removeFromRight (52).withTrimmedRight (4), juce::Justification::centredRight);
    }
    glowText (g, gr.label.toUpperCase().replace (juce::String (juce::CharPointer_UTF8 ("\xe2\x98\x85 ")), ""), r,
              juce::Justification::centredLeft, col, selected ? 0.2f : 0.0f);
}

void TMUEditor::PresetModel::paintListBoxItem (int row, juce::Graphics& g, int w, int h, bool selected)
{
    if (ed.entries == nullptr || row < 0 || row >= (int) ed.filtered.size()) return;
    const auto& e = (*ed.entries)[(size_t) ed.filtered[(size_t) row]];
    const bool isLoaded = e.phdrIndex == ed.lastInfo.phdrIndex && e.file == ed.lastInfo.file;

    auto r = juce::Rectangle<int> (0, 0, w, h).reduced (2, 1);
    if (isLoaded)      { g.setColour (violet.withAlpha (0.35f)); g.fillRect (r); }
    else if (selected) { g.setColour (violet.withAlpha (0.15f)); g.fillRect (r); }
    r.removeFromLeft (6);
    if (isLoaded) { g.setColour (violetHi); g.fillRect (r.removeFromLeft (8).withSizeKeepingCentre (6, 6)); }
    else r.removeFromLeft (8);
    r.removeFromLeft (8);

    const auto textCol = isLoaded ? juce::Colours::white : (e.hidden != 0 ? screenDim : screenTxt);
    if (w >= 480)
    {
        const auto right = ed.proc.uiView == "source" ? juce::String (tmu::categoryName (e.category)) : e.source;
        g.setFont (ed.look.pixel (8.0f));
        g.setColour (screenTxt.withAlpha (isLoaded ? 0.8f : 0.45f));
        g.drawText (right.toUpperCase(), r.removeFromRight (juce::jmin (230, w / 2)).withTrimmedRight (6), juce::Justification::centredRight, true);
    }
    const int st = ed.prefs->getStars (e.key);
    drawStars (g, starsArea (w, h), st, violetHi, violetHi.withAlpha (selected || isLoaded ? 0.25f : 0.1f));
    r.setRight (starsArea (w, h).getX() - 6);
    g.setFont (ed.look.pixel (10.0f));
    glowText (g, e.name.toUpperCase(), r, juce::Justification::centredLeft, textCol, isLoaded ? 0.25f : 0.0f);
}

void TMUEditor::GroupModel::selectedRowsChanged (int row)
{
    if (ed.suppressSelection || row < 0 || row >= (int) ed.groups.size()) return;
    ed.currentGroup = ed.groups[(size_t) row].key;
    ed.proc.uiGroup = ed.currentGroup;
    ed.rebuildFiltered();
}


juce::Rectangle<int> TMUEditor::starsArea (int w, int h)
{
    const int right = w >= 480 ? juce::jmin (230, w / 2) : 0;
    return { w - right - 2 - 6 - 42, 0, 42, h };
}

void TMUEditor::setStarsForRow (int row, int n)
{
    if (entries == nullptr || row < 0 || row >= (int) filtered.size()) return;
    prefs->setStars ((*entries)[(size_t) filtered[(size_t) row]].key, n);
    prefsVersion = prefs->getVersion();
    // actualizar el contador de favoritos sin perder el lugar en la lista
    int favs = 0;
    for (auto& e : *entries) if (isVisible (e) && prefs->getStars (e.key) > 0) ++favs;
    for (auto& gr : groups) if (gr.key == "fav") gr.count = favs;
    groupList.repaint();
    if (currentGroup == "fav") rebuildFiltered();
    presetList.repaint();
    repaint (miniLcdArea);
}

void TMUEditor::PresetModel::listBoxItemClicked (int row, const juce::MouseEvent& e)
{
    if (ed.entries == nullptr || row < 0 || row >= (int) ed.filtered.size()) return;
    const auto key = (*ed.entries)[(size_t) ed.filtered[(size_t) row]].key;
    const int current = ed.prefs->getStars (key);

    if (e.mods.isPopupMenu())
    {
        juce::PopupMenu m;
        m.addSectionHeader ("Favoritos");
        const juce::String star (juce::CharPointer_UTF8 ("\xe2\x98\x85"));
        for (int n = 1; n <= 3; ++n)
            m.addItem (juce::String::repeatedString (star, n), true, current == n, [this, row, n] { ed.setStarsForRow (row, n); });
        m.addItem ("Quitar de favoritos", current > 0, false, [this, row] { ed.setStarsForRow (row, 0); });
        m.showMenuAsync (juce::PopupMenu::Options().withMousePosition());
        return;
    }
    auto sa = starsArea (ed.presetList.getVisibleRowWidth(), ed.presetList.getRowHeight());
    if (sa.contains (e.getPosition()))
    {
        const int n = juce::jlimit (1, 3, (e.x - sa.getX()) / 14 + 1);
        ed.setStarsForRow (row, n == current ? 0 : n);
    }
}

void TMUEditor::PresetModel::selectedRowsChanged (int row)
{
    if (ed.suppressSelection || row < 0) return;
    ed.choosePreset (row);
}
