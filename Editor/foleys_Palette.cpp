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

#include "foleys_Palette.h"

namespace foleys
{


Palette::Palette (MagicGUIBuilder& builderToUse)
  : builder (builderToUse)
{
    addAndMakeVisible (paletteList);
    paletteModel.setListBox (&paletteList);

    update();
}

void Palette::paint (juce::Graphics& g)
{
    g.fillAll (EditorColours::background);
}

void Palette::resized()
{
    paletteList.setBounds (getLocalBounds());
}

void Palette::update()
{
    paletteModel.setFactoryNames (builder.getFactoryNames());
    paletteList.updateContent();
}

//==============================================================================

void Palette::PaletteListModel::setFactoryNames (juce::StringArray names)
{
    factoryNames = names;

    // On first load, collapse all categories except Favourites
    if (collapsedCategories.empty())
    {
        for (const auto& name : factoryNames)
            if (name.endsWithChar (':') && name != "Favourites:")
                collapsedCategories.insert (name);
    }

    rebuildVisibleRows();
}

void Palette::PaletteListModel::rebuildVisibleRows()
{
    visibleRows.clear();

    juce::String currentCategory;
    bool currentCollapsed = false;

    for (int i = 0; i < factoryNames.size(); ++i)
    {
        const auto& name = factoryNames[i];

        if (name.endsWithChar (':'))
        {
            // Category header — always visible
            visibleRows.push_back (i);
            currentCategory = name;
            currentCollapsed = collapsedCategories.count (name) > 0;
        }
        else
        {
            // Item — only visible if category is not collapsed
            if (!currentCollapsed)
                visibleRows.push_back (i);
        }
    }
}

int Palette::PaletteListModel::getNumRows()
{
    return static_cast<int> (visibleRows.size());
}

void Palette::PaletteListModel::paintListBoxItem (int rowNumber, juce::Graphics &g, int width, int height, bool rowIsSelected)
{
    if (rowNumber < 0 || rowNumber >= static_cast<int> (visibleRows.size()))
        return;

    const auto actualIndex = visibleRows[static_cast<size_t> (rowNumber)];
    const auto& name = factoryNames[actualIndex];
    const bool isCategory = name.endsWithChar (':');

    auto b = juce::Rectangle<int> (1, 1, width - 2, height - 2).toFloat();
    const auto r = b.getHeight() / 2.0f;

    g.fillAll (EditorColours::background);

    if (!isCategory)
    {
        g.setColour (EditorColours::outline);
        g.drawRoundedRectangle (b, r, 1);
    }

    const auto box = juce::Rectangle<int> (juce::roundToInt (r), 0, juce::roundToInt (width - 2 * r), height);

    if (isCategory)
    {
        g.setColour (EditorColours::disabledText);

        // Draw disclosure triangle using LookAndFeel for consistency with tree view and properties panel
        bool isOpen = collapsedCategories.count (name) == 0;

        float triSize = height * 0.85f;
        float triX = 1.0f;
        float triY = (height - triSize) * 0.5f;
        auto triArea = juce::Rectangle<float> (triX, triY, triSize, triSize);

        if (listBox != nullptr)
            listBox->getLookAndFeel().drawTreeviewPlusMinusBox (g, triArea, EditorColours::disabledText, isOpen, false);

        // Draw category text offset to the right of the triangle
        auto textBox = box.withLeft (juce::roundToInt (triX + triSize + 2.0f));
        g.drawFittedText (name.dropLastCharacters (1), textBox, juce::Justification::left, 1);
    }
    else
    {
        g.setColour (EditorColours::text);
        g.drawFittedText (name, box, juce::Justification::left, 1);

        g.setColour (EditorColours::disabledText);
        g.drawFittedText (juce::String (juce::CharPointer_UTF8 ("\xe2\x9c\xa5")), box, juce::Justification::right, 1);  // ✥
    }
}

void Palette::PaletteListModel::listBoxItemClicked (int row, const juce::MouseEvent&)
{
    if (row < 0 || row >= static_cast<int> (visibleRows.size()))
        return;

    const auto actualIndex = visibleRows[static_cast<size_t> (row)];
    const auto& name = factoryNames[actualIndex];

    if (!name.endsWithChar (':'))
        return;

    // Toggle collapsed state
    if (collapsedCategories.count (name))
        collapsedCategories.erase (name);
    else
        collapsedCategories.insert (name);

    rebuildVisibleRows();

    if (listBox != nullptr)
        listBox->updateContent();
}

juce::var Palette::PaletteListModel::getDragSourceDescription (const juce::SparseSet<int> &rowsToDescribe)
{
    if (rowsToDescribe[0] < 0 || rowsToDescribe[0] >= static_cast<int> (visibleRows.size()))
        return {};

    const auto actualIndex = visibleRows[static_cast<size_t> (rowsToDescribe[0])];
    const auto& name = factoryNames[actualIndex];

    if (name.endsWithChar (':'))
        return {};

    return juce::ValueTree {name}.toXmlString();
}


} // namespace foleys
