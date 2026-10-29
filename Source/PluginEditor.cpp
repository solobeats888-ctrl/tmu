#include "PluginEditor.h"

namespace
{
const juce::Colour bg      (0xff0f0f13);
const juce::Colour panel   (0xff17171d);
const juce::Colour panel2  (0xff1f1f28);
const juce::Colour line    (0xff2b2b37);
const juce::Colour text    (0xffececf1);
const juce::Colour dim     (0xff8b8b9c);
const juce::Colour accent  (0xffff3d6e);

juce::Font font (float size, bool bold = false)
{
    return juce::Font (juce::FontOptions (size, bold ? juce::Font::bold : juce::Font::plain));
}

juce::Colour categoryColour (int c)
{
    return juce::Colour::fromHSV ((float) c / (float) tmu::NumCategories, 0.55f, 0.95f, 1.0f);
}
}

//==============================================================================
TMULook::TMULook()
{
    setColour (juce::ListBox::backgroundColourId, panel);
    setColour (juce::ListBox::outlineColourId, line);
    setColour (juce::TextEditor::textColourId, text);
    setColour (juce::TextEditor::highlightColourId, accent.withAlpha (0.35f));
    setColour (juce::CaretComponent::caretColourId, accent);
    setColour (juce::TextButton::textColourOffId, dim);
    setColour (juce::TextButton::textColourOnId, juce::Colours::white);
    setColour (juce::ScrollBar::thumbColourId, line.brighter (0.3f));
    setColour (juce::MidiKeyboardComponent::keyDownOverlayColourId, accent.withAlpha (0.8f));
    setColour (juce::MidiKeyboardComponent::mouseOverKeyOverlayColourId, accent.withAlpha (0.25f));
    setColour (juce::MidiKeyboardComponent::whiteNoteColourId, juce::Colour (0xffe9e9ee));
    setColour (juce::MidiKeyboardComponent::blackNoteColourId, juce::Colour (0xff1a1a20));
    setColour (juce::MidiKeyboardComponent::keySeparatorLineColourId, juce::Colour (0xff9a9aa8));
    setColour (juce::MidiKeyboardComponent::shadowColourId, juce::Colours::black.withAlpha (0.4f));
}

