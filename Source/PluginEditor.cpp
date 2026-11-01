#include "PluginEditor.h"
#include <BinaryData.h>

namespace
{
// ---- paleta "netbook": tapa negra mate, plastico blanco, teclas chiclet negras,
// letras celestes de la tecla Fn y escritorio azul de fondo en la pantalla
const juce::Colour bezel      (0xff111113);
const juce::Colour deckTop    (0xfff0eee9);
const juce::Colour deckBottom (0xffdcd9d2);
const juce::Colour keyCap     (0xff1d1d20);
const juce::Colour keyCapOver (0xff2a2a2f);
const juce::Colour legend     (0xffeeeeee);
const juce::Colour fnBlue     (0xff3d9de6);
const juce::Colour ledBlue    (0xff59b8ff);
const juce::Colour ink        (0xff57544f);   // texto impreso sobre el plastico blanco
const juce::Colour winDeep    (0xff040b26);
const juce::Colour winBlue    (0xff1a56c4);
const juce::Colour scrHi      (0xffffffff);
const juce::Colour screenTxt  (0xffdce7ff);
const juce::Colour screenDim  (0xff7d93c2);
const juce::Colour selBlue    (0xff3399ff);
const juce::Colour starGold   (0xffffd24a);
const juce::Colour newMint    (0xff62f5a8);   // soundfonts recien agregados (RECIENTES)

constexpr int W = 1000, H = 658;
constexpr int M = 70;            // margen izquierdo / derecho de todo el contenido
constexpr int R = W - M;         // borde derecho
constexpr int HEADER = 64;       // franja de arriba de la tapa (botones)
constexpr int HINGE = 436;       // donde termina la tapa y empieza la bisagra
constexpr int DECK = 446;        // donde empieza el plastico blanco

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

// texto de la pantalla con sombrita, como los iconos del escritorio
void glowText (juce::Graphics& g, const juce::String& s, juce::Rectangle<int> r, juce::Justification j, juce::Colour c, float glow = 0.0f)
{
    g.setColour (juce::Colours::black.withAlpha (0.55f + glow));
    g.drawText (s, r.translated (1, 1), j, true);
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
    juce::ColourGradient glow (ledBlue.withAlpha (0.55f), c, ledBlue.withAlpha (0.0f), c.translated (radius * 3.0f, 0), true);
    g.setGradientFill (glow);
    g.fillEllipse (juce::Rectangle<float> (radius * 6, radius * 6).withCentre (c));
    g.setColour (ledBlue.brighter (0.4f));
    g.fillEllipse (juce::Rectangle<float> (radius * 2, radius * 2).withCentre (c));
}

// tecla chiclet negra (botones, piano)
void drawKeyCap (juce::Graphics& g, juce::Rectangle<float> r, bool over, bool down, bool lit, float rad = 4.0f)
{
    g.setColour (juce::Colours::black.withAlpha (0.45f));
    g.fillRoundedRectangle (r.translated (0, 1.5f), rad);
    auto f = r.translated (0, down ? 1.0f : 0.0f);
    juce::ColourGradient cg (lit ? fnBlue.darker (0.1f) : (over ? keyCapOver : keyCap).brighter (0.08f), 0, f.getY(),
                             lit ? fnBlue.darker (0.6f) : (over ? keyCapOver : keyCap).darker (0.25f), 0, f.getBottom(), false);
    g.setGradientFill (cg);
    g.fillRoundedRectangle (f, rad);
    g.setColour (juce::Colours::white.withAlpha (0.07f));
    g.drawHorizontalLine ((int) f.getY() + 1, f.getX() + rad, f.getRight() - rad);
}

// calco troquelado: la forma con borde blanco y sombrita; despues se pinta el contenido
void stickerBase (juce::Graphics& g, const juce::Path& shape, float border = 7.0f)
{
    juce::Path outline;
    juce::PathStrokeType (border * 2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded).createStrokedPath (outline, shape);
    outline.addPath (shape);
    outline.setUsingNonZeroWinding (true);
    g.setColour (juce::Colours::black.withAlpha (0.35f));
    g.fillPath (outline, juce::AffineTransform::translation (1.2f, 2.2f));
    g.setColour (juce::Colour (0xfff8f8f5));
    g.fillPath (outline);
}

juce::Path textPath (const juce::Font& f, const juce::String& t, float cx, float cy)
{
    juce::GlyphArrangement ga;
    ga.addLineOfText (f, t, 0.0f, 0.0f);
    juce::Path p;
    ga.createPath (p);
    const auto b = p.getBounds();
    p.applyTransform (juce::AffineTransform::translation (cx - b.getCentreX(), cy - b.getCentreY()));
    return p;
}
} // namespace

