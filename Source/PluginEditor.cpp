#include "PluginEditor.h"
#include <map>
#include <array>

namespace
{
// ---- paleta: bordo, rojo y negro (+ un hueso apagado solo para teclas y etiquetas)
const juce::Colour outerBg   (0xff050304);
const juce::Colour black     (0xff0b0809);
const juce::Colour bodyTop   (0xff201719);
const juce::Colour bodyBot   (0xff080606);
const juce::Colour wine      (0xff5e0b18);
const juce::Colour wineDark  (0xff360510);
const juce::Colour red       (0xffe3172f);
const juce::Colour redHi     (0xffff4a5e);
const juce::Colour redDim    (0xff7a1222);
const juce::Colour bone      (0xffe8d9d9);
const juce::Colour glassBase (0xff0d0607);

constexpr int W = 940, H = 640;

const juce::StringArray& installedFonts()
{
    static const juce::StringArray names = juce::Font::findAllTypefaceNames();
    return names;
}

juce::String firstInstalled (std::initializer_list<const char*> wanted, const juce::String& fallback)
{
    for (auto* n : wanted)
        if (installedFonts().contains (n)) return n;
    return fallback;
}

juce::Font scriptFont (float h)
{
    static const juce::String name = firstInstalled ({ "Snell Roundhand", "Apple Chancery", "Zapfino", "Brush Script MT" }, {});
    if (name.isEmpty())
        return juce::Font (juce::FontOptions (juce::Font::getDefaultSerifFontName(), h, juce::Font::italic));
    const float k = name == "Snell Roundhand" ? 1.3f : (name == "Zapfino" ? 0.75f : 1.15f);
    return juce::Font (juce::FontOptions (name, h * k, juce::Font::plain));
}

juce::Font monoFont (float h, bool bold = false)
{
    static const juce::String name = firstInstalled ({ "Menlo", "Monaco", "Andale Mono", "DejaVu Sans Mono" }, juce::Font::getDefaultMonospacedFontName());
    return juce::Font (juce::FontOptions (name, h, bold ? juce::Font::bold : juce::Font::plain));
}


const char* const* glyphFor (juce::juce_wchar ch)
{
    // Fuente propia de 5x7 pixeles
    static const std::map<juce::juce_wchar, std::array<const char*, 7>> font = {
        { 'A', {" ### ","#   #","#   #","#####","#   #","#   #","#   #"} },
        { 'B', {"#### ","#   #","#   #","#### ","#   #","#   #","#### "} },
        { 'C', {" ### ","#   #","#    ","#    ","#    ","#   #"," ### "} },
        { 'D', {"###  ","#  # ","#   #","#   #","#   #","#  # ","###  "} },
        { 'E', {"#####","#    ","#    ","#### ","#    ","#    ","#####"} },
        { 'F', {"#####","#    ","#    ","#### ","#    ","#    ","#    "} },
        { 'G', {" ### ","#   #","#    ","# ###","#   #","#   #"," ####"} },
        { 'H', {"#   #","#   #","#   #","#####","#   #","#   #","#   #"} },
        { 'I', {" ### ","  #  ","  #  ","  #  ","  #  ","  #  "," ### "} },
        { 'J', {"  ###","   # ","   # ","   # ","   # ","#  # "," ##  "} },
        { 'K', {"#   #","#  # ","# #  ","##   ","# #  ","#  # ","#   #"} },
        { 'L', {"#    ","#    ","#    ","#    ","#    ","#    ","#####"} },
        { 'M', {"#   #","## ##","# # #","# # #","#   #","#   #","#   #"} },
        { 'N', {"#   #","#   #","##  #","# # #","#  ##","#   #","#   #"} },
        { 'O', {" ### ","#   #","#   #","#   #","#   #","#   #"," ### "} },
        { 'P', {"#### ","#   #","#   #","#### ","#    ","#    ","#    "} },
        { 'Q', {" ### ","#   #","#   #","#   #","# # #","#  # "," ## #"} },
        { 'R', {"#### ","#   #","#   #","#### ","# #  ","#  # ","#   #"} },
        { 'S', {" ####","#    ","#    "," ### ","    #","    #","#### "} },
        { 'T', {"#####","  #  ","  #  ","  #  ","  #  ","  #  ","  #  "} },
        { 'U', {"#   #","#   #","#   #","#   #","#   #","#   #"," ### "} },
        { 'V', {"#   #","#   #","#   #","#   #","#   #"," # # ","  #  "} },
        { 'W', {"#   #","#   #","#   #","# # #","# # #","# # #"," # # "} },
        { 'X', {"#   #","#   #"," # # ","  #  "," # # ","#   #","#   #"} },
        { 'Y', {"#   #","#   #"," # # ","  #  ","  #  ","  #  ","  #  "} },
        { 'Z', {"#####","    #","   # ","  #  "," #   ","#    ","#####"} },
        { '0', {" ### ","#   #","#  ##","# # #","##  #","#   #"," ### "} },
        { '1', {"  #  "," ##  ","  #  ","  #  ","  #  ","  #  "," ### "} },
        { '2', {" ### ","#   #","    #","   # ","  #  "," #   ","#####"} },
        { '3', {"#####","   # ","  #  ","   # ","    #","#   #"," ### "} },
        { '4', {"   # ","  ## "," # # ","#  # ","#####","   # ","   # "} },
        { '5', {"#####","#    ","#### ","    #","    #","#   #"," ### "} },
        { '6', {"  ## "," #   ","#    ","#### ","#   #","#   #"," ### "} },
        { '7', {"#####","    #","   # ","  #  "," #   "," #   "," #   "} },
        { '8', {" ### ","#   #","#   #"," ### ","#   #","#   #"," ### "} },
        { '9', {" ### ","#   #","#   #"," ####","    #","   # "," ##  "} },
        { ' ', {"     ","     ","     ","     ","     ","     ","     "} },
        { '.', {"     ","     ","     ","     ","     "," ##  "," ##  "} },
        { ',', {"     ","     ","     ","     "," ##  ","  #  "," #   "} },
        { ':', {"     "," ##  "," ##  ","     "," ##  "," ##  ","     "} },
        { '/', {"     ","    #","   # ","  #  "," #   ","#    ","     "} },
        { '-', {"     ","     ","     ","#####","     ","     ","     "} },
        { '_', {"     ","     ","     ","     ","     ","     ","#####"} },
        { '>', {" #   ","  #  ","   # ","    #","   # ","  #  "," #   "} },
        { '<', {"   # ","  #  "," #   ","#    "," #   ","  #  ","   # "} },
        { '+', {"     ","  #  ","  #  ","#####","  #  ","  #  ","     "} },
        { '=', {"     ","     ","#####","     ","#####","     ","     "} },
        { '\'',{"  #  ","  #  "," #   ","     ","     ","     ","     "} },
        { '"', {" # # "," # # ","     ","     ","     ","     ","     "} },
        { '!', {"  #  ","  #  ","  #  ","  #  ","  #  ","     ","  #  "} },
        { '?', {" ### ","#   #","    #","   # ","  #  ","     ","  #  "} },
        { '(', {"   # ","  #  "," #   "," #   "," #   ","  #  ","   # "} },
        { ')', {" #   ","  #  ","   # ","   # ","   # ","  #  "," #   "} },
        { '[', {" ### "," #   "," #   "," #   "," #   "," #   "," ### "} },
        { ']', {" ### ","   # ","   # ","   # ","   # ","   # "," ### "} },
        { '&', {" ##  ","#  # ","# #  "," #   ","# # #","#  # "," ## #"} },
        { '#', {" # # "," # # ","#####"," # # ","#####"," # # "," # # "} },
        { '*', {"     ","  #  ","# # #"," ### ","# # #","  #  ","     "} },
        { '%', {"##   ","##  #","   # ","  #  "," #   ","#  ##","   ##"} },
        { '@', {" ### ","#   #","# ###","# # #","# ###","#    "," ### "} },
        { '$', {"  #  "," ####","# #  "," ### ","  # #","#### ","  #  "} },
        { '~', {"     ","     "," #  #","# ## ","     ","     ","     "} },
    };
    auto it = font.find (ch);
    if (it == font.end()) it = font.find ('?');
    return it->second.data();
}

juce::juce_wchar basicChar (juce::juce_wchar c)
{
    c = juce::CharacterFunctions::toUpperCase (c);
    switch (c)
    {
        case 0xC1: case 0xC0: case 0xC2: case 0xC4: case 0xC3: return 'A';
        case 0xC9: case 0xC8: case 0xCA: case 0xCB: return 'E';
        case 0xCD: case 0xCC: case 0xCE: case 0xCF: return 'I';
        case 0xD3: case 0xD2: case 0xD4: case 0xD6: case 0xD5: return 'O';
        case 0xDA: case 0xD9: case 0xDB: case 0xDC: return 'U';
        case 0xD1: return 'N';
        case 0xC7: return 'C';
        case 0xE9: case 0xE8: return 'E';   // por si toUpperCase no las convierte
        case 0xE1: case 0xE0: return 'A';
        case 0xED: return 'I';
        case 0xF3: return 'O';
        case 0xFA: case 0xFC: return 'U';
        case 0xF1: return 'N';
        default: return c;
    }
}

//==============================================================================
// Texto pixelado con la fuente 5x7 de arriba, agrandado x1/x2/x3 sin suavizar.
const juce::Image& pixelImage (const juce::String& text, int /*px*/, bool bold)
{
    static std::map<juce::String, juce::Image> cache;
    const auto key = (bold ? "b|" : "|") + text;
    auto it = cache.find (key);
    if (it != cache.end()) return it->second;
    if (cache.size() > 2000) cache.clear();

    const int n = juce::jmax (1, text.length());
    const int w = n * 6 + (bold ? 1 : 0), h = 7;
    juce::Image img (juce::Image::ARGB, w, h, true);
    {
        juce::Image::BitmapData bd (img, juce::Image::BitmapData::writeOnly);
        for (int i = 0; i < text.length(); ++i)
        {
            const auto* rows = glyphFor (basicChar (text[i]));
            for (int y = 0; y < 7; ++y)
                for (int x = 0; x < 5; ++x)
                    if (rows[y][x] == '#')
                    {
                        bd.setPixelColour (i * 6 + x, y, juce::Colours::white);
                        if (bold) bd.setPixelColour (i * 6 + x + 1, y, juce::Colours::white);
                    }
        }
    }
    return cache.emplace (key, img).first->second;
}

int pixelWidth (const juce::String& t, int px, int scale, bool bold = false) { return pixelImage (t, px, bold).getWidth() * scale; }

void pixelText (juce::Graphics& g, juce::String text, juce::Rectangle<int> r, juce::Justification j, juce::Colour c,
                int px = 8, int scale = 2, bool bold = false, bool glow = true)
{
    if (text.isEmpty()) return;
    while (text.length() > 1 && pixelWidth (text, px, scale, bold) > r.getWidth())
        text = text.dropLastCharacters (text.endsWith ("..") ? 3 : 1).trimEnd() + "..";
    const auto& img = pixelImage (text, px, bold);
    const int dw = img.getWidth() * scale, dh = img.getHeight() * scale;
    int x = r.getX();
    if (j.testFlags (juce::Justification::horizontallyCentred)) x = r.getCentreX() - dw / 2;
    else if (j.testFlags (juce::Justification::right)) x = r.getRight() - dw;
    const int y = r.getCentreY() - dh / 2;

    g.setImageResamplingQuality (juce::Graphics::lowResamplingQuality);
    if (glow)
    {
        g.setColour (c.withAlpha (0.18f));
        for (auto d : { juce::Point<int> (scale, 0), juce::Point<int> (-scale, 0), juce::Point<int> (0, scale), juce::Point<int> (0, -scale) })
            g.drawImage (img, x + d.x, y + d.y, dw, dh, 0, 0, img.getWidth(), img.getHeight(), true);
    }
    g.setColour (c);
    g.drawImage (img, x, y, dw, dh, 0, 0, img.getWidth(), img.getHeight(), true);
}

//==============================================================================
void drawLed (juce::Graphics& g, juce::Point<float> c, float radius, bool on)
{
    if (on)
    {
        juce::ColourGradient glow (red.withAlpha (0.5f), c, red.withAlpha (0.0f), c.translated (radius * 3.5f, 0), true);
        g.setGradientFill (glow);
        g.fillEllipse (juce::Rectangle<float> (radius * 7, radius * 7).withCentre (c));
    }
    auto r = juce::Rectangle<float> (radius * 2, radius * 2).withCentre (c);
    g.setColour (juce::Colours::black);
    g.fillEllipse (r.expanded (1.2f));
    juce::ColourGradient body (on ? redHi.brighter (0.4f) : juce::Colour (0xff3a0a12), c.translated (-radius * 0.4f, -radius * 0.5f),
                               on ? red : juce::Colour (0xff1a0307), c.translated (radius, radius), true);
    g.setGradientFill (body);
    g.fillEllipse (r);
}

void recess (juce::Graphics& g, juce::Rectangle<float> r, float radius)
{
    g.setColour (juce::Colours::white.withAlpha (0.06f));
    g.fillRoundedRectangle (r.translated (0, 1.5f).expanded (1.0f), radius + 1.0f);
    juce::ColourGradient b (juce::Colour (0xff000000), r.getX(), r.getY(), juce::Colour (0xff1a1012), r.getX(), r.getBottom(), false);
    g.setGradientFill (b);
    g.fillRoundedRectangle (r, radius);
}

// plastico brillante tipo consola
void plastic (juce::Graphics& g, juce::Rectangle<float> r, float radius, juce::Colour top, juce::Colour bottom)
{
    g.setColour (juce::Colours::black.withAlpha (0.6f));
    g.fillRoundedRectangle (r.translated (0, 3.0f), radius);
    juce::ColourGradient grad (top, r.getX(), r.getY(), bottom, r.getX(), r.getBottom(), false);
    g.setGradientFill (grad);
    g.fillRoundedRectangle (r, radius);
    juce::ColourGradient shine (juce::Colours::white.withAlpha (0.10f), r.getX(), r.getY(), juce::Colours::transparentWhite, r.getX(), r.getY() + r.getHeight() * 0.5f, false);
    g.setGradientFill (shine);
    g.fillRoundedRectangle (r.reduced (1.0f), radius);
    g.setColour (juce::Colours::white.withAlpha (0.08f));
    g.drawRoundedRectangle (r.reduced (0.5f), radius, 1.0f);
}
} // namespace

