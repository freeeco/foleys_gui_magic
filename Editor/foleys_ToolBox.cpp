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

#include "foleys_ToolBox.h"

namespace foleys
{

namespace IDs
{
    static juce::String lastLocation { "lastLocation" };
}

ToolBox::ToolBox (juce::Component* parentToUse, MagicGUIBuilder& builderToControl)
  : parent (parentToUse),
    builder (builderToControl),
    undo (builder.getUndoManager())
{
    appProperties.setStorageParameters (getApplicationPropertyStorage());

    if (auto* properties = appProperties.getUserSettings())
    {
        setToolboxPosition (ToolBox::positionOptionFromString (properties->getValue ("position")));
        setAlwaysOnTop (properties->getValue ("alwaysOnTop") == "true");
        GuiItem::selectionToFront = properties->getValue ("selectionToFront") == "true";
    }

    EditorColours::background          = ToolBoxColours::bg;
    EditorColours::outline             = ToolBoxColours::border;
    EditorColours::text                = ToolBoxColours::text;
    EditorColours::disabledText        = ToolBoxColours::textDim;
    EditorColours::removeButton        = ToolBoxColours::textDim.darker (0.4f);
    EditorColours::selectedBackground  = ToolBoxColours::accent;

    setLookAndFeel (&toolBoxLAF);
    tooltipWindow.setLookAndFeel (&defaultLAF);

    setOpaque (true);
    setWantsKeyboardFocus (true);

    fileMenu.setConnectedEdges (juce::TextButton::ConnectedOnLeft | juce::TextButton::ConnectedOnRight);
    editMenu.setConnectedEdges (juce::TextButton::ConnectedOnLeft | juce::TextButton::ConnectedOnRight);
    viewMenu.setConnectedEdges (juce::TextButton::ConnectedOnLeft | juce::TextButton::ConnectedOnRight);
    snippetsButton.setConnectedEdges (juce::TextButton::ConnectedOnLeft | juce::TextButton::ConnectedOnRight);
    editSwitch.setConnectedEdges (juce::TextButton::ConnectedOnLeft | juce::TextButton::ConnectedOnRight);

    addAndMakeVisible (fileMenu);
    addAndMakeVisible (editMenu);
    addAndMakeVisible (viewMenu);
    addAndMakeVisible (snippetsButton);
    addAndMakeVisible (editSwitch);
    
    editSwitch.setLookAndFeel (&editSwitchLAF);

    //==========================================================================
    // File menu
    //==========================================================================
    fileMenu.onClick = [&]
    {
        juce::PopupMenu file;

        {
            juce::PopupMenu::Item it ("Load XML");
            it.action = [&] { loadDialog(); };
            it.shortcutKeyDescription = "Cmd+O";
            file.addItem (it);
        }
        {
            juce::PopupMenu::Item it ("Save XML");
            it.action = [&] { save(); };
            it.shortcutKeyDescription = "Cmd+S";
            file.addItem (it);
        }
        {
            juce::PopupMenu::Item it ("Save XML As...");
            it.action = [&] { saveDialog(); };
            it.shortcutKeyDescription = "Shift+Cmd+S";
            file.addItem (it);
        }

        file.addSeparator();
        file.addItem ("Clear", [&] { builder.clearGUI(); });
        file.addSeparator();

        {
            juce::PopupMenu::Item it ("Refresh");
            it.action = [&] { builder.updateComponents(); };
            it.shortcutKeyDescription = "Cmd+R";
            file.addItem (it);
        }

        file.setLookAndFeel (&toolBoxLAF);
        file.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&fileMenu));
    };

    //==========================================================================
    // View menu
    //==========================================================================
    viewMenu.onClick = [&]
    {
        juce::PopupMenu view;

        view.addItem ("Left",     true, positionOption == left,     [&]() { setToolboxPosition (left); });
        view.addItem ("Right",    true, positionOption == right,    [&]() { setToolboxPosition (right); });
        view.addItem ("Detached", true, positionOption == detached, [&]() { setToolboxPosition (detached); });
        view.addSeparator();
        view.addItem ("AlwaysOnTop", true, isAlwaysOnTop(), [&]()
        {
            setAlwaysOnTop (!isAlwaysOnTop());
            if (auto* properties = appProperties.getUserSettings())
                properties->setValue ("alwaysOnTop", isAlwaysOnTop() ? "true" : "false");
        });
        view.addSeparator();

        {
            juce::PopupMenu::Item it ("Expand All");
            it.action = [&] { treeEditor.expandAll(); };
            view.addItem (it);
        }
        {
            juce::PopupMenu::Item it ("Collapse All");
            it.action = [&] { treeEditor.collapseAll(); };
            view.addItem (it);
        }

        view.addSeparator();
        
        {
            juce::PopupMenu::Item it (showItemsPanel ? "Hide Items Panel" : "Show Items Panel");
            it.shortcutKeyDescription = "Tab";
            it.action = [&]
            {
                showItemsPanel = !showItemsPanel;
                palette.setVisible (showItemsPanel);
                resizer3.setVisible (showItemsPanel);
                resized();
            };
            view.addItem (it);
        }
        
        view.addSeparator();
        
        view.addSeparator();
        {
            juce::PopupMenu previewMenu;
            previewMenu.addItem ("Desktop", true, previewMode == desktop, [&]
            {
                previewMode = desktop;
                builder.getMagicState().getPropertyAsValue ("system:is-desktop").setValue (true);
                builder.getMagicState().getPropertyAsValue ("system:is-ios").setValue (false);
            });
            previewMenu.addItem ("iOS", true, previewMode == ios, [&]
            {
                previewMode = ios;
                builder.getMagicState().getPropertyAsValue ("system:is-desktop").setValue (false);
                builder.getMagicState().getPropertyAsValue ("system:is-ios").setValue (true);
            });
            view.addSubMenu ("Preview", previewMenu);
        }

        view.setLookAndFeel (&toolBoxLAF);
        view.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&viewMenu));
    };

    //==========================================================================
    // Edit menu (replaces the old Undo button)
    //==========================================================================
    editMenu.onClick = [&]
    {
        juce::PopupMenu edit;

        {
            auto undoDesc = undo.getUndoDescription();
            juce::PopupMenu::Item it (undoDesc.isNotEmpty() ? "Undo " + undoDesc : "Undo");
            it.action = [&] { performUndo(); };
            it.shortcutKeyDescription = "Cmd+Z";
            it.isEnabled = undo.canUndo();
            edit.addItem (it);
        }
        {
            auto redoDesc = undo.getRedoDescription();
            juce::PopupMenu::Item it (redoDesc.isNotEmpty() ? "Redo " + redoDesc : "Redo");
            it.action = [&] { performRedo(); };
            it.shortcutKeyDescription = "Shift+Cmd+Z";
            it.isEnabled = undo.canRedo();
            edit.addItem (it);
        }

        edit.addSeparator();

        {
            juce::PopupMenu::Item it ("Cut");
            it.action = [&] { performCut(); };
            it.shortcutKeyDescription = "Cmd+X";
            edit.addItem (it);
        }
        {
            juce::PopupMenu::Item it ("Copy");
            it.action = [&] { performCopy(); };
            it.shortcutKeyDescription = "Cmd+C";
            edit.addItem (it);
        }
        {
            juce::PopupMenu::Item it ("Paste");
            it.action = [&] { performPaste(); };
            it.shortcutKeyDescription = "Cmd+V";
            edit.addItem (it);
        }
        
        edit.addSeparator();
        
        {
            juce::PopupMenu::Item it ("Paste Unique");
            it.action = [&] { performPasteUnique(); };
            it.shortcutKeyDescription = "Shift+Cmd+V";
            edit.addItem (it);
        }
        {
            juce::PopupMenu::Item it ("Paste Dimensions");
            it.action = [&] { performPasteDimensions(); };
            it.shortcutKeyDescription = "Shift+Control+V";
            edit.addItem (it);
        }
        {
            juce::PopupMenu::Item it ("Paste Item Properties");
            it.action = [&] { performPasteItemProperties(); };
            it.shortcutKeyDescription = "Shift+Opt+Cmd+V";
            edit.addItem (it);
        }
        {
            juce::PopupMenu::Item it ("Paste Styling");
            it.action = [&] { performPasteStyling(); };
            it.shortcutKeyDescription = "Cmd+T";
            edit.addItem (it);
        }
        {
            juce::PopupMenu::Item it ("Paste Replace");
            it.action = [&] { performPasteReplace(); };
            it.shortcutKeyDescription = "Opt+Cmd+V";
            edit.addItem (it);
        }
        
        edit.addSeparator();
        
        {
            juce::PopupMenu::Item it ("Duplicate");
            it.action = [&] { performDuplicate(); };
            it.shortcutKeyDescription = "Opt+Cmd+D";
            edit.addItem (it);
        }
        {
            juce::PopupMenu::Item it ("Duplicate Unique");
            it.action = [&] { performDuplicateUnique(); };
            it.shortcutKeyDescription = "Cmd+D";
            edit.addItem (it);
        }
        
        edit.addSeparator();
        
        {
            juce::PopupMenu::Item it ("Delete");
            it.action = [&]
            {
                auto selected = builder.getSelectedNode();
                if (selected.isValid())
                {
                    auto p = selected.getParent();
                    if (p.isValid())
                    {
                        undo.beginNewTransaction ("Delete");
                        p.removeChild (selected, &undo);
                    }
                }
            };
            it.shortcutKeyDescription = "Delete";
            edit.addItem (it);
        }
        {
            juce::PopupMenu::Item it ("Delete All Children");
            it.action = [&]
            {
                auto selected = builder.getSelectedNode();
                if (selected.isValid())
                {
                    undo.beginNewTransaction ("Delete All Children");
                    selected.removeAllChildren (&undo);
                }
            };
            it.shortcutKeyDescription = "Cmd+Delete";
            edit.addItem (it);
        }
        
        edit.addSeparator();
        
        {
            juce::PopupMenu insertMenu;

            {
                juce::PopupMenu::Item it ("View (Contents)");
                it.action = [&] { performInsertViewContents(); };
                it.shortcutKeyDescription = "Cmd+N";
                insertMenu.addItem (it);
            }
            {
                juce::PopupMenu::Item it ("View (Flexbox)");
                it.action = [&] { performInsertViewFlexbox(); };
                it.shortcutKeyDescription = "Shift+Cmd+N";
                insertMenu.addItem (it);
            }
            {
                juce::PopupMenu::Item it ("View (Tabbed)");
                it.action = [&] { performInsertViewTabbed(); };
                it.shortcutKeyDescription = "Opt+Cmd+N";
                insertMenu.addItem (it);
            }

            edit.addSubMenu ("Insert", insertMenu);
        }
        
        edit.addSeparator();

        {
            juce::PopupMenu::Item it ("Send to Back");
            it.action = [&] { performSendToBack(); };
            it.shortcutKeyDescription = "Shift+Cmd+[";
            edit.addItem (it);
        }
        {
            juce::PopupMenu::Item it ("Send Back");
            it.action = [&] { performSendBack(); };
            it.shortcutKeyDescription = "Cmd+[";
            edit.addItem (it);
        }
        {
            juce::PopupMenu::Item it ("Bring Forward");
            it.action = [&] { performBringForward(); };
            it.shortcutKeyDescription = "Cmd+]";
            edit.addItem (it);
        }
        {
            juce::PopupMenu::Item it ("Bring to Front");
            it.action = [&] { performBringToFront(); };
            it.shortcutKeyDescription = "Shift+Cmd+]";
            edit.addItem (it);
        }

        edit.addSeparator();

        {
            juce::PopupMenu::Item it ("Select Parent");
            it.action = [&] { performSelectParent(); };
            it.shortcutKeyDescription = "Cmd+P";
            edit.addItem (it);
        }
        {
            juce::PopupMenu::Item it ("Deselect");
            it.action = [&] { performDeselect(); };
            it.shortcutKeyDescription = "Cmd+U";
            edit.addItem (it);
        }

        edit.addSeparator();

        {
            juce::PopupMenu::Item it ("Clear Dimensions");
            it.action = [&] { performClearDimensions(); };
            edit.addItem (it);
        }
        {
            juce::PopupMenu::Item it ("Wrap in View");
            it.action = [&] { performWrapInView(); };
            it.shortcutKeyDescription = "Cmd+W";
            edit.addItem (it);
        }
        edit.addSeparator();
        {
            juce::PopupMenu::Item it ("Selection To Front");
            it.isTicked = GuiItem::selectionToFront;
            it.shortcutKeyDescription = "Opt+Cmd+F";
            it.action = [&]
            {
                GuiItem::selectionToFront = !GuiItem::selectionToFront;
                if (auto* properties = appProperties.getUserSettings())
                    properties->setValue ("selectionToFront", GuiItem::selectionToFront ? "true" : "false");
                
                auto selected = builder.getSelectedNode();
                if (selected.isValid())
                {
                    if (GuiItem::selectionToFront)
                    {
                        if (auto* item = builder.findGuiItem (selected))
                            item->toFrontForEditing();
                    }
                    else
                    {
                        builder.restoreZOrderForAll ();
                    }
                }
            };
            edit.addItem (it);
        }

        edit.addSeparator();

        {
            juce::PopupMenu::Item it ("Edit Mode");
            it.isTicked = builder.isEditModeOn();
            it.shortcutKeyDescription = "Cmd+E";
            it.action = [&] { editSwitch.triggerClick(); };
            edit.addItem (it);
        }

        edit.setLookAndFeel (&toolBoxLAF);
        edit.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&editMenu));
    };

    //==========================================================================
    // Edit mode toggle
    //==========================================================================
    editSwitch.setClickingTogglesState (true);
    editSwitch.setColour (juce::TextButton::buttonOnColourId, EditorColours::selectedBackground);
    editSwitch.onClick = [&]
    {
        builder.setEditMode (editSwitch.getToggleState());
        editSwitch.repaint();
    };

    //==========================================================================
    // Snippets menu
    //==========================================================================
    snippetsButton.onClick = [&]
    {
        juce::PopupMenu snippets;

        auto snippetsFolder = juce::File::getRealUserHomeDirectory()
                                  .getChildFile ("GitHub")
                                  .getChildFile ("toybox_plugins")
                                  .getChildFile ("XML Snippets");

        if (!snippetsFolder.exists())
            snippetsFolder = juce::File::getCurrentWorkingDirectory().getChildFile ("XML Snippets");

        // Save Snippet As...
        snippets.addItem ("Save Snippet As...", [&, snippetsFolder]()
        {
            auto selected = builder.getSelectedNode();
            if (!selected.isValid())
            {
                juce::AlertWindow::showMessageBoxAsync (juce::AlertWindow::WarningIcon,
                                                       "No Selection",
                                                       "Please select a node to save as a snippet.");
                return;
            }

            if (!snippetsFolder.exists())
                snippetsFolder.createDirectory();

            // Pre-fill filename from node id, falling back to node type
            juce::String defaultName = "snippet";
            if (selected.hasProperty ("id"))
                defaultName = selected.getProperty ("id").toString();
            else if (selected.getType().isValid())
                defaultName = selected.getType().toString();

            auto suggestedFile = snippetsFolder.getChildFile (defaultName).withFileExtension (".xml");

            auto dialog = std::make_unique<FileBrowserDialog>(NEEDS_TRANS ("Cancel"), NEEDS_TRANS ("Save"),
                                                              juce::FileBrowserComponent::saveMode |
                                                              juce::FileBrowserComponent::canSelectFiles |
                                                              juce::FileBrowserComponent::warnAboutOverwriting,
                                                              suggestedFile,
                                                              getFileFilter());
            dialog->setAcceptFunction ([&, dlg=dialog.get(), selected]
            {
                auto snippetFile = dlg->getFile();
                if (!snippetFile.hasFileExtension (".xml"))
                    snippetFile = snippetFile.withFileExtension (".xml");

                if (auto stream = snippetFile.createOutputStream())
                    stream->writeString (selected.toXmlString());

                builder.closeOverlayDialog();
            });
            dialog->setCancelFunction ([&] { builder.closeOverlayDialog(); });

            builder.showOverlayDialog (std::move (dialog));
        });

        // Open Snippets Folder
        snippets.addItem ("Open Snippets Folder", [snippetsFolder]()
        {
            if (!snippetsFolder.exists())
                snippetsFolder.createDirectory();

            snippetsFolder.revealToUser();
        });

        snippets.addSeparator();

        // Recursive folder contents
        std::function<void(juce::PopupMenu&, const juce::File&)> addFolderContents;
        addFolderContents = [&](juce::PopupMenu& menu, const juce::File& folder)
        {
            juce::Array<juce::File> items;

            juce::RangedDirectoryIterator iter (folder, false, "*", juce::File::findFilesAndDirectories);
            for (const auto& entry : iter)
            {
                auto file = entry.getFile();
                if (file.isDirectory() || file.hasFileExtension (".xml"))
                    items.add (file);
            }

            struct FileComparator
            {
                int compareElements (const juce::File& a, const juce::File& b) const
                {
                    if (a.isDirectory() && !b.isDirectory()) return -1;
                    if (!a.isDirectory() && b.isDirectory()) return 1;
                    return a.getFileName().compareIgnoreCase (b.getFileName());
                }
            };

            FileComparator comparator;
            items.sort (comparator);

            for (auto& item : items)
            {
                if (item.isDirectory())
                {
                    juce::PopupMenu subMenu;
                    addFolderContents (subMenu, item);
                    menu.addSubMenu (item.getFileName(), subMenu);
                }
                else if (item.hasFileExtension (".xml"))
                {
                    menu.addItem (item.getFileNameWithoutExtension(), [&, item]()
                    {
                        insertSnippet (item);
                    });
                }
            }
        };

        if (snippetsFolder.exists() && snippetsFolder.isDirectory())
        {
            addFolderContents (snippets, snippetsFolder);
        }
        else
        {
            snippets.addItem ("XML Snippets folder not found", false, false, [](){});
            snippets.addSeparator();
            snippets.addItem ("Expected location: " + snippetsFolder.getFullPathName(), false, false, [](){});
        }
        
        snippets.setLookAndFeel (&toolBoxLAF);
        snippets.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&snippetsButton));
    };

    //==========================================================================

    addAndMakeVisible (treeEditor);
    addAndMakeVisible (resizer1);
    addAndMakeVisible (propertiesEditor);
    addAndMakeVisible (resizer3);
    addAndMakeVisible (palette);

    resizeManager.setItemLayout (0, 1, -1.0, -0.4);
    resizeManager.setItemLayout (1, 6, 6, 6);
    resizeManager.setItemLayout (2, 1, -1.0, -0.3);
    resizeManager.setItemLayout (3, 6, 6, 6);
    resizeManager.setItemLayout (4, 1, -1.0, -0.3);

    addChildComponent (resizeCorner);
    resizeCorner.setAlwaysOnTop (true);

    int x = 100;
    int y = 0;
    int width = 300;
    int height = 800;

    if (auto* display = juce::Desktop::getInstance().getDisplays().getPrimaryDisplay())
    {
        height = display->userArea.getHeight();
        y = display->userArea.getY();
    }