//==============================================================================
TMULook::TMULook()
{
    accent = fnBlue;
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
    setColour (juce::TextButton::textColourOffId, legend);
    setColour (juce::TextButton::textColourOnId, legend);
    setColour (juce::MidiKeyboardComponent::shadowColourId, juce::Colours::transparentBlack);
    setColour (juce::MidiKeyboardComponent::keySeparatorLineColourId, juce::Colours::transparentBlack);
    setColour (juce::MidiKeyboardComponent::whiteNoteColourId, juce::Colours::transparentBlack);
    setColour (juce::BubbleComponent::backgroundColourId, bezel);
    setColour (juce::BubbleComponent::outlineColourId, fnBlue);
    setColour (juce::TooltipWindow::textColourId, legend);
    setColour (juce::TextEditor::textColourId, scrHi);
    setColour (juce::TextEditor::highlightColourId, selBlue.withAlpha (0.45f));
    setColour (juce::TextEditor::highlightedTextColourId, juce::Colours::white);
    setColour (juce::CaretComponent::caretColourId, scrHi);
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

    // aro impreso en el plastico + valor en celeste (el color de la tecla Fn)
    juce::Path track, value;
    track.addCentredArc (c.x, c.y, rOuter - 3, rOuter - 3, 0, start, end, true);
    const float from = bipolar ? (start + end) * 0.5f : start;
    value.addCentredArc (c.x, c.y, rOuter - 3, rOuter - 3, 0, juce::jmin (from, angle), juce::jmax (from, angle), true);
    g.setColour (juce::Colour (0xffc6c2ba));
    g.strokePath (track, juce::PathStrokeType (3.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour (fnBlue);
    g.strokePath (value, juce::PathStrokeType (3.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // perilla: plastico negro como las teclas
    const float rKnob = rOuter - 8.0f;
    auto kr = juce::Rectangle<float> (rKnob * 2, rKnob * 2).withCentre (c);
    g.setColour (juce::Colours::black.withAlpha (0.3f));
    g.fillEllipse (kr.translated (0, 2.5f));
    juce::ColourGradient body (juce::Colour (0xff35353a), kr.getX(), kr.getY(), juce::Colour (0xff111113), kr.getRight(), kr.getBottom(), false);
    g.setGradientFill (body);
    g.fillEllipse (kr);
    g.setColour (juce::Colours::white.withAlpha (0.08f));
    g.drawEllipse (kr.reduced (1.0f), 1.0f);
    const juce::Point<float> p0 (c.x + (rKnob * 0.25f) * std::sin (angle), c.y - (rKnob * 0.25f) * std::cos (angle));
    const juce::Point<float> p1 (c.x + (rKnob - 4) * std::sin (angle), c.y - (rKnob - 4) * std::cos (angle));
    g.setColour (legend);
    g.drawLine ({ p0, p1 }, 2.5f);
}

void TMULook::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool over, bool down)
{
    // todos los botones son teclas chiclet negras; la activa se prende en celeste por abajo
    auto r = b.getLocalBounds().toFloat().reduced (1.0f, 1.5f);
    drawKeyCap (g, r, over, down, false);
    if (b.getToggleState())
    {
        g.setColour (fnBlue);
        g.fillRoundedRectangle (r.getX() + 8.0f, r.getBottom() - (down ? 3.0f : 4.0f), r.getWidth() - 16.0f, 2.0f, 1.0f);
    }
}

void TMULook::drawButtonText (juce::Graphics& g, juce::TextButton& b, bool over, bool down)
{
    auto r = b.getLocalBounds().translated (0, down ? 1 : 0);
    const bool on = b.getToggleState();
    const auto col = on ? fnBlue.brighter (0.35f) : (over ? juce::Colours::white : legend);
    if (b.getProperties()["star"])
    {
        g.setColour (on ? starGold : col);
        g.fillPath (starPath ({ (float) r.getCentreX() - 18.0f, (float) r.getCentreY() }, 6.0f));
        r.removeFromLeft (r.getWidth() / 2 - 8);
        g.setFont (pixel (10.0f));
        g.setColour (col);
        g.drawText (b.getButtonText(), r, juce::Justification::centredLeft);
        return;
    }
    g.setFont (pixel (10.0f));
    g.setColour (col);
    g.drawText (b.getButtonText(), r, juce::Justification::centred);
}

void TMULook::drawScrollbar (juce::Graphics& g, juce::ScrollBar&, int x, int y, int w, int h, bool vertical,
                             int thumbStart, int thumbSize, bool over, bool)
{
    juce::Rectangle<int> thumb = vertical ? juce::Rectangle<int> (x + 2, thumbStart, w - 4, thumbSize)
                                          : juce::Rectangle<int> (thumbStart, y + 2, thumbSize, h - 4);
    g.setColour (juce::Colours::white.withAlpha (over ? 0.45f : 0.28f));
    g.fillRoundedRectangle (thumb.toFloat(), 2.0f);
}

//==============================================================================
// Mini piano con teclas chiclet de netbook: las "blancas" son teclas negras con la nota escrita,
// las "negras" son mas chicas y tienen el puntito celeste de las teclas con Fn
void RetroKeyboard::drawWhiteNote (int note, juce::Graphics& g, juce::Rectangle<float> area, bool isDown, bool isOver, juce::Colour, juce::Colour)
{
    auto r = area.reduced (1.5f, 0.0f).withTrimmedTop (2.0f).withTrimmedBottom (2.0f);
    drawKeyCap (g, r, isOver, isDown, isDown, 3.0f);
    if (note % 12 == 0)
    {
        g.setFont (look.pixel (7.0f));
        g.setColour (isDown ? juce::Colours::white : legend.withAlpha (0.85f));
        g.drawText ("C" + juce::String (note / 12), r.withTrimmedBottom (5).removeFromBottom (10).toNearestInt(), juce::Justification::centred);
    }
}

void RetroKeyboard::drawBlackNote (int, juce::Graphics& g, juce::Rectangle<float> area, bool isDown, bool isOver, juce::Colour)
{
    auto r = area.reduced (1.0f, 0.0f).withTrimmedTop (2.0f);
    g.setColour (juce::Colour (0xff050506));   // canaleta entre teclas
    g.fillRoundedRectangle (r.expanded (2.0f, 0.0f).withTrimmedTop (-2.0f).withTrimmedBottom (-2.0f), 3.0f);
    juce::ColourGradient cg (isDown ? fnBlue : (isOver ? juce::Colour (0xff45454c) : juce::Colour (0xff38383e)), 0, r.getY(),
                             isDown ? fnBlue.darker (0.6f) : juce::Colour (0xff1c1c20), 0, r.getBottom(), false);
    g.setGradientFill (cg);
    g.fillRoundedRectangle (r, 2.5f);
    g.setColour (isDown ? juce::Colours::white : fnBlue);
    g.fillEllipse (juce::Rectangle<float> (3.0f, 3.0f).withCentre ({ r.getCentreX(), r.getBottom() - 6.0f }));
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
        g.setColour (scrHi);
        for (float i = 0; i < st.spot.getWidth() + 12; i += 10)   // borde punteado
        {
            g.fillRect ((float) st.spot.getX() - 6 + i, (float) st.spot.getY() - 7, 5.0f, 2.0f);
            g.fillRect ((float) st.spot.getX() - 6 + i, (float) st.spot.getBottom() + 5, 5.0f, 2.0f);
        }
    }

    auto box = boxArea();
    g.setColour (juce::Colours::black.withAlpha (0.6f));
    g.fillRoundedRectangle (box.toFloat().translated (0, 5), 14.0f);
    juce::ColourGradient tb (juce::Colour (0xff10224f), 0, (float) box.getY(), juce::Colour (0xff060d24), 0, (float) box.getBottom(), false);
    g.setGradientFill (tb);
    g.fillRoundedRectangle (box.toFloat(), 8.0f);
    g.setColour (selBlue.withAlpha (0.8f));
    g.drawRoundedRectangle (box.toFloat().reduced (1), 8.0f, 1.5f);

    auto inner = box.reduced (18, 16);
    g.setFont (look.pixel (8.0f));
    g.setColour (screenDim.brighter (0.4f));
    g.drawText (juce::String ((int) index + 1) + "/" + juce::String ((int) steps.size()), inner.removeFromTop (12), juce::Justification::centredRight);
    g.setFont (look.pixel (13.0f));
    glowText (g, st.title, inner.removeFromTop (22), juce::Justification::centredLeft, scrHi);
    inner.removeFromTop (8);
    juce::AttributedString as;
    as.append (st.text, look.pixel (9.0f), screenTxt);
    as.setLineSpacing (7.0f);
    as.setWordWrap (juce::AttributedString::byWord);
    juce::TextLayout tl;
    tl.createLayout (as, (float) inner.getWidth());
    tl.draw (g, inner.withTrimmedBottom (40).toFloat());

    const bool last = index + 1 >= steps.size();
    g.setColour (fnBlue);
    g.fillRoundedRectangle (nextArea().toFloat(), 5.0f);
    g.setFont (look.pixel (9.0f));
    g.setColour (juce::Colours::white);
    g.drawText (last ? "A JUGAR!" : "SIGUIENTE >", nextArea(), juce::Justification::centred);
    if (! last)
    {
        g.setColour (screenTxt.withAlpha (0.6f));
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
LicenseOverlay::LicenseOverlay (TMULook& l) : look (l)
{
    setInterceptsMouseClicks (true, true);
    for (auto* ed : { &emailBox, &serialBox })
    {
        ed->setFont (look.pixel (12.0f));
        ed->setIndents (8, 6);
        ed->setColour (juce::TextEditor::backgroundColourId, winDeep);
        ed->setColour (juce::TextEditor::textColourId, scrHi);
        ed->setColour (juce::TextEditor::outlineColourId, winBlue);
        ed->setColour (juce::TextEditor::focusedOutlineColourId, selBlue);
        ed->setColour (juce::TextEditor::highlightColourId, winBlue);
        ed->setColour (juce::CaretComponent::caretColourId, scrHi);
        ed->setColour (juce::TextEditor::outlineColourId, juce::Colours::transparentBlack);
        ed->setColour (juce::TextEditor::focusedOutlineColourId, juce::Colours::transparentBlack);
        ed->onFocusLost = [this] { repaint(); };
        addAndMakeVisible (*ed);
    }
    emailBox.setTextToShowWhenEmpty ("el mail con el que compraste", screenDim);
    serialBox.setTextToShowWhenEmpty ("pega aca el numero de serie", screenDim);
    serialBox.setMultiLine (true, true);
    serialBox.setReturnKeyStartsNewLine (false);
    emailBox.onReturnKey = serialBox.onReturnKey = [this] { tryActivate(); };
    activateButton.onClick = [this] { tryActivate(); };
    addAndMakeVisible (activateButton);
}

void LicenseOverlay::tryActivate()
{
    if (busy) return;
    const auto mail = emailBox.getText(), serial = serialBox.getText();
    if (! tmulicense::verify (mail, serial))
    {
        error = true;
        message = "El mail y el serial no coinciden. Revisa que sea el mail de la compra.";
        repaint();
        return;
    }

    busy = true;
    error = false;
    message = "Activando... (la primera vez necesita internet)";
    activateButton.setEnabled (false);
    repaint();

    juce::Component::SafePointer<LicenseOverlay> safe (this);
    juce::Thread::launch ([safe, mail, serial]
    {
        const auto r = tmulicense::activateOnline (mail, serial);
        juce::MessageManager::callAsync ([safe, mail, serial, r]
        {
            if (auto* self = safe.getComponent())
                self->onlineResult (mail, serial, r);
        });
    });
}

void LicenseOverlay::onlineResult (const juce::String& mail, const juce::String& serial, tmulicense::OnlineResult r)
{
    busy = false;
    activateButton.setEnabled (true);
    using R = tmulicense::OnlineResult;
    if (r == R::ok && state->activate (mail, serial))
    {
        error = false;
        message = {};
        setVisible (false);
        if (onActivated) onActivated();
        return;
    }
    error = true;
    message = r == R::limit    ? "Esta licencia ya se activo en el maximo de compus. Escribime si cambiaste de compu."
            : r == R::rejected ? "Esta licencia no es valida. Escribime a tmucontacto@gmail.com."
                               : "No me pude conectar. Revisa tu internet y toca ACTIVAR de nuevo.";
    repaint();
}

void LicenseOverlay::resized()
{
    auto inner = boxArea().reduced (28, 24);
    inner.removeFromTop (118);
    emailBox.setBounds (inner.removeFromTop (34));
    inner.removeFromTop (12);
    serialBox.setBounds (inner.removeFromTop (70));
    inner.removeFromTop (16);
    activateButton.setBounds (inner.removeFromTop (34).removeFromRight (150));
}

void LicenseOverlay::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colours::black.withAlpha (0.82f));
    auto box = boxArea();
    g.setColour (juce::Colour (0xff0a1f55));
    g.fillRoundedRectangle (box.toFloat(), 14.0f);
    g.setColour (selBlue);
    g.drawRoundedRectangle (box.toFloat().reduced (1), 14.0f, 2.0f);

    auto inner = box.reduced (28, 24);
    g.setFont (look.pixel (18.0f));
    glowText (g, "ACTIVA TMU", inner.removeFromTop (28), juce::Justification::centredLeft, scrHi);
    inner.removeFromTop (8);
    juce::AttributedString as;
    as.append ("Pone el mail con el que compraste y el numero de serie que te llego. "
               "Se activa una sola vez en esta compu. Si no lo tenes, escribi a tmucontacto@gmail.com.",
               look.pixel (10.0f), screenTxt);
    as.setLineSpacing (5.0f);
    as.setWordWrap (juce::AttributedString::byWord);
    juce::TextLayout tl;
    tl.createLayout (as, (float) inner.getWidth());
    tl.draw (g, inner.removeFromTop (70).toFloat());

    for (auto* ed : { &emailBox, &serialBox })
    {
        g.setColour (winDeep);
        g.fillRoundedRectangle (ed->getBounds().toFloat().expanded (3), 8.0f);
        g.setColour (ed->hasKeyboardFocus (true) ? selBlue : winBlue);
        g.drawRoundedRectangle (ed->getBounds().toFloat().expanded (3), 8.0f, 1.5f);
    }

    auto msgArea = juce::Rectangle<int> (box.getX() + 28, activateButton.getY(), activateButton.getX() - box.getX() - 40, activateButton.getHeight());
    g.setFont (look.pixel (9.0f));
    g.setColour (error ? juce::Colour (0xffff6b8a) : screenDim.brighter (0.5f));
    g.drawFittedText (message.isNotEmpty() ? message : juce::String ("viszeversza.shop"), msgArea, juce::Justification::centredLeft, 2);
}

//==============================================================================
// Capas
juce::Rectangle<int> LayerStrip::slot (int i) const
{
    const int n = TMUProcessor::numLayers;
    const int w = (getWidth() - gap * (n - 1)) / n;
    return { i * (w + gap), 0, w, getHeight() };
}

juce::Rectangle<int> LayerStrip::ledArea (int i) const
{
    const auto s = slot (i);
    return juce::Rectangle<int> (22, 22).withPosition (s.getX() + 4, s.getCentreY() - 11);
}

juce::Rectangle<int> LayerStrip::barArea (int i) const
{
    const auto s = slot (i);
    return { s.getX() + 34, s.getBottom() - 13, s.getWidth() - 34 - 8, 11 };
}

int LayerStrip::slotAt (juce::Point<int> p) const
{
    for (int i = 0; i < TMUProcessor::numLayers; ++i)
        if (slot (i).contains (p)) return i;
    return -1;
}

bool LayerStrip::isOn (int i) const
{
    return proc.params.getRawParameterValue (TMUProcessor::layerOnId (i))->load() > 0.5f;
}

void LayerStrip::refresh()
{
    juce::String sig;
    for (int i = 0; i < TMUProcessor::numLayers; ++i)
    {
        const auto info = proc.getLoadedInfo (i);
        sig << info.name << "|" << (int) info.loading << "|" << info.error << "|" << (int) isOn (i) << "|";
        if (auto* prm = proc.params.getParameter (TMUProcessor::layerParamId ("volume", i))) sig << prm->getValue() << "|";
    }
    sig << proc.getActiveLayer();
    if (sig != lastSig)
    {
        lastSig = sig;
        repaint();
    }
}

void LayerStrip::paint (juce::Graphics& g)
{
    for (int i = 0; i < TMUProcessor::numLayers; ++i)
    {
        const auto s = slot (i);
        const bool selected = i == proc.getActiveLayer();
        const auto info = proc.getLoadedInfo (i);
        const bool has = info.phdrIndex >= 0 && info.name.isNotEmpty();
        const bool on = isOn (i);

        g.setColour (juce::Colours::black.withAlpha (selected ? 0.58f : 0.38f));
        g.fillRoundedRectangle (s.toFloat(), 4.0f);
        g.setColour (selected ? selBlue : juce::Colours::white.withAlpha (0.16f));
        g.drawRoundedRectangle (s.toFloat().reduced (0.5f), 4.0f, selected ? 1.6f : 1.0f);

        // cuadrito con el numero: prendido (celeste) = la capa suena
        const auto led = ledArea (i);
        const bool lit = has && on;
        g.setColour (lit ? fnBlue : juce::Colours::white.withAlpha (0.14f));
        g.fillRoundedRectangle (led.toFloat(), 4.0f);
        g.setFont (look.pixel (10.0f));
        g.setColour (lit ? juce::Colours::white : screenDim);
        g.drawText (juce::String (i + 1), led, juce::Justification::centred);

        // nombre del sonido (o el volumen mientras se arrastra la barra)
        juce::String label;
        auto col = on ? screenTxt : screenDim;
        if (dragLayer == i)
        {
            label = juce::String (proc.params.getRawParameterValue (TMUProcessor::layerParamId ("volume", i))->load(), 1) + " dB";
            col = scrHi;
        }
        else if (info.loading)           { label = "CARGANDO..."; col = screenDim.brighter (0.5f); }
        else if (has)                    label = info.name.toUpperCase();
        else if (info.error.isNotEmpty()) { label = "ERROR"; col = juce::Colour (0xffff6b8a); }
        else                             { label = i == 0 ? juce::String ("---") : juce::String ("+ SUMAR SONIDO"); col = screenDim; }
        g.setFont (look.pixel (9.0f));
        glowText (g, label, { s.getX() + 34, s.getY() + 3, s.getWidth() - 34 - 8, 14 }, juce::Justification::centredLeft, col);

        // barra de volumen con la marca de 0 dB
        if (auto* prm = proc.params.getParameter (TMUProcessor::layerParamId ("volume", i)))
        {
            const auto bar = barArea (i).withSizeKeepingCentre (barArea (i).getWidth(), 4).toFloat();
            g.setColour (juce::Colours::white.withAlpha (0.16f));
            g.fillRoundedRectangle (bar, 2.0f);
            g.setColour (on ? selBlue.brighter (0.3f) : screenDim.withAlpha (0.7f));
            g.fillRoundedRectangle (bar.withWidth (juce::jmax (2.0f, bar.getWidth() * prm->getValue())), 2.0f);
            g.setColour (juce::Colours::white.withAlpha (0.45f));
            g.fillRect (bar.getX() + bar.getWidth() * prm->convertTo0to1 (0.0f) - 0.5f, bar.getY() - 2.0f, 1.0f, bar.getHeight() + 4.0f);
        }
    }
}

void LayerStrip::selectLayer (int i)
{
    if (i == proc.getActiveLayer()) return;
    proc.setActiveLayer (i);
    if (onChange) onChange();
    repaint();
}

void LayerStrip::toggleOn (int i)
{
    if (auto* prm = proc.params.getParameter (TMUProcessor::layerOnId (i)))
    {
        prm->beginChangeGesture();
        prm->setValueNotifyingHost (isOn (i) ? 0.0f : 1.0f);
        prm->endChangeGesture();
    }
    repaint();
}

void LayerStrip::resetVolume (int i)
{
    if (auto* prm = proc.params.getParameter (TMUProcessor::layerParamId ("volume", i)))
    {
        prm->beginChangeGesture();
        prm->setValueNotifyingHost (prm->convertTo0to1 (0.0f));
        prm->endChangeGesture();
    }
    repaint();
}

void LayerStrip::setVolumeFromX (int i, int x)
{
    auto* prm = proc.params.getParameter (TMUProcessor::layerParamId ("volume", i));
    if (prm == nullptr) return;
    const auto bar = barArea (i);
    float n = juce::jlimit (0.0f, 1.0f, (float) (x - bar.getX()) / (float) juce::jmax (1, bar.getWidth()));
    const float zero = prm->convertTo0to1 (0.0f);
    if (std::abs (n - zero) < 0.02f) n = zero;   // se pega a 0 dB
    prm->setValueNotifyingHost (n);
    repaint();
}

void LayerStrip::mouseDown (const juce::MouseEvent& e)
{
    const int i = slotAt (e.getPosition());
    if (i < 0) return;
    selectLayer (i);

    if (e.mods.isPopupMenu()) { showMenu (i); return; }

    if (ledArea (i).expanded (3).contains (e.getPosition()))
        toggleOn (i);
    else if (barArea (i).expanded (0, 3).contains (e.getPosition()))
    {
        dragLayer = i;
        if (auto* prm = proc.params.getParameter (TMUProcessor::layerParamId ("volume", i))) prm->beginChangeGesture();
        setVolumeFromX (i, e.x);
    }
}

void LayerStrip::mouseDrag (const juce::MouseEvent& e)
{
    if (dragLayer >= 0) setVolumeFromX (dragLayer, e.x);
}

void LayerStrip::mouseUp (const juce::MouseEvent&)
{
    if (dragLayer < 0) return;
    if (auto* prm = proc.params.getParameter (TMUProcessor::layerParamId ("volume", dragLayer))) prm->endChangeGesture();
    dragLayer = -1;
    repaint();
}

void LayerStrip::mouseDoubleClick (const juce::MouseEvent& e)
{
    const int i = slotAt (e.getPosition());
    if (i >= 0 && barArea (i).expanded (0, 3).contains (e.getPosition()))
        resetVolume (i);
}

void LayerStrip::showMenu (int i)
{
    const auto info = proc.getLoadedInfo (i);
    const bool has = info.phdrIndex >= 0 && info.name.isNotEmpty();
    juce::PopupMenu m;
    m.addSectionHeader ("Capa " + juce::String (i + 1));
    m.addItem (isOn (i) ? "Apagar capa" : "Prender capa", [this, i] { toggleOn (i); });
    m.addItem ("Volumen a 0 dB", [this, i] { resetVolume (i); });
    m.addItem ("Vaciar capa", i > 0 && has, false, [this, i]
    {
        proc.clearLayer (i);
        proc.resetLayerParams (i);
        if (onChange) onChange();
        repaint();
    });
    m.showMenuAsync (juce::PopupMenu::Options().withMousePosition());
}

//==============================================================================
TMUEditor::TMUEditor (TMUProcessor& p)
    : juce::AudioProcessorEditor (&p), proc (p),
      keyboard (p.keyboardState, look),
      layerStrip (p, look)
{
    setLookAndFeel (&look);

    for (auto* b : { &viewType, &viewSource, &viewAZ, &dupButton, &favButton, &helpButton, &getButton })
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
    getButton.setTooltip ("Donde conseguir soundfonts gratis (paginas de sus autores)");
    getButton.onClick = [this] { showSoundfontLinks(); };
    favButton.onClick  = [this]
    {
        const int row = presetList.getSelectedRow();
        if (entries == nullptr || row < 0 || row >= (int) filtered.size()) return;
        setStarsForRow (row, (prefs->getStars ((*entries)[(size_t) filtered[(size_t) row]].key) + 1) % 4);
    };

    search.setFont (look.pixel (13.0f));
    search.setIndents (4, 3);
    search.setColour (juce::TextEditor::textColourId, scrHi);
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
        const auto velId = TMUProcessor::layerParamId ("velcurve", proc.getActiveLayer());
        if (auto* prm = proc.params.getParameter (velId))
        {
            const int n = ((int) proc.params.getRawParameterValue (velId)->load() + 1) % 4;
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

    attachKnobs();

    keyboard.setAvailableRange (48, 84);   // C4 a C7 (como FL)
    keyboard.setOctaveForMiddleC (5);
    keyboard.setBlackNoteLengthProportion (0.6f);
    keyboard.setWantsKeyboardFocus (false);
    keyboard.setScrollButtonsVisible (false);
    keyboard.setOpaque (false);
    addAndMakeVisible (keyboard);

    addAndMakeVisible (layerStrip);
    layerStrip.onChange = [this] { onLayerChanged(); };
    lastLayer = proc.getActiveLayer();

    addChildComponent (tutorial);
    addChildComponent (licenseOverlay);
    licenseOverlay.onActivated = [this] { if (! prefs->getTutorialSeen()) showTutorial(); };
    tutorial.onFinished = [this] { prefs->setTutorialSeen (true); prefsVersion = prefs->getVersion(); };

    setSize (W, H);

    currentGroup = proc.uiGroup;
    setView (proc.uiView.isEmpty() ? juce::String ("type") : proc.uiView);
    startTimerHz (15);

    if (! proc.license->isActive())
    {
        licenseOverlay.setVisible (true);
        licenseOverlay.toFront (true);
    }
    else if (! prefs->getTutorialSeen())
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
        { "BIENVENIDO A TMU",  "Todos tus soundfonts en una netbook. Son miles de sonidos ordenados para que encuentres rapido lo que buscas.\n\nEn 7 pasos te muestro como se usa.", {} },
        { "LA PANTALLA",       "Elegi un sonido con un click o con las flechas arriba/abajo. A la izquierda estan los grupos. Arriba escribis para buscar: bass, pad, mario...", lcdArea },
        { "COMO SE ORDENA",    "TIPO: pianos, bajos, pads...\nSF2: por soundfont.\nA-Z: alfabetico.\nSIN REP: oculta los sonidos repetidos y los que no suenan.", pills },
        { "FAVORITOS",         "Toca FAV (o las teclas 1, 2, 3) para darle estrellas al sonido elegido. Los vas a encontrar en el grupo FAVORITOS de la pantalla.", favButton.getBounds() },
        { "CAPAS",             "Suma hasta 4 sonidos que suenan a la vez. Toca una capa para elegirla y despues elegi el sonido en la lista. Cada capa tiene sus propias perillas: las de abajo manejan la capa elegida (dice CAPA 1, 2...). El cuadrito con el numero la prende o apaga, la barra es su volumen (doble click = 0 dB) y click derecho te deja vaciarla. Al sumar una capa nueva, sus perillas arrancan en 0.", layerArea },
        { "PERILLAS",          "MOD: vibrato y filtro que se mueven estilo Roland (arranca despues de un ratito, tambien con la rueda de tu teclado). RATE: velocidad. FILTRO, ST, SWING (atrasa los 16avos), HUMAN (velocity y tiempo humanos) y VEL (curva de velocity). Click derecho: asignar a tu MIDI.", knobs },
        { "PROBA Y SUMA MAS",  "Toca el mini piano con el mouse. < > pasan de sonido. CARPETA abre la carpeta para sumar tus .sf2 y RESCAN los carga: los sonidos nuevos aparecen en verde y en el grupo RECIENTES (14 dias). BAJAR SF2 (arriba) te muestra paginas con soundfonts gratis.", keySlot.getUnion (folderButton.getBounds()) },
        { "LISTO!",            "Si te olvidas de algo, toca ? arriba a la derecha y vuelve este tutorial.", helpButton.getBounds() },
    });
    tutorial.setBounds (getLocalBounds());
    licenseOverlay.setBounds (getLocalBounds());
}

//==============================================================================
void TMUEditor::buildChassis()
{
    const float scale = chassisScale;
    chassis = juce::Image (juce::Image::ARGB, (int) std::ceil (W * scale), (int) std::ceil (H * scale), true);
    juce::Graphics g (chassis);
    g.addTransform (juce::AffineTransform::scale (scale));
    juce::Random rnd (1115);   // siempre la misma "suciedad" y el mismo humo

    // ---- tapa negra mate
    {
        juce::ColourGradient lid (juce::Colour (0xff1a1a1d), 0, 0, bezel, 0, (float) HINGE, false);
        g.setGradientFill (lid);
        g.fillRect (0, 0, W, HINGE);
        // camara y microfono
        const juce::Point<float> cam ((float) W * 0.5f, 9.0f);
        g.setColour (juce::Colour (0xff2b2b30));
        g.fillEllipse (juce::Rectangle<float> (9, 9).withCentre (cam));
        g.setColour (juce::Colour (0xff070708));
        g.fillEllipse (juce::Rectangle<float> (5, 5).withCentre (cam));
        g.setColour (juce::Colour (0xff26262a));
        g.fillEllipse (juce::Rectangle<float> (3, 3).withCentre (cam.translated (-40, 0)));
        g.fillEllipse (juce::Rectangle<float> (3, 3).withCentre (cam.translated (40, 0)));
    }

    // ---- pantalla: escritorio azul con humo (se dibuja una sola vez)
    auto glass = lcdArea.toFloat().reduced (14.0f);
    {
        g.setColour (juce::Colour (0xff050506));
        g.fillRect (glass.expanded (3.0f));
        g.saveState();
        g.reduceClipRegion (glass.toNearestInt());
        g.setColour (winDeep);
        g.fillRect (glass);
        const juce::Point<float> core (glass.getX() + glass.getWidth() * 0.58f, glass.getY() + glass.getHeight() * 0.32f);
        juce::ColourGradient halo (winBlue.withAlpha (0.75f), core, winBlue.withAlpha (0.0f), core.translated (glass.getWidth() * 0.42f, 0), true);
        g.setGradientFill (halo);
        g.fillRect (glass);
        for (int i = 0; i < 22; ++i)   // humo
        {
            const float rx = 40.0f + rnd.nextFloat() * 150.0f, ry = rx * (0.35f + rnd.nextFloat() * 0.5f);
            const juce::Point<float> c (glass.getX() + rnd.nextFloat() * glass.getWidth(), glass.getY() + rnd.nextFloat() * glass.getHeight());
            const auto col = (i % 3 == 0 ? juce::Colour (0xff7fd0ff) : juce::Colour (0xff3a6cff)).withAlpha (0.10f + rnd.nextFloat() * 0.12f);
            juce::ColourGradient sm (col, c, col.withAlpha (0.0f), c.translated (rx, 0), true);
            g.setGradientFill (sm);
            g.fillEllipse (juce::Rectangle<float> (rx * 2, ry * 2).withCentre (c));
        }
        juce::ColourGradient spark (juce::Colour (0xccd8f2ff), core, juce::Colour (0x00d8f2ff), core.translated (38, 0), true);
        g.setGradientFill (spark);
        g.fillEllipse (juce::Rectangle<float> (76, 76).withCentre (core));
        // oscurecer debajo de las listas para que se lea bien
        g.setColour (juce::Colours::black.withAlpha (0.30f));
        g.fillRect (glass);
        juce::ColourGradient vig (juce::Colours::transparentBlack, glass.getCentreX(), glass.getCentreY(),
                                  juce::Colours::black.withAlpha (0.55f), glass.getX(), glass.getY(), true);
        g.setGradientFill (vig);
        g.fillRect (glass);
        // barra de tareas abajo
        auto bar = glass.withTop (glass.getBottom() - 50.0f);
        g.setColour (juce::Colour (0xd0060a18));
        g.fillRect (bar);
        g.setColour (juce::Colours::white.withAlpha (0.14f));
        g.fillRect (bar.withHeight (1.0f));
        g.restoreState();
    }

    // ---- calco "TMU" pegado en la tapa (troquelado con borde blanco)
    {
        juce::GlyphArrangement ga;
        ga.addLineOfText (look.heavy (46.0f), "TMU", 0.0f, 0.0f);
        juce::Path letters;
        ga.createPath (letters);
        const auto lb = letters.getBounds();
        const auto tr = juce::AffineTransform::translation (-lb.getX(), -lb.getY())
                            .rotated (-0.05f)
                            .translated ((float) M + 6.0f, (float) HEADER * 0.5f - lb.getHeight() * 0.5f + 2.0f);
        letters.applyTransform (tr);
        juce::Path outline;
        juce::PathStrokeType (12.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded).createStrokedPath (outline, letters);
        g.setColour (juce::Colours::black.withAlpha (0.45f));
        g.fillPath (outline, juce::AffineTransform::translation (1.5f, 2.5f));
        g.setColour (juce::Colour (0xfff7f7f4));
        g.fillPath (outline);
        g.fillPath (letters);
        const auto b = letters.getBounds();
        juce::ColourGradient lg (fnBlue.brighter (0.4f), 0, b.getY(), juce::Colour (0xff0d3f9a), 0, b.getBottom(), false);
        g.setGradientFill (lg);
        g.fillPath (letters);
        g.setColour (juce::Colour (0xff0a1a3a));
        g.strokePath (letters, juce::PathStrokeType (1.5f));

        // al lado, impreso en la tapa
        g.setFont (look.pixel (8.0f));
        g.setColour (juce::Colour (0xff8a8a90));
        g.drawText ("SOUND FONT PLAYER", juce::Rectangle<int> ((int) b.getRight() + 22, HEADER / 2 - 12, 200, 12), juce::Justification::centredLeft);
        g.setColour (juce::Colour (0xff5f5f66));
        g.drawText ("para compus de verdad", juce::Rectangle<int> ((int) b.getRight() + 22, HEADER / 2 + 1, 200, 12), juce::Justification::centredLeft);
    }

    // ---- abajo de la pantalla: dos rayitas y texto impreso (como en la tapa original)
    {
        const float y = glass.getBottom() + 12.0f;
        g.setColour (juce::Colour (0xff6e6e74));
        g.fillRect ((float) M, y, 20.0f, 5.0f);
        g.fillRect ((float) M, y + 9.0f, 20.0f, 5.0f);
        g.setFont (look.pixel (8.0f));
        g.setColour (juce::Colour (0xff8a8a90));
        g.drawText ("hecho en una netbook", juce::Rectangle<int> (M + 28, (int) y - 3, 200, 11), juce::Justification::centredLeft);
        g.drawText ("por viszeversza", juce::Rectangle<int> (M + 28, (int) y + 7, 200, 11), juce::Justification::centredLeft);
    }

    // ---- bisagra
    {
        juce::ColourGradient hg (juce::Colour (0xff2c2c30), 0, (float) HINGE, juce::Colour (0xff0b0b0c), 0, (float) DECK, false);
        g.setGradientFill (hg);
        g.fillRect (0, HINGE, W, DECK - HINGE);
    }

    // ---- plastico blanco, un poco gastado
    {
        juce::ColourGradient dg (deckTop, 0, (float) DECK, deckBottom, 0, (float) H, false);
        g.setGradientFill (dg);
        g.fillRect (0, DECK, W, H - DECK);
        g.setColour (juce::Colours::white.withAlpha (0.6f));
        g.fillRect (0, DECK, W, 1);
        for (int i = 0; i < 1400; ++i)
        {
            g.setColour (juce::Colour (0xff5a5045).withAlpha (0.03f + rnd.nextFloat() * 0.06f));
            const float sz = 0.8f + rnd.nextFloat() * 1.6f;
            g.fillEllipse ((float) rnd.nextInt (W), (float) (DECK + rnd.nextInt (H - DECK)), sz, sz);
        }
        g.setColour (juce::Colour (0xffbdb9b1));
        g.fillRect (0, H - 2, W, 2);
    }

    // ---- calco holografico con la firma viszeversa (pegado entre la tapa y la bisagra)
    {
        auto logo = juce::ImageCache::getFromMemory (BinaryData::viszeversa_png, BinaryData::viszeversa_pngSize);
        const float sh = 30.0f, lw = 150.0f;
        auto st = juce::Rectangle<float> (lw + 26.0f, sh).withPosition ((float) R - lw - 26.0f, glass.getBottom() + 14.0f);
        g.saveState();
        g.addTransform (juce::AffineTransform::rotation (-0.045f, st.getCentreX(), st.getCentreY()));
        g.setColour (juce::Colours::black.withAlpha (0.4f));
        g.fillRoundedRectangle (st.translated (1.5f, 2.5f).expanded (3.0f), 9.0f);
        g.setColour (juce::Colour (0xfff7f7f4));
        g.fillRoundedRectangle (st.expanded (3.0f), 9.0f);
        juce::ColourGradient holo (juce::Colour (0xffb9f3ff), st.getX(), st.getY(), juce::Colour (0xffffc6f1), st.getRight(), st.getBottom(), false);
        holo.addColour (0.35, juce::Colour (0xfffff4b0));
        holo.addColour (0.65, juce::Colour (0xffc9c4ff));
        g.setGradientFill (holo);
        g.fillRoundedRectangle (st, 7.0f);
        for (int i = 0; i < 60; ++i)   // brillitos
        {
            g.setColour (juce::Colours::white.withAlpha (0.35f + rnd.nextFloat() * 0.5f));
            g.fillRect (st.getX() + rnd.nextFloat() * st.getWidth(), st.getY() + rnd.nextFloat() * st.getHeight(), 1.5f, 1.5f);
        }
        const float h = sh * 0.62f, w = h * (float) logo.getWidth() / (float) juce::jmax (1, logo.getHeight());
        g.setColour (juce::Colour (0xff1b1b22));
        g.drawImage (logo, juce::Rectangle<float> (w, h).withCentre (st.getCentre()), juce::RectanglePlacement::stretchToFit, true);
        g.restoreState();
    }

    // ---- bandeja del teclado (debajo del piano)
    g.setColour (juce::Colour (0xff0b0b0c));
    g.fillRoundedRectangle (keySlot.toFloat().expanded (6.0f, 6.0f), 6.0f);

    // ---- touchpad (debajo de los botones de la derecha)
    {
        auto pad = prevButton.getBounds().getUnion (folderButton.getBounds()).toFloat().expanded (10.0f, 12.0f).withTrimmedTop (-18.0f);
        g.setColour (juce::Colour (0xffc9c5bd));
        g.fillRoundedRectangle (pad.translated (0, -1.0f), 9.0f);
        juce::ColourGradient pg (juce::Colour (0xffe2dfd8), 0, pad.getY(), juce::Colour (0xffd5d1c9), 0, pad.getBottom(), false);
        g.setGradientFill (pg);
        g.fillRoundedRectangle (pad.reduced (1.0f), 8.0f);
    }

    // ---- parlante: agujeritos debajo del piano
    for (int row = 0; row < 3; ++row)
        for (int i = 0; i < 60; ++i)
        {
            const float x = (float) keySlot.getX() + 4.0f + (float) i * ((float) keySlot.getWidth() - 8.0f) / 59.0f + (row == 1 ? 3.5f : 0.0f);
            const float y = (float) keySlot.getBottom() + 16.0f + (float) row * 6.0f;
            if (x > keySlot.getRight() - 3) continue;
            g.setColour (juce::Colour (0xff8e8a83));
            g.fillEllipse (x, y, 2.6f, 2.6f);
        }

    // ---- etiquetas impresas en el plastico
    g.setFont (look.pixel (8.0f));
    g.setColour (ink);
    auto knobLabel = [&] (juce::Rectangle<int> a, const char* t) { g.drawText (t, a.withY (a.getBottom() + 6).withHeight (10).expanded (20, 0), juce::Justification::centred); };
    knobLabel (volArea, "VOL");
    knobLabel (modArea, "MOD");
    knobLabel (filterArea, "FILTRO");
    knobLabel (rateArea, "RATE");
    knobLabel (swingArea, "SWING");
    knobLabel (humanArea, "HUMAN");
    knobLabel (velArea, "VEL");
    g.drawText ("SONIDO", prevButton.getBounds().getUnion (nextButton.getBounds()).withY (prevButton.getY() - 14).withHeight (10), juce::Justification::centred);

    // ---- etiqueta plateada abajo, como la de fabrica: INDUSTRIA ARGENTINA
    {
        auto lab = juce::Rectangle<float> (170.0f, 20.0f).withCentre ({ (float) knobsArea.getCentreX(), (float) H - 21.0f });
        g.setColour (juce::Colours::black.withAlpha (0.18f));
        g.fillRoundedRectangle (lab.translated (0, 1.0f), 2.5f);
        juce::ColourGradient sil (juce::Colour (0xffe9e9ea), lab.getX(), lab.getY(), juce::Colour (0xffb9babd), lab.getRight(), lab.getBottom(), false);
        sil.addColour (0.5, juce::Colour (0xfff7f7f8));
        g.setGradientFill (sil);
        g.fillRoundedRectangle (lab, 2.5f);
        g.setColour (juce::Colour (0xff8d8e92));
        g.drawRoundedRectangle (lab.reduced (2.0f), 1.5f, 0.6f);
        // bandera chiquita
        auto fl = juce::Rectangle<float> (15.0f, 10.0f).withPosition (lab.getX() + 7.0f, lab.getCentreY() - 5.0f);
        g.setColour (juce::Colour (0xff74acdf));
        g.fillRect (fl);
        g.setColour (juce::Colours::white);
        g.fillRect (fl.withTrimmedTop (fl.getHeight() / 3.0f).withHeight (fl.getHeight() / 3.0f));
        g.setColour (juce::Colour (0xfff6b40e));
        g.fillEllipse (juce::Rectangle<float> (2.6f, 2.6f).withCentre (fl.getCentre()));
        g.setColour (juce::Colour (0xff2b2c30));
        g.setFont (look.pixel (8.0f));
        g.drawText ("INDUSTRIA ARGENTINA", lab.withTrimmedLeft (28.0f).toNearestInt(), juce::Justification::centredLeft);
    }

    // ---- boton de encendido y luz azul
    {
        auto pw = juce::Rectangle<float> (30.0f, 11.0f).withPosition ((float) prevButton.getX(), (float) keySlot.getY() - 2.0f);
        g.setColour (juce::Colour (0xffa9a59d));
        g.drawRoundedRectangle (pw, 5.5f, 1.2f);
        g.setColour (ink);
        g.drawText ("POWER", pw.toNearestInt().translated (36, 0).withWidth (60), juce::Justification::centredLeft);
        drawLed (g, { (float) R - 6.0f, pw.getCentreY() }, 2.5f);
    }
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
    glowText (g, "BUSCAR>", gi.withHeight (28).withWidth (110), juce::Justification::centredLeft, scrHi);
    juce::String status;
    if (proc.library->isScanning()) status = "CARGANDO...";
    else if (entries != nullptr) status = juce::String (filtered.size()) + "/" + juce::String (groups.empty() ? (int) entries->size() : groups[0].count);
    g.setFont (look.pixel (11.0f));
    glowText (g, status, gi.withHeight (28).withTrimmedLeft (gi.getWidth() - 220), juce::Justification::centredRight, screenTxt);

    g.setColour (juce::Colours::white.withAlpha (0.18f));
    g.fillRect (gi.getX(), gi.getY() + 34, gi.getWidth(), 1);
    g.fillRect (groupList.getRight() + 6, gi.getY() + 42, 1, presetList.getHeight());

    if (entries != nullptr && entries->empty() && ! proc.library->isScanning())
    {
        g.setFont (look.pixel (12.0f));
        glowText (g, "NO HAY SOUNDFONTS", presetList.getBounds().withHeight (30).translated (0, 40), juce::Justification::centred, scrHi);
        g.setFont (look.pixel (8.0f));
        glowText (g, "Toca BAJAR SF2 (arriba) o pone tus .sf2 en Musica/TMU SoundFonts y toca RESCAN", presetList.getBounds().withHeight (20).translated (0, 76), juce::Justification::centred, screenTxt);
    }

    // ---- sonando (con pista de ayuda)
    auto mi = miniLcdArea;
    juce::String top = proc.getActiveLayer() == 0 ? juce::String ("SONANDO") : "CAPA " + juce::String (proc.getActiveLayer() + 1);
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
        g.setColour (juce::Colours::white.withAlpha (0.15f));
        g.fillRect (bar);
        g.setColour (selBlue.brighter (0.3f));
        g.fillRect (bar.withWidth (bar.getWidth() * (float) lv / 127.0f));
    }
    if (info.name.isNotEmpty() && ! info.loading)
    {
        const int st = prefs->getStars (PresetEntry::makeKey (juce::File (info.file).getFileName(), info.phdrIndex, info.name));
        drawStars (g, topRow.removeFromRight (44), st, starGold, scrHi.withAlpha (0.18f), 5.0f);
        topRow.removeFromRight (8);
        glowText (g, juce::String (tmu::categoryName (info.category)).toUpperCase() + " / " + info.source.toUpperCase(), topRow,
                  juce::Justification::centredRight, screenTxt.withAlpha (0.8f), 0.0f);
    }
    auto nameRow = mi;
    g.setFont (look.pixel (8.0f));
    g.setColour (screenDim.brighter (0.5f));
    g.drawText ("? = AYUDA", nameRow.removeFromRight (90), juce::Justification::centredRight);
    g.setFont (look.pixel (16.0f));
    glowText (g, info.name.isNotEmpty() ? info.name.toUpperCase() : juce::String ("---"), nameRow, juce::Justification::centredLeft, scrHi, 0.0f);

    // ---- a que capa pertenecen las perillas
    g.setFont (look.pixel (8.0f));
    g.setColour (fnBlue.darker (0.15f));
    g.drawText ("CAPA " + juce::String (proc.getActiveLayer() + 1), knobsArea.getX(), DECK + 3, 80, 10, juce::Justification::centredLeft);

    // ---- transporte y CC asignados
    const int tr = (int) transpose.getValue();
    g.setFont (look.pixel (8.0f));
    g.setColour (ink);
    g.drawText ("ST " + juce::String (tr > 0 ? "+" : "") + juce::String (tr), trArea.withY (trArea.getBottom() + 6).withHeight (10).expanded (20, 0), juce::Justification::centred);
    const auto learning = proc.getLearningParam();
    for (auto* k : allKnobs())
    {
        const int cc = proc.getAssignedCC (k->paramId);
        const bool isLearning = learning == k->paramId;
        if (cc < 0 && ! isLearning) continue;
        g.setColour (isLearning ? fnBlue.darker (0.2f) : ink.withAlpha (0.75f));
        g.drawText (isLearning ? juce::String ("MOVE...") : "CC" + juce::String (cc), k->getBounds().withY (k->getY() - 14).withHeight (10).expanded (20, 0),
                    juce::Justification::centred);
    }
}