//==============================================================================
TMULook::TMULook()
{
    accent = red;
    setColour (juce::ListBox::backgroundColourId, juce::Colours::transparentBlack);
    setColour (juce::ListBox::outlineColourId, juce::Colours::transparentBlack);
    setColour (juce::TextButton::textColourOffId, bone);
    setColour (juce::TextButton::textColourOnId, bone);
    setColour (juce::MidiKeyboardComponent::shadowColourId, juce::Colours::transparentBlack);
    setColour (juce::MidiKeyboardComponent::keySeparatorLineColourId, juce::Colours::transparentBlack);
    setColour (juce::BubbleComponent::backgroundColourId, black);
    setColour (juce::BubbleComponent::outlineColourId, red);
    setColour (juce::TooltipWindow::textColourId, bone);
    setColour (juce::TextEditor::textColourId, redHi);
    setColour (juce::TextEditor::highlightColourId, red.withAlpha (0.35f));
    setColour (juce::TextEditor::highlightedTextColourId, bone);
    setColour (juce::CaretComponent::caretColourId, redHi);
}

void TMULook::setAccent (juce::Colour c) { accent = c; }

void TMULook::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float start, float end, juce::Slider&)
{
    auto area = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h);
    const float size = juce::jmin (area.getWidth(), area.getHeight());
    const auto c = area.getCentre();
    const float rOuter = size * 0.5f;
    const float rKnob = rOuter - 8.0f;

    // aro de segmentos rojos (como una barra de energia)
    const int segs = 12;
    for (int i = 0; i < segs; ++i)
    {
        const float a0 = start + (end - start) * ((float) i + 0.15f) / segs;
        const float a1 = start + (end - start) * ((float) i + 0.85f) / segs;
        juce::Path seg;
        seg.addCentredArc (c.x, c.y, rOuter - 3, rOuter - 3, 0, a0, a1, true);
        const bool lit = (float) i < pos * segs - 0.01f || (i == 0 && pos > 0.001f);
        g.setColour (lit ? red : redDim.withAlpha (0.45f));
        g.strokePath (seg, juce::PathStrokeType (3.5f));
    }

    auto kr = juce::Rectangle<float> (rKnob * 2, rKnob * 2).withCentre (c);
    g.setColour (juce::Colours::black.withAlpha (0.6f));
    g.fillEllipse (kr.translated (0, 3.0f));
    juce::ColourGradient body (juce::Colour (0xff2a2023), kr.getX(), kr.getY(), black, kr.getRight(), kr.getBottom(), false);
    g.setGradientFill (body);
    g.fillEllipse (kr);
    g.setColour (juce::Colours::white.withAlpha (0.12f));
    g.drawEllipse (kr.reduced (1.0f), 1.0f);

    const float angle = start + pos * (end - start);
    const juce::Point<float> p1 (c.x + (rKnob - 6) * std::sin (angle), c.y - (rKnob - 6) * std::cos (angle));
    g.setColour (red);
    g.fillEllipse (juce::Rectangle<float> (6.0f, 6.0f).withCentre (p1));
}