#if defined TOOLBOX_X
    x = TOOLBOX_X;
#endif
#if defined TOOLBOX_Y
    y = TOOLBOX_Y;
#endif
#if defined TOOLBOX_WIDTH
    width = TOOLBOX_WIDTH;
#endif
#if defined TOOLBOX_HEIGHT
    height = TOOLBOX_HEIGHT;
#endif

    setBounds (x, y, width, height);
    addToDesktop (getLookAndFeel().getMenuWindowFlags());

    startTimer (Timers::WindowDrag, 100);
    startTimer (ModifierKeys, 10);

    setVisible (true);

    stateWasReloaded();

    parent->addKeyListener (this);
    resizer1.addMouseListener (this, false);
}

ToolBox::~ToolBox()
{
    if (parent != nullptr)
        parent->removeKeyListener (this);

    stopTimer (Timers::WindowDrag);
    stopTimer (Timers::AutoSave);

    if (autoSaveFile.existsAsFile() && lastLocation.hasIdenticalContentTo (autoSaveFile))
        autoSaveFile.deleteFile();
    
    editSwitch.setLookAndFeel (nullptr);
    setLookAndFeel (nullptr);
}

//==============================================================================
// Edit operations
//==============================================================================

void ToolBox::performUndo()
{
    undo.undo();
}

