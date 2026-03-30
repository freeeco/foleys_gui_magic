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
        
        file.addItem ("Clear", [&]
        {
            builder.prepareForTreeSwap();
            builder.clearGUI();
            builder.completeTreeSwap();
            stateWasReloaded();
        });
        
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

        bool hasSelection = builder.getSelectedNode().isValid();
        bool hasClipboard = juce::ValueTree::fromXml (juce::SystemClipboard::getTextFromClipboard()).isValid();
        bool multiSelected = hasMultipleSelected();

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
            it.setEnabled (hasSelection);
            edit.addItem (it);
        }
        {
            juce::PopupMenu::Item it ("Copy");
            it.action = [&] { performCopy(); };
            it.shortcutKeyDescription = "Cmd+C";
            it.setEnabled (hasSelection);
            edit.addItem (it);
        }
        {
            juce::PopupMenu::Item it ("Paste");
            it.action = [&] { performPaste(); };
            it.shortcutKeyDescription = "Cmd+V";
            it.setEnabled (hasClipboard && hasSelection);
            edit.addItem (it);
        }
        
        edit.addSeparator();
        
        {
            juce::PopupMenu::Item it ("Paste Unique");
            it.action = [&] { performPasteUnique(); };
            it.shortcutKeyDescription = "Shift+Cmd+V";
            it.setEnabled (hasClipboard && hasSelection);
            edit.addItem (it);
        }
        {
            juce::PopupMenu::Item it ("Paste Dimensions");
            it.action = [&] { performPasteDimensions(); };
            it.shortcutKeyDescription = "Shift+Control+V";
            it.setEnabled (hasClipboard && hasSelection);
            edit.addItem (it);
        }
        {
            juce::PopupMenu::Item it ("Paste Item Properties");
            it.action = [&] { performPasteItemProperties(); };
            it.shortcutKeyDescription = "Shift+Opt+Cmd+V";
            it.setEnabled (hasClipboard && hasSelection);
            edit.addItem (it);
        }
        {
            juce::PopupMenu::Item it ("Paste Styling");
            it.action = [&] { performPasteStyling(); };
            it.shortcutKeyDescription = "Cmd+T";
            it.setEnabled (hasClipboard && hasSelection);
            edit.addItem (it);
        }
        {
            juce::PopupMenu::Item it ("Paste Replace");
            it.action = [&] { performPasteReplace(); };
            it.shortcutKeyDescription = "Opt+Cmd+V";
            it.setEnabled (hasClipboard && hasSelection);
            edit.addItem (it);
        }
        
        edit.addSeparator();
        
        {
            juce::PopupMenu::Item it ("Duplicate");
            it.action = [&] { performDuplicate(); };
            it.shortcutKeyDescription = "Opt+Cmd+D";
            it.setEnabled (hasSelection);
            edit.addItem (it);
        }
        {
            juce::PopupMenu::Item it ("Duplicate Unique");
            it.action = [&] { performDuplicateUnique(); };
            it.shortcutKeyDescription = "Cmd+D";
            it.setEnabled (hasSelection);
            edit.addItem (it);
        }
        
        edit.addSeparator();
        
        {
            juce::PopupMenu::Item it ("Delete");
            it.action = [&]
            {
                auto nodes = getSelectedNodes();
                if (!nodes.isEmpty())
                {
                    undo.beginNewTransaction ("Delete");
                    for (int i = nodes.size(); --i >= 0;)
                    {
                        auto p = nodes[i].getParent();
                        if (p.isValid())
                            p.removeChild (nodes[i], &undo);
                    }
                }
            };
            it.shortcutKeyDescription = "Delete";
            it.setEnabled (hasSelection);
            edit.addItem (it);
        }
        {
            juce::PopupMenu::Item it ("Delete All Children");
            it.action = [&]
            {
                auto nodes = getSelectedNodes();
                if (!nodes.isEmpty())
                {
                    undo.beginNewTransaction ("Delete All Children");
                    for (auto& node : nodes)
                        if (node.isValid())
                            node.removeAllChildren (&undo);
                }
            };
            it.shortcutKeyDescription = "Cmd+Delete";
            it.setEnabled (hasSelection);
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

            edit.addSubMenu ("Insert", insertMenu, hasSelection);
        }
        
        edit.addSeparator();

        {
            juce::PopupMenu::Item it ("Send to Back");
            it.action = [&] { performSendToBack(); };
            it.shortcutKeyDescription = "Shift+Cmd+[";
            it.setEnabled (hasSelection);
            edit.addItem (it);
        }
        {
            juce::PopupMenu::Item it ("Send Back");
            it.action = [&] { performSendBack(); };
            it.shortcutKeyDescription = "Cmd+[";
            it.setEnabled (hasSelection);
            edit.addItem (it);
        }
        {
            juce::PopupMenu::Item it ("Bring Forward");
            it.action = [&] { performBringForward(); };
            it.shortcutKeyDescription = "Cmd+]";
            it.setEnabled (hasSelection);
            edit.addItem (it);
        }
        {
            juce::PopupMenu::Item it ("Bring to Front");
            it.action = [&] { performBringToFront(); };
            it.shortcutKeyDescription = "Shift+Cmd+]";
            it.setEnabled (hasSelection);
            edit.addItem (it);
        }

        edit.addSeparator();

        {
            juce::PopupMenu alignMenu;
            alignMenu.addItem ("Left",         [&] { performAlignLeft(); });
            alignMenu.addItem ("Right",        [&] { performAlignRight(); });
            alignMenu.addItem ("Top",          [&] { performAlignTop(); });
            alignMenu.addItem ("Bottom",       [&] { performAlignBottom(); });
            alignMenu.addSeparator();
            alignMenu.addItem ("Horizontally", [&] { performAlignHorizontalCenters(); });
            alignMenu.addItem ("Vertically",   [&] { performAlignVerticalCenters(); });
            edit.addSubMenu ("Align", alignMenu, multiSelected);
        }
        {
            juce::PopupMenu distributeMenu;
            distributeMenu.addItem ("Horizontally", [&] { performDistributeHorizontally(); });
            distributeMenu.addItem ("Vertically",   [&] { performDistributeVertically(); });
            edit.addSubMenu ("Distribute", distributeMenu, multiSelected);
        }

        edit.addSeparator();

        {
            juce::PopupMenu::Item it ("Select All");
            it.action = [&] { performSelectAll(); };
            it.shortcutKeyDescription = "Cmd+A";
            edit.addItem (it);
        }
        {
            juce::PopupMenu::Item it ("Select Parent");
            it.action = [&] { performSelectParent(); };
            it.shortcutKeyDescription = "Cmd+P";
            it.setEnabled (hasSelection);
            edit.addItem (it);
        }
        {
            juce::PopupMenu::Item it ("Deselect");
            it.action = [&] { performDeselect(); };
            it.shortcutKeyDescription = "Cmd+U";
            it.setEnabled (hasSelection);
            edit.addItem (it);
        }

        edit.addSeparator();

        {
            juce::PopupMenu::Item it ("Clear Dimensions");
            it.action = [&] { performClearDimensions(); };
            it.setEnabled (hasSelection);
            edit.addItem (it);
        }
        {
            juce::PopupMenu::Item it ("Wrap in View");
            it.action = [&] { performWrapInView(); };
            it.shortcutKeyDescription = "Cmd+W";
            it.setEnabled (hasSelection);
            edit.addItem (it);
        }
        {
            juce::PopupMenu::Item it ("Group");
            it.action = [&] { performGroup(); };
            it.shortcutKeyDescription = "Cmd+G";
            it.setEnabled (multiSelected);
            edit.addItem (it);
        }
        {
            juce::PopupMenu::Item it ("Ungroup");
            it.action = [&] { performUngroup(); };
            it.shortcutKeyDescription = "Shift+Cmd+G";
            it.setEnabled (hasSelection);
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
            auto nodes = getSelectedNodes();
            if (nodes.isEmpty())
            {
                juce::AlertWindow::showMessageBoxAsync (juce::AlertWindow::WarningIcon,
                                                       "No Selection",
                                                       "Please select a node to save as a snippet.");
                return;
            }

            if (!snippetsFolder.exists())
                snippetsFolder.createDirectory();

            // Pre-fill filename from first node's id, falling back to node type
            auto& first = nodes.getReference (0);
            juce::String defaultName = "snippet";
            if (first.hasProperty ("id"))
                defaultName = first.getProperty ("id").toString();
            else if (first.getType().isValid())
                defaultName = first.getType().toString();

            auto suggestedFile = snippetsFolder.getChildFile (defaultName).withFileExtension (".xml");

            auto dialog = std::make_unique<FileBrowserDialog>(NEEDS_TRANS ("Cancel"), NEEDS_TRANS ("Save"),
                                                              juce::FileBrowserComponent::saveMode |
                                                              juce::FileBrowserComponent::canSelectFiles |
                                                              juce::FileBrowserComponent::warnAboutOverwriting,
                                                              suggestedFile,
                                                              getFileFilter());
            dialog->setAcceptFunction ([&, dlg=dialog.get(), nodes]
            {
                auto snippetFile = dlg->getFile();
                if (!snippetFile.hasFileExtension (".xml"))
                    snippetFile = snippetFile.withFileExtension (".xml");

                if (auto stream = snippetFile.createOutputStream())
                {
                    if (nodes.size() == 1)
                    {
                        stream->writeString (nodes.getFirst().toXmlString());
                    }
                    else
                    {
                        juce::ValueTree container ("_multiCopy");
                        for (auto& node : nodes)
                            container.appendChild (juce::ValueTree::fromXml (node.toXmlString()), nullptr);
                        stream->writeString (container.toXmlString());
                    }
                }

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

//==============================================================================
// Multi-select helpers
//==============================================================================

void ToolBox::selectNodes (const juce::Array<juce::ValueTree>& nodes)
{
    for (int i = 0; i < nodes.size(); ++i)
        if (auto* item = treeEditor.getItemForNode (nodes[i]))
            item->setSelected (true, i == 0, juce::dontSendNotification);

    if (!nodes.isEmpty())
        builder.setSelectedNode (nodes.getFirst());
}

juce::Array<juce::ValueTree> ToolBox::getSelectedNodes() const
{
    // Delegate to tree editor which queries TreeView::getSelectedItem()
    return treeEditor.getSelectedNodes();
}

void ToolBox::forEachSelected (std::function<void (juce::ValueTree&)> fn)
{
    auto nodes = getSelectedNodes();

    for (auto& node : nodes)
        if (node.isValid())
            fn (node);
}

bool ToolBox::hasMultipleSelected() const
{
    return getSelectedNodes().size() > 1;
}

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
    auto nodes = getSelectedNodes();
    if (nodes.isEmpty()) return;

    // Copy to clipboard (same logic as performCopy)
    if (nodes.size() == 1)
    {
        juce::SystemClipboard::copyTextToClipboard (nodes.getFirst().toXmlString());
    }
    else
    {
        juce::ValueTree container ("_multiCopy");
        for (auto& node : nodes)
            container.appendChild (juce::ValueTree::fromXml (node.toXmlString()), nullptr);
        juce::SystemClipboard::copyTextToClipboard (container.toXmlString());
    }

    undo.beginNewTransaction ("Cut");

    // Remove in reverse child order so indices don't shift
    for (int i = nodes.size(); --i >= 0;)
    {
        auto p = nodes[i].getParent();
        if (p.isValid())
            p.removeChild (nodes[i], &undo);
    }
}

void ToolBox::performCopy()
{
    auto nodes = getSelectedNodes();
    if (nodes.isEmpty()) return;

    if (nodes.size() == 1)
    {
        juce::SystemClipboard::copyTextToClipboard (nodes.getFirst().toXmlString());
    }
    else
    {
        // Wrap in a container so clipboard has a single root element
        juce::ValueTree container ("_multiCopy");
        for (auto& node : nodes)
            container.appendChild (juce::ValueTree::fromXml (node.toXmlString()), nullptr);
        juce::SystemClipboard::copyTextToClipboard (container.toXmlString());
    }
}

void ToolBox::performPaste()
{
    auto paste = juce::ValueTree::fromXml (juce::SystemClipboard::getTextFromClipboard());
    auto selected = builder.getSelectedNode();
    if (!paste.isValid() || !selected.isValid())
        return;

    undo.beginNewTransaction ("Paste");

    juce::Array<juce::ValueTree> pasted;

    // If selected is a View, paste into it. Otherwise paste after it as a sibling.
    bool intoContainer = (selected.getType() == IDs::view);

    if (paste.getType().toString() == "_multiCopy")
    {
        for (int i = 0; i < paste.getNumChildren(); ++i)
        {
            auto child = paste.getChild (i).createCopy();
            if (intoContainer)
            {
                builder.draggedItemOnto (child, selected);
            }
            else
            {
                auto parent = selected.getParent();
                int idx = parent.indexOf (selected) + 1 + i;
                builder.draggedItemOnto (child, parent, idx);
            }
            pasted.add (child);
        }
    }
    else
    {
        if (intoContainer)
        {
            builder.draggedItemOnto (paste, selected);
        }
        else
        {
            auto parent = selected.getParent();
            int idx = parent.indexOf (selected) + 1;
            builder.draggedItemOnto (paste, parent, idx);
        }
        pasted.add (paste);
    }

    // Select only the pasted items
    for (int i = 0; i < pasted.size(); ++i)
        if (auto* item = treeEditor.getItemForNode (pasted[i]))
            item->setSelected (true, i == 0, juce::dontSendNotification);

    if (!pasted.isEmpty())
        builder.setSelectedNode (pasted.getLast());
}

void ToolBox::performPasteUnique()
{
    auto paste = juce::ValueTree::fromXml (juce::SystemClipboard::getTextFromClipboard());
    auto selected = builder.getSelectedNode();
    if (!paste.isValid() || !selected.isValid())
        return;

    undo.beginNewTransaction ("Paste Unique");

    juce::Array<juce::ValueTree> pasted;

    bool intoContainer = (selected.getType() == IDs::view);

    if (paste.getType().toString() == "_multiCopy")
    {
        for (int i = 0; i < paste.getNumChildren(); ++i)
        {
            auto child = makeParameterRefsUnique (paste.getChild (i).createCopy());
            if (intoContainer)
            {
                builder.draggedItemOnto (child, selected);
            }
            else
            {
                auto parent = selected.getParent();
                int idx = parent.indexOf (selected) + 1 + i;
                builder.draggedItemOnto (child, parent, idx);
            }
            pasted.add (child);
        }
    }
    else
    {
        if (intoContainer)
        {
            auto unique = makeParameterRefsUnique (paste);
            builder.draggedItemOnto (unique, selected);
            pasted.add (unique);
        }
        else
        {
            auto unique = makeParameterRefsUnique (paste);
            auto parent = selected.getParent();
            int idx = parent.indexOf (selected) + 1;
            builder.draggedItemOnto (unique, parent, idx);
            pasted.add (unique);
        }
    }

    // Select only the pasted items
    for (int i = 0; i < pasted.size(); ++i)
        if (auto* item = treeEditor.getItemForNode (pasted[i]))
            item->setSelected (true, i == 0, juce::dontSendNotification);

    if (!pasted.isEmpty())
        builder.setSelectedNode (pasted.getLast());
}

void ToolBox::performPasteDimensions()
{
    auto paste = juce::ValueTree::fromXml (juce::SystemClipboard::getTextFromClipboard());
    auto nodes = getSelectedNodes();

    if (!paste.isValid() || nodes.isEmpty())
        return;

    static const juce::Identifier dimensionProps[] = {
        juce::Identifier ("pos-x"),
        juce::Identifier ("pos-y"),
        juce::Identifier ("pos-width"),
        juce::Identifier ("pos-height"),
        juce::Identifier ("dont-snap-to-pixels")
    };

    const juce::ScopedValueSetter<bool> svs (isMirroring, true);
    undo.beginNewTransaction("Paste Dimensions");

    for (auto& selected : nodes)
        for (const auto& prop : dimensionProps)
            if (paste.hasProperty (prop))
                selected.setProperty (prop, paste.getProperty (prop), &undo);
}

void ToolBox::performPasteItemProperties()
{
    auto paste = juce::ValueTree::fromXml (juce::SystemClipboard::getTextFromClipboard());
    auto nodes = getSelectedNodes();

    if (!paste.isValid() || nodes.isEmpty())
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

    const juce::ScopedValueSetter<bool> svs (isMirroring, true);
    undo.beginNewTransaction("Paste Item Properties");

    for (auto& selected : nodes)
        for (const auto& prop : itemProps)
            if (paste.hasProperty (prop))
                selected.setProperty (prop, paste.getProperty (prop), &undo);
}

void ToolBox::performPasteStyling()
{
    auto paste = juce::ValueTree::fromXml (juce::SystemClipboard::getTextFromClipboard());
    auto nodes = getSelectedNodes();

    if (!paste.isValid() || nodes.isEmpty())
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

    const juce::ScopedValueSetter<bool> svs (isMirroring, true);
    undo.beginNewTransaction("Paste Styling");

    for (auto& selected : nodes)
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
    auto nodes = getSelectedNodes();
    if (nodes.isEmpty()) return;

    auto parent = nodes.getFirst().getParent();
    if (!parent.isValid()) return;

    const juce::ScopedValueSetter<bool> svs (isMirroring, true);
    undo.beginNewTransaction ("Duplicate");

    // Sort by index so duplicates stay in order
    std::sort (nodes.begin(), nodes.end(),
               [&](const juce::ValueTree& a, const juce::ValueTree& b)
               {
                   return parent.indexOf (a) < parent.indexOf (b);
               });

    // Insert all duplicates after the last selected node
    int insertAfter = parent.indexOf (nodes.getLast());
    juce::Array<juce::ValueTree> duplicated;

    for (int i = 0; i < nodes.size(); ++i)
    {
        auto paste = juce::ValueTree::fromXml (nodes[i].toXmlString());
        if (paste.isValid())
        {
            offsetDuplicatePosition (paste, parent);
            parent.addChild (paste, insertAfter + 1 + i, &undo);
            duplicated.add (paste);
        }
    }

    if (!duplicated.isEmpty())
        selectNodes (duplicated);
}

void ToolBox::performDuplicateUnique()
{
    auto nodes = getSelectedNodes();
    if (nodes.isEmpty()) return;

    auto parent = nodes.getFirst().getParent();
    if (!parent.isValid()) return;

    const juce::ScopedValueSetter<bool> svs (isMirroring, true);
    undo.beginNewTransaction ("Duplicate Unique");

    std::sort (nodes.begin(), nodes.end(),
               [&](const juce::ValueTree& a, const juce::ValueTree& b)
               {
                   return parent.indexOf (a) < parent.indexOf (b);
               });

    int insertAfter = parent.indexOf (nodes.getLast());
    juce::Array<juce::ValueTree> duplicated;

    for (int i = 0; i < nodes.size(); ++i)
    {
        auto paste = makeParameterRefsUnique (juce::ValueTree::fromXml (nodes[i].toXmlString()));
        if (paste.isValid())
        {
            offsetDuplicatePosition (paste, parent);
            parent.addChild (paste, insertAfter + 1 + i, &undo);
            duplicated.add (paste);
        }
    }

    if (!duplicated.isEmpty())
        selectNodes (duplicated);
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

//==============================================================================
// Align operations (multi-select, siblings only)
//==============================================================================

static juce::var toPositionValue (float pixelValue, int parentSize, const juce::var& originalValue)
{
    if (originalValue.toString().containsChar ('%'))
    {
        auto pct = parentSize > 0 ? (pixelValue * 100.0f / parentSize) : 0.0f;
        return juce::String (pct, 1) + "%";
    }

    return juce::roundToInt (pixelValue);
}

void ToolBox::performAlignLeft()
{
    auto nodes = getSelectedNodes();
    if (nodes.size() < 2) return;

    const juce::ScopedValueSetter<bool> svs (isMirroring, true);

    int minX = std::numeric_limits<int>::max();
    for (auto& node : nodes)
        if (auto* item = builder.findGuiItem (node))
            minX = juce::jmin (minX, item->getBounds().getX());

    undo.beginNewTransaction ("Align Left");
    for (auto& node : nodes)
    {
        if (auto* item = builder.findGuiItem (node))
        {
            auto parentW = item->getParentComponent() ? item->getParentComponent()->getWidth() : 1;
            node.setProperty ("pos-x", toPositionValue (minX, parentW, node.getProperty ("pos-x")), &undo);
        }
    }
}

void ToolBox::performAlignRight()
{
    auto nodes = getSelectedNodes();
    if (nodes.size() < 2) return;

    const juce::ScopedValueSetter<bool> svs (isMirroring, true);

    int maxRight = std::numeric_limits<int>::min();
    for (auto& node : nodes)
        if (auto* item = builder.findGuiItem (node))
            maxRight = juce::jmax (maxRight, item->getBounds().getRight());

    undo.beginNewTransaction ("Align Right");
    for (auto& node : nodes)
    {
        if (auto* item = builder.findGuiItem (node))
        {
            auto parentW = item->getParentComponent() ? item->getParentComponent()->getWidth() : 1;
            node.setProperty ("pos-x", toPositionValue (maxRight - item->getBounds().getWidth(), parentW, node.getProperty ("pos-x")), &undo);
        }
    }
}

void ToolBox::performAlignTop()
{
    auto nodes = getSelectedNodes();
    if (nodes.size() < 2) return;

    const juce::ScopedValueSetter<bool> svs (isMirroring, true);

    int minY = std::numeric_limits<int>::max();
    for (auto& node : nodes)
        if (auto* item = builder.findGuiItem (node))
            minY = juce::jmin (minY, item->getBounds().getY());

    undo.beginNewTransaction ("Align Top");
    for (auto& node : nodes)
    {
        if (auto* item = builder.findGuiItem (node))
        {
            auto parentH = item->getParentComponent() ? item->getParentComponent()->getHeight() : 1;
            node.setProperty ("pos-y", toPositionValue (minY, parentH, node.getProperty ("pos-y")), &undo);
        }
    }
}

void ToolBox::performAlignBottom()
{
    auto nodes = getSelectedNodes();
    if (nodes.size() < 2) return;

    const juce::ScopedValueSetter<bool> svs (isMirroring, true);

    int maxBottom = std::numeric_limits<int>::min();
    for (auto& node : nodes)
        if (auto* item = builder.findGuiItem (node))
            maxBottom = juce::jmax (maxBottom, item->getBounds().getBottom());

    undo.beginNewTransaction ("Align Bottom");
    for (auto& node : nodes)
    {
        if (auto* item = builder.findGuiItem (node))
        {
            auto parentH = item->getParentComponent() ? item->getParentComponent()->getHeight() : 1;
            node.setProperty ("pos-y", toPositionValue (maxBottom - item->getBounds().getHeight(), parentH, node.getProperty ("pos-y")), &undo);
        }
    }
}

void ToolBox::performAlignHorizontalCenters()
{
    auto nodes = getSelectedNodes();
    if (nodes.size() < 2) return;

    const juce::ScopedValueSetter<bool> svs (isMirroring, true);

    bool allPct = true;
    for (auto& node : nodes)
        if (!node.getProperty ("pos-x").toString().containsChar ('%')
         || !node.getProperty ("pos-width").toString().containsChar ('%'))
            { allPct = false; break; }

    undo.beginNewTransaction ("Align Horizontal Centers");

    if (allPct)
    {
        float totalCentre = 0.0f;
        for (auto& node : nodes)
            totalCentre += node.getProperty ("pos-x").toString().getFloatValue()
                         + node.getProperty ("pos-width").toString().getFloatValue() / 2.0f;

        float target = totalCentre / nodes.size();

        for (auto& node : nodes)
        {
            float w = node.getProperty ("pos-width").toString().getFloatValue();
            node.setProperty ("pos-x", juce::String (target - w / 2.0f, 1) + "%", &undo);
        }
    }
    else
    {
        float totalCentre = 0.0f;
        int count = 0;
        for (auto& node : nodes)
            if (auto* item = builder.findGuiItem (node))
                { totalCentre += item->getBounds().getCentreX(); ++count; }

        if (count == 0) return;
        float target = totalCentre / count;

        for (auto& node : nodes)
            if (auto* item = builder.findGuiItem (node))
            {
                auto parentW = item->getParentComponent() ? item->getParentComponent()->getWidth() : 1;
                node.setProperty ("pos-x", toPositionValue (target - item->getBounds().getWidth() / 2.0f, parentW, node.getProperty ("pos-x")), &undo);
            }
    }
}

void ToolBox::performAlignVerticalCenters()
{
    auto nodes = getSelectedNodes();
    if (nodes.size() < 2) return;

    const juce::ScopedValueSetter<bool> svs (isMirroring, true);

    bool allPct = true;
    for (auto& node : nodes)
        if (!node.getProperty ("pos-y").toString().containsChar ('%')
         || !node.getProperty ("pos-height").toString().containsChar ('%'))
            { allPct = false; break; }

    undo.beginNewTransaction ("Align Vertical Centers");

    if (allPct)
    {
        float totalCentre = 0.0f;
        for (auto& node : nodes)
            totalCentre += node.getProperty ("pos-y").toString().getFloatValue()
                         + node.getProperty ("pos-height").toString().getFloatValue() / 2.0f;

        float target = totalCentre / nodes.size();

        for (auto& node : nodes)
        {
            float h = node.getProperty ("pos-height").toString().getFloatValue();
            node.setProperty ("pos-y", juce::String (target - h / 2.0f, 1) + "%", &undo);
        }
    }
    else
    {
        float totalCentre = 0.0f;
        int count = 0;
        for (auto& node : nodes)
            if (auto* item = builder.findGuiItem (node))
                { totalCentre += item->getBounds().getCentreY(); ++count; }

        if (count == 0) return;
        float target = totalCentre / count;

        for (auto& node : nodes)
            if (auto* item = builder.findGuiItem (node))
            {
                auto parentH = item->getParentComponent() ? item->getParentComponent()->getHeight() : 1;
                node.setProperty ("pos-y", toPositionValue (target - item->getBounds().getHeight() / 2.0f, parentH, node.getProperty ("pos-y")), &undo);
            }
    }
}

//==============================================================================
// Distribute operations (multi-select, siblings only)
//==============================================================================

void ToolBox::performDistributeHorizontally()
{
    auto nodes = getSelectedNodes();
    if (nodes.size() < 2) return;

    const juce::ScopedValueSetter<bool> svs (isMirroring, true);
    undo.beginNewTransaction ("Distribute Horizontally");

    float pct = 100.0f / nodes.size();

    // Sort by current position so distribution order matches visual order
    std::sort (nodes.begin(), nodes.end(),
               [&](const juce::ValueTree& a, const juce::ValueTree& b)
               {
                   auto* ia = builder.findGuiItem (a);
                   auto* ib = builder.findGuiItem (b);
                   if (ia && ib) return ia->getBounds().getX() < ib->getBounds().getX();
                   return false;
               });

    for (int i = 0; i < nodes.size(); ++i)
    {
        nodes[i].setProperty ("pos-x",     juce::String (pct * i, 1) + "%", &undo);
        nodes[i].setProperty ("pos-width", juce::String (pct, 1) + "%", &undo);
    }
}

void ToolBox::performDistributeVertically()
{
    auto nodes = getSelectedNodes();
    if (nodes.size() < 2) return;

    const juce::ScopedValueSetter<bool> svs (isMirroring, true);
    undo.beginNewTransaction ("Distribute Vertically");

    float pct = 100.0f / nodes.size();

    // Sort by current position so distribution order matches visual order
    std::sort (nodes.begin(), nodes.end(),
               [&](const juce::ValueTree& a, const juce::ValueTree& b)
               {
                   auto* ia = builder.findGuiItem (a);
                   auto* ib = builder.findGuiItem (b);
                   if (ia && ib) return ia->getBounds().getY() < ib->getBounds().getY();
                   return false;
               });

    for (int i = 0; i < nodes.size(); ++i)
    {
        nodes[i].setProperty ("pos-y",      juce::String (pct * i, 1) + "%", &undo);
        nodes[i].setProperty ("pos-height", juce::String (pct, 1) + "%", &undo);
    }
}

//==============================================================================

void ToolBox::performSelectAll()
{
    auto selected = builder.getSelectedNode();
    juce::ValueTree parent;

    if (selected.isValid())
        parent = selected.getParent();

    if (!parent.isValid())
        parent = builder.getGuiRootNode();

    if (!parent.isValid())
        return;

    // Select all children of the parent
    for (int i = 0; i < parent.getNumChildren(); ++i)
    {
        auto child = parent.getChild (i);
        if (auto* item = treeEditor.getItemForNode (child))
            item->setSelected (true, i == 0, juce::dontSendNotification);
    }

    // Set primary to first child
    if (parent.getNumChildren() > 0)
        builder.setSelectedNode (parent.getChild (0));
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
    treeEditor.getTreeView().clearSelectedItems();
    builder.setSelectedNode ({});
}

void ToolBox::performClearDimensions()
{
    auto nodes = getSelectedNodes();
    if (nodes.isEmpty())
        return;

    const juce::ScopedValueSetter<bool> svs (isMirroring, true);
    undo.beginNewTransaction("Clear Dimensions");

    // For multi-select, clear dimensions on each node directly
    static const juce::Identifier dimProps[] = {
        juce::Identifier ("pos-x"),
        juce::Identifier ("pos-y"),
        juce::Identifier ("pos-width"),
        juce::Identifier ("pos-height"),
        juce::Identifier ("dont-snap-to-pixels")
    };

    for (auto& node : nodes)
        for (const auto& prop : dimProps)
            node.removeProperty (prop, &undo);
}

void ToolBox::performWrapInView()
{
    auto nodes = getSelectedNodes();
    if (nodes.isEmpty())
        return;

    // Verify all selected nodes share the same parent
    auto parent = nodes.getFirst().getParent();
    if (!parent.isValid())
        return;

    for (auto& node : nodes)
    {
        if (node.getParent() != parent)
        {
            juce::AlertWindow::showMessageBoxAsync (juce::AlertWindow::WarningIcon,
                                                    "Wrap In View",
                                                    "Selected items must be at the same level.");
            return;
        }
    }

    undo.beginNewTransaction ("Wrap In View");

    // Sort by current index so they stay in order inside the wrapper
    std::sort (nodes.begin(), nodes.end(),
       [&](const juce::ValueTree& a, const juce::ValueTree& b)
       {
           return parent.indexOf (a) < parent.indexOf (b);
       });

    int insertIndex = parent.indexOf (nodes.getFirst());

    juce::ValueTree wrapper ("View");

    // Transfer dimension properties only for single selection
    if (nodes.size() == 1)
    {
        auto& selected = nodes.getReference (0);

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

        for (const auto& prop : dimensionProps)
            selected.removeProperty (prop, &undo);
    }

    // Insert wrapper where the first selected node was
    parent.addChild (wrapper, insertIndex, &undo);

    // Remove nodes from parent in reverse order (preserves indices)
    for (int i = nodes.size(); --i >= 0;)
        parent.removeChild (nodes[i], &undo);

    // Append them to wrapper in forward order
    for (auto& node : nodes)
        wrapper.appendChild (node, &undo);

    setSelectedNode (wrapper);
}

void ToolBox::performGroup()
{
    auto nodes = getSelectedNodes();
    if (nodes.size() < 2)
        return;

    // Verify all selected nodes share the same parent
    auto parent = nodes.getFirst().getParent();
    if (!parent.isValid())
        return;

    for (auto& node : nodes)
    {
        if (node.getParent() != parent)
        {
            juce::AlertWindow::showMessageBoxAsync (juce::AlertWindow::WarningIcon,
                                                    "Group",
                                                    "Selected items must be siblings.");
            return;
        }
    }

    const juce::ScopedValueSetter<bool> svs (isMirroring, true);
    undo.beginNewTransaction ("Group");

    // Sort by current index
    std::sort (nodes.begin(), nodes.end(),
               [&](const juce::ValueTree& a, const juce::ValueTree& b)
               {
                   return parent.indexOf (a) < parent.indexOf (b);
               });

    // Detect whether items use percentage positioning
    bool usePct = nodes.getFirst().getProperty ("pos-x").toString().containsChar ('%');

    // Capture all pixel bounds BEFORE modifying the tree
    juce::Rectangle<int> bbox;
    int parentW = 1, parentH = 1;
    bool first = true;

    for (auto& node : nodes)
    {
        if (auto* item = builder.findGuiItem (node))
        {
            auto bounds = item->getBounds();

            // Store bounds as temp properties to survive tree changes
            node.setProperty ("_groupBoundsX", bounds.getX(), nullptr);
            node.setProperty ("_groupBoundsY", bounds.getY(), nullptr);
            node.setProperty ("_groupBoundsW", bounds.getWidth(), nullptr);
            node.setProperty ("_groupBoundsH", bounds.getHeight(), nullptr);
            node.setProperty ("_groupHasBounds", true, nullptr);

            if (first)
            {
                bbox = bounds;
                if (auto* pc = item->getParentComponent())
                {
                    parentW = pc->getWidth();
                    parentH = pc->getHeight();
                }
                first = false;
            }
            else
            {
                bbox = bbox.getUnion (bounds);
            }
        }
    }

    if (bbox.isEmpty())
        return;

    // Guard against zero-width or zero-height bounding box
    if (bbox.getWidth() == 0)  bbox.setWidth (1);
    if (bbox.getHeight() == 0) bbox.setHeight (1);

    int insertIndex = parent.indexOf (nodes.getFirst());

    // Create wrapper with contents layout
    juce::ValueTree wrapper ("View");
    wrapper.setProperty ("display", "contents", nullptr);

    if (usePct)
    {
        wrapper.setProperty ("pos-x",      juce::String (bbox.getX()      * 100.0f / parentW, 1) + "%", nullptr);
        wrapper.setProperty ("pos-y",      juce::String (bbox.getY()      * 100.0f / parentH, 1) + "%", nullptr);
        wrapper.setProperty ("pos-width",  juce::String (bbox.getWidth()  * 100.0f / parentW, 1) + "%", nullptr);
        wrapper.setProperty ("pos-height", juce::String (bbox.getHeight() * 100.0f / parentH, 1) + "%", nullptr);
    }
    else
    {
        wrapper.setProperty ("pos-x",      bbox.getX(),      nullptr);
        wrapper.setProperty ("pos-y",      bbox.getY(),      nullptr);
        wrapper.setProperty ("pos-width",  bbox.getWidth(),  nullptr);
        wrapper.setProperty ("pos-height", bbox.getHeight(), nullptr);
    }

    // Insert wrapper
    parent.addChild (wrapper, insertIndex, &undo);

    // Remove nodes from parent in reverse order
    for (int i = nodes.size(); --i >= 0;)
        parent.removeChild (nodes[i], &undo);

    // Add nodes to wrapper with positions relative to the bounding box
    float bboxW = (float) bbox.getWidth();
    float bboxH = (float) bbox.getHeight();

    for (auto& node : nodes)
    {
        bool hasBounds = (bool) node.getProperty ("_groupHasBounds", false);

        if (hasBounds)
        {
            int bx = (int) node.getProperty ("_groupBoundsX");
            int by = (int) node.getProperty ("_groupBoundsY");
            int bw = (int) node.getProperty ("_groupBoundsW");
            int bh = (int) node.getProperty ("_groupBoundsH");

            if (usePct)
            {
                node.setProperty ("pos-x",      juce::String ((bx - bbox.getX()) * 100.0f / bboxW, 1) + "%", &undo);
                node.setProperty ("pos-y",      juce::String ((by - bbox.getY()) * 100.0f / bboxH, 1) + "%", &undo);
                node.setProperty ("pos-width",  juce::String (bw * 100.0f / bboxW, 1) + "%", &undo);
                node.setProperty ("pos-height", juce::String (bh * 100.0f / bboxH, 1) + "%", &undo);
            }
            else
            {
                node.setProperty ("pos-x",      bx - bbox.getX(), &undo);
                node.setProperty ("pos-y",      by - bbox.getY(), &undo);
                node.setProperty ("pos-width",  bw,               &undo);
                node.setProperty ("pos-height", bh,               &undo);
            }
        }
        // else: headless item (no visible component) — keep its existing properties

        // Clean up temp properties
        node.removeProperty ("_groupBoundsX", nullptr);
        node.removeProperty ("_groupBoundsY", nullptr);
        node.removeProperty ("_groupBoundsW", nullptr);
        node.removeProperty ("_groupBoundsH", nullptr);
        node.removeProperty ("_groupHasBounds", nullptr);

        wrapper.appendChild (node, &undo);
    }

    setSelectedNode (wrapper);
}

void ToolBox::performUngroup()
{
    auto selected = builder.getSelectedNode();
    if (!selected.isValid() || selected.getNumChildren() == 0)
        return;

    auto grandparent = selected.getParent();
    if (!grandparent.isValid())
        return;

    const juce::ScopedValueSetter<bool> svs (isMirroring, true);
    undo.beginNewTransaction ("Ungroup");

    // Get container's pixel bounds and grandparent dimensions
    auto* containerItem = builder.findGuiItem (selected);
    if (!containerItem)
        return;

    auto containerBounds = containerItem->getBounds();

    // Detect whether the container uses percentage positioning
    bool usePct = selected.getProperty ("pos-x").toString().containsChar ('%');

    int grandparentW = 1, grandparentH = 1;
    if (auto* gpc = containerItem->getParentComponent())
    {
        grandparentW = gpc->getWidth();
        grandparentH = gpc->getHeight();
    }

    int insertIndex = grandparent.indexOf (selected);

    // Collect children and their absolute pixel positions
    struct ChildInfo
    {
        juce::ValueTree node;
        juce::Rectangle<int> absoluteBounds;
    };
    juce::Array<ChildInfo> children;

    for (int i = 0; i < selected.getNumChildren(); ++i)
    {
        auto child = selected.getChild (i);
        if (auto* childItem = builder.findGuiItem (child))
        {
            auto childBounds = childItem->getBounds();
            auto absoluteBounds = childBounds.translated (containerBounds.getX(), containerBounds.getY());
            children.add ({ child, absoluteBounds });
        }
    }

    // Remove children from container in reverse
    for (int i = children.size(); --i >= 0;)
        selected.removeChild (children[i].node, &undo);

    // Remove empty container
    grandparent.removeChild (selected, &undo);

    // Add children to grandparent with absolute positions
    for (int i = 0; i < children.size(); ++i)
    {
        auto& info = children.getReference (i);
        auto& b = info.absoluteBounds;

        if (usePct)
        {
            info.node.setProperty ("pos-x",      juce::String (b.getX()     * 100.0f / grandparentW, 1) + "%", &undo);
            info.node.setProperty ("pos-y",      juce::String (b.getY()     * 100.0f / grandparentH, 1) + "%", &undo);
            info.node.setProperty ("pos-width",  juce::String (b.getWidth() * 100.0f / grandparentW, 1) + "%", &undo);
            info.node.setProperty ("pos-height", juce::String (b.getHeight()* 100.0f / grandparentH, 1) + "%", &undo);
        }
        else
        {
            info.node.setProperty ("pos-x",      b.getX(),      &undo);
            info.node.setProperty ("pos-y",      b.getY(),      &undo);
            info.node.setProperty ("pos-width",  b.getWidth(),  &undo);
            info.node.setProperty ("pos-height", b.getHeight(), &undo);
        }

        grandparent.addChild (info.node, insertIndex + i, &undo);
    }

    // Select all ungrouped children
    if (!children.isEmpty())
    {
        juce::Array<juce::ValueTree> ungrouped;
        for (auto& info : children)
            ungrouped.add (info.node);
        selectNodes (ungrouped);
    }
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

    auto selected = builder.getSelectedNode();
    auto root = builder.getGuiRootNode();

    juce::Array<juce::ValueTree> inserted;

    if (snippet.getType().toString() == "_multiCopy")
    {
        auto target = (selected.isValid() && selected != root && selected.getParent().isValid())
                        ? selected.getParent() : root;
        int index = (target != root && target == selected.getParent())
                        ? target.indexOf (selected) + 1 : -1;

        for (int i = 0; i < snippet.getNumChildren(); ++i)
        {
            auto child = snippet.getChild (i).createCopy();
            if (!optionHeld)
                child = makeParameterRefsUnique (child);
            target.addChild (child, index >= 0 ? index + i : -1, &undo);
            inserted.add (child);
        }
    }
    else
    {
        if (!optionHeld)
            snippet = makeParameterRefsUnique (snippet);

        if (!selected.isValid() || selected == root)
        {
            builder.draggedItemOnto (snippet, root);
        }
        else
        {
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
        inserted.add (snippet);
    }

    if (!inserted.isEmpty())
        selectNodes (inserted);
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
    if (mirroredNode.isValid())
        mirroredNode.removeListener (this);

    mirroredNode = node;

    if (mirroredNode.isValid())
        mirroredNode.addListener (this);
    
    treeEditor.setSelectedNode (node);
    propertiesEditor.setNodeToEdit (node);
    builder.setSelectedNode (node);
}

void ToolBox::setNodeToEdit (juce::ValueTree node)
{
    propertiesEditor.setNodeToEdit (node);
}

void ToolBox::valueTreePropertyChanged (juce::ValueTree& tree,
                                        const juce::Identifier& property)
{
    if (isMirroring || tree != mirroredNode)
        return;

    auto nodes = getSelectedNodes();
    if (nodes.size() <= 1)
        return;

    auto value = tree.getProperty (property);

    juce::MessageManager::callAsync ([safeThis = juce::Component::SafePointer<ToolBox> (this),
                                      nodes, tree, property, value]() mutable
    {
        if (safeThis == nullptr)
            return;

        safeThis->isMirroring = true;

        for (auto& node : nodes)
        {
            if (node != tree && node.isValid())
            {
                if (tree.hasProperty (property))
                    node.setProperty (property, value, &safeThis->undo);
                else
                    node.removeProperty (property, &safeThis->undo);
            }
        }

        safeThis->isMirroring = false;
    });
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
    // Delete / Backspace - remove selected node(s)
    if ((key.isKeyCode (juce::KeyPress::backspaceKey) || key.isKeyCode (juce::KeyPress::deleteKey)) && !key.getModifiers().isCommandDown())
    {
        flashMenuButton (editMenu);
        auto nodes = getSelectedNodes();
        if (!nodes.isEmpty())
        {
            undo.beginNewTransaction ("Delete");
            for (int i = nodes.size(); --i >= 0;)
            {
                auto p = nodes[i].getParent();
                if (p.isValid())
                    p.removeChild (nodes[i], &undo);
            }
        }

        return true;
    }

    // Cmd+Delete - remove all children of selected node(s)
    if ((key.isKeyCode (juce::KeyPress::backspaceKey) || key.isKeyCode (juce::KeyPress::deleteKey)) && key.getModifiers().isCommandDown())
    {
        flashMenuButton (editMenu);
        auto nodes = getSelectedNodes();
        if (!nodes.isEmpty())
        {
            undo.beginNewTransaction ("Delete All Children");
            for (auto& node : nodes)
                if (node.isValid())
                    node.removeAllChildren (&undo);
        }

        return true;
    }

    // Cmd+Z / Cmd+Shift+Z - undo / redo
    if (key.isKeyCode ('Z') && key.getModifiers().isCommandDown())
    {
        flashMenuButton (editMenu);
        if (key.getModifiers().isShiftDown())
            performRedo();
        else
            performUndo();

        return true;
    }

    // Cmd+X - cut
    if (key.isKeyCode ('X') && key.getModifiers().isCommandDown())
    {
        flashMenuButton (editMenu);
        performCut();
        return true;
    }

    // Cmd+C - copy
    if (key.isKeyCode ('C') && key.getModifiers().isCommandDown())
    {
        flashMenuButton (editMenu);
        performCopy();
        return true;
    }

    if (key.isKeyCode ('V') && key.getModifiers().isCommandDown())
    {
        flashMenuButton (editMenu);
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
        flashMenuButton (editMenu);
        performPasteDimensions();
        return true;
    }

    // Cmd+D - duplicate (plain), Opt+Cmd+D - duplicate unique
    if (key.isKeyCode ('D') && key.getModifiers().isCommandDown())
    {
        flashMenuButton (editMenu);
        if (key.getModifiers().isAltDown())
            performDuplicate();
        else
            performDuplicateUnique();

        return true;
    }

    // Cmd+S - save, Cmd+Shift+S - save as
    if (key.isKeyCode ('S') && key.getModifiers().isCommandDown() && !key.getModifiers().isShiftDown())
    {
        flashMenuButton (fileMenu);
        save();
        return true;
    }

    if (key.isKeyCode ('S') && key.getModifiers().isCommandDown() && key.getModifiers().isShiftDown())
    {
        flashMenuButton (fileMenu);
        saveDialog();
        return true;
    }

    // Cmd+O - load
    if (key.isKeyCode ('O') && key.getModifiers().isCommandDown())
    {
        flashMenuButton (fileMenu);
        loadDialog();
        return true;
    }

    // Cmd+R - refresh
    if (key.isKeyCode ('R') && key.getModifiers().isCommandDown())
    {
        flashMenuButton (fileMenu);
        builder.updateComponents();
        return true;
    }

    // Cmd+E - toggle edit mode
    if (key.isKeyCode ('E') && key.getModifiers().isCommandDown())
    {
        flashMenuButton (editMenu);
        editSwitch.triggerClick();
        return true;
    }

    // Arrow keys - nudge selected item(s)
    if (key.isKeyCode (juce::KeyPress::leftKey))
    {
        const juce::ScopedValueSetter<bool> svs (isMirroring, true);
        bool handled = false;
        forEachSelected ([&](auto& node) {
            if (auto* item = builder.findGuiItem (node))
            {
                item->nudgeLeft();
                handled = true;
            }
        });
        return handled;
    }

    if (key.isKeyCode (juce::KeyPress::rightKey))
    {
        const juce::ScopedValueSetter<bool> svs (isMirroring, true);
        bool handled = false;
        forEachSelected ([&](auto& node) {
            if (auto* item = builder.findGuiItem (node))
            {
                item->nudgeRight();
                handled = true;
            }
        });
        return handled;
    }

    if (key.isKeyCode (juce::KeyPress::upKey))
    {
        const juce::ScopedValueSetter<bool> svs (isMirroring, true);
        bool handled = false;
        forEachSelected ([&](auto& node) {
            if (auto* item = builder.findGuiItem (node))
            {
                item->nudgeUp();
                handled = true;
            }
        });
        return handled;
    }

    if (key.isKeyCode (juce::KeyPress::downKey))
    {
        const juce::ScopedValueSetter<bool> svs (isMirroring, true);
        bool handled = false;
        forEachSelected ([&](auto& node) {
            if (auto* item = builder.findGuiItem (node))
            {
                item->nudgeDown();
                handled = true;
            }
        });
        return handled;
    }
    
    // Cmd+[ - send back, Cmd+{ (Shift+[) - send to back
    if (key.isKeyCode ('[') && key.getModifiers().isCommandDown())
    {
        flashMenuButton (editMenu);
        performSendBack();
        return true;
    }
    if (key.isKeyCode ('{') && key.getModifiers().isCommandDown())
    {
        flashMenuButton (editMenu);
        performSendToBack();
        return true;
    }

    // Cmd+] - bring forward, Cmd+} (Shift+]) - bring to front
    if (key.isKeyCode (']') && key.getModifiers().isCommandDown())
    {
        flashMenuButton (editMenu);
        performBringForward();
        return true;
    }
    if (key.isKeyCode ('}') && key.getModifiers().isCommandDown())
    {
        flashMenuButton (editMenu);
        performBringToFront();
        return true;
    }

    // Cmd+A - select all siblings
    if (key.isKeyCode ('A') && key.getModifiers().isCommandDown())
    {
        flashMenuButton (editMenu);
        performSelectAll();
        return true;
    }

    // Cmd+P - select parent
    if (key.isKeyCode ('P') && key.getModifiers().isCommandDown())
    {
        flashMenuButton (editMenu);
        performSelectParent();
        return true;
    }

    // Cmd+U - deselect
    if (key.isKeyCode ('U') && key.getModifiers().isCommandDown())
    {
        flashMenuButton (editMenu);
        performDeselect();
        return true;
    }

    // Cmd+T - paste styling
    if (key.isKeyCode ('T') && key.getModifiers().isCommandDown())
    {
        flashMenuButton (editMenu);
        performPasteStyling();
        return true;
    }

    // Cmd+W - wrap in view
    if (key.isKeyCode ('W') && key.getModifiers().isCommandDown())
    {
        flashMenuButton (editMenu);
        performWrapInView();
        return true;
    }

    // Cmd+G - group, Shift+Cmd+G - ungroup
    if (key.isKeyCode ('G') && key.getModifiers().isCommandDown())
    {
        flashMenuButton (editMenu);
        if (key.getModifiers().isShiftDown())
            performUngroup();
        else
            performGroup();
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
        flashMenuButton (editMenu);
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
        flashMenuButton (editMenu);
        performInsertViewContents();
        return true;
    }

    // Shift+Cmd+N - Insert View (Flexbox)
    if (key.isKeyCode ('N') && key.getModifiers().isCommandDown() && key.getModifiers().isShiftDown() && !key.getModifiers().isAltDown())
    {
        flashMenuButton (editMenu);
        performInsertViewFlexbox();
        return true;
    }

    // Opt+Cmd+N - Insert View (Tabbed)
    if (key.isKeyCode ('N') && key.getModifiers().isCommandDown() && key.getModifiers().isAltDown() && !key.getModifiers().isShiftDown())
    {
        flashMenuButton (editMenu);
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
            treeEditor.getTreeView().setMultiSelectEnabled (false);
            builder.setEditMode (true);
            builder.setSelectedNode ({});
            temporaryEditMode = true;
        }
        else if (!shouldTempEdit && temporaryEditMode)
        {
            builder.setEditMode (false);
            treeEditor.getTreeView().setMultiSelectEnabled (true);
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

void ToolBox::flashMenuButton (juce::TextButton& button)
{
    button.setState (juce::Button::buttonDown);
    juce::Timer::callAfterDelay (80, [safeButton = juce::Component::SafePointer<juce::TextButton> (&button)]()
    {
        if (safeButton != nullptr)
            safeButton->setState (juce::Button::buttonNormal);
    });
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

bool ToolBox::isNodeSelected (const juce::ValueTree& node) const
{
    auto nodes = treeEditor.getSelectedNodes();
    return nodes.contains (node);
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