void TMULook::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool over, bool down)
{
    const auto style = b.getProperties()["style"].toString();
    auto r = b.getLocalBounds().toFloat();

    if (style == "round")   // boton redondo de joystick
    {
        auto c = juce::Rectangle<float> (40, 40).withCentre ({ r.getCentreX(), r.getY() + 22.0f });
        const bool on = b.getToggleState();
        if (on)
        {
            juce::ColourGradient glow (red.withAlpha (0.45f), c.getCentre(), red.withAlpha (0.0f), c.getCentre().translated (34, 0), true);
            g.setGradientFill (glow);
            g.fillEllipse (c.expanded (14));
        }
        g.setColour (juce::Colours::black);
        g.fillEllipse (c.expanded (3.0f));
        auto face = c.translated (0, down ? 1.5f : 0.0f);
        g.setColour (juce::Colours::black.withAlpha (0.6f));
        g.fillEllipse (c.translated (0, 3.0f));
        juce::ColourGradient body ((on ? redHi : (over ? redDim.brighter (0.25f) : redDim)), face.getX(), face.getY(),
                                   (on ? red.darker (0.3f) : wineDark), face.getRight(), face.getBottom(), false);
        g.setGradientFill (body);
        g.fillEllipse (face);
        g.setColour (juce::Colours::white.withAlpha (on ? 0.35f : 0.15f));
        g.fillEllipse (juce::Rectangle<float> (16, 8).withCentre (face.getCentre().translated (-4, -9)));
        return;
    }
    if (style == "dpad")    // brazo de la cruceta
    {
        if (down)
        {
            g.setColour (juce::Colours::black.withAlpha (0.35f));
            g.fillRect (r.reduced (1.0f));
        }
        return;
    }
    // "pill": boton alargado tipo START / SELECT
    auto pill = r.withSizeKeepingCentre (r.getWidth(), 16.0f);
    g.setColour (juce::Colours::black);
    g.fillRoundedRectangle (pill.expanded (2.5f), 10.0f);
    auto face = pill.translated (0, down ? 1.0f : 0.0f);
    juce::ColourGradient body (over ? juce::Colour (0xff3a2a2e) : juce::Colour (0xff2b1f22), face.getX(), face.getY(), black, face.getX(), face.getBottom(), false);
    g.setGradientFill (body);
    g.fillRoundedRectangle (face, 8.0f);
    g.setColour (juce::Colours::white.withAlpha (0.1f));
    g.drawHorizontalLine ((int) face.getY() + 2, face.getX() + 8, face.getRight() - 8);
}