void ToolBox::performRedo()
{
    undo.redo();
}

void ToolBox::performCut()
{
    auto selected = builder.getSelectedNode();
    if (selected.isValid())
    {
        juce::SystemClipboard::copyTextToClipboard (selected.toXmlString());
        auto p = selected.getParent();
        if (p.isValid()){
            undo.beginNewTransaction ("Cut");
            p.removeChild (selected, &undo);
        }
    }
}

void ToolBox::performCopy()
{
    auto selected = builder.getSelectedNode();
    if (selected.isValid())
        juce::SystemClipboard::copyTextToClipboard (selected.toXmlString());
}

void ToolBox::performPaste()
{
    auto paste = juce::ValueTree::fromXml (juce::SystemClipboard::getTextFromClipboard());
    auto selected = builder.getSelectedNode();
    if (paste.isValid() && selected.isValid()){
        undo.beginNewTransaction("Paste");
        builder.draggedItemOnto (paste, selected);
    }
}

void ToolBox::performPasteUnique()
{
    auto paste = juce::ValueTree::fromXml (juce::SystemClipboard::getTextFromClipboard());
    auto selected = builder.getSelectedNode();
    if (paste.isValid() && selected.isValid()){
        undo.beginNewTransaction("Paste Unique");
        builder.draggedItemOnto (makeParameterRefsUnique (paste), selected);
    }
}

