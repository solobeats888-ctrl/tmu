#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"

class TMULook : public juce::LookAndFeel_V4
{
public:
    TMULook();
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos, float start, float end, juce::Slider&) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool over, bool down) override;
    void drawTextEditorOutline (juce::Graphics&, int w, int h, juce::TextEditor&) override;
    void fillTextEditorBackground (juce::Graphics&, int w, int h, juce::TextEditor&) override;
    juce::Font getTextButtonFont (juce::TextButton&, int h) override;
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

    //==============================================================================
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

    TMUProcessor& proc;
    TMULook look;

    std::shared_ptr<const std::vector<PresetEntry>> entries;
    std::vector<Group> groups;
    std::vector<int> filtered;
    juce::String currentGroup;
    bool suppressSelection = false;
    int tick = 0;
    TMUProcessor::LoadedInfo lastInfo;

    GroupModel groupModel { *this };
    PresetModel presetModel { *this };
    juce::ListBox groupList { "groups", &groupModel }, presetList { "presets", &presetModel };

    juce::TextButton viewType { "POR TIPO" }, viewSource { "POR SOUNDFONT" }, viewAZ { "A - Z" };
    juce::TextEditor search;
    juce::TextButton rescanButton { "Reescanear" }, folderButton { "Carpeta" };
    juce::TextButton prevButton { "<" }, nextButton { ">" };

    juce::Slider volume { juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::NoTextBox };
    juce::Slider transpose { juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::NoTextBox };
    juce::AudioProcessorValueTreeState::SliderAttachment volumeAtt, transposeAtt;

    juce::MidiKeyboardComponent keyboard;

    juce::Rectangle<int> headerArea, footerArea, nowArea, volArea, trArea;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TMUEditor)
};