void TMULook::drawButtonText (juce::Graphics& g, juce::TextButton& b, bool over, bool down)
{
    const auto style = b.getProperties()["style"].toString();
    auto r = b.getLocalBounds();
    if (style == "round")
    {
        pixelText (g, b.getButtonText(), r.removeFromBottom (16), juce::Justification::centred,
                   b.getToggleState() ? redHi : bone.withAlpha (0.75f), 8, 2, true, b.getToggleState());
        return;
    }
    if (style == "dpad")
    {
        auto c = r.toFloat().getCentre().translated (0, down ? 1.0f : 0.0f);
        const float s = b.getButtonText() == "<" ? -1.0f : 1.0f;
        juce::Path p;
        p.addTriangle (c.x - 4 * s, c.y - 6, c.x - 4 * s, c.y + 6, c.x + 5 * s, c.y);
        g.setColour (juce::Colours::black.withAlpha (0.8f));
        g.fillPath (p, juce::AffineTransform::translation (0, 1));
        g.setColour (over ? redHi : juce::Colour (0xff4a383c));
        g.fillPath (p);
        return;
    }
    juce::ignoreUnused (over, down);
}

void TMULook::drawScrollbar (juce::Graphics& g, juce::ScrollBar&, int x, int y, int w, int h, bool vertical,
                             int thumbStart, int thumbSize, bool, bool)
{
    // barra de pixeles
    juce::Rectangle<int> thumb = vertical ? juce::Rectangle<int> (x + 2, thumbStart, w - 4, thumbSize)
                                          : juce::Rectangle<int> (thumbStart, y + 2, thumbSize, h - 4);
    g.setColour (red.withAlpha (0.55f));
    for (int yy = thumb.getY(); yy < thumb.getBottom(); yy += 4)
        g.fillRect (thumb.getX(), yy, thumb.getWidth(), 2);
}

//==============================================================================
void RetroKeyboard::drawWhiteNote (int note, juce::Graphics& g, juce::Rectangle<float> area, bool isDown, bool isOver, juce::Colour, juce::Colour)
{
    auto r = area.reduced (0.8f, 0.0f);
    juce::Path key;
    key.addRoundedRectangle (r.getX(), r.getY() - 6.0f, r.getWidth(), r.getHeight() + 6.0f, 4.0f);

    juce::Colour top (0xffe9dcdc), bottom (0xffc9b5b7);
    if (isOver) top = top.brighter (0.05f);
    if (isDown) { top = redHi; bottom = red.darker (0.2f); }

    g.saveState();
    g.reduceClipRegion (area.toNearestInt());
    juce::ColourGradient grad (top, r.getX(), r.getY(), bottom, r.getX(), r.getBottom(), false);
    g.setGradientFill (grad);
    g.fillPath (key);
    g.setColour (juce::Colours::black.withAlpha (isDown ? 0.1f : 0.2f));
    g.fillRect (r.withTop (r.getBottom() - (isDown ? 3.0f : 6.0f)));
    g.setColour (wineDark.withAlpha (0.6f));
    g.drawVerticalLine ((int) area.getX(), area.getY(), area.getBottom());
    g.restoreState();

    if (note % 12 == 0)
        pixelText (g, "C" + juce::String (note / 12), area.withTrimmedBottom (8).removeFromBottom (12).toNearestInt(),
                   juce::Justification::centred, isDown ? bone : wine, 7, 1, true, false);
}