void ToolBox::performPasteDimensions()
{
    auto paste = juce::ValueTree::fromXml (juce::SystemClipboard::getTextFromClipboard());
    auto selected = builder.getSelectedNode();

    if (!paste.isValid() || !selected.isValid())
        return;

    static const juce::Identifier dimensionProps[] = {
        juce::Identifier ("pos-x"),
        juce::Identifier ("pos-y"),
        juce::Identifier ("pos-width"),
        juce::Identifier ("pos-height"),
        juce::Identifier ("dont-snap-to-pixels")
    };

    undo.beginNewTransaction("Paste Dimensions");

    for (const auto& prop : dimensionProps)
        if (paste.hasProperty (prop))
            selected.setProperty (prop, paste.getProperty (prop), &undo);
}

void ToolBox::performPasteItemProperties()
{
    auto paste = juce::ValueTree::fromXml (juce::SystemClipboard::getTextFromClipboard());
    auto selected = builder.getSelectedNode();

    if (!paste.isValid() || !selected.isValid())
        return;

    static const juce::Identifier itemProps[] = {
        juce::Identifier ("pos-x"),
        juce::Identifier ("pos-y"),
        juce::Identifier ("pos-width"),
        juce::Identifier ("pos-height"),
        juce::Identifier ("dont-snap-to-pixels"),
        juce::Identifier ("width"),
        juce::Identifier ("height"),
        juce::Identifier ("min-width"),
        juce::Identifier ("min-height"),
        juce::Identifier ("max-width"),
        juce::Identifier ("max-height"),
        juce::Identifier ("flex-grow"),
        juce::Identifier ("flex-shrink"),
        juce::Identifier ("flex-order"),
        juce::Identifier ("flex-align-self"),
        juce::Identifier ("scale"),
        juce::Identifier ("width-scale"),
        juce::Identifier ("height-scale"),
        juce::Identifier ("horizontal-position"),
        juce::Identifier ("vertical-position"),
        juce::Identifier ("rotate"),
        juce::Identifier ("opacity"),
        juce::Identifier ("glow-radius"),
        juce::Identifier ("glow-distance"),
        juce::Identifier ("glow-angle"),
        juce::Identifier ("glow-opacity"),
        juce::Identifier ("shadow-enable"),
        juce::Identifier ("shadow-colour"),
        juce::Identifier ("redraw-all"),
        juce::Identifier ("scale-value"),
        juce::Identifier ("width-scale-value"),
        juce::Identifier ("height-scale-value"),
        juce::Identifier ("horizontal-position-value"),
        juce::Identifier ("vertical-position-value"),
        juce::Identifier ("rotate-value"),
        juce::Identifier ("origin-x"),
        juce::Identifier ("origin-y"),
        juce::Identifier ("origin-x-offset"),
        juce::Identifier ("origin-y-offset"),
        juce::Identifier ("opacity-value")
    };

    undo.beginNewTransaction("Paste Item Properties");

    for (const auto& prop : itemProps)
        if (paste.hasProperty (prop))
            selected.setProperty (prop, paste.getProperty (prop), &undo);
}

void ToolBox::performPasteStyling()
{
    auto paste = juce::ValueTree::fromXml (juce::SystemClipboard::getTextFromClipboard());
    auto selected = builder.getSelectedNode();

    if (!paste.isValid() || !selected.isValid())
        return;

    static const juce::Identifier stylingProps[] = {
        juce::Identifier ("style-class"),
        juce::Identifier ("lookAndFeel"),
        juce::Identifier ("background-colour"),
        juce::Identifier ("border-colour"),
        juce::Identifier ("border"),
        juce::Identifier ("border-radius"),
        juce::Identifier ("padding"),
        juce::Identifier ("margin"),
        juce::Identifier ("background-image"),
        juce::Identifier ("image-placement"),
        juce::Identifier ("background-gradient"),
        juce::Identifier ("gradient-pos1"),
        juce::Identifier ("gradient-pos2"),
        juce::Identifier ("gradient-colour1"),
        juce::Identifier ("gradient-colour2"),
        juce::Identifier ("gradient-type"),
        juce::Identifier ("caption"),
        juce::Identifier ("caption-size"),
        juce::Identifier ("caption-colour"),
        juce::Identifier ("caption-placement"),
        juce::Identifier ("font-size"),
        juce::Identifier ("justification"),
        juce::Identifier ("tooltip"),
        juce::Identifier ("tab-caption"),
        juce::Identifier ("tab-colour"),
        juce::Identifier ("visibility"),
        juce::Identifier ("alpha")
    };

    undo.beginNewTransaction("Paste Styling");

    for (const auto& prop : stylingProps)
        if (paste.hasProperty (prop))
            selected.setProperty (prop, paste.getProperty (prop), &undo);
}

void ToolBox::performPasteReplace()
{
    auto paste = juce::ValueTree::fromXml (juce::SystemClipboard::getTextFromClipboard());
    auto selected = builder.getSelectedNode();

    if (!paste.isValid())
    {
        juce::AlertWindow::showMessageBoxAsync (juce::AlertWindow::WarningIcon,
                                                "Nothing To Paste",
                                                "Nothing to paste");
        return;
    }

    if (!selected.isValid())
    {
        juce::AlertWindow::showMessageBoxAsync (juce::AlertWindow::WarningIcon,
                                                "Nothing Selected",
                                                "Nothing selected");
        return;
    }

    auto parent = selected.getParent();
    if (!parent.isValid())
    {
        juce::AlertWindow::showMessageBoxAsync (juce::AlertWindow::WarningIcon,
                                                "Invalid Parent",
                                                "Invalid parent");
        return;
    }

    auto index = parent.indexOf (selected);

    undo.beginNewTransaction ("Paste Replace");

    auto replacement = makeParameterRefsUnique (paste);
    parent.removeChild (selected, &undo);
    parent.addChild (replacement, index, &undo);
    builder.setSelectedNode (replacement);
}

void ToolBox::performDuplicate()
{
    auto selected = builder.getSelectedNode();
    auto paste    = juce::ValueTree::fromXml (selected.toXmlString());

    if (paste.isValid() && selected.isValid())
    {
        undo.beginNewTransaction ("Duplicate");
        offsetDuplicatePosition (paste, selected.getParent());
        builder.draggedItemOnto (paste,
                                 selected.getParent(),
                                 selected.getParent().indexOf (selected) + 1);

        this->setSelectedNode (paste);
        juce::MessageManager::callAsync ([this, paste]() mutable
        {
            if (auto* item = this->treeEditor.getItemForNode (paste))
                this->treeEditor.getTreeView().scrollToKeepItemVisible (item);
        });
    }
}

void ToolBox::performDuplicateUnique()
{
    auto selected = builder.getSelectedNode();
    auto paste    = makeParameterRefsUnique (juce::ValueTree::fromXml (selected.toXmlString()));

    if (paste.isValid() && selected.isValid())
    {
        undo.beginNewTransaction ("Duplicate Unique");
        offsetDuplicatePosition (paste, selected.getParent());
        builder.draggedItemOnto (paste,
                                 selected.getParent(),
                                 selected.getParent().indexOf (selected) + 1);

        this->setSelectedNode (paste);
        juce::MessageManager::callAsync ([this, paste]() mutable
        {
            if (auto* item = this->treeEditor.getItemForNode (paste))
                this->treeEditor.getTreeView().scrollToKeepItemVisible (item);
        });
    }
}

