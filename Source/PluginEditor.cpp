#include "PluginEditor.h"
#include <BinaryData.h>

namespace
{
// ---- paleta: violeta, purpura y negro (+ lavanda clara para teclas y etiquetas)
const juce::Colour outerBg    (0xff050406);
const juce::Colour black      (0xff0b090d);
const juce::Colour bodyTop    (0xff231e27);
const juce::Colour violetHi   (0xffc98cff);
const juce::Colour violet     (0xff8b3dff);
const juce::Colour purple     (0xff5b1a9e);
const juce::Colour purpleDark (0xff2a0a4a);
const juce::Colour screenTxt  (0xffb77dff);
const juce::Colour screenDim  (0xff5a3a86);
const juce::Colour glassBase  (0xff0d0514);
const juce::Colour lavender   (0xffece2ff);

constexpr int W = 1000, H = 720;

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

void glowText (juce::Graphics& g, const juce::String& s, juce::Rectangle<int> r, juce::Justification j, juce::Colour c, float glow = 0.18f)
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
    press = juce::Typeface::createSystemTypefaceFor (BinaryData::PressStart2PRegular_ttf, (size_t) BinaryData::PressStart2PRegular_ttfSize);

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

juce::Font TMULook::pixel (float h) const { return juce::Font (juce::FontOptions (press).withHeight (h)); }

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

    if (style == "stripe")   // pill sobre la franja violeta
    {
        g.setColour (on ? black : (over ? juce::Colours::black.withAlpha (0.28f) : juce::Colours::black.withAlpha (0.16f)));
        g.fillRoundedRectangle (r.translated (0, down ? 1.0f : 0.0f), rad);
        return;
    }
    // "body": pill oscura sobre el cuerpo negro
    g.setColour (juce::Colours::black.withAlpha (0.6f));
    g.fillRoundedRectangle (r.translated (0, 2.0f), rad);
    juce::ColourGradient gr (over ? juce::Colour (0xff3a3340) : juce::Colour (0xff2c2631), 0, r.getY(), juce::Colour (0xff17131a), 0, r.getBottom(), false);
    g.setGradientFill (gr);
    g.fillRoundedRectangle (r.translated (0, down ? 1.0f : 0.0f), rad);
    g.setColour (juce::Colours::white.withAlpha (0.08f));
    g.drawRoundedRectangle (r.reduced (0.5f), rad, 1.0f);
}

void TMULook::drawButtonText (juce::Graphics& g, juce::TextButton& b, bool over, bool down)
{
    const auto style = b.getProperties()["style"].toString();
    auto r = b.getLocalBounds().translated (0, down ? 1 : 0);
    const bool on = b.getToggleState();
    const auto col = style == "stripe" ? (on ? juce::Colours::white : black) : (over ? juce::Colours::white : lavender);
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
    g.setColour (isDown ? violetHi : (isOver ? juce::Colours::white : lavender));
    g.fillPath (key);
    g.restoreState();
    if (note % 12 == 0)
    {
        g.setFont (look.pixel (7.0f));
        g.setColour (isDown ? juce::Colours::white : purple);
        g.drawText ("C" + juce::String (note / 12), area.withTrimmedBottom (5).removeFromBottom (10).toNearestInt(), juce::Justification::centred);
    }
}