void RetroKeyboard::drawBlackNote (int, juce::Graphics& g, juce::Rectangle<float> area, bool isDown, bool isOver, juce::Colour)
{
    auto r = area.reduced (1.0f, 0.0f);
    g.setColour (juce::Colours::black.withAlpha (0.5f));
    g.fillRoundedRectangle (r.translated (1.5f, 0.0f).withTrimmedTop (-4.0f), 3.0f);

    juce::Colour top (0xff4a0c17), bottom (0xff12030a);   // teclas negras en bordo
    if (isOver) top = top.brighter (0.2f);
    if (isDown) { top = redHi; bottom = red.darker (0.4f); }
    juce::ColourGradient grad (top, r.getX(), r.getY(), bottom, r.getX(), r.getBottom(), false);
    g.setGradientFill (grad);
    g.fillRoundedRectangle (r.withTrimmedTop (-4.0f), 3.0f);
    auto face = r.reduced (2.5f, 0.0f).withTrimmedBottom (isDown ? 4.0f : 7.0f);
    g.setColour (juce::Colours::white.withAlpha (0.08f));
    g.fillRoundedRectangle (face, 2.0f);
}

void RetroKeyboard::drawUpDownButton (juce::Graphics& g, int w, int h, bool over, bool, bool up)
{
    g.fillAll (black);
    juce::Path p;
    const float cx = (float) w * 0.5f, cy = (float) h * 0.5f;
    if (up) p.addTriangle (cx - 4, cy - 6, cx - 4, cy + 6, cx + 5, cy);
    else    p.addTriangle (cx + 4, cy - 6, cx + 4, cy + 6, cx - 5, cy);
    g.setColour (over ? redHi : red.withAlpha (0.7f));
    g.fillPath (p);
}

//==============================================================================
TMUEditor::TMUEditor (TMUProcessor& p)
    : juce::AudioProcessorEditor (&p), proc (p),
      volumeAtt (p.params, "volume", volume),
      transposeAtt (p.params, "transpose", transpose),
      keyboard (p.keyboardState, look)
{
    setLookAndFeel (&look);

    for (auto* b : { &viewType, &viewSource, &viewAZ })
    {
        b->getProperties().set ("style", "round");
        addAndMakeVisible (b);
    }
    viewType.onClick   = [this] { setView ("type"); };
    viewSource.onClick = [this] { setView ("source"); };
    viewAZ.onClick     = [this] { setView ("az"); };

    // el texto real se dibuja pixelado debajo; la letra mono de 20 px mide 12 px por caracter,
    // igual que la fuente pixel x2, asi el cursor queda alineado
    search.setFont (monoFont (20.0f));
    search.setIndents (4, 2);
    search.setColour (juce::TextEditor::textColourId, juce::Colours::transparentBlack);
    search.setText (proc.uiSearch, false);
    search.onTextChange = [this] { proc.uiSearch = search.getText(); rebuildFiltered(); repaint (lcdArea); };
    search.onEscapeKey  = [this] { search.clear(); proc.uiSearch = {}; rebuildFiltered(); };
    addAndMakeVisible (search);

    for (auto* b : { &rescanButton, &folderButton })
    {
        b->getProperties().set ("style", "pill");
        addAndMakeVisible (b);
    }
    rescanButton.onClick = [this] { proc.library->rescan(); };
    folderButton.onClick = [] { auto f = PresetLibrary::getUserFolder(); f.createDirectory(); f.startAsProcess(); };
    folderButton.setTooltip ("Abre la carpeta donde podes agregar mas .sf2");

    for (auto* b : { &prevButton, &nextButton })
    {
        b->getProperties().set ("style", "dpad");
        addAndMakeVisible (b);
    }
    prevButton.onClick = [this] { step (-1); };
    nextButton.onClick = [this] { step (1); };

    for (auto* lb : { &groupList, &presetList })
    {
        lb->setRowHeight (24);
        lb->setOutlineThickness (0);
        addAndMakeVisible (lb);
    }
    presetList.setWantsKeyboardFocus (true);

    volume.setDoubleClickReturnValue (true, 0.0);
    transpose.setDoubleClickReturnValue (true, 0.0);
    volume.setPopupDisplayEnabled (true, true, this);
    transpose.setPopupDisplayEnabled (true, true, this);
    addAndMakeVisible (volume);
    addAndMakeVisible (transpose);

    keyboard.setAvailableRange (24, 108);
    keyboard.setOctaveForMiddleC (5);   // igual que FL Studio: la nota MIDI 60 es C5
    keyboard.setKeyWidth (22.0f);
    keyboard.setBlackNoteLengthProportion (0.62f);
    keyboard.setLowestVisibleKey (36);
    keyboard.setWantsKeyboardFocus (false);
    keyboard.setScrollButtonsVisible (true);
    addAndMakeVisible (keyboard);

    setSize (W, H);

    currentGroup = proc.uiGroup;
    setView (proc.uiView.isEmpty() ? juce::String ("type") : proc.uiView);
    startTimerHz (15);
}

TMUEditor::~TMUEditor()
{
    setLookAndFeel (nullptr);
}