void ToolBox::performSendToBack()
{
    auto selected = builder.getSelectedNode();
    if (!selected.isValid())
        return;

    auto parent = selected.getParent();
    if (!parent.isValid())
        return;

    auto currentIndex = parent.indexOf (selected);
    if (currentIndex <= 0)
        return;

    undo.beginNewTransaction ("Send To Back");
    parent.moveChild (currentIndex, 0, &undo);
}

void ToolBox::performSendBack()
{
    auto selected = builder.getSelectedNode();
    if (!selected.isValid())
        return;

    auto parent = selected.getParent();
    if (!parent.isValid())
        return;

    auto currentIndex = parent.indexOf (selected);
    if (currentIndex <= 0)
        return;

    undo.beginNewTransaction ("Send Back");
    parent.moveChild (currentIndex, currentIndex - 1, &undo);
}

void ToolBox::performBringForward()
{
    auto selected = builder.getSelectedNode();
    if (!selected.isValid())
        return;

    auto parent = selected.getParent();
    if (!parent.isValid())
        return;

    auto currentIndex = parent.indexOf (selected);
    if (currentIndex >= parent.getNumChildren() - 1)
        return;

    undo.beginNewTransaction ("Bring Foward");
    parent.moveChild (currentIndex, currentIndex + 1, &undo);
}

void ToolBox::performBringToFront()
{
    auto selected = builder.getSelectedNode();
    if (!selected.isValid())
        return;

    auto parent = selected.getParent();
    if (!parent.isValid())
        return;

    auto currentIndex = parent.indexOf (selected);
    if (currentIndex >= parent.getNumChildren() - 1)
        return;

    undo.beginNewTransaction ("Bring To Front");
    parent.moveChild (currentIndex, parent.getNumChildren() - 1, &undo);
}

void ToolBox::performSelectParent()
{
    auto selected = builder.getSelectedNode();
    if (!selected.isValid())
        return;

    auto parent = selected.getParent();
    if (parent.isValid())
        setSelectedNode (parent);
}

void ToolBox::performDeselect()
{
    setSelectedNode ({});
    builder.setSelectedNode ({});
}

void ToolBox::performClearDimensions()
{
    auto selected = builder.getSelectedNode();
    if (!selected.isValid())
        return;

    undo.beginNewTransaction("Clear Dimensions");

    propertiesEditor.removeProperties ({
        juce::Identifier ("pos-x"),
        juce::Identifier ("pos-y"),
        juce::Identifier ("pos-width"),
        juce::Identifier ("pos-height"),
        juce::Identifier ("dont-snap-to-pixels")
    });
}

void ToolBox::performWrapInView()
{
    auto selected = builder.getSelectedNode();
    if (!selected.isValid())
        return;

    auto parent = selected.getParent();
    if (!parent.isValid())
        return;

    undo.beginNewTransaction("Wrap In View");

    auto index = parent.indexOf (selected);

    // Create the wrapper View node
    juce::ValueTree wrapper ("View");

    // Transfer dimension properties from selected to wrapper,
    // breaking live bindings before removal
    static const juce::Identifier dimensionProps[] = {
        juce::Identifier ("pos-x"),
        juce::Identifier ("pos-y"),
        juce::Identifier ("pos-width"),
        juce::Identifier ("pos-height"),
        juce::Identifier ("dont-snap-to-pixels")
    };

    for (const auto& prop : dimensionProps)
        if (selected.hasProperty (prop))
            wrapper.setProperty (prop, selected.getProperty (prop), nullptr);

    propertiesEditor.removeProperties ({
        juce::Identifier ("pos-x"),
        juce::Identifier ("pos-y"),
        juce::Identifier ("pos-width"),
        juce::Identifier ("pos-height"),
        juce::Identifier ("dont-snap-to-pixels")
    });

    // Insert wrapper where the selected node was
    builder.draggedItemOnto (wrapper, parent, index);

    // Move selected into the wrapper
    builder.draggedItemOnto (selected, wrapper);

    setSelectedNode (wrapper);
}

void ToolBox::performInsertViewContents()
{
    auto selected = builder.getSelectedNode();
    if (!selected.isValid()) return;

    undo.beginNewTransaction ("Insert View (Contents)");
    juce::ValueTree newView ("View");
    newView.setProperty ("display", "contents", &undo);

    if (selected == builder.getGuiRootNode())
        selected.appendChild (newView, &undo);
    else
    {
        auto parent = selected.getParent();
        if (!parent.isValid()) return;
        parent.addChild (newView, parent.indexOf (selected) + 1, &undo);
    }

    builder.setSelectedNode (newView);
}

void ToolBox::performInsertViewFlexbox()
{
    auto selected = builder.getSelectedNode();
    if (!selected.isValid()) return;

    undo.beginNewTransaction ("Insert View (Flexbox)");
    juce::ValueTree newView ("View");

    if (selected == builder.getGuiRootNode())
        selected.appendChild (newView, &undo);
    else
    {
        auto parent = selected.getParent();
        if (!parent.isValid()) return;
        parent.addChild (newView, parent.indexOf (selected) + 1, &undo);
    }

    builder.setSelectedNode (newView);
}

void ToolBox::performInsertViewTabbed()
{
    auto selected = builder.getSelectedNode();
    if (!selected.isValid()) return;

    undo.beginNewTransaction ("Insert View (Tabbed)");
    juce::ValueTree newView ("View");
    newView.setProperty ("display", "tabbed", &undo);

    if (selected == builder.getGuiRootNode())
        selected.appendChild (newView, &undo);
    else
    {
        auto parent = selected.getParent();
        if (!parent.isValid()) return;
        parent.addChild (newView, parent.indexOf (selected) + 1, &undo);
    }

    builder.setSelectedNode (newView);
}

//==============================================================================
// Snippet insertion
//==============================================================================

void ToolBox::insertSnippet (const juce::File& file)
{
    auto xmlContent = file.loadFileAsString();
    auto snippet = juce::ValueTree::fromXml (xmlContent);

    if (!snippet.isValid())
        return;

    undo.beginNewTransaction ("Insert Snippet");

    const bool optionHeld = juce::ModifierKeys::getCurrentModifiers().isAltDown();
    if (!optionHeld)
        snippet = makeParameterRefsUnique (snippet);

    auto selected = builder.getSelectedNode();
    auto root = builder.getGuiRootNode();

    if (!selected.isValid() || selected == root)
    {
        // Nothing selected or root selected — insert inside root
        builder.draggedItemOnto (snippet, root);
    }
    else
    {
        // Insert as sibling: add to parent, right after the selected node
        auto parent = selected.getParent();
        if (parent.isValid())
        {
            auto index = parent.indexOf (selected) + 1;
            parent.addChild (snippet, index, &undo);
        }
        else
        {
            builder.draggedItemOnto (snippet, root);
        }
    }
}

//==============================================================================

void ToolBox::mouseDown (const juce::MouseEvent& e)
{
    if (e.originalComponent == &resizer1)
        return;
    if (positionOption == PositionOption::detached)
        componentDragger.startDraggingComponent (this, e);
}

