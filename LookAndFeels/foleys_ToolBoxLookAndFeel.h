/*
 ==============================================================================
    Modern dark LookAndFeel for the PGM ToolBox editor.
    Drop-in replacement for the default JUCE V4 look.
    
    Set on the ToolBox component and it cascades to all children.
 ==============================================================================
 */

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace foleys
{

struct ToolBoxColours
{
    // Backgrounds
    static inline const juce::Colour bg             { 0xff1e1e1e };  // Main background
    static inline const juce::Colour bgLight        { 0xff2b2b2b };  // Panels, tree items
    static inline const juce::Colour bgLighter      { 0xff333333 };  // Hover states
    static inline const juce::Colour bgInput        { 0xff252525 };  // Text fields, combo boxes

    // Borders & lines
    static inline const juce::Colour border          { 0xff3c3c3c };  // Subtle borders
    static inline const juce::Colour borderLight     { 0xff4a4a4a };  // Hover borders
    static inline const juce::Colour divider         { 0xff2f2f2f };  // Separators

    // Text
    static inline const juce::Colour text            { 0xffd4d4d4 };  // Primary text
    static inline const juce::Colour textDim         { 0xff808080 };  // Secondary / disabled
    static inline const juce::Colour textBright      { 0xffffffff };  // Emphasis

    // Accent
    static inline const juce::Colour accent          { 0xff4a9eff };  // Selection, focus
    static inline const juce::Colour accentDim       { 0xff2d6bbf };  // Pressed accent
    static inline const juce::Colour accentSubtle    { 0x204a9eff };  // Selection background

    // Status
    static inline const juce::Colour danger          { 0xffcf4444 };  // Delete, remove
};


class ToolBoxLookAndFeel : public juce::LookAndFeel_V4
{
public:
    ToolBoxLookAndFeel()
    {
        // Base colours
        setColour (juce::ResizableWindow::backgroundColourId, ToolBoxColours::bg);
        setColour (juce::TextEditor::backgroundColourId,      ToolBoxColours::bgInput);
        setColour (juce::TextEditor::outlineColourId,         ToolBoxColours::border);
        setColour (juce::TextEditor::focusedOutlineColourId,  ToolBoxColours::accent);
        setColour (juce::TextEditor::textColourId,            ToolBoxColours::text);
        setColour (juce::TextEditor::highlightColourId,       ToolBoxColours::accentSubtle);

        setColour (juce::Label::textColourId,                 ToolBoxColours::text);

        setColour (juce::ListBox::backgroundColourId,         ToolBoxColours::bg);
        setColour (juce::ListBox::outlineColourId,            ToolBoxColours::border);
        setColour (juce::ListBox::textColourId,               ToolBoxColours::text);

        setColour (juce::TreeView::backgroundColourId,              ToolBoxColours::bg);
        setColour (juce::TreeView::linesColourId,                   ToolBoxColours::border);
        setColour (juce::TreeView::selectedItemBackgroundColourId,  ToolBoxColours::accentSubtle);

        setColour (juce::TextButton::buttonColourId,          ToolBoxColours::bgLight);
        setColour (juce::TextButton::buttonOnColourId,        ToolBoxColours::accentDim);
        setColour (juce::TextButton::textColourOffId,         ToolBoxColours::text);
        setColour (juce::TextButton::textColourOnId,          ToolBoxColours::textBright);

        setColour (juce::ComboBox::backgroundColourId,        ToolBoxColours::bgInput);
        setColour (juce::ComboBox::outlineColourId,           ToolBoxColours::border);
        setColour (juce::ComboBox::textColourId,              ToolBoxColours::text);
        setColour (juce::ComboBox::arrowColourId,             ToolBoxColours::textDim);

        setColour (juce::PopupMenu::backgroundColourId,            ToolBoxColours::bgLight);
        setColour (juce::PopupMenu::textColourId,                  ToolBoxColours::text);
        setColour (juce::PopupMenu::highlightedBackgroundColourId, ToolBoxColours::accent);
        setColour (juce::PopupMenu::highlightedTextColourId,       ToolBoxColours::textBright);
        setColour (juce::PopupMenu::headerTextColourId,            ToolBoxColours::textDim);

        setColour (juce::ScrollBar::thumbColourId,            ToolBoxColours::borderLight);
        setColour (juce::ScrollBar::trackColourId,            juce::Colours::transparentBlack);

        setColour (juce::PropertyComponent::backgroundColourId, ToolBoxColours::bg);
        setColour (juce::PropertyComponent::labelTextColourId,  ToolBoxColours::text);

        setColour (juce::TooltipWindow::backgroundColourId,   ToolBoxColours::bgLighter);
        setColour (juce::TooltipWindow::textColourId,         ToolBoxColours::text);
        setColour (juce::TooltipWindow::outlineColourId,      ToolBoxColours::border);
    }

    //==============================================================================
    // TEXT BUTTONS — flat with subtle hover/press
    //==============================================================================

    void drawButtonBackground (juce::Graphics& g,
                               juce::Button& button,
                               const juce::Colour& backgroundColour,
                               bool shouldDrawButtonAsHighlighted,
                               bool shouldDrawButtonAsDown) override
    {
        auto bounds = button.getLocalBounds().toFloat().reduced (0.5f);
        auto r = 3.0f;

        auto baseColour = backgroundColour;

        if (shouldDrawButtonAsDown)
            baseColour = baseColour.brighter (0.1f);
        else if (shouldDrawButtonAsHighlighted)
            baseColour = baseColour.brighter (0.05f);

        g.setColour (baseColour);
        g.fillRoundedRectangle (bounds, r);

        g.setColour (ToolBoxColours::border);
        g.drawRoundedRectangle (bounds, r, 1.0f);
    }

    //==============================================================================
    // SCROLLBAR — thin capsule, macOS-style
    //==============================================================================

    void drawScrollbar (juce::Graphics& g,
                        juce::ScrollBar& scrollbar,
                        int x, int y, int width, int height,
                        bool isScrollbarVertical,
                        int thumbStartPosition, int thumbSize,
                        bool isMouseOver, bool isMouseDown) override
    {
        juce::ignoreUnused (scrollbar);

        if (thumbSize <= 0)
            return;

        auto alpha = (isMouseOver || isMouseDown) ? 0.6f : 0.3f;
        auto thumbColour = ToolBoxColours::text.withAlpha (alpha);

        juce::Rectangle<float> thumbBounds;

        if (isScrollbarVertical)
        {
            auto thumbWidth = juce::jmax (4.0f, width * 0.9f);
            auto xOffset = (width - thumbWidth) * 0.5f;
            thumbBounds = { x + xOffset, (float) thumbStartPosition,
                            thumbWidth, (float) thumbSize };
        }
        else
        {
            auto thumbHeight = juce::jmax (4.0f, height * 0.9f);
            auto yOffset = (height - thumbHeight) * 0.5f;
            thumbBounds = { (float) thumbStartPosition, y + yOffset,
                            (float) thumbSize, thumbHeight };
        }

        auto cornerSize = juce::jmin (thumbBounds.getWidth(), thumbBounds.getHeight()) * 0.5f;
        g.setColour (thumbColour);
        g.fillRoundedRectangle (thumbBounds.reduced (1.0f), cornerSize);
    }

    int getDefaultScrollbarWidth() override { return 8; }

    //==============================================================================
    // TREEVIEW — modern chevrons instead of old triangles
    //==============================================================================

    void drawTreeviewPlusMinusBox (juce::Graphics& g,
                                   const juce::Rectangle<float>& area,
                                   juce::Colour,
                                   bool isOpen, bool isMouseOver) override
    {
        auto r = area.reduced (area.getWidth() * 0.25f);
        auto colour = isMouseOver ? ToolBoxColours::text : ToolBoxColours::textDim;

        juce::Path p;

        if (isOpen)
        {
            // Downward chevron
            p.addTriangle (r.getX(), r.getY() + r.getHeight() * 0.25f,
                           r.getRight(), r.getY() + r.getHeight() * 0.25f,
                           r.getCentreX(), r.getBottom() - r.getHeight() * 0.25f);
        }
        else
        {
            // Rightward chevron
            p.addTriangle (r.getX() + r.getWidth() * 0.25f, r.getY(),
                           r.getRight() - r.getWidth() * 0.25f, r.getCentreY(),
                           r.getX() + r.getWidth() * 0.25f, r.getBottom());
        }

        g.setColour (colour);
        g.fillPath (p);
    }

    //==============================================================================
    // COMBOBOX — flat dark
    //==============================================================================

    void drawComboBox (juce::Graphics& g, int width, int height,
                       bool, int, int, int, int,
                       juce::ComboBox& box) override
    {
        auto bounds = juce::Rectangle<float> (0, 0, (float) width, (float) height).reduced (0.5f);
        auto r = 3.0f;

        g.setColour (box.findColour (juce::ComboBox::backgroundColourId));
        g.fillRoundedRectangle (bounds, r);

        g.setColour (box.findColour (juce::ComboBox::outlineColourId));
        g.drawRoundedRectangle (bounds, r, 1.0f);

        // Arrow
        auto arrowZone = juce::Rectangle<float> ((float) width - 20.0f, 0.0f, 14.0f, (float) height);
        juce::Path arrow;
        arrow.addTriangle (arrowZone.getCentreX() - 3.0f, arrowZone.getCentreY() - 2.0f,
                           arrowZone.getCentreX() + 3.0f, arrowZone.getCentreY() - 2.0f,
                           arrowZone.getCentreX(),         arrowZone.getCentreY() + 3.0f);

        g.setColour (box.findColour (juce::ComboBox::arrowColourId));
        g.fillPath (arrow);
    }

    //==============================================================================
    // POPUP MENU — styled to match plugin PopupMenuLookAndFeel
    //==============================================================================

    juce::Font getPopupMenuFont() override
    {
        return juce::Font (juce::FontOptions().withHeight (popupFontSize));
    }

    void drawPopupMenuBackground (juce::Graphics& g, int width, int height) override
    {
        // Fill full rect first to eliminate white corner artifacts on macOS
        g.setColour (ToolBoxColours::bgLight);
        g.fillRect (0, 0, width, height);
        g.fillRoundedRectangle (0, 0, (float) width, (float) height, popupCornerSize);
    }

    void drawPopupMenuItem (juce::Graphics& g,
                            const juce::Rectangle<int>& area,
                            bool isSeparator, bool isActive, bool isHighlighted,
                            bool isTicked, bool hasSubMenu,
                            const juce::String& text,
                            const juce::String& shortcutKeyText,
                            const juce::Drawable* icon,
                            const juce::Colour* textColour) override
    {
        juce::ignoreUnused (textColour, icon);

        // Separator
        if (isSeparator)
        {
            g.setColour (ToolBoxColours::borderLight);
            g.drawLine ((float) area.getX() + popupPadding * 0.5f,
                        area.getHeight() * 0.5f,
                        (float) area.getWidth() - popupPadding * 0.5f,
                        area.getHeight() * 0.5f,
                        1.0f);
            return;
        }

        // Highlighted background — rounded rect with inset padding
        auto textColourToUse = isActive ? ToolBoxColours::text : ToolBoxColours::textDim;

        if (isHighlighted && isActive)
        {
            g.setColour (ToolBoxColours::bgLighter);
            g.fillRoundedRectangle (
                area.getX() + popupHighlightPadding,
                area.getY() + popupHighlightPadding,
                area.getWidth() - popupHighlightPadding * 2.0f,
                area.getHeight() - popupHighlightPadding * 2.0f,
                popupCornerSize);

            textColourToUse = ToolBoxColours::textBright;
        }

        // Opacity for disabled items
        g.setOpacity (isActive ? 1.0f : 0.5f);

        // Tick mark — small checkmark ✓ to the left of text
        if (isTicked)
        {
            auto tickSize = area.getHeight() * 0.28f;
            auto tickX = area.getX() + 7.0f;
            auto tickY = area.getCentreY();

            juce::Path tick;
            tick.startNewSubPath (tickX, tickY);
            tick.lineTo (tickX + tickSize * 0.4f, tickY + tickSize * 0.4f);
            tick.lineTo (tickX + tickSize, tickY - tickSize * 0.35f);

            g.setColour (textColourToUse);
            g.strokePath (tick, juce::PathStrokeType (1.5f, juce::PathStrokeType::curved,
                                                      juce::PathStrokeType::rounded));
        }

        // Submenu arrow — drawn as a small triangle
        if (hasSubMenu)
        {
            auto arrowSize = area.getHeight() * 0.3f;
            auto arrowX = area.getRight() - arrowSize - popupPadding * 0.4f;
            auto arrowY = area.getCentreY();

            juce::Path arrow;
            arrow.addTriangle (arrowX, arrowY - arrowSize * 0.5f,
                               arrowX + arrowSize * 0.7f, arrowY,
                               arrowX, arrowY + arrowSize * 0.5f);

            g.setColour (textColourToUse);
            g.fillPath (arrow);
        }

        g.setColour (textColourToUse);
        g.setFont (popupFontSize);

        auto textX = area.getX() + (int) (popupPadding * (area.getHeight() * 0.03f)) + 4;
        auto textR = juce::Rectangle<int> (textX, area.getY(),
                                           area.getWidth() - textX, area.getHeight());

        
        // Shortcut key text on the right
        if (shortcutKeyText.isNotEmpty())
        {
            g.setFont (popupFontSize * 0.8f);
            g.setColour (isHighlighted ? ToolBoxColours::textBright.withAlpha (0.7f)
                                       : ToolBoxColours::textDim);
            g.drawText (shortcutKeyText,
                        textR.withTrimmedRight ((int) popupPadding * 0.8f),
                        juce::Justification::centredRight, true);
        }

        g.setFont (popupFontSize);
        g.setColour (textColourToUse);

        g.drawText (text, textR, juce::Justification::centredLeft, true);
    }

    // Popup menu styling constants
    static constexpr float popupPadding          = 24.0f;
    static constexpr float popupHighlightPadding = 3.0f;
    static constexpr float popupCornerSize       = 5.0f;
    static constexpr float popupFontSize         = 15.0f;
    static constexpr float popupItemHeight       = 26;   // default is 20

    //==============================================================================
    // PROPERTY COMPONENTS — clean dark rows
    //==============================================================================

    void drawPropertyPanelSectionHeader (juce::Graphics& g,
                                         const juce::String& name,
                                         bool isOpen, int width, int height) override
    {
        auto area = juce::Rectangle<float> (0.0f, 0.0f, (float) width, (float) height);

        g.setColour (ToolBoxColours::bgLighter);
        g.fillRect (area);

        g.setColour (ToolBoxColours::divider);
        g.fillRect (area.removeFromBottom (1.0f));

        auto arrowArea = area.removeFromLeft ((float) height).reduced ((float) height * 0.3f);

        juce::Path arrow;
        if (isOpen)
            arrow.addTriangle (arrowArea.getX(), arrowArea.getY(),
                               arrowArea.getRight(), arrowArea.getY(),
                               arrowArea.getCentreX(), arrowArea.getBottom());
        else
            arrow.addTriangle (arrowArea.getX(), arrowArea.getY(),
                               arrowArea.getRight(), arrowArea.getCentreY(),
                               arrowArea.getX(), arrowArea.getBottom());

        g.setColour (ToolBoxColours::textDim);
        g.fillPath (arrow);

        g.setColour (ToolBoxColours::text);
        
        bool hasValueMessages = name.contains ("\\has_value_messages");
        auto displayName = name.replace ("\\has_value_messages", "");

        g.setColour (ToolBoxColours::text);
        g.drawText (displayName, area.withTrimmedLeft (4.0f), juce::Justification::centredLeft);

        if (hasValueMessages)
        {
            const float circleSize = (float) height * 0.29f;
            juce::Font font (juce::FontOptions().withHeight ((float) height * 0.6f));
            juce::GlyphArrangement ga;
            ga.addLineOfText (font, displayName, 0, 0);
            const float cx = (float) height + 4.0f + ga.getBoundingBox (0, -1, true).getWidth() + 9.0f;
            const float cy = ((float) height - circleSize) * 0.5f;
            g.setColour (juce::Colour (0xff4a9eff).withAlpha (0.8f).darker (0.2f));
            g.fillEllipse (cx, cy, circleSize, circleSize);
        }
    }

    void drawPropertyComponentBackground (juce::Graphics& g,
                                           int width, int height,
                                           juce::PropertyComponent& component) override
    {
        juce::ignoreUnused (component);
        g.setColour (ToolBoxColours::bg);
        g.fillRect (0, 0, width, height);
        g.setColour (ToolBoxColours::divider);
        g.drawHorizontalLine (height - 1, 0.0f, (float) width);
    }

    void drawPropertyComponentLabel (juce::Graphics& g,
                                      int width, int height,
                                      juce::PropertyComponent& component) override
    {
        juce::ignoreUnused (width);
        auto indent = 6;
        g.setColour (ToolBoxColours::text.withAlpha (component.isEnabled() ? 1.0f : 0.5f));
        g.setFont ((float) juce::jmin (height, 24) * 0.6f);
        g.drawText (component.getName(),
                    indent, 0, width / 2 - indent, height,
                    juce::Justification::centredLeft, true);
    }

    //==============================================================================
    // RESIZER BAR — subtle dark with dots
    //==============================================================================

    void drawStretchableLayoutResizerBar (juce::Graphics& g,
                                          int w, int h,
                                          bool, bool isMouseOver, bool isMouseDragging) override
    {
        g.setColour (isMouseOver || isMouseDragging ? ToolBoxColours::borderLight
                                                    : ToolBoxColours::divider);
        g.fillRect (0, 0, w, h);

        g.setColour (ToolBoxColours::textDim.withAlpha (0.5f));
        auto cx = w * 0.5f;
        auto cy = h * 0.5f;
        auto dotSize = 2.0f;
        auto spacing = 5.0f;
        g.fillEllipse (cx - spacing - dotSize * 0.5f, cy - dotSize * 0.5f, dotSize, dotSize);
        g.fillEllipse (cx - dotSize * 0.5f,           cy - dotSize * 0.5f, dotSize, dotSize);
        g.fillEllipse (cx + spacing - dotSize * 0.5f, cy - dotSize * 0.5f, dotSize, dotSize);
    }

    //==============================================================================
    // TOOLTIP — dark, minimal
    //==============================================================================

    void drawTooltip (juce::Graphics& g, const juce::String& text,
                      int width, int height) override
    {
        auto bounds = juce::Rectangle<int> (width, height).toFloat();
        g.setColour (ToolBoxColours::bgLighter);
        g.fillRoundedRectangle (bounds, 3.0f);
        g.setColour (ToolBoxColours::border);
        g.drawRoundedRectangle (bounds.reduced (0.5f), 3.0f, 0.5f);

        g.setColour (ToolBoxColours::text);
        g.setFont (13.0f);
        g.drawFittedText (text, bounds.toNearestInt().reduced (4, 2),
                          juce::Justification::centredLeft, 4);
    }

    //==============================================================================
    // TEXT EDITOR — dark field
    //==============================================================================

    void fillTextEditorBackground (juce::Graphics& g, int width, int height,
                                   juce::TextEditor& editor) override
    {
        auto bounds = juce::Rectangle<float> (0, 0, (float) width, (float) height);
        g.setColour (editor.findColour (juce::TextEditor::backgroundColourId));
        g.fillRoundedRectangle (bounds, 2.0f);
    }

    void drawTextEditorOutline (juce::Graphics& g, int width, int height,
                                juce::TextEditor& editor) override
    {
        auto bounds = juce::Rectangle<float> (0, 0, (float) width, (float) height);
        auto colour = editor.hasKeyboardFocus (true)
                          ? editor.findColour (juce::TextEditor::focusedOutlineColourId)
                          : editor.findColour (juce::TextEditor::outlineColourId);
        g.setColour (colour);
        g.drawRoundedRectangle (bounds.reduced (0.5f), 2.0f, 1.0f);
    }

    //==============================================================================
    // LABEL — when editable
    //==============================================================================

    void drawLabel (juce::Graphics& g, juce::Label& label) override
    {
        g.fillAll (label.findColour (juce::Label::backgroundColourId));

        if (! label.isBeingEdited())
        {
            auto textColour = label.findColour (juce::Label::textColourId);
            auto area = getLabelBorderSize (label).subtractedFrom (label.getLocalBounds());

            g.setColour (textColour);
            g.setFont (getLabelFont (label));
            g.drawFittedText (label.getText(), area, label.getJustificationType(),
                              juce::jmax (1, (int) ((float) area.getHeight() / g.getCurrentFont().getHeight())),
                              label.getMinimumHorizontalScale());
        }
    }
    
    void getIdealPopupMenuItemSize (const juce::String& text, bool isSeparator,
                                    int standardMenuItemHeight, int& idealWidth, int& idealHeight) override
    {
        juce::LookAndFeel_V4::getIdealPopupMenuItemSize (text, isSeparator, standardMenuItemHeight, idealWidth, idealHeight);
        if (!isSeparator)
            idealHeight = popupItemHeight;
    }
};

} // namespace foleys