void TMUEditor::resized()
{
    // Grilla: todo arranca en x = M y termina en x = R; 24 px entre bloques.
    lcdArea = { M - 14, HEADER + 2, R - M + 28, 352 };   // la pantalla (vidrio = lcdArea.reduced (14))
    auto glass = lcdArea.reduced (14).reduced (18, 10);
    miniLcdArea = { glass.getX(), glass.getBottom() - 36, glass.getWidth(), 36 };
    layerArea = { glass.getX(), miniLcdArea.getY() - 6 - 30, glass.getWidth(), 30 };   // las 4 capas, arriba de la barra de tareas

    const int rowTop = DECK + 24;
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
    const int bh = 30, by = (HEADER - bh) / 2;
    for (auto* b : { &helpButton, &favButton, &dupButton, &viewAZ, &viewSource, &viewType, &getButton })
    {
        const int bw = b == &helpButton ? bh : (b == &getButton ? 104 : 80);
        x -= bw;
        b->setBounds (x, by, bw, bh);
        x -= 8;
    }

    search.setBounds (glass.getX() + 112, glass.getY(), 340, 28);
    auto body = glass.withTrimmedTop (42).withTrimmedBottom (glass.getBottom() - layerArea.getY() + 6);
    layerStrip.setBounds (layerArea);
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
    licenseOverlay.setBounds (getLocalBounds());
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

    if (proc.getActiveLayer() != lastLayer) onLayerChanged();   // por ej. al cargar un proyecto
    layerStrip.refresh();

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
    return names[juce::jlimit (0, 3, (int) proc.params.getRawParameterValue (TMUProcessor::layerParamId ("velcurve", proc.getActiveLayer()))->load())];
}