void ToolBox::mouseDrag (const juce::MouseEvent& e)
{
    if (e.originalComponent == &resizer1)
        return;
    if (positionOption == PositionOption::detached)
        componentDragger.dragComponent (this, e, nullptr);
}

void ToolBox::mouseUp (const juce::MouseEvent& e)
{
    if (e.originalComponent == &resizer1)
    {
        if (showItemsPanel)
            savedTreeHeightShowing = treeEditor.getHeight();
        else
            savedTreeHeightHidden = treeEditor.getHeight();
    }
}

void ToolBox::loadDialog()
{
    auto dialog = std::make_unique<FileBrowserDialog>(NEEDS_TRANS ("Cancel"), NEEDS_TRANS ("Load"),
                                                      juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                                                      lastLocation, getFileFilter());
    dialog->setAcceptFunction ([&, dlg=dialog.get()]
    {
        loadGUI (dlg->getFile());
        builder.closeOverlayDialog();
    });
    dialog->setCancelFunction ([&]
    {
        builder.closeOverlayDialog();
    });

    builder.showOverlayDialog (std::move (dialog));
}

void ToolBox::save()
{
    auto xmlFile = lastLocation;
    if (xmlFile.existsAsFile())
    {
        saveGUI (xmlFile);

        // Flash the button as a save indicator
        fileMenu.setColour (juce::TextButton::buttonColourId, juce::Colours::grey);
        juce::Timer::callAfterDelay (300, [this]() {
            fileMenu.removeColour (juce::TextButton::buttonColourId);
        });
    }
    else
    {
        saveDialog();
    }
}

void ToolBox::saveDialog()
{
    auto dialog = std::make_unique<FileBrowserDialog>(NEEDS_TRANS ("Cancel"), NEEDS_TRANS ("Save"),
                                                      juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles | juce::FileBrowserComponent::warnAboutOverwriting,
                                                      lastLocation, getFileFilter());
    dialog->setAcceptFunction ([&, dlg=dialog.get()]
    {
        auto xmlFile = dlg->getFile();
        saveGUI (xmlFile);
        setLastLocation (xmlFile);

        builder.closeOverlayDialog();
    });
    dialog->setCancelFunction ([&]
    {
        builder.closeOverlayDialog();
    });

    builder.showOverlayDialog (std::move (dialog));
}

void ToolBox::loadGUI (const juce::File& xmlFile)
{
    juce::FileInputStream stream (xmlFile);
    auto tree = juce::ValueTree::fromXml (stream.readEntireStreamAsString());

    if (tree.isValid() && tree.getType() == IDs::magic)
    {
        builder.prepareForTreeSwap();
        builder.getMagicState().setGuiValueTree (tree);
        builder.completeTreeSwap();
        stateWasReloaded();
    }

    setLastLocation (xmlFile);
}

bool ToolBox::saveGUI (const juce::File& xmlFile)
{
    juce::TemporaryFile temp (xmlFile);

    if (auto stream = temp.getFile().createOutputStream())
    {
        auto saved = stream->writeString (builder.getConfigTree().toXmlString());
        stream.reset();

        if (saved)
            return temp.overwriteTargetFileWithTemporary();
    }

    return false;
}

void ToolBox::setSelectedNode (const juce::ValueTree& node)
{
    treeEditor.setSelectedNode (node);
    propertiesEditor.setNodeToEdit (node);
    builder.setSelectedNode (node);
}

void ToolBox::setNodeToEdit (juce::ValueTree node)
{
    propertiesEditor.setNodeToEdit (node);
}

void ToolBox::stateWasReloaded()
{
    treeEditor.updateTree();
    propertiesEditor.setStyle (builder.getStylesheet().getCurrentStyle());
    palette.update();
    builder.updateComponents();
}

void ToolBox::paint (juce::Graphics& g)
{
    g.fillAll (EditorColours::background);
    
    // Fill behind toolbar buttons to hide rounded corner gaps
    g.setColour (ToolBoxColours::border);
    g.fillRect (getLocalBounds().reduced (2).withTop (24).removeFromTop (24));
    
    g.setColour (EditorColours::outline);
    g.drawRect (getLocalBounds().toFloat(), 2.0f);
    g.setColour (EditorColours::text);
    g.drawFittedText ("GUI Editor", getLocalBounds().withHeight (24), juce::Justification::centred, 1);
}

void ToolBox::resized()
{
    auto bounds = getLocalBounds().reduced (2).withTop (24);
    auto buttons = bounds.removeFromTop (24);
    auto w = buttons.getWidth() / 5;
    fileMenu.setBounds       (buttons.removeFromLeft (w));
    editMenu.setBounds       (buttons.removeFromLeft (w));
    viewMenu.setBounds       (buttons.removeFromLeft (w));
    snippetsButton.setBounds (buttons.removeFromLeft (w));
    editSwitch.setBounds     (buttons.removeFromLeft (w));

    if (showItemsPanel != lastShowItemsPanel)
    {
        lastShowItemsPanel = showItemsPanel;

        if (showItemsPanel)
        {
            resizeManager.setItemLayout (0, 1, -1.0, savedTreeHeightShowing > 0 ? savedTreeHeightShowing : -0.4);
            resizeManager.setItemLayout (1, 6, 6, 6);
            resizeManager.setItemLayout (2, 1, -1.0, -0.3);
            resizeManager.setItemLayout (3, 6, 6, 6);
            resizeManager.setItemLayout (4, 1, -1.0, -0.3);
        }
        else
        {
            resizeManager.setItemLayout (0, 1, -1.0, savedTreeHeightHidden > 0 ? savedTreeHeightHidden : treeEditor.getHeight());
            resizeManager.setItemLayout (1, 6, 6, 6);
            resizeManager.setItemLayout (2, 1, -1.0, -1.0);
            resizeManager.setItemLayout (3, 0, 0, 0);
            resizeManager.setItemLayout (4, 0, 0, 0);
        }
    }

    juce::Component* comps[] = {
        &treeEditor,
        &resizer1,
        &propertiesEditor,
        &resizer3,
        &palette
    };

    resizeManager.layOutComponents (comps, 5,
                                    bounds.getX(),
                                    bounds.getY(),
                                    bounds.getWidth(),
                                    bounds.getHeight(),
                                    true, true);

    const int resizeCornerSize { 20 };
    const auto bottomRight { getLocalBounds().getBottomRight() };

    juce::Rectangle<int> resizeCornerArea { bottomRight.getX() - resizeCornerSize,
                                            bottomRight.getY() - resizeCornerSize,
                                            resizeCornerSize,
                                            resizeCornerSize };
    resizeCorner.setBounds (resizeCornerArea);
}

bool ToolBox::keyPressed (const juce::KeyPress& key, juce::Component*)
{
    return keyPressed (key);
}