void TMULook::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float start, float end, juce::Slider&)
{
    auto r = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (4.0f);
    const float size = juce::jmin (r.getWidth(), r.getHeight());
    r = r.withSizeKeepingCentre (size, size);
    const auto c = r.getCentre();
    const float radius = size * 0.5f;
    const float angle = start + pos * (end - start);

    juce::Path track, value;
    track.addCentredArc (c.x, c.y, radius - 2, radius - 2, 0, start, end, true);
    value.addCentredArc (c.x, c.y, radius - 2, radius - 2, 0, start, angle, true);
    g.setColour (line);
    g.strokePath (track, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour (accent);
    g.strokePath (value, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    g.setColour (panel2);
    g.fillEllipse (r.reduced (7.0f));
    g.setColour (text);
    const juce::Point<float> tip (c.x + (radius - 11) * std::sin (angle), c.y - (radius - 11) * std::cos (angle));
    g.drawLine ({ c, tip }, 2.0f);
}

void TMULook::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool over, bool down)
{
    auto r = b.getLocalBounds().toFloat().reduced (0.5f);
    const bool on = b.getToggleState();
    g.setColour (on ? accent : (down ? line.brighter (0.2f) : (over ? line : panel2)));
    g.fillRoundedRectangle (r, 6.0f);
    if (! on)
    {
        g.setColour (line);
        g.drawRoundedRectangle (r, 6.0f, 1.0f);
    }
}

void TMULook::drawTextEditorOutline (juce::Graphics& g, int w, int h, juce::TextEditor& te)
{
    g.setColour (te.hasKeyboardFocus (true) ? accent : line);
    g.drawRoundedRectangle (juce::Rectangle<float> (0, 0, (float) w, (float) h).reduced (0.5f), 6.0f, 1.0f);
}

void TMULook::fillTextEditorBackground (juce::Graphics& g, int w, int h, juce::TextEditor&)
{
    g.setColour (panel2);
    g.fillRoundedRectangle (juce::Rectangle<float> (0, 0, (float) w, (float) h), 6.0f);
}

juce::Font TMULook::getTextButtonFont (juce::TextButton&, int h)
{
    return font (juce::jmin (13.0f, (float) h * 0.5f), true);
}

//==============================================================================
TMUEditor::TMUEditor (TMUProcessor& p)
    : juce::AudioProcessorEditor (&p), proc (p),
      volumeAtt (p.params, "volume", volume),
      transposeAtt (p.params, "transpose", transpose),
      keyboard (p.keyboardState, juce::MidiKeyboardComponent::horizontalKeyboard)
{
    setLookAndFeel (&look);

    for (auto* b : { &viewType, &viewSource, &viewAZ })
    {
        b->setClickingTogglesState (false);
        addAndMakeVisible (b);
    }
    viewType.onClick   = [this] { setView ("type"); };
    viewSource.onClick = [this] { setView ("source"); };
    viewAZ.onClick     = [this] { setView ("az"); };

    search.setTextToShowWhenEmpty ("Buscar sonido...", dim);
    search.setFont (font (14.0f));
    search.setIndents (10, 7);
    search.setText (proc.uiSearch, false);
    search.onTextChange = [this] { proc.uiSearch = search.getText(); rebuildFiltered(); };
    search.onEscapeKey  = [this] { search.clear(); proc.uiSearch = {}; rebuildFiltered(); };
    addAndMakeVisible (search);

    rescanButton.onClick = [this] { proc.library->rescan(); };
    folderButton.onClick = [] { auto f = PresetLibrary::getUserFolder(); f.createDirectory(); f.startAsProcess(); };
    folderButton.setTooltip ("Abre la carpeta donde podes agregar mas .sf2");
    addAndMakeVisible (rescanButton);
    addAndMakeVisible (folderButton);

    prevButton.onClick = [this] { step (-1); };
    nextButton.onClick = [this] { step (1); };
    addAndMakeVisible (prevButton);
    addAndMakeVisible (nextButton);

    for (auto* lb : { &groupList, &presetList })
    {
        lb->setRowHeight (lb == &groupList ? 30 : 28);
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
    keyboard.setLowestVisibleKey (36);
    keyboard.setKeyWidth (18.0f);
    keyboard.setWantsKeyboardFocus (false);
    keyboard.setScrollButtonsVisible (true);
    addAndMakeVisible (keyboard);

    setSize (880, 580);

    currentGroup = proc.uiGroup;
    setView (proc.uiView.isEmpty() ? juce::String ("type") : proc.uiView);
    startTimerHz (15);
}

TMUEditor::~TMUEditor()
{
    setLookAndFeel (nullptr);
}

//==============================================================================
void TMUEditor::paint (juce::Graphics& g)
{
    g.fillAll (bg);

    // ---- header
    g.setColour (panel);
    g.fillRect (headerArea);
    g.setColour (line);
    g.drawHorizontalLine (headerArea.getBottom(), 0.0f, (float) getWidth());

    auto logo = headerArea.withWidth (190).reduced (18, 0);
    g.setColour (accent);
    g.setFont (font (30.0f, true));
    g.drawText ("TMU", logo.removeFromLeft (70), juce::Justification::centredLeft);
    g.setColour (dim);
    g.setFont (font (10.5f, true));
    g.drawFittedText ("SOUNDFONT\nLIBRARY", logo, juce::Justification::centredLeft, 2);

    // ---- footer
    g.setColour (panel);
    g.fillRect (footerArea);
    g.setColour (line);
    g.drawHorizontalLine (footerArea.getY(), 0.0f, (float) getWidth());

    const auto info = proc.getLoadedInfo();
    auto na = nowArea;
    g.setColour (dim);
    g.setFont (font (11.0f, true));
    g.drawText ("SONANDO", na.removeFromTop (16), juce::Justification::centredLeft);
    g.setColour (text);
    g.setFont (font (20.0f, true));
    g.drawText (info.name.isNotEmpty() ? info.name : juce::String ("(ningun sonido)"), na.removeFromTop (26), juce::Justification::centredLeft, true);

    auto meta = na.removeFromTop (20);
    if (info.name.isNotEmpty())
    {
        g.setColour (categoryColour (info.category));
        g.fillEllipse (meta.removeFromLeft (10).withSizeKeepingCentre (8, 8).toFloat());
        meta.removeFromLeft (6);
        g.setColour (dim);
        g.setFont (font (12.5f));
        g.drawText (juce::String (tmu::categoryName (info.category)) + "  /  " + info.source, meta, juce::Justification::centredLeft, true);
    }

    juce::String status;
    if (info.loading) status = "Cargando...";
    else if (info.error.isNotEmpty()) status = info.error;
    else if (proc.library->isScanning()) status = "Escaneando soundfonts...";
    if (status.isNotEmpty())
    {
        g.setColour (info.error.isNotEmpty() && ! info.loading ? accent : dim);
        g.setFont (font (12.0f));
        g.drawText (status, nowArea.withTrimmedTop (62), juce::Justification::topLeft, true);
    }

    g.setColour (dim);
    g.setFont (font (10.5f, true));
    g.drawText ("VOLUMEN", volArea.withY (volArea.getBottom() + 2).withHeight (14), juce::Justification::centred);
    g.drawText ("TRANSPORTE " + juce::String ((int) transpose.getValue()), trArea.withY (trArea.getBottom() + 2).withHeight (14).expanded (20, 0), juce::Justification::centred);

    // ---- mensaje si no hay sonidos
    if (entries != nullptr && entries->empty() && ! proc.library->isScanning())
    {
        g.setColour (dim);
        g.setFont (font (14.0f));
        g.drawFittedText ("No se encontraron soundfonts.\nInstala TMU con el instalador .pkg, o pone archivos .sf2 en\nMusica / TMU SoundFonts y toca Reescanear.",
                          presetList.getBounds().reduced (20), juce::Justification::centred, 4);
    }
}

void TMUEditor::resized()
{
    auto r = getLocalBounds();
    headerArea = r.removeFromTop (64);
    footerArea = r.removeFromBottom (170);

    auto h = headerArea.reduced (14, 15);
    h.removeFromLeft (180);
    viewType.setBounds (h.removeFromLeft (92));   h.removeFromLeft (6);
    viewSource.setBounds (h.removeFromLeft (128)); h.removeFromLeft (6);
    viewAZ.setBounds (h.removeFromLeft (64));     h.removeFromLeft (14);
    folderButton.setBounds (h.removeFromRight (78)); h.removeFromRight (6);
    rescanButton.setBounds (h.removeFromRight (96)); h.removeFromRight (10);
    search.setBounds (h);

    auto body = r.reduced (12, 12);
    groupList.setBounds (body.removeFromLeft (220));
    body.removeFromLeft (10);
    presetList.setBounds (body);

    auto f = footerArea.reduced (14, 10);
    keyboard.setBounds (f.removeFromBottom (66));
    f.removeFromBottom (8);

    auto controls = f.removeFromRight (300);
    trArea = controls.removeFromRight (70).withTrimmedBottom (16);
    controls.removeFromRight (16);
    volArea = controls.removeFromRight (70).withTrimmedBottom (16);
    volume.setBounds (volArea);
    transpose.setBounds (trArea);
    controls.removeFromRight (20);
    auto nav = controls.removeFromRight (90).withSizeKeepingCentre (90, 34);
    prevButton.setBounds (nav.removeFromLeft (42));
    nextButton.setBounds (nav.removeFromRight (42));

    nowArea = f;
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
        repaint (footerArea);
    }
    if (++tick % 8 == 0) repaint (footerArea);   // estado de escaneo y valor del transporte
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
    auto r = juce::Rectangle<int> (0, 0, w, h).reduced (6, 2);

    if (selected)
    {
        g.setColour (panel2.brighter (0.08f));
        g.fillRoundedRectangle (r.toFloat(), 5.0f);
        g.setColour (accent);
        g.fillRoundedRectangle (r.removeFromLeft (3).toFloat(), 1.5f);
    }
    else r.removeFromLeft (3);

    r.removeFromLeft (8);
    if (ed.proc.uiView == "type" && gr.key != "*")
    {
        g.setColour (categoryColour (gr.key.getIntValue()));
        g.fillEllipse (r.removeFromLeft (8).withSizeKeepingCentre (8, 8).toFloat());
        r.removeFromLeft (8);
    }
    g.setColour (dim);
    g.setFont (font (11.5f));
    g.drawText (juce::String (gr.count), r.removeFromRight (44).withTrimmedRight (6), juce::Justification::centredRight);
    g.setColour (selected ? text : text.withAlpha (0.8f));
    g.setFont (font (13.5f, selected));
    g.drawText (gr.label, r, juce::Justification::centredLeft, true);
}

void TMUEditor::GroupModel::selectedRowsChanged (int row)
{
    if (ed.suppressSelection || row < 0 || row >= (int) ed.groups.size()) return;
    ed.currentGroup = ed.groups[(size_t) row].key;
    ed.proc.uiGroup = ed.currentGroup;
    ed.rebuildFiltered();
}

void TMUEditor::PresetModel::paintListBoxItem (int row, juce::Graphics& g, int w, int h, bool selected)
{
    if (ed.entries == nullptr || row < 0 || row >= (int) ed.filtered.size()) return;
    const auto& e = (*ed.entries)[(size_t) ed.filtered[(size_t) row]];
    const bool isLoaded = e.phdrIndex == ed.lastInfo.phdrIndex && e.file == ed.lastInfo.file;

    auto r = juce::Rectangle<int> (0, 0, w, h).reduced (6, 1);
    if (selected || isLoaded)
    {
        g.setColour (isLoaded ? accent.withAlpha (0.16f) : panel2);
        g.fillRoundedRectangle (r.toFloat(), 5.0f);
    }
    else if (row % 2 == 1)
    {
        g.setColour (panel.brighter (0.025f));
        g.fillRect (r);
    }

    r.removeFromLeft (10);
    g.setColour (categoryColour (e.category));
    g.fillEllipse (r.removeFromLeft (7).withSizeKeepingCentre (7, 7).toFloat());
    r.removeFromLeft (10);

    const auto right = ed.proc.uiView == "source" ? juce::String (tmu::categoryName (e.category)) : e.source;
    auto rightArea = r.removeFromRight (juce::jmin (300, w / 2)).withTrimmedRight (10);
    g.setColour (dim);
    g.setFont (font (12.0f));
    g.drawText (right, rightArea, juce::Justification::centredRight, true);

    g.setColour (isLoaded ? accent.brighter (0.4f) : text);
    g.setFont (font (14.0f, isLoaded));
    g.drawText (e.name, r, juce::Justification::centredLeft, true);
}

void TMUEditor::PresetModel::selectedRowsChanged (int row)
{
    if (ed.suppressSelection || row < 0) return;
    ed.choosePreset (row);
}