void TMUEditor::updateVelButton()
{
    velButton.setButtonText (velText());
    velButton.setToggleState (velButton.getButtonText() != "NORMAL", juce::dontSendNotification);
}

void TMUEditor::attachKnobs()
{
    const auto knobs = allKnobs();
    for (size_t i = 0; i < knobs.size(); ++i)
    {
        knobAtt[i].reset();   // primero se suelta la capa anterior
        knobAtt[i] = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
            proc.params, TMUProcessor::layerParamId (knobs[i]->paramId, proc.getActiveLayer()), *knobs[i]);
    }
}

void TMUEditor::onLayerChanged()
{
    // otra capa elegida (o vaciada): la lista, la pantallita y las perillas muestran los de esa capa
    lastLayer = proc.getActiveLayer();
    lastInfo = proc.getLoadedInfo();
    attachKnobs();
    updateVelButton();
    selectLoadedRow();
    presetList.repaint();
    repaint (miniLcdArea);
    repaint (knobsArea.expanded (20, 16));
    layerStrip.repaint();
}

void TMUEditor::showSoundfontLinks()
{
    // Links a las paginas originales: TMU no incluye ni reparte soundfonts.
    struct Link { const char* name; const char* url; };
    static const Link links[] = {
        { "Musical Artifacts - miles de .sf2 gratis (con su licencia)", "https://musical-artifacts.com/artifacts?formats=sf2" },
        { "Polyphone - soundfonts gratis por categoria",                "https://www.polyphone.io/en/soundfonts" },
        { "GeneralUser GS - banco General MIDI completo (S. Christian Collins)", "https://www.schristiancollins.com" },
        { "MuseScore - lista de soundfonts gratis recomendados",       "https://musescore.org/en/handbook/soundfonts" },
    };
    juce::PopupMenu m;
    m.addSectionHeader ("Soundfonts gratis (se bajan de la pagina de cada autor)");
    for (auto& l : links)
        m.addItem (l.name, [u = juce::String (l.url)] { juce::URL (u).launchInDefaultBrowser(); });
    m.addSeparator();
    m.addItem ("Abrir mi carpeta TMU SoundFonts", [] { auto f = PresetLibrary::getUserFolder(); f.createDirectory(); f.startAsProcess(); });
    m.addItem ("Ya los baje: RESCAN", [this] { proc.library->rescan(); });
    m.addSeparator();
    m.addItem ("Cada soundfont es de su autor: fijate su licencia antes de usarlo en un tema.", false, false, [] {});
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&getButton));
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
        if (auto* prm = proc.params.getParameter (TMUProcessor::layerParamId (knob.paramId, proc.getActiveLayer())))
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
    if (key == "new") return e.isRecent();
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

    int visible = 0, favs = 0, recents = 0;
    for (auto& e : *entries)
        if (isVisible (e))
        {
            ++visible;
            if (prefs->getStars (e.key) > 0) ++favs;
            if (e.isRecent()) ++recents;
        }
    groups.push_back ({ "*", "Todos", visible });
    groups.push_back ({ "fav", juce::String (juce::CharPointer_UTF8 ("\xe2\x98\x85 Favoritos")), favs });
    if (recents > 0) groups.push_back ({ "new", "Recientes", recents });   // .sf2 agregados hace poco

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
        else if (currentGroup == "new")   // recientes: lo ultimo que agregaste primero, y por soundfont
            std::stable_sort (filtered.begin(), filtered.end(), [this] (int a, int b)
            {
                const auto& ea = (*entries)[(size_t) a];
                const auto& eb = (*entries)[(size_t) b];
                if (ea.addedMs != eb.addedMs) return ea.addedMs > eb.addedMs;
                return ea.source.compareNatural (eb.source) < 0;
            });
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
    const int layer = proc.getActiveLayer();
    const auto before = proc.getLoadedInfo (layer);
    const bool adding = layer > 0 && before.phdrIndex < 0 && ! before.loading;   // capa vacia que recibe su primer sonido
    proc.loadPreset ((*entries)[(size_t) filtered[(size_t) row]]);

    if (adding)
        proc.resetLayerParams (layer);   // capa nueva: todos sus controles arrancan en 0 (y queda prendida)
    else if (auto* on = proc.params.getParameter (TMUProcessor::layerOnId (layer)))
        if (on->getValue() < 0.5f)       // sonido nuevo en una capa apagada: se prende sola
        {
            on->beginChangeGesture();
            on->setValueNotifyingHost (1.0f);
            on->endChangeGesture();
        }
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
        g.setColour (selBlue.withAlpha (0.32f));
        g.fillRoundedRectangle (r.toFloat(), 2.0f);
        g.setColour (selBlue.withAlpha (0.8f));
        g.drawRoundedRectangle (r.toFloat().reduced (0.5f), 2.0f, 1.0f);
    }
    r.removeFromLeft (6);
    auto col = selected ? scrHi : screenTxt.withAlpha (0.85f);
    g.setFont (ed.look.pixel (10.0f));
    glowText (g, selected ? ">" : "", r.removeFromLeft (16), juce::Justification::centredLeft, scrHi);
    if (gr.key == "fav")
    {
        g.setColour (starGold);
        g.fillPath (starPath ({ (float) r.getX() + 6.0f, (float) r.getCentreY() }, 6.0f));
        r.removeFromLeft (16);
    }
    else if (gr.key == "new")
    {
        g.setColour (newMint);
        g.fillRoundedRectangle (juce::Rectangle<float> (8.0f, 8.0f).withCentre ({ (float) r.getX() + 6.0f, (float) r.getCentreY() }), 2.0f);
        r.removeFromLeft (16);
        if (! selected) col = newMint;
    }
    if (selected)
    {
        g.setColour (screenTxt.withAlpha (0.7f));
        g.drawText (juce::String (gr.count), r.removeFromRight (52).withTrimmedRight (4), juce::Justification::centredRight);
    }
    glowText (g, gr.label.toUpperCase().replace (juce::String (juce::CharPointer_UTF8 ("\xe2\x98\x85 ")), ""), r,
              juce::Justification::centredLeft, col, 0.0f);
}