bool ToolBox::keyPressed (const juce::KeyPress& key)
{
    // Delete / Backspace - remove selected node
    if ((key.isKeyCode (juce::KeyPress::backspaceKey) || key.isKeyCode (juce::KeyPress::deleteKey)) && !key.getModifiers().isCommandDown())
    {
        auto selected = builder.getSelectedNode();
        if (selected.isValid())
        {
            auto p = selected.getParent();
            if (p.isValid())
                undo.beginNewTransaction ("Delete");
                p.removeChild (selected, &undo);
        }

        return true;
    }

    // Cmd+Delete - remove all children of selected node
    if ((key.isKeyCode (juce::KeyPress::backspaceKey) || key.isKeyCode (juce::KeyPress::deleteKey)) && key.getModifiers().isCommandDown())
    {
        auto selected = builder.getSelectedNode();
        if (selected.isValid()){
            undo.beginNewTransaction ("Delete All Children");
            selected.removeAllChildren (&undo);
        }

        return true;
    }

    // Cmd+Z / Cmd+Shift+Z - undo / redo
    if (key.isKeyCode ('Z') && key.getModifiers().isCommandDown())
    {
        if (key.getModifiers().isShiftDown())
            performRedo();
        else
            performUndo();

        return true;
    }

    // Cmd+X - cut
    if (key.isKeyCode ('X') && key.getModifiers().isCommandDown())
    {
        performCut();
        return true;
    }

    // Cmd+C - copy
    if (key.isKeyCode ('C') && key.getModifiers().isCommandDown())
    {
        performCopy();
        return true;
    }

    if (key.isKeyCode ('V') && key.getModifiers().isCommandDown())
    {
        if (key.getModifiers().isShiftDown() && key.getModifiers().isAltDown())
            performPasteItemProperties();
        else if (key.getModifiers().isShiftDown())
            performPasteUnique();
        else if (key.getModifiers().isAltDown())
            performPasteReplace();
        else
            performPaste();

        return true;
    }

    // Shift+Control+V - paste dimensions
    if (key.isKeyCode ('V') && key.getModifiers().isShiftDown() && key.getModifiers().isCtrlDown()
        && !key.getModifiers().isCommandDown())
    {
        performPasteDimensions();
        return true;
    }

    // Cmd+D - duplicate (plain), Opt+Cmd+D - duplicate unique
    if (key.isKeyCode ('D') && key.getModifiers().isCommandDown())
    {
        if (key.getModifiers().isAltDown())
            performDuplicate();
        else
            performDuplicateUnique();

        return true;
    }

    // Cmd+S - save, Cmd+Shift+S - save as
    if (key.isKeyCode ('S') && key.getModifiers().isCommandDown() && !key.getModifiers().isShiftDown())
    {
        save();
        return true;
    }

    if (key.isKeyCode ('S') && key.getModifiers().isCommandDown() && key.getModifiers().isShiftDown())
    {
        saveDialog();
        return true;
    }

    // Cmd+O - load
    if (key.isKeyCode ('O') && key.getModifiers().isCommandDown())
    {
        loadDialog();
        return true;
    }

    // Cmd+R - refresh
    if (key.isKeyCode ('R') && key.getModifiers().isCommandDown())
    {
        builder.updateComponents();
        return true;
    }

    // Cmd+E - toggle edit mode
    if (key.isKeyCode ('E') && key.getModifiers().isCommandDown())
    {
        editSwitch.triggerClick();
        return true;
    }

    // Arrow keys - nudge selected item
    if (key.isKeyCode (juce::KeyPress::leftKey))
    {
        auto selected = builder.getSelectedNode();
        if (auto* item = builder.findGuiItem (selected))
        {
            item->nudgeLeft();
            return true;
        }
        return false;
    }

    if (key.isKeyCode (juce::KeyPress::rightKey))
    {
        auto selected = builder.getSelectedNode();
        if (auto* item = builder.findGuiItem (selected))
        {
            item->nudgeRight();
            return true;
        }
        return false;
    }

    if (key.isKeyCode (juce::KeyPress::upKey))
    {
        auto selected = builder.getSelectedNode();
        if (auto* item = builder.findGuiItem (selected))
        {
            item->nudgeUp();
            return true;
        }
        return false;
    }

    if (key.isKeyCode (juce::KeyPress::downKey))
    {
        auto selected = builder.getSelectedNode();
        if (auto* item = builder.findGuiItem (selected))
        {
            item->nudgeDown();
            return true;
        }
        return false;
    }
    
    // Cmd+[ - send back, Cmd+{ (Shift+[) - send to back
    if (key.isKeyCode ('[') && key.getModifiers().isCommandDown())
    {
        performSendBack();
        return true;
    }
    if (key.isKeyCode ('{') && key.getModifiers().isCommandDown())
    {
        performSendToBack();
        return true;
    }

    // Cmd+] - bring forward, Cmd+} (Shift+]) - bring to front
    if (key.isKeyCode (']') && key.getModifiers().isCommandDown())
    {
        performBringForward();
        return true;
    }
    if (key.isKeyCode ('}') && key.getModifiers().isCommandDown())
    {
        performBringToFront();
        return true;
    }

    // Cmd+P - select parent
    if (key.isKeyCode ('P') && key.getModifiers().isCommandDown())
    {
        performSelectParent();
        return true;
    }

    // Cmd+U - deselect
    if (key.isKeyCode ('U') && key.getModifiers().isCommandDown())
    {
        performDeselect();
        return true;
    }

    // Cmd+T - paste styling
    if (key.isKeyCode ('T') && key.getModifiers().isCommandDown())
    {
        performPasteStyling();
        return true;
    }

    // Cmd+W - wrap in view
    if (key.isKeyCode ('W') && key.getModifiers().isCommandDown())
    {
        performWrapInView();
        return true;
    }
    
    // Tab - toggle items panel
    if (key.isKeyCode (juce::KeyPress::tabKey))
    {
        showItemsPanel = !showItemsPanel;
        palette.setVisible (showItemsPanel);
        resizer3.setVisible (showItemsPanel);
        resized();
        return true;
    }
    
    // Alt+Cmd+F - toggle selection to front
    if (key.isKeyCode ('F') && key.getModifiers().isCommandDown() && key.getModifiers().isAltDown())
    {
        GuiItem::selectionToFront = !GuiItem::selectionToFront;
        if (auto* properties = appProperties.getUserSettings())
            properties->setValue ("selectionToFront", GuiItem::selectionToFront ? "true" : "false");

        auto selected = builder.getSelectedNode();
        if (selected.isValid())
        {
            if (GuiItem::selectionToFront)
            {
                if (auto* item = builder.findGuiItem (selected))
                    item->toFrontForEditing();
            }
            else
            {
                builder.restoreZOrderForAll ();
            }
        }

        return true;
    }
    
    // Cmd+N - Insert View (Contents)
    if (key.isKeyCode ('N') && key.getModifiers().isCommandDown() && !key.getModifiers().isShiftDown() && !key.getModifiers().isAltDown())
    {
        performInsertViewContents();
        return true;
    }

    // Shift+Cmd+N - Insert View (Flexbox)
    if (key.isKeyCode ('N') && key.getModifiers().isCommandDown() && key.getModifiers().isShiftDown() && !key.getModifiers().isAltDown())
    {
        performInsertViewFlexbox();
        return true;
    }

    // Opt+Cmd+N - Insert View (Tabbed)
    if (key.isKeyCode ('N') && key.getModifiers().isCommandDown() && key.getModifiers().isAltDown() && !key.getModifiers().isShiftDown())
    {
        performInsertViewTabbed();
        return true;
    }

    return false;
}

