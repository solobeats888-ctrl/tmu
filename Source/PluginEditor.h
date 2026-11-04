#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"

// Estetica: consola de cartucho (diseno original) en negro con franja violeta / purpura:
// pantalla en la ranura del cartucho, botones pill, perillas, mini piano y tutorial al empezar.
class TMULook : public juce::LookAndFeel_V4
{
public:
    TMULook();
    void setAccent (juce::Colour c) { accent = c; }
    juce::Colour accent;

    juce::Font pixel (float h) const;   // fuente principal: JetBrains Mono Bold
    juce::Font heavy (float h) const;   // JetBrains Mono ExtraBold (logo y titulos)
    juce::Font signature (float h) const;   // firma "viszeversa": Kihim (si esta) o la cursiva del sistema
    float mainScale = 1.0f;   // ajuste de tamano segun la fuente principal

    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos, float start, float end, juce::Slider&) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool over, bool down) override;
    void drawButtonText (juce::Graphics&, juce::TextButton&, bool over, bool down) override;
    void drawTextEditorOutline (juce::Graphics&, int, int, juce::TextEditor&) override {}
    void fillTextEditorBackground (juce::Graphics&, int, int, juce::TextEditor&) override {}
    void drawScrollbar (juce::Graphics&, juce::ScrollBar&, int x, int y, int w, int h, bool vertical,
                        int thumbStart, int thumbSize, bool over, bool down) override;
    int getDefaultScrollbarWidth() override { return 8; }

private:
    juce::Typeface::Ptr press, pressHeavy, sig;
};

// Mini piano pintado
class RetroKeyboard : public juce::MidiKeyboardComponent
{
public:
    RetroKeyboard (juce::MidiKeyboardState& s, TMULook& l) : juce::MidiKeyboardComponent (s, horizontalKeyboard), look (l) {}
    void drawWhiteNote (int note, juce::Graphics&, juce::Rectangle<float> area, bool isDown, bool isOver, juce::Colour, juce::Colour) override;
    void drawBlackNote (int note, juce::Graphics&, juce::Rectangle<float> area, bool isDown, bool isOver, juce::Colour) override;
    void drawUpDownButton (juce::Graphics&, int w, int h, bool over, bool down, bool movesOctavesUp) override;
private:
    TMULook& look;
};

// Tutorial: tapa la interfaz, ilumina una zona y explica que hace
class TutorialOverlay : public juce::Component
{
public:
    struct Step { juce::String title, text; juce::Rectangle<int> spot; };
    TutorialOverlay (TMULook& l) : look (l) { setInterceptsMouseClicks (true, true); }
    void start (std::vector<Step> s) { steps = std::move (s); index = 0; setVisible (true); toFront (true); repaint(); }
    std::function<void()> onFinished;
    void paint (juce::Graphics&) override;
    void mouseUp (const juce::MouseEvent&) override;
private:
    juce::Rectangle<int> boxArea() const;
    juce::Rectangle<int> nextArea() const, skipArea() const;
    TMULook& look;
    std::vector<Step> steps;
    size_t index = 0;
};

// Activacion: tapa la interfaz hasta que se pone el mail y el numero de serie de la compra
class LicenseOverlay : public juce::Component
{
public:
    explicit LicenseOverlay (TMULook&);
    std::function<void()> onActivated;
    void paint (juce::Graphics&) override;
    void resized() override;
private:
    juce::Rectangle<int> boxArea() const { return getLocalBounds().withSizeKeepingCentre (600, 360); }
    void tryActivate();
    void onlineResult (const juce::String& mail, const juce::String& serial, tmulicense::OnlineResult r);
    bool busy = false;
    TMULook& look;
    juce::SharedResourcePointer<tmulicense::State> state;
    juce::TextEditor emailBox, serialBox;
    juce::TextButton activateButton { "ACTIVAR" };
    juce::String message;
    bool error = false;
};

// Perilla con click derecho: MIDI learn propio + el menu del host (FL: "Link to controller")
class ParamKnob : public juce::Slider
{
public:
    explicit ParamKnob (juce::String id) : juce::Slider (RotaryHorizontalVerticalDrag, NoTextBox), paramId (std::move (id)) {}
    void mouseDown (const juce::MouseEvent& e) override
    {
        if (e.mods.isPopupMenu()) { if (onRightClick) onRightClick (*this); return; }
        juce::Slider::mouseDown (e);
    }
    const juce::String paramId;
    std::function<void (ParamKnob&)> onRightClick;
};

// Capas (layering): 4 casilleros con el sonido de cada capa. Click = elegir la capa (a donde va el
// sonido que elijas en la lista), cuadrito = prender/apagar, barra = volumen, click derecho = mas opciones.
class LayerStrip : public juce::Component,
                   public juce::SettableTooltipClient
{
public:
    LayerStrip (TMUProcessor& p, TMULook& l) : proc (p), look (l)
    {
        setTooltip ("Capas: click elige la capa, el cuadrito la prende o apaga, la barra es su volumen (doble click = 0 dB). Click derecho: mas opciones");
    }
    std::function<void()> onChange;   // se eligio otra capa o se vacio una
    void refresh();                   // repinta solo si algo cambio

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;

private:
    static constexpr int gap = 8;
    juce::Rectangle<int> slot (int i) const;
    juce::Rectangle<int> ledArea (int i) const;
    juce::Rectangle<int> barArea (int i) const;
    int slotAt (juce::Point<int>) const;
    bool isOn (int i) const;
    void selectLayer (int i);
    void toggleOn (int i);
    void resetVolume (int i);
    void setVolumeFromX (int i, int x);
    void showMenu (int i);

    juce::String nameOfLayer (int i) const;   // nombre que se muestra (el propio si lo renombro)

    TMUProcessor& proc;
    TMULook& look;
    juce::SharedResourcePointer<UserPrefs> prefs;
    int dragLayer = -1;
    juce::String lastSig;
};

