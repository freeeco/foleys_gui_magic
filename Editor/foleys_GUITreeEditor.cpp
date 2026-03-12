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

#include "foleys_GUITreeEditor.h"

namespace foleys
{

GUITreeEditor::GUITreeEditor (MagicGUIBuilder& builderToEdit)
  : builder (builderToEdit),
    undo (builder.getUndoManager())
{
    treeView.setRootItemVisible (true);
    treeView.setMultiSelectEnabled (true);

    setValueTree (tree);

    addAndMakeVisible (treeView);
}

void GUITreeEditor::paint (juce::Graphics& g)
{
    g.setColour (EditorColours::outline);
    g.drawRect (getLocalBounds(), 1);
    g.setColour (EditorColours::text);
    g.drawFittedText (TRANS ("GUI tree"), 2, 2, getWidth() - 4, 20, juce::Justification::centred, 1);
}

void GUITreeEditor::resized()
{
    auto bounds = getLocalBounds().reduced (1);
    bounds.removeFromTop (24);

    treeView.setBounds (bounds);
}

void GUITreeEditor::setValueTree (juce::ValueTree& refTree)
{
    auto scrollPosition = treeView.getViewport()->getViewPositionY();
    
    auto restorer = treeView.getRootItem() != nullptr ? treeView.getOpennessState (true)
                                                      : std::unique_ptr<juce::XmlElement>();
    
    tree = refTree;
    tree.addListener (this);

    treeView.setRootItem (nullptr);
    if (tree.isValid())
    {
        rootItem = std::make_unique<GUITreeEditor::GuiTreeItem> (builder, tree);
        treeView.setRootItem (rootItem.get());
    }

    if (restorer.get() != nullptr)
        treeView.restoreOpennessState (*restorer, true);
    
    juce::MessageManager::callAsync ([=, this]() {
        treeView.getViewport()->setViewPosition(0, scrollPosition);
    });
}

void GUITreeEditor::updateTree()
{
    int viewY = 0;
    if (auto* vp = treeView.getViewport())
    {
        viewY = vp->getViewPositionY();
    }

    auto selected = builder.getSelectedNode();
    auto guiNode = builder.getConfigTree().getChildWithName (IDs::view);
    setValueTree (guiNode);
    
    if (auto* vp = treeView.getViewport())
        vp->setViewPosition (0, viewY);

    if (selected.isValid())
        setSelectedNode (selected);
}

juce::TreeViewItem* GUITreeEditor::getItemForNode (const juce::ValueTree& node)
{
    if (rootItem.get() == nullptr || node.isAChildOf (tree) == false)
        return nullptr;

    std::stack<int> path;
    auto probe = node;

    while (probe != tree)
    {
        auto parent = probe.getParent();
        const int idx = parent.indexOf (probe);
        if (idx < 0)
            return nullptr;

        path.push (idx);
        probe = parent;
    }

    juce::TreeViewItem* item = rootItem.get();

    while (! path.empty())
    {
        item->setOpen (true);

        const int childIndex = path.top();
        path.pop();

        if (item->getNumSubItems() == 0 && item->mightContainSubItems())
            item->setOpen (true);

        auto* child = item->getSubItem (childIndex);
        if (child == nullptr)
            return nullptr;

        item = child;
    }

    return item;
}

void GUITreeEditor::setSelectedNode (const juce::ValueTree& node)
{
    if (rootItem.get() == nullptr || node.isAChildOf (tree) == false)
        return;

    std::stack<int> path;
    auto probe = node;
    while (probe != tree)
    {
        auto parent = probe.getParent();
        path.push (parent.indexOf (probe));
        probe = parent;
    }

    juce::TreeViewItem* itemToSelect = rootItem.get();
    while (path.empty() ==  false)
    {
        itemToSelect->setOpen (true);
        auto* childItem = itemToSelect->getSubItem (path.top());
        path.pop();
        itemToSelect = childItem;
    }

    bool alreadySelected = itemToSelect->isSelected();
    bool deselectOthers = !alreadySelected
                       && !juce::ModifierKeys::getCurrentModifiers().isCommandDown()
                       && !juce::ModifierKeys::getCurrentModifiers().isShiftDown();

    if (!treeView.isMultiSelectEnabled())
        deselectOthers = true;

    itemToSelect->setSelected (true, deselectOthers, juce::dontSendNotification);
    juce::MessageManager::callAsync ([this, itemToSelect]()
    {
        if (itemToSelect != nullptr)
            treeView.scrollToKeepItemVisible (itemToSelect);
    });
}

void GUITreeEditor::valueTreePropertyChanged (juce::ValueTree&, const juce::Identifier& property)
{
    if (property == IDs::id || property == IDs::caption || property == IDs::visible)
        updateTree();
    else
        treeView.repaint();
}

void GUITreeEditor::valueTreeChildAdded (juce::ValueTree&, juce::ValueTree&)
{
    updateTree();
}

void GUITreeEditor::valueTreeChildRemoved (juce::ValueTree&, juce::ValueTree&, int)
{
    updateTree();
}

void GUITreeEditor::valueTreeChildOrderChanged (juce::ValueTree&, int, int)
{
    updateTree();
}

void GUITreeEditor::valueTreeParentChanged (juce::ValueTree&)
{
    updateTree();
}

//==============================================================================

GUITreeEditor::GuiTreeItem::GuiTreeItem (MagicGUIBuilder& builderToUse, juce::ValueTree& refValueTree)
  : builder (builderToUse),
    itemNode (refValueTree)
{
}

juce::String GUITreeEditor::GuiTreeItem::getUniqueName() const
{
    const auto* parent = getParentItem();
    auto name = (parent != nullptr) ? parent->getUniqueName() + "|" + itemNode.getType().toString()
                                    : itemNode.getType().toString();
    auto parentNode = itemNode.getParent();
    if (parentNode.isValid())
        name += "(" + juce::String (parentNode.indexOf (itemNode)) + ")";

    return name;
}

bool GUITreeEditor::GuiTreeItem::mightContainSubItems()
{
    return itemNode.getNumChildren() > 0;
}

void GUITreeEditor::GuiTreeItem::paintItem (juce::Graphics& g, int width, int height)
{
    if (isSelected())
        g.fillAll (EditorColours::selectedBackground.withAlpha (0.5f));

    bool isVisible = true;
    if (itemNode.hasProperty (IDs::visible))
        isVisible = (bool) itemNode.getProperty (IDs::visible);
    else if (auto item = builder.createGuiItem (juce::ValueTree (itemNode.getType()), true))
        isVisible = item->isVisibleByDefault();

    bool hasValueMessages = false;
    for (int i = 0; i < itemNode.getNumProperties(); ++i)
    {
        auto value = itemNode.getPropertyAsValue (itemNode.getPropertyName (i), nullptr).toString();
        if (value.contains (":") && !value.startsWithIgnoreCase ("http")
            && value.fromFirstOccurrenceOf (":", false, false).trimStart() == value.fromFirstOccurrenceOf (":", false, false))
            { hasValueMessages = true; break; }
    }

    const float circleSize = height * 0.29f;
    const float textX      = 4.0f;

    g.setColour (EditorColours::text.darker (isVisible ? 0.0f : 0.2f));
    g.setFont (juce::Font (juce::FontOptions().withHeight (height * 0.7f)
                                              .withStyle (isVisible ? "Regular" : "Italic")));

    juce::String name = itemNode.getType().toString();

    if (itemNode.hasProperty (IDs::id) && itemNode.getProperty (IDs::id).toString().isNotEmpty())
        name += " (" + itemNode.getProperty (IDs::id).toString() + ")";

    if (itemNode.hasProperty (IDs::caption))
        name += ": " + itemNode.getProperty (IDs::caption).toString();

    g.drawText (name, (int)textX, 0, width - (int)textX - 4, height, juce::Justification::centredLeft, true);

    if (hasValueMessages)
    {
        juce::GlyphArrangement ga;
        ga.addLineOfText (g.getCurrentFont(), name, 0, 0);
        const float cx = textX + ga.getBoundingBox (0, -1, true).getWidth() + 7.0f;
        const float cy = (height - circleSize) * 0.6f;
//        g.setColour (EditorColours::text.darker (0.2f));
        g.setColour (juce::Colour (0xff4a9eff).withAlpha (0.8f).darker (0.2f));
        g.fillEllipse (cx, cy, circleSize, circleSize);
    }
}

void GUITreeEditor::GuiTreeItem::itemOpennessChanged (bool isNowOpen)
{
    if (isNowOpen && getNumSubItems() == 0)
    {
        for (auto child : itemNode)
            addSubItem (new GuiTreeItem (builder, child));
    }
}

void GUITreeEditor::GuiTreeItem::itemSelectionChanged (bool isNowSelected)
{
    if (isNowSelected)
        builder.setSelectedNode (itemNode);
    else
        if (auto* item = builder.findGuiItem (itemNode))
            item->setDraggable (false);
}

juce::var GUITreeEditor::GuiTreeItem::getDragSourceDescription()
{
    return IDs::dragSelected;
}

bool GUITreeEditor::GuiTreeItem::isInterestedInDragSource (const juce::DragAndDropTarget::SourceDetails&)
{
    return itemNode.getType() == IDs::view;
}

void GUITreeEditor::GuiTreeItem::itemDropped (const juce::DragAndDropTarget::SourceDetails &dragSourceDetails, int index)
{
    const auto text = dragSourceDetails.description.toString();
    if (text == IDs::dragSelected)
    {
        auto selectedNode = builder.getSelectedNode();
        if (selectedNode.getParent() == itemNode && selectedNode.getParent().indexOf (selectedNode) < index) {
                builder.draggedItemOnto (selectedNode, itemNode, index - 1);
        } else {
            builder.draggedItemOnto (selectedNode, itemNode, index);
        }
        return;
    }

    const auto node = juce::ValueTree::fromXml (text);
    if (node.isValid())
    {
        builder.draggedItemOnto (node, itemNode, index);
        return;
    }
}

void GUITreeEditor::collapseAll()
{
    if (rootItem == nullptr)
        return;

    std::function<void(juce::TreeViewItem*)> collapseRecursive;
    collapseRecursive = [&](juce::TreeViewItem* item)
    {
        for (int i = 0; i < item->getNumSubItems(); ++i)
            collapseRecursive (item->getSubItem (i));

        item->setOpen (false);
    };

    collapseRecursive (rootItem.get());

    // Re-open root so it's always visible
    rootItem->setOpen (true);
}

void GUITreeEditor::expandAll()
{
    if (rootItem == nullptr)
        return;

    std::function<void(juce::TreeViewItem*)> expandRecursive;
    expandRecursive = [&](juce::TreeViewItem* item)
    {
        item->setOpen (true);

        for (int i = 0; i < item->getNumSubItems(); ++i)
            expandRecursive (item->getSubItem (i));
    };

    expandRecursive (rootItem.get());
}

juce::Array<juce::ValueTree> GUITreeEditor::getSelectedNodes() const
{
    juce::Array<juce::ValueTree> nodes;
    for (int i = 0; i < treeView.getNumSelectedItems(); ++i)
    {
        if (auto* item = dynamic_cast<GuiTreeItem*> (treeView.getSelectedItem (i)))
            nodes.add (item->getTree());
    }
    return nodes;
}


} // namespace foleys
