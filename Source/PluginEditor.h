#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"

// Estetica: consola de videojuegos de 16 bits (diseno original) en bordo, rojo y negro:
// letras pixeladas, pantalla tipo CRT, cruceta para pasar de sonido y botones redondos de joystick.
class TMULook : public juce::LookAndFeel_V4
{
public:
    TMULook();
    void setAccent (juce::Colour c);
    juce::Colour accent;

    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos, float start, float end, juce::Slider&) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool over, bool down) override;
    void drawButtonText (juce::Graphics&, juce::TextButton&, bool over, bool down) override;
    void drawTextEditorOutline (juce::Graphics&, int, int, juce::TextEditor&) override {}
    void fillTextEditorBackground (juce::Graphics&, int, int, juce::TextEditor&) override {}
    void drawScrollbar (juce::Graphics&, juce::ScrollBar&, int x, int y, int w, int h, bool vertical,
                        int thumbStart, int thumbSize, bool over, bool down) override;
    int getDefaultScrollbarWidth() override { return 8; }
};

// Teclado con teclas redondeadas en la paleta de la consola
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
        TMUEditor& ed;
    };

    void timerCallback() override;
    void setView (const juce::String& v);
    void rebuildGroups();
    void rebuildFiltered();
    void choosePreset (int filteredRow);
    void step (int delta);
    void selectLoadedRow();
    bool entryInGroup (const PresetEntry& e, const juce::String& groupKey) const;
    void buildChassis();

    TMUProcessor& proc;
    TMULook look;

    std::shared_ptr<const std::vector<PresetEntry>> entries;
    std::vector<Group> groups;
    std::vector<int> filtered;
    juce::String currentGroup;
    bool suppressSelection = false;
    int tick = 0;
    TMUProcessor::LoadedInfo lastInfo;
    juce::Colour targetAccent;
    juce::Image chassis;   // fondo pre-renderizado (madera, bordo, tornillos, serigrafia)

    GroupModel groupModel { *this };
    PresetModel presetModel { *this };
    juce::ListBox groupList { "groups", &groupModel }, presetList { "presets", &presetModel };

    juce::TextButton viewType { "TIPO" }, viewSource { "SOUNDFONT" }, viewAZ { "A - Z" };
    juce::TextEditor search;
    juce::TextButton rescanButton { "RESCAN" }, folderButton { "CARPETA" };
    juce::TextButton prevButton { "<" }, nextButton { ">" };

    juce::Slider volume { juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::NoTextBox };
    juce::Slider transpose { juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::NoTextBox };
    juce::AudioProcessorValueTreeState::SliderAttachment volumeAtt, transposeAtt;

    RetroKeyboard keyboard;

    juce::Rectangle<int> lcdArea, miniLcdArea, keySlot, volArea, trArea;
    juce::Point<int> dpadCentre;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TMUEditor)
};
