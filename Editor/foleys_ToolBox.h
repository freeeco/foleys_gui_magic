/*
 ==============================================================================
    Copyright (c) 2019-2022 Foleys Finest Audio - Daniel Walz
    All rights reserved.

    License for non-commercial projects:

    Redistribution and use in source and binary forms, with or without modification,
    are permitted provided that the following conditions are met:
    1. Redistributions of source code must retain the above copyright notice, this
       list of conditions and the following disclaimer.
    2. Redistributions in binary form must reproduce the above copyright notice,
       this list of conditions and the following disclaimer in the documentation
       and/or other materials provided with the distribution.
    3. Neither the name of the copyright holder nor the names of its contributors
       may be used to endorse or promote products derived from this software without
       specific prior written permission.

    License for commercial products:

    To sell commercial products containing this module, you are required to buy a
    License from https://foleysfinest.com/developer/pluginguimagic/

    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
    ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
    WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
    IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
    INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
    BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
    DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
    LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
    OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
    OF THE POSSIBILITY OF SUCH DAMAGE.
 ==============================================================================
 */

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "foleys_GUITreeEditor.h"
#include "foleys_PropertiesEditor.h"
#include "foleys_Palette.h"

namespace foleys
{

class MagicGUIBuilder;

/**
 The Toolbox defines a floating window, that allows live editing of the currently loaded GUI.
 */
class ToolBox  : public juce::Component,
                 public juce::DragAndDropContainer,
                 public juce::KeyListener,
                 private juce::MultiTimer
{
public:
    /**
     Create a ToolBox floating window to edit the currently shown GUI.
     The window will float attached to the edited window.

     @param parent is the window to attach to
     @param builder is the builder instance that manages the GUI
     */
    ToolBox (juce::Component* parent, MagicGUIBuilder& builder);
    ~ToolBox() override;

    enum PositionOption  { left, right, detached };

    void loadDialog();
    void save();
    void saveDialog();

    void loadGUI (const juce::File& file);
    bool saveGUI (const juce::File& file);

    void paint (juce::Graphics& g) override;

    void resized() override;

    void timerCallback (int timer) override;

    void setSelectedNode (const juce::ValueTree& node);
    void setNodeToEdit (juce::ValueTree node);

    void setToolboxPosition (PositionOption position);

    void stateWasReloaded();

    bool keyPressed (const juce::KeyPress& key) override;
    bool keyPressed (const juce::KeyPress& key, juce::Component* originalComponent) override;

    static juce::PropertiesFile::Options getApplicationPropertyStorage();

    void setLastLocation (juce::File file);

private:
    struct IconButtonLookAndFeel : public juce::LookAndFeel_V4
    {
        IconButtonLookAndFeel (juce::Typeface::Ptr tf) : typeface (tf) {}
        
        juce::Font getTextButtonFont (juce::TextButton&, int buttonHeight) override
        {
            return juce::Font (typeface).withHeight (buttonHeight * 0.6f);
        }
        
        juce::Typeface::Ptr typeface;
    };

    enum Timers : int
    {
        WindowDrag=1,
        AutoSave
    };

    static juce::String positionOptionToString (PositionOption option);
    static PositionOption positionOptionFromString (const juce::String& text);

    static std::unique_ptr<juce::FileFilter> getFileFilter();

    /**
     Recursively walks a ValueTree and makes all PGM parameter references unique
     by appending a timestamp suffix. Detects references by the presence of ":"
     in the property value, while excluding URLs and values with whitespace.
     Any existing timestamp suffix (8+ digit number after a dash) is stripped
     first to avoid ever-growing suffixes on re-used snippets.
     */
    static juce::ValueTree makeParameterRefsUnique (juce::ValueTree tree);

    /**
     Inserts a snippet from a file into the currently selected node (or root).
     If the Option/Alt key is held at call time, the snippet is inserted as-is
     without making parameter references unique - useful for global/shared
     parameters such as BPM that should remain the same across copies.
     */
    void insertSnippet (const juce::File& file);

    // Edit operations - called from both the Edit menu and keyboard shortcuts
    void performUndo();
    void performRedo();
    void performCut();
    void performCopy();
    void performPaste();
    void performPasteUnique();
    void performPasteDimensions();
    void performPasteItemProperties();
    void performDuplicate();
    void performDuplicateUnique();
    void performSendToBack();
    void performSendBack();
    void performBringForward();
    void performBringToFront();
    void performSelectParent();
    void performDeselect();
    void performPasteStyling();
    void performClearDimensions();
    void performWrapInView();

    juce::Component::SafePointer<juce::Component> parent;

    MagicGUIBuilder&            builder;
    juce::UndoManager&          undo;
    juce::ApplicationProperties appProperties;

    juce::TextButton    fileMenu       { TRANS ("File") };
    juce::TextButton    editMenu       { TRANS ("Edit") };
    juce::TextButton    viewMenu       { TRANS ("View") };
    juce::TextButton    snippetsButton { TRANS ("Snippets") };
//    juce::TextButton    editSwitch { juce::String (juce::CharPointer_UTF8 ("\xf0\x9f\x94\x92")) };
    juce::TextButton    editSwitch { juce::String::fromUTF8 (u8"\uf140") };

    PositionOption      positionOption      { left };

    GUITreeEditor       treeEditor          { builder };
    PropertiesEditor    propertiesEditor    { builder };
    Palette             palette             { builder };

    juce::StretchableLayoutManager    resizeManager;
    juce::StretchableLayoutResizerBar resizer1 { &resizeManager, 1, false };
    juce::StretchableLayoutResizerBar resizer3 { &resizeManager, 3, false };

    std::unique_ptr<juce::FileBrowserComponent> fileBrowser;
    juce::File                                  lastLocation;
    juce::File                                  autoSaveFile;
    
    juce::LookAndFeel_V4        defaultLAF;
    std::unique_ptr<IconButtonLookAndFeel> editSwitchLAF;
    juce::Typeface::Ptr         fontAudio { juce::Typeface::createSystemTypefaceFor (BinaryData::FontAudio_ttf, BinaryData::FontAudio_ttfSize) };
    
    juce::TooltipWindow tooltipWindow { this, 1500 };  // 500ms delay before showing

    void updateToolboxPosition();
    juce::ResizableCornerComponent resizeCorner { this, nullptr };
    juce::ComponentDragger componentDragger;

    void mouseDown (const juce::MouseEvent& e) override;
    void mouseDrag (const juce::MouseEvent& e) override;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ToolBox)
};

} // namespace foleys