//==============================================================================
void TMUEditor::buildChassis()
{
    const float scale = 2.0f;
    chassis = juce::Image (juce::Image::ARGB, (int) (W * scale), (int) (H * scale), true);
    juce::Graphics g (chassis);
    g.addTransform (juce::AffineTransform::scale (scale));

    g.fillAll (outerBg);

    // ---- carcasa negra brillante
    auto body = juce::Rectangle<float> (4, 4, W - 8, H - 8);
    plastic (g, body, 30.0f, bodyTop, bodyBot);

    // filete rojo
    g.setColour (red.withAlpha (0.55f));
    g.drawRoundedRectangle (body.reduced (9.0f), 22.0f, 1.2f);

    // rejillas de ventilacion a los costados de la pantalla
    for (int side = 0; side < 2; ++side)
        for (int i = 0; i < 16; ++i)
        {
            const float y = 110.0f + (float) i * 19.0f;
            const float x = side == 0 ? 20.0f : (float) W - 46.0f;
            g.setColour (juce::Colours::black);
            g.fillRoundedRectangle (x, y, 26.0f, 7.0f, 3.5f);
            g.setColour (juce::Colours::white.withAlpha (0.07f));
            g.drawHorizontalLine ((int) y + 7, x + 3, x + 23);
        }

    // ---- tapa superior bordo
    auto deck = juce::Rectangle<float> (18, 14, W - 36, 72);
    plastic (g, deck, 20.0f, wine, wineDark);
    // ranuras decorativas en la tapa
    for (int i = 0; i < 5; ++i)
    {
        const float y = deck.getY() + 14.0f + (float) i * 10.0f;
        g.setColour (juce::Colours::black.withAlpha (0.45f));
        g.fillRoundedRectangle (W * 0.5f - 150.0f, y, 150.0f, 3.0f, 1.5f);
        g.setColour (juce::Colours::white.withAlpha (0.08f));
        g.drawHorizontalLine ((int) y + 3, W * 0.5f - 148.0f, W * 0.5f - 2.0f);
    }

    // logo
    pixelText (g, "TMU", juce::Rectangle<int> (46, 14, 160, 56).translated (4, 4), juce::Justification::centredLeft, juce::Colours::black.withAlpha (0.7f), 9, 6, false, false);
    pixelText (g, "TMU", { 46, 14, 160, 56 }, juce::Justification::centredLeft, red, 9, 6, false, false);
    pixelText (g, "SOUND FONT SYSTEM", { 48, 66, 200, 14 }, juce::Justification::centredLeft, bone.withAlpha (0.7f), 7, 1, true, false);
    g.setFont (scriptFont (18.0f));
    g.setColour (redHi);
    g.drawText ("viszeversa", juce::Rectangle<int> (174, 30, 130, 28), juce::Justification::centredLeft);

    pixelText (g, "POWER", { W - 445, 58, 50, 12 }, juce::Justification::centred, bone.withAlpha (0.6f), 7, 1, true, false);

    // ---- pantalla tipo CRT
    recess (g, lcdArea.toFloat(), 24.0f);
    auto rim = lcdArea.toFloat().reduced (5.0f);
    juce::ColourGradient rimG (juce::Colour (0xff1c1214), rim.getX(), rim.getY(), black, rim.getX(), rim.getBottom(), false);
    g.setGradientFill (rimG);
    g.fillRoundedRectangle (rim, 20.0f);
    g.setColour (red.withAlpha (0.35f));
    g.drawRoundedRectangle (rim.reduced (1.0f), 20.0f, 1.0f);

    // ---- panel de controles bordo
    auto ctrl = juce::Rectangle<float> (54, 432, W - 108, 94);
    plastic (g, ctrl, 18.0f, wine, wineDark);
    recess (g, miniLcdArea.toFloat(), 12.0f);

    // cruceta (los brazos izquierdo/derecho son botones)
    const juce::Point<float> dc (dpadCentre.toFloat());
    juce::Path cross;
    cross.addRoundedRectangle (dc.x - 46, dc.y - 15, 92, 30, 5.0f);
    cross.addRoundedRectangle (dc.x - 15, dc.y - 46 + 14, 30, 64, 5.0f);
    g.setColour (juce::Colours::black.withAlpha (0.6f));
    g.fillPath (cross, juce::AffineTransform::translation (0, 3));
    juce::ColourGradient cg (juce::Colour (0xff2c2124), dc.x, dc.y - 40, black, dc.x, dc.y + 30, false);
    g.setGradientFill (cg);
    g.fillPath (cross);
    g.setColour (juce::Colours::black.withAlpha (0.5f));
    g.fillEllipse (juce::Rectangle<float> (10, 10).withCentre (dc));
    pixelText (g, "SONIDO", { (int) dc.x - 50, 512, 100, 12 }, juce::Justification::centred, bone.withAlpha (0.7f), 7, 1, true, false);

    pixelText (g, "VOL", volArea.withY (508).withHeight (12), juce::Justification::centred, bone.withAlpha (0.7f), 7, 1, true, false);
    pixelText (g, "BIBLIOTECA", { 746, 440, 132, 12 }, juce::Justification::centred, bone.withAlpha (0.7f), 7, 1, true, false);
    pixelText (g, "RESCAN", rescanButton.getBounds().withY (484).withHeight (12), juce::Justification::centred, bone.withAlpha (0.7f), 7, 1, true, false);
    pixelText (g, "CARPETA", folderButton.getBounds().withY (484).withHeight (12), juce::Justification::centred, bone.withAlpha (0.7f), 7, 1, true, false);

    // ---- hueco del teclado
    recess (g, keySlot.toFloat(), 14.0f);
}