void RetroKeyboard::drawBlackNote (int, juce::Graphics& g, juce::Rectangle<float> area, bool isDown, bool isOver, juce::Colour)
{
    auto r = area.reduced (1.5f, 0.0f).withTrimmedTop (-4.0f);
    g.setColour (isDown ? violet : (isOver ? juce::Colour (0xff2a1a3a) : black));
    g.fillRoundedRectangle (r, 2.5f);
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
    search.setIndents (4, 7);
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

    for (auto* k : { &volume, &modKnob, &filterKnob, &transpose })
    {
        k->setDoubleClickReturnValue (true, 0.0);
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
    const auto knobs = volArea.getUnion (trArea).withTrimmedBottom (-16);
    tutorial.start ({
        { "BIENVENIDO A TMU",  "Todos tus soundfonts en una consola. Son miles de sonidos ordenados para que encuentres rapido lo que buscas.\n\nEn 6 pasos te muestro como se usa.", {} },
        { "LA PANTALLA",       "Elegi un sonido con un click o con las flechas arriba/abajo. A la izquierda estan los grupos. Arriba escribis para buscar: bass, pad, mario...", lcdArea },
        { "COMO SE ORDENA",    "TIPO: pianos, bajos, pads...\nSF2: por soundfont.\nA-Z: alfabetico.\nSIN REP: oculta los sonidos repetidos y los que no suenan.", pills },
        { "FAVORITOS",         "Toca FAV (o las teclas 1, 2, 3) para darle estrellas al sonido elegido. Los vas a encontrar en el grupo FAVORITOS de la pantalla.", favButton.getBounds() },
        { "PERILLAS",          "VOL, MOD (vibrato, tambien con la rueda de tu teclado), FILTRO (izq. corta agudos, der. corta graves) y ST (semitonos). Click derecho: asignar a una perilla de tu MIDI.", knobs },
        { "PROBA Y SUMA MAS",  "Toca el mini piano con el mouse. < > pasan de sonido. CARPETA abre la carpeta para sumar tus .sf2 y RESCAN los carga.", keySlot.getUnion (folderButton.getBounds()) },
        { "LISTO!",            "Si te olvidas de algo, toca ? arriba a la derecha y vuelve este tutorial.", helpButton.getBounds() },
    });
    tutorial.setBounds (getLocalBounds());
}

//==============================================================================
void TMUEditor::buildChassis()
{
    const float scale = 2.0f;
    chassis = juce::Image (juce::Image::ARGB, (int) (W * scale), (int) (H * scale), true);
    juce::Graphics g (chassis);
    g.addTransform (juce::AffineTransform::scale (scale));

    g.fillAll (outerBg);
    juce::ColourGradient floor (juce::Colour (0xff160c1e), W * 0.5f, H * 0.55f, outerBg, 0, 0, true);
    g.setGradientFill (floor);
    g.fillAll();

    // ---- cuerpo negro
    auto body = juce::Rectangle<float> (30, 24, W - 60, H - 48);
    g.setColour (juce::Colours::black.withAlpha (0.7f));
    g.fillRoundedRectangle (body.translated (0, 12), 40.0f);
    juce::ColourGradient bg (bodyTop, 0, body.getY(), black, 0, body.getY() + body.getHeight() * 0.6f, false);
    g.setGradientFill (bg);
    g.fillRoundedRectangle (body, 40.0f);
    g.setColour (juce::Colours::white.withAlpha (0.10f));
    g.drawRoundedRectangle (body.reduced (0.5f).translated (0, 1), 40.0f, 1.2f);

    // ---- franja violeta superior
    {
        juce::Path p;
        p.addRoundedRectangle (body.getX(), body.getY(), body.getWidth(), 96.0f, 40.0f, 40.0f, true, true, false, false);
        juce::ColourGradient vg (juce::Colour (0xffb06bff), 0, body.getY(), purple, 0, body.getY() + 96, false);
        vg.addColour (0.5, violet);
        g.setGradientFill (vg);
        g.fillPath (p);
        juce::ColourGradient shine (juce::Colours::white.withAlpha (0.25f), 0, body.getY(), juce::Colours::transparentWhite, 0, body.getY() + 24, false);
        g.setGradientFill (shine);
        g.fillPath (p);
        g.setColour (juce::Colours::black.withAlpha (0.35f));
        g.fillRect (body.getX(), body.getY() + 93, body.getWidth(), 3.0f);
    }
    g.setFont (look.pixel (46.0f));
    g.setColour (purpleDark);
    g.drawText ("TMU", juce::Rectangle<int> (74, 40, 220, 64).translated (4, 4), juce::Justification::centredLeft);
    g.setColour (black);
    g.drawText ("TMU", juce::Rectangle<int> (74, 40, 220, 64), juce::Justification::centredLeft);
    g.setFont (scriptFont (30.0f));
    g.drawText ("viszeversa", juce::Rectangle<int> (240, 46, 200, 50), juce::Justification::centredLeft);

    // ---- ranura del cartucho (pantalla)
    auto slot = lcdArea.toFloat();
    g.setColour (juce::Colours::white.withAlpha (0.07f));
    g.fillRoundedRectangle (slot.translated (0, 1.5f).expanded (1), 16.0f);
    g.setColour (juce::Colour (0xff040305));
    g.fillRoundedRectangle (slot, 16.0f);

    // ---- rejilla de ventilacion
    for (int i = 0; i < 26; ++i)
    {
        const float x = 80.0f + (float) i * 16.0f;
        g.setColour (juce::Colours::black);
        g.fillRoundedRectangle (x, 650.0f, 8.0f, 26.0f, 3.0f);
        g.setColour (juce::Colours::white.withAlpha (0.05f));
        g.fillRect (x, 676.0f, 8.0f, 1.0f);
    }

    // ---- etiquetas
    g.setFont (look.pixel (8.0f));
    g.setColour (lavender.withAlpha (0.8f));
    auto knobLabel = [&] (juce::Rectangle<int> a, const char* t) { g.drawText (t, a.withY (a.getBottom() + 6).withHeight (10).expanded (20, 0), juce::Justification::centred); };
    knobLabel (volArea, "VOL");
    knobLabel (modArea, "MOD");
    knobLabel (filterArea, "FILTRO");
    g.drawText ("POWER", juce::Rectangle<int> (prevButton.getX(), 512, 80, 12), juce::Justification::centredLeft);
    g.drawText ("SONIDO", prevButton.getBounds().getUnion (nextButton.getBounds()).withY (prevButton.getY() - 14).withHeight (10), juce::Justification::centred);
}

void TMUEditor::paint (juce::Graphics& g)
{
    g.drawImage (chassis, getLocalBounds().toFloat());
    drawLed (g, { (float) prevButton.getX() + 88.0f, 517.0f }, 5.0f);

    // ---- vidrio
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
    glowText (g, top, topRow, juce::Justification::centredLeft, screenDim.brighter (0.7f), 0.0f);
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
    for (auto* k : { &volume, &modKnob, &filterKnob, &transpose })
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
    lcdArea = { 70, 140, W - 140, 340 };
    auto glass = lcdArea.reduced (14).reduced (18, 10);
    miniLcdArea = { glass.getX(), glass.getBottom() - 36, glass.getWidth(), 36 };
    keySlot = { 80, 506, 410, 126 };
    volArea = { 520, 512, 62, 62 };
    modArea = { 594, 512, 62, 62 };
    filterArea = { 668, 512, 62, 62 };
    trArea = { 742, 512, 62, 62 };

    // botones de la franja violeta
    int x = W - 60;
    struct B { juce::TextButton* b; int w; };
    for (auto b : { B { &helpButton, 40 }, B { &favButton, 78 }, B { &dupButton, 96 }, B { &viewAZ, 62 }, B { &viewSource, 62 }, B { &viewType, 72 } })
    {
        x -= b.w;
        b.b->setBounds (x, 54, b.w, 32);
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

    prevButton.setBounds (832, 546, 52, 30);
    nextButton.setBounds (888, 546, 52, 30);
    rescanButton.setBounds (832, 588, 108, 30);
    folderButton.setBounds (832, 626, 108, 30);

    keyboard.setBounds (keySlot);
    keyboard.setKeyWidth ((float) keySlot.getWidth() / 22.0f);   // C4..C7 = 22 teclas blancas

    tutorial.setBounds (getLocalBounds());
    buildChassis();
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

    if (++tick % 8 == 0 || proc.getLearningParam().isNotEmpty())
    {
        repaint (miniLcdArea.expanded (4));
        repaint (volArea.getUnion (trArea).expanded (30, 20));
        repaint (lcdArea.withHeight (70));
    }

    const int row = presetList.getSelectedRow();
    const bool selFav = entries != nullptr && row >= 0 && row < (int) filtered.size() && prefs->getStars ((*entries)[(size_t) filtered[(size_t) row]].key) > 0;
    if (favButton.getToggleState() != selFav) favButton.setToggleState (selFav, juce::dontSendNotification);
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
              juce::Justification::centredLeft, col, selected ? 0.2f : 0.1f);
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
    glowText (g, e.name.toUpperCase(), r, juce::Justification::centredLeft, textCol, isLoaded ? 0.25f : 0.12f);
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