void ToolBox::timerCallback (int timer)
{
    if (timer == Timers::WindowDrag)
        updateToolboxPosition();
    else if (timer == Timers::AutoSave)
        saveGUI (autoSaveFile);
    else if (timer == ModifierKeys)
    {
        auto mods = juce::ModifierKeys::getCurrentModifiersRealtime();
        bool shouldTempEdit = mods.isCommandDown() && mods.isShiftDown() && mods.isAltDown();

        if (shouldTempEdit && !builder.isEditModeOn())
        {
            builder.setEditMode (true);
            temporaryEditMode = true;
        }
        else if (!shouldTempEdit && temporaryEditMode)
        {
            builder.setEditMode (false);
            temporaryEditMode = false;
        }
    }
}

void ToolBox::setToolboxPosition (PositionOption position)
{
    positionOption = position;
    const auto isDetached = (positionOption == PositionOption::detached);

    auto* userSettings = appProperties.getUserSettings();
    userSettings->setValue ("position", ToolBox::positionOptionToString (positionOption));

    resizeCorner.setVisible (isDetached);

    if (isDetached)
        stopTimer (Timers::WindowDrag);
    else
        startTimer (Timers::WindowDrag, 100);
}

void ToolBox::updateToolboxPosition()
{
    if (parent == nullptr || positionOption == PositionOption::detached)
        return;

    const auto parentBounds = parent->getScreenBounds();
    const auto width { 280 };
    const auto height = parentBounds.getHeight();

    if (positionOption == PositionOption::left)
        setBounds (parentBounds.getX() - width, parentBounds.getY(), width, height);
    else if (positionOption == PositionOption::right)
        setBounds (parentBounds.getRight(), parentBounds.getY(), width, height);
}

void ToolBox::setLastLocation (juce::File file)
{
    if (file.getFullPathName().isEmpty())
        return;

    if (file.isDirectory())
        file = file.getChildFile ("magic.xml");

    lastLocation = file;

    autoSaveFile.deleteFile();
    auto autoSaveFileDirectory = lastLocation.getParentDirectory()
                                             .getChildFile ("auto-saved");

    if (!autoSaveFileDirectory.exists())
    {
        const auto result = autoSaveFileDirectory.createDirectory();
        if (result.failed())
        {
            DBG ("Could not create auto-saved file directory: " + result.getErrorMessage());
            jassertfalse;
        }
    }

    autoSaveFile = autoSaveFileDirectory.getNonexistentChildFile (file.getFileNameWithoutExtension() + ".sav", ".xml");

#if defined AUTO_SAVE_MINUTES
    startTimer (Timers::AutoSave, 6000 * AUTO_SAVE_MINUTES);
#else
    startTimer (Timers::AutoSave, 6000 * 5);
#endif
}

std::unique_ptr<juce::FileFilter> ToolBox::getFileFilter()
{
    return std::make_unique<juce::WildcardFileFilter> ("*.xml", "*", "XML files");
}

juce::String ToolBox::positionOptionToString (PositionOption option)
{
    switch (option)
    {
        case PositionOption::right:    return "right";
        case PositionOption::detached: return "detached";
        case PositionOption::left:
        default:                       return "left";
    }
}

ToolBox::PositionOption ToolBox::positionOptionFromString (const juce::String& text)
{
    if (text == ToolBox::positionOptionToString (PositionOption::detached))
        return PositionOption::detached;
    if (text == ToolBox::positionOptionToString (PositionOption::right))
        return PositionOption::right;

    return PositionOption::left;
}

juce::PropertiesFile::Options ToolBox::getApplicationPropertyStorage()
{
    juce::PropertiesFile::Options options;
    options.folderName           = "FoleysFinest";
    options.applicationName      = "foleys_gui_magic";
    options.filenameSuffix       = ".settings";
    options.osxLibrarySubFolder  = "Application Support";
    return options;
}

juce::ValueTree ToolBox::makeParameterRefsUnique (juce::ValueTree tree)
{
    // Generate a unique suffix: milliseconds since year 2000
    auto msSince2000 = juce::String (juce::int64 (
        (juce::Time::getCurrentTime() - juce::Time (2000, 0, 1, 0, 0, 0)).inMilliseconds()));

    int counter = 0;
    std::map<juce::String, juce::String> valueMap;

    std::function<void(juce::ValueTree&)> processTree;
    processTree = [&](juce::ValueTree& node)
    {
        for (int i = 0; i < node.getNumProperties(); ++i)
        {
            auto propValue = node.getProperty (node.getPropertyName (i)).toString();

            // Detect PGM parameter references: contain ":" but are not URLs or
            // human-readable strings. Add further exclusions here if needed.
            if (propValue.contains (":")
                && propValue == propValue.removeCharacters (" \t\n\r")
                && !propValue.startsWithIgnoreCase ("http"))
            {
                // If we've already remapped this exact value, reuse the same new value
                // so that properties sharing the same ID stay linked
                auto it = valueMap.find (propValue);
                if (it != valueMap.end())
                {
                    node.setProperty (node.getPropertyName (i), it->second, nullptr);
                    continue;
                }

                // Strip any existing timestamp suffix to avoid ever-growing strings.
                // A suffix is a "-" followed by 8+ digits.
                auto cleanValue = propValue;
                auto lastDash = propValue.lastIndexOf ("-");
                if (lastDash >= 0)
                {
                    auto suffix = propValue.substring (lastDash + 1);
                    if (suffix.containsOnly ("0123456789") && suffix.length() >= 8)
                        cleanValue = propValue.substring (0, lastDash);
                }

                auto newValue = cleanValue + "-" + juce::String (msSince2000.getLargeIntValue() + counter++);
                valueMap[propValue] = newValue;
                node.setProperty (node.getPropertyName (i), newValue, nullptr);
            }
        }

        for (auto child : node)
            processTree (child);
    };

    processTree (tree);
    return tree;
}

void ToolBox::offsetDuplicatePosition (juce::ValueTree& paste, const juce::ValueTree& parentNode)
{
    if (parentNode.getProperty (IDs::display).toString() != IDs::contents)
        return;

    auto* parentItem = builder.findGuiItem (parentNode);
    if (parentItem == nullptr)
        return;

    auto parentBounds = parentItem->getClientBounds();
    const int offset = 4;

    auto offsetAxis = [&](const juce::Identifier& posProp,
                          const juce::Identifier& sizeProp,
                          int parentSize)
    {
        auto posStr  = paste.getProperty (posProp).toString();
        auto sizeStr = paste.getProperty (sizeProp).toString();
        if (posStr.isEmpty() || parentSize <= 0) return;

        bool isPct     = posStr.endsWith ("%");
        double posPx   = isPct ? posStr.getDoubleValue() * 0.01 * parentSize
                               : posStr.getDoubleValue();

        bool sizePct   = sizeStr.endsWith ("%");
        double sizePx  = sizePct ? sizeStr.getDoubleValue() * 0.01 * parentSize
                                 : sizeStr.getDoubleValue();

        double maxPos  = parentSize - sizePx;
        double newPos  = juce::jmin (posPx + offset, juce::jmax (posPx, maxPos));

        if (isPct)
            paste.setProperty (posProp, juce::String (newPos * 100.0 / parentSize) + "%", nullptr);
        else
            paste.setProperty (posProp, juce::String ((int) newPos), nullptr);
    };

    offsetAxis (IDs::posX, IDs::posWidth,  parentBounds.getWidth());
    offsetAxis (IDs::posY, IDs::posHeight, parentBounds.getHeight());
}

} // namespace foleys
