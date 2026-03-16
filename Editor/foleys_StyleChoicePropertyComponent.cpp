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

#include "foleys_StyleChoicePropertyComponent.h"

namespace foleys
{

// Static clipboard shared across all instances
juce::String StyleChoicePropertyComponent::clipboard;

StyleChoicePropertyComponent::StyleChoicePropertyComponent (MagicGUIBuilder& builderToUse,
                                                            juce::Identifier propertyToUse,
                                                            juce::ValueTree& nodeToUse,
                                                            juce::StringArray choicesToUse)
  : StylePropertyComponent (builderToUse, propertyToUse, nodeToUse),
    choices (choicesToUse)
{
    initialiseComboBox (false);
}

StyleChoicePropertyComponent::StyleChoicePropertyComponent (MagicGUIBuilder& builderToUse, juce::Identifier propertyToUse, juce::ValueTree& nodeToUse, std::function<void(juce::ComboBox&)> lambdaToUse, const juce::String& uidPrefixToUse)
  : StylePropertyComponent (builderToUse, propertyToUse, nodeToUse),
    menuCreationLambda (lambdaToUse),
    uidPrefix (uidPrefixToUse)
{
    initialiseComboBox (false);
}

bool StyleChoicePropertyComponent::isPropertiesMenu (juce::ComboBox& combo)
{
    auto* menu = combo.getRootMenu();
    if (menu == nullptr)
        return false;

    juce::PopupMenu::MenuItemIterator iter (*menu, false);
    while (iter.next())
    {
        if (iter.getItem().text == NEEDS_TRANS ("New / Edit Value"))
        {
            auto currentValue = node.getProperty (property).toString();

            auto makeUniqueLabel = uidPrefix.isNotEmpty()
                ? NEEDS_TRANS ("Make UID Unique")
                : NEEDS_TRANS ("Make Value Unique");

            menu->addItem (makeUniqueLabel, [this]()
            {
                auto secondsSince2000 = juce::String (juce::int64 (
                    (juce::Time::getCurrentTime() - juce::Time (2000, 0, 1, 0, 0, 0)).inSeconds()));

                juce::String newValue;
                auto currentValue = node.getProperty (property).toString();

                if (uidPrefix.isNotEmpty())
                {
                    // UID format: Prefix_timestamp
                    if (currentValue.isEmpty())
                    {
                        newValue = uidPrefix + "_" + secondsSince2000;
                    }
                    else
                    {
                        auto lastUnderscore = currentValue.lastIndexOf ("_");
                        auto cleanValue = currentValue;
                        if (lastUnderscore >= 0)
                        {
                            auto suffix = currentValue.substring (lastUnderscore + 1);
                            if (suffix.containsOnly ("0123456789") && suffix.length() >= 8)
                                cleanValue = currentValue.substring (0, lastUnderscore);
                        }
                        newValue = cleanValue + "_" + secondsSince2000;
                    }
                }
                else
                {
                    // Standard value format
                    if (currentValue.isEmpty())
                    {
                        juce::String nodePrefix;
                        if (node.hasProperty ("id") && node.getProperty ("id").toString().isNotEmpty())
                            nodePrefix = node.getProperty ("id").toString().replace (" ", "-");
                        else
                            nodePrefix = node.getType().toString().replace (" ", "-");

                        newValue = nodePrefix + ":" + property.toString() + "-" + secondsSince2000;
                    }
                    else
                    {
                        auto cleanValue = currentValue;
                        auto lastDash = currentValue.lastIndexOf ("-");
                        if (lastDash >= 0)
                        {
                            auto suffix = currentValue.substring (lastDash + 1);
                            if (suffix.containsOnly ("0123456789") && suffix.length() >= 8)
                                cleanValue = currentValue.substring (0, lastDash);
                        }
                        newValue = cleanValue + "-" + secondsSince2000;
                    }
                }

                if (auto* c = dynamic_cast<juce::ComboBox*>(editor.get()))
                    c->setText (newValue, juce::sendNotificationSync);
                node.setProperty (property, newValue, &builder.getUndoManager());
                refresh();
            });

            menu->addSeparator();

            int useCount = 0;
            if (currentValue.isNotEmpty())
            {
                std::function<void(const juce::ValueTree&)> countUses = [&](const juce::ValueTree& tree)
                {
                    for (auto child : tree)
                    {
                        for (int i = 0; i < child.getNumProperties(); ++i)
                            if (child.getProperty (child.getPropertyName (i)).toString() == currentValue)
                                { ++useCount; break; }
                        countUses (child);
                    }
                };
                countUses (builder.getGuiRootNode());
            }

            menu->addItem (NEEDS_TRANS ("Find Next Use"),
                           useCount >= 2,
                           false,
                           [this, currentValue]()
            {
                auto root = builder.getGuiRootNode();
                auto currentNode = builder.getSelectedNode();
                bool foundCurrent = false;
                juce::ValueTree result;

                std::function<void(const juce::ValueTree&)> search = [&](const juce::ValueTree& tree)
                {
                    if (result.isValid()) return;
                    for (auto child : tree)
                    {
                        if (result.isValid()) return;
                        if (child == currentNode) { foundCurrent = true; search (child); continue; }
                        if (foundCurrent)
                            for (int i = 0; i < child.getNumProperties(); ++i)
                                if (child.getProperty (child.getPropertyName (i)).toString() == currentValue)
                                    { result = child; return; }
                        search (child);
                    }
                };

                search (root);

                if (! result.isValid())
                {
                    std::function<void(const juce::ValueTree&)> searchFromStart = [&](const juce::ValueTree& tree)
                    {
                        if (result.isValid()) return;
                        for (auto child : tree)
                        {
                            if (result.isValid() || child == currentNode) return;
                            for (int i = 0; i < child.getNumProperties(); ++i)
                                if (child.getProperty (child.getPropertyName (i)).toString() == currentValue)
                                    { result = child; return; }
                            searchFromStart (child);
                        }
                    };
                    searchFromStart (root);
                }

                if (result.isValid())
                    builder.setSelectedNode (result);
            });

            return true;
        }
    }

    return false;
}

void StyleChoicePropertyComponent::initialiseComboBox (bool editable)
{
    auto combo = std::make_unique<RefreshableComboBox>();
    combo->setEditableText (editable);

    if (! choices.isEmpty())
    {
        int index = 0;
        for (const auto& name : choices)
            combo->addItem (name, ++index);
    }
    else if (menuCreationLambda)
    {
        combo->refreshLambda = menuCreationLambda;
        combo->owner = this;
        menuCreationLambda (*combo);   // populate once at init so current value displays
    }

    addAndMakeVisible (combo.get());

    // Detect if this is a properties/values menu — sets hasCopyPaste
    hasCopyPaste = isPropertiesMenu (*combo);

    // For properties menus, make the combo editable so clicking the text
    // opens the text editor, and the arrow still opens the dropdown.
    if (hasCopyPaste)
        combo->setEditableText (true);

    combo->onChange = [&]
    {
        if (auto* c = dynamic_cast<juce::ComboBox*>(editor.get()))
            node.setProperty (property, c->getText().replace (" ", "-"), &builder.getUndoManager());

        refresh();
    };

    editor = std::move (combo);

    // --- Copy/Paste buttons (only for properties/values menus) ---
    if (hasCopyPaste)
    {
        addAndMakeVisible (copyButton);
        addAndMakeVisible (pasteButton);

        copyButton.setColour  (juce::TextButton::buttonColourId, ToolBoxColours::bgLight);
        pasteButton.setColour (juce::TextButton::buttonColourId, ToolBoxColours::bgLight);

        copyButton.setConnectedEdges (juce::TextButton::ConnectedOnLeft | juce::TextButton::ConnectedOnRight);
        pasteButton.setConnectedEdges (juce::TextButton::ConnectedOnLeft | juce::TextButton::ConnectedOnRight);

        copyButton.onClick = [this]
        {
            if (auto* c = dynamic_cast<juce::ComboBox*>(editor.get()))
            {
                auto currentText = c->getText();
                
                if (currentText.isEmpty())
                {
                    auto secondsSince2000 = juce::String (juce::int64 (
                        (juce::Time::getCurrentTime() - juce::Time (2000, 0, 1, 0, 0, 0)).inSeconds()));

                    if (uidPrefix.isNotEmpty())
                    {
                        // UID format: Prefix_timestamp
                        currentText = uidPrefix + "_" + secondsSince2000;
                    }
                    else
                    {
                        // Standard value format
                        juce::String nodePrefix;
                        if (node.hasProperty ("id") && node.getProperty ("id").toString().isNotEmpty())
                            nodePrefix = node.getProperty ("id").toString().replace (" ", "-");
                        else
                            nodePrefix = node.getType().toString().replace (" ", "-");

                        currentText = nodePrefix + ":" + property.toString() + "-" + secondsSince2000;
                    }

                    // Write it into the field and the node immediately
                    c->setText (currentText, juce::sendNotificationSync);
                    node.setProperty (property, currentText, &builder.getUndoManager());
                    refresh();
                }
                
                clipboard = currentText;
            }
        };

        pasteButton.onClick = [this]
        {
            if (clipboard.isNotEmpty())
            {
                if (auto* c = dynamic_cast<juce::ComboBox*>(editor.get()))
                {
                    c->setText (clipboard, juce::sendNotificationSync);
                    node.setProperty (property, clipboard, &builder.getUndoManager());
                    refresh();
                }
            }
        };
    }

    proxy.addListener (this);
}

void StyleChoicePropertyComponent::resized()
{
    auto b = getLocalBounds().reduced (1).withLeft (getWidth() / 2);
    remove.setBounds (b.removeFromRight (getHeight()));

    if (hasCopyPaste)
    {
        const auto buttonW = juce::roundToInt (getHeight() * 0.6f);
        auto buttonsArea = juce::Rectangle<int> (b.getX() - buttonW * 2, b.getY(), buttonW * 2, b.getHeight());
        pasteButton.setBounds (buttonsArea.removeFromLeft (buttonW).translated (1, 0));
        copyButton.setBounds (buttonsArea);
    }

    if (editor)
        editor->setBounds (b);
}

void StyleChoicePropertyComponent::refresh()
{
    const auto value = lookupValue();

    if (auto* combo = dynamic_cast<juce::ComboBox*>(editor.get()))
    {
        if (node == inheritedFrom)
        {
            proxy.referTo (node.getPropertyAsValue (property, &builder.getUndoManager()));
        }
        else
        {
            proxy.referTo (juce::Value());  // also fix the self-referral bug while we're here
            combo->setText (value.toString(), juce::dontSendNotification);
        }

        auto valueStr = value.toString();
        auto hasValueMessage = valueStr.contains (":") && !valueStr.startsWithIgnoreCase ("http")
            && valueStr.fromFirstOccurrenceOf (":", false, false).trimStart() == valueStr.fromFirstOccurrenceOf (":", false, false);
        combo->setColour (juce::ComboBox::textColourId,
                          hasValueMessage ? juce::Colour (0xff4a9eff)
                                          : ToolBoxColours::text);
    }

    repaint();
}

void StyleChoicePropertyComponent::valueChanged (juce::Value&)
{
    if (updating)
        return;

    juce::ScopedValueSetter<bool> updateFlag (updating, true);
    auto v = proxy.getValue().toString();

    if (auto* combo = dynamic_cast<juce::ComboBox*>(editor.get()))
    {
        if (combo->getText() != v)
            combo->setText (v, juce::sendNotificationSync);
    }

    if (property == IDs::lookAndFeel)
    {
        // this hack is needed, since the changing will have to trigger all colours a refresh
        // to fetch fallback colours
        if (auto* panel = findParentComponentOfClass<juce::PropertyPanel>())
            panel->refreshAll();
    }
    else
    {
        refresh();
    }
}


} // namespace foleys