void TMUEditor::PresetModel::paintListBoxItem (int row, juce::Graphics& g, int w, int h, bool selected)
{
    if (ed.entries == nullptr || row < 0 || row >= (int) ed.filtered.size()) return;
    const auto& e = (*ed.entries)[(size_t) ed.filtered[(size_t) row]];
    const bool isLoaded = e.phdrIndex == ed.lastInfo.phdrIndex && e.file == ed.lastInfo.file;

    auto r = juce::Rectangle<int> (0, 0, w, h).reduced (2, 1);
    if (isLoaded || selected)   // seleccion estilo escritorio
    {
        g.setColour (selBlue.withAlpha (isLoaded ? 0.38f : 0.16f));
        g.fillRoundedRectangle (r.toFloat(), 2.0f);
        g.setColour (selBlue.withAlpha (isLoaded ? 0.85f : 0.45f));
        g.drawRoundedRectangle (r.toFloat().reduced (0.5f), 2.0f, 1.0f);
    }
    r.removeFromLeft (6);
    if (isLoaded) { g.setColour (scrHi); g.fillRect (r.removeFromLeft (8).withSizeKeepingCentre (6, 6)); }
    else r.removeFromLeft (8);
    r.removeFromLeft (8);

    const bool recent = e.isRecent();
    const auto textCol = isLoaded ? juce::Colours::white
                                  : (recent ? (e.hidden != 0 ? newMint.withAlpha (0.6f) : newMint)
                                            : (e.hidden != 0 ? screenDim : screenTxt));
    if (w >= 480)
    {
        const auto right = ed.proc.uiView == "source" ? juce::String (tmu::categoryName (e.category)) : e.source;
        g.setFont (ed.look.pixel (8.0f));
        g.setColour ((recent ? newMint : screenTxt).withAlpha (isLoaded ? 0.8f : (recent ? 0.7f : 0.45f)));
        g.drawText (right.toUpperCase(), r.removeFromRight (juce::jmin (230, w / 2)).withTrimmedRight (6), juce::Justification::centredRight, true);
    }
    const int st = ed.prefs->getStars (e.key);
    drawStars (g, starsArea (w, h), st, starGold, juce::Colours::white.withAlpha (selected || isLoaded ? 0.28f : 0.13f));
    r.setRight (starsArea (w, h).getX() - 6);
    g.setFont (ed.look.pixel (10.0f));
    glowText (g, e.name.toUpperCase(), r, juce::Justification::centredLeft, textCol, 0.0f);
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