void TMUEditor::paint (juce::Graphics& g)
{
    g.drawImage (chassis, getLocalBounds().toFloat());

    drawLed (g, { (float) W - 420.0f, 44.0f }, 5.0f, true);

    // ---- vidrio CRT
    auto drawGlass = [&] (juce::Rectangle<float> r, float radius)
    {
        g.setColour (glassBase);
        g.fillRoundedRectangle (r, radius);
        juce::ColourGradient glow (red.withAlpha (0.10f), r.getCentreX(), r.getCentreY(), juce::Colours::transparentBlack, r.getX(), r.getY(), true);
        g.setGradientFill (glow);
        g.fillRoundedRectangle (r, radius);
        g.saveState();
        g.reduceClipRegion (r.toNearestInt());
        g.setColour (juce::Colours::black.withAlpha (0.28f));
        for (float y = r.getY(); y < r.getBottom(); y += 3.0f) g.drawHorizontalLine ((int) y, r.getX(), r.getRight());
        // vineta de tubo
        juce::ColourGradient vig (juce::Colours::transparentBlack, r.getCentreX(), r.getCentreY(), juce::Colours::black.withAlpha (0.55f),
                                  r.getX() - 30, r.getY() - 30, true);
        g.setGradientFill (vig);
        g.fillRect (r);
        g.restoreState();
        juce::ColourGradient refl (juce::Colours::white.withAlpha (0.06f), r.getX(), r.getY(), juce::Colours::transparentWhite, r.getX(), r.getY() + r.getHeight() * 0.35f, false);
        g.setGradientFill (refl);
        g.fillRoundedRectangle (r.reduced (6.0f), radius);
    };
    auto glass = lcdArea.toFloat().reduced (14.0f);
    drawGlass (glass, 16.0f);
    auto mini = miniLcdArea.toFloat().reduced (6.0f);
    drawGlass (mini, 8.0f);

    // ---- contenido fijo de la pantalla
    const auto info = proc.getLoadedInfo();
    auto gi = glass.toNearestInt();
    pixelText (g, "BUSCAR>", { gi.getX() + 16, gi.getY() + 10, 90, 24 }, juce::Justification::centredLeft, red, 8, 2, true);
    if (search.getText().isEmpty())
        pixelText (g, "NOMBRE, TIPO O SOUNDFONT_", search.getBounds().translated (4, 0), juce::Justification::centredLeft, redDim, 8, 2, false, false);
    else
        pixelText (g, search.getText().toUpperCase(), search.getBounds().withTrimmedLeft (4), juce::Justification::centredLeft, redHi, 8, 2, false, true);

    juce::String status;
    if (proc.library->isScanning()) status = "ESCANEANDO...";
    else if (entries != nullptr) status = juce::String (filtered.size()) + "/" + juce::String (entries->size());
    pixelText (g, status, { gi.getRight() - 200, gi.getY() + 10, 184, 24 }, juce::Justification::centredRight, red.withAlpha (0.8f), 8, 2, true);

    g.setColour (red.withAlpha (0.45f));
    for (int x = gi.getX() + 12; x < gi.getRight() - 12; x += 6) g.fillRect (x, gi.getY() + 40, 3, 2);   // linea punteada
    for (int y = gi.getY() + 48; y < gi.getBottom() - 10; y += 6) g.fillRect (groupList.getRight() + 5, y, 2, 3);

    if (entries != nullptr && entries->empty() && ! proc.library->isScanning())
    {
        auto r = presetList.getBounds();
        pixelText (g, "NO HAY SOUNDFONTS", r.withHeight (30).withY (r.getCentreY() - 40), juce::Justification::centred, redHi, 8, 2, true);
        pixelText (g, "INSTALA TMU CON EL .PKG O PONE .SF2 EN", r.withHeight (24).withY (r.getCentreY() - 6), juce::Justification::centred, red, 8, 2);
        pixelText (g, "MUSICA/TMU SOUNDFONTS Y TOCA RESCAN", r.withHeight (24).withY (r.getCentreY() + 18), juce::Justification::centred, red, 8, 2);
    }

    // ---- mini pantalla: sonido actual
    auto mi = mini.toNearestInt().reduced (12, 6);
    juce::String top = "SONANDO";
    if (info.loading) top = "CARGANDO...";
    else if (info.error.isNotEmpty()) top = "ERROR: " + info.error;
    pixelText (g, top, mi.removeFromTop (14), juce::Justification::centredLeft, redDim.brighter (0.6f), 7, 1, true, false);
    const auto nameText = info.name.isNotEmpty() ? info.name.toUpperCase() : juce::String ("---");
    const int nameScale = pixelWidth (nameText, 9, 3) <= mi.getWidth() ? 3 : 2;
    pixelText (g, nameText, mi.removeFromTop (30), juce::Justification::centredLeft, redHi, 9, nameScale, nameScale == 2, true);
    if (info.name.isNotEmpty())
        pixelText (g, juce::String (tmu::categoryName (info.category)).toUpperCase() + " / " + info.source.toUpperCase(), mi.removeFromTop (14),
                   juce::Justification::centredLeft, red.withAlpha (0.9f), 7, 1, true, false);

    // ---- transporte (valor en vivo)
    const int tr = (int) transpose.getValue();
    pixelText (g, "TRANSP " + juce::String (tr > 0 ? "+" : "") + juce::String (tr), trArea.withY (508).withHeight (12).expanded (30, 0),
               juce::Justification::centred, bone.withAlpha (0.7f), 7, 1, true, false);
}