class TMUEditor : public juce::AudioProcessorEditor,
                  private juce::Timer
{
public:
    explicit TMUEditor (TMUProcessor&);
    ~TMUEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    bool keyPressed (const juce::KeyPress&) override;

private:
    struct Group { juce::String key, label; int count = 0; };

    struct GroupModel : public juce::ListBoxModel
    {
        explicit GroupModel (TMUEditor& e) : ed (e) {}
        int getNumRows() override { return (int) ed.groups.size(); }
        void paintListBoxItem (int row, juce::Graphics&, int w, int h, bool selected) override;
        void selectedRowsChanged (int row) override;
        TMUEditor& ed;
    };

    struct PresetModel : public juce::ListBoxModel
    {
        explicit PresetModel (TMUEditor& e) : ed (e) {}
        int getNumRows() override { return (int) ed.filtered.size(); }
        void paintListBoxItem (int row, juce::Graphics&, int w, int h, bool selected) override;
        void selectedRowsChanged (int row) override;
        void listBoxItemClicked (int row, const juce::MouseEvent&) override;
        TMUEditor& ed;
    };

    void timerCallback() override;
    bool isVisible (const PresetEntry& e) const { return showDuplicates || e.hidden == 0; }
    void setStarsForRow (int filteredRow, int stars);
    juce::String shownName (const PresetEntry& e) const { return prefs->shownName (e.key, e.name); }   // nombre propio si lo renombro
    void renameRow (int filteredRow);        // abre la ventanita para cambiarle el nombre al sonido
    void applyCustomName (const juce::String& key, const juce::String& newName);
    static juce::Rectangle<int> starsArea (int rowWidth, int rowHeight);
    void setView (const juce::String& v);
    void rebuildGroups();
    void rebuildFiltered();
    void choosePreset (int filteredRow);
    void step (int delta);
    void selectLoadedRow();
    bool entryInGroup (const PresetEntry& e, const juce::String& groupKey) const;
    void buildChassis();
    void showKnobMenu (ParamKnob&);
    void onLayerChanged();

    TMUProcessor& proc;
    TMULook look;

    std::shared_ptr<const std::vector<PresetEntry>> entries;
    std::vector<Group> groups;
    std::vector<int> filtered;
    juce::String currentGroup;
    bool suppressSelection = false;
    int tick = 0;
    TMUProcessor::LoadedInfo lastInfo;
    juce::Image chassis;
    float chassisScale = 2.0f;
    juce::String lastPaintSig;

    GroupModel groupModel { *this };
    PresetModel presetModel { *this };
    juce::ListBox groupList { "groups", &groupModel }, presetList { "presets", &presetModel };

    juce::TextButton viewType { "TIPO" }, viewSource { "SF2" }, viewAZ { "A-Z" }, dupButton { "SIN REP" }, favButton { "FAV" }, helpButton { "?" };
    juce::TextButton getButton { "BAJAR SF2" };   // lista de paginas con soundfonts gratis
    void showSoundfontLinks();
    juce::TextEditor search;
    juce::TextButton rescanButton { "RESCAN" }, folderButton { "CARPETA" }, prevButton { "<" }, nextButton { ">" };
    TutorialOverlay tutorial { look };
    LicenseOverlay licenseOverlay { look };
    void showTutorial();
    juce::SharedResourcePointer<UserPrefs> prefs;
    int prefsVersion = -1;
    bool showDuplicates = false;

    ParamKnob volume { "volume" }, modKnob { "mod" }, filterKnob { "filter" }, transpose { "transpose" },
              rateKnob { "rate" }, swingKnob { "swing" }, humanKnob { "humanize" },
              inKnob { "fadein" }, outKnob { "fadeout" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> knobAtt[9];   // mismo orden que allKnobs()
    void attachKnobs();   // las perillas manejan los parametros de la capa elegida
    juce::TextButton velButton;   // curva de velocity: SUAVE / NORMAL / DURA / FIJA
    std::array<ParamKnob*, 9> allKnobs() { return { &volume, &modKnob, &rateKnob, &filterKnob, &transpose, &swingKnob, &humanKnob, &inKnob, &outKnob }; }
    void updateVelButton();
    juce::String velText() const;

    RetroKeyboard keyboard;
    LayerStrip layerStrip;
    int lastLayer = 0;

    juce::Rectangle<int> lcdArea, miniLcdArea, layerArea, keySlot, volArea, modArea, filterArea, trArea,
                         rateArea, swingArea, humanArea, velArea, inArea, outArea, knobsArea;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TMUEditor)
};