void TMUEditor::resized()
{
    lcdArea = { 54, 94, W - 108, 330 };
    miniLcdArea = { 66, 440, 360, 78 };
    keySlot = { 54, 536, W - 108, 90 };
    dpadCentre = { 500, 470 };
    volArea = { 580, 442, 62, 62 };
    trArea = { 660, 442, 62, 62 };

    viewType.setBounds   (W - 380, 18, 90, 64);
    viewSource.setBounds (W - 272, 18, 124, 64);
    viewAZ.setBounds     (W - 132, 18, 90, 64);

    auto glass = lcdArea.reduced (14);
    search.setBounds (glass.getX() + 116, glass.getY() + 8, 360, 28);
    auto body = glass.withTrimmedTop (48).reduced (10, 6);
    groupList.setBounds (body.removeFromLeft (270));
    body.removeFromLeft (14);
    presetList.setBounds (body);

    prevButton.setBounds (dpadCentre.x - 46, dpadCentre.y - 15, 31, 30);
    nextButton.setBounds (dpadCentre.x + 15, dpadCentre.y - 15, 31, 30);
    volume.setBounds (volArea);
    transpose.setBounds (trArea);
    rescanButton.setBounds (750, 458, 58, 22);
    folderButton.setBounds (820, 458, 58, 22);

    keyboard.setBounds (keySlot.reduced (8, 7));

    buildChassis();
}

bool TMUEditor::keyPressed (const juce::KeyPress& k)
{
    if (k == juce::KeyPress::upKey)   { step (-1); return true; }
    if (k == juce::KeyPress::downKey) { step (1);  return true; }
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

    if (++tick % 8 == 0) { repaint (miniLcdArea); repaint (trArea.expanded (40, 20)); repaint (lcdArea.withHeight (64)); }
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

    groups.push_back ({ "*", "Todos", (int) entries->size() });

    if (proc.uiView == "type")
    {
        std::array<int, tmu::NumCategories> counts {};
        for (auto& e : *entries) counts[(size_t) e.category]++;
        for (int c = 0; c < tmu::NumCategories; ++c)
            if (counts[(size_t) c] > 0) groups.push_back ({ juce::String (c), tmu::categoryName (c), counts[(size_t) c] });
    }
    else if (proc.uiView == "source")
    {
        std::map<juce::String, int> counts;
        for (auto& e : *entries) counts[e.source]++;
        std::vector<std::pair<juce::String, int>> v (counts.begin(), counts.end());
        std::sort (v.begin(), v.end(), [] (auto& a, auto& b) { return a.first.compareNatural (b.first) < 0; });
        for (auto& [name, n] : v) groups.push_back ({ name, name, n });
    }
    else
    {
        std::map<juce::String, int> counts;
        for (auto& e : *entries)
        {
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
            if (! entryInGroup (e, currentGroup)) continue;
            bool ok = true;
            for (auto& w : words)
                if (! e.name.containsIgnoreCase (w) && ! e.source.containsIgnoreCase (w)
                    && ! juce::String (tmu::categoryName (e.category)).containsIgnoreCase (w)) { ok = false; break; }
            if (ok) filtered.push_back (i);
        }
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
void TMUEditor::GroupModel::paintListBoxItem (int row, juce::Graphics& g, int w, int h, bool selected)
{
    if (row < 0 || row >= (int) ed.groups.size()) return;
    const auto& gr = ed.groups[(size_t) row];
    auto r = juce::Rectangle<int> (0, 0, w, h).reduced (2, 1);

    if (selected)
    {
        g.setColour (red.withAlpha (0.18f));
        g.fillRect (r);
    }
    r.removeFromLeft (4);
    pixelText (g, selected ? ">" : "", r.removeFromLeft (16), juce::Justification::centredLeft, redHi, 8, 2, true);
    pixelText (g, juce::String (gr.count), r.removeFromRight (42).withTrimmedRight (4), juce::Justification::centredRight, redDim.brighter (0.4f), 7, 2, false, false);
    r.removeFromRight (10);
    pixelText (g, gr.label.toUpperCase(), r, juce::Justification::centredLeft, selected ? redHi : red.withAlpha (0.7f), 8, 2, selected, selected);
}

void TMUEditor::PresetModel::paintListBoxItem (int row, juce::Graphics& g, int w, int h, bool selected)
{
    if (ed.entries == nullptr || row < 0 || row >= (int) ed.filtered.size()) return;
    const auto& e = (*ed.entries)[(size_t) ed.filtered[(size_t) row]];
    const bool isLoaded = e.phdrIndex == ed.lastInfo.phdrIndex && e.file == ed.lastInfo.file;

    auto r = juce::Rectangle<int> (0, 0, w, h).reduced (2, 1);
    if (isLoaded || selected)
    {
        g.setColour (red.withAlpha (isLoaded ? 0.22f : 0.10f));
        g.fillRect (r);
    }
    r.removeFromLeft (6);
    if (isLoaded)
    {
        g.setColour (redHi);
        g.fillRect (r.removeFromLeft (8).withSizeKeepingCentre (6, 6));
    }
    else r.removeFromLeft (8);
    r.removeFromLeft (8);

    const auto right = ed.proc.uiView == "source" ? juce::String (tmu::categoryName (e.category)) : e.source;
    pixelText (g, right.toUpperCase(), r.removeFromRight (juce::jmin (230, w / 2)).withTrimmedRight (6),
               juce::Justification::centredRight, redDim.brighter (0.45f), 7, 2, false, false);
    pixelText (g, e.name.toUpperCase(), r, juce::Justification::centredLeft, isLoaded ? redHi : red.withAlpha (0.85f), 8, 2, isLoaded, true);
}

void TMUEditor::GroupModel::selectedRowsChanged (int row)
{
    if (ed.suppressSelection || row < 0 || row >= (int) ed.groups.size()) return;
    ed.currentGroup = ed.groups[(size_t) row].key;
    ed.proc.uiGroup = ed.currentGroup;
    ed.rebuildFiltered();
}


void TMUEditor::PresetModel::selectedRowsChanged (int row)
{
    if (ed.suppressSelection || row < 0) return;
    ed.choosePreset (row);
}
