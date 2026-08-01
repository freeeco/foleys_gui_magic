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

#include "foleys_Decorator.h"
#include "../General/foleys_SettableProperties.h"

#include <melatonin_blur/melatonin_blur.h>

namespace foleys
{

class MagicGUIBuilder;
class MagicGUIState;

enum class LayoutType;

//==============================================================================
/**
 Drag sources implement this to translate the parameter id a dragParamAssign
 drop would write. GuiItem's drag handlers query it via dynamic_cast on the
 source component: only parameter ids the source can translate highlight as
 targets, and the translated value is what gets written on drop. Sources
 with translation inactive behave exactly as plain param-assign draggers.
*/
struct ParamAssignTranslator
{
    virtual ~ParamAssignTranslator() = default;

    /** Returns the value to write for this parameter id; empty = not a valid target. */
    virtual juce::String translateParamAssign (const juce::String& parameterID) const = 0;

    /** False = pass-through mode, the plain parameter id is written unchanged. */
    virtual bool isTranslationActive() const = 0;
};

/**
 The GuiItem class will draw borders and descriptions around widgets, if defined.
 It also owns the Component and the Attachment, in case the Component is connected
 to an AudioProcessorValueTreeState.
 */
class GuiItem   : public juce::Component,
                  public juce::Value::Listener,
                  private juce::ValueTree::Listener,
                  public juce::DragAndDropTarget

{
public:
    GuiItem (MagicGUIBuilder& builder, juce::ValueTree node);
    ~GuiItem() override;

    /**
     Allows accessing the Component inside that GuiItem. Don't keep this pointer!
     */
    virtual juce::Component* getWrappedComponent() = 0;

    /**
     In update() the ValueTree properties should be used to set all properties to the component.
     You can use the magicBuilder to resolve properties from CSS.
     The Colours will be handled by default.
     */
    virtual void update() = 0;

    /**
     Set colours in the wrapped Component to the value from the stylesheet and palette.
     */
    virtual void updateColours();

    /**
     Override this to return each settable option the designer should be able to configure on your component.
     */
    virtual std::vector<SettableProperty> getSettableProperties() const { return {}; }

    /**
     For each factory you can register a translation table, which will forward the colours from the
     Stylesheet to the Components.
     */
    void setColourTranslation (std::vector<std::pair<juce::String, int>> mapping);

    /**
     Return the names of configurable colours
     */
    juce::StringArray getColourNames() const;

    /**
     Returns the parameterID that is controlled from this component.
     To allow multiple return values depending of the position where the drop arrived
     there is the drop position supplied.
     */
    virtual juce::String getControlledParameterID (juce::Point<int>) { return {}; }

    /**
     Look up a value through the DOM and CSS
     */
    juce::var getProperty (const juce::Identifier& property);

    MagicGUIState& getMagicState();

    /**
     Lookup a Component through the tree. It will return the first with that id regardless if there is another one.
     We discourage using that function, because that Component can be deleted and recreated at any time without notice.
     */
    virtual GuiItem* findGuiItemWithId (const juce::String& name);

    /**
     Reread properties from the config ValueTree
     */
    void updateInternal();
    
    void suspendNodeListening();
    void resumeNodeListening();

    void paint (juce::Graphics& g) final;
    void resized() override;

    virtual bool isContainer() const { return false; }

    /** Batch-edit gate: while true, GuiItem tree callbacks (property changed,
        child added/removed) return immediately. Value bindings are unaffected.
        The batching caller is responsible for one explicit reconcile/refresh
        after clearing it. */
    static inline bool treeEditGate = false;

    /** RAII form of treeEditGate. close() ends the batch early (before the
        caller's explicit reconcile/refresh); the destructor re-clears
        harmlessly and guards any exit path against leaving the gate open. */
    struct ScopedTreeEditGate
    {
        ScopedTreeEditGate()  { treeEditGate = true; }
        ~ScopedTreeEditGate() { treeEditGate = false; }
        void close()          { treeEditGate = false; }
    };

    virtual void createSubComponents() {}
    
    /** Order-change handler. The default rebuilds; Container overrides with a
        non-destructive permute so a pure moveChild doesn't destroy children. */
    virtual void reorderSubComponents() { createSubComponents(); }

    /** Membership-change handler (child added/removed). The default rebuilds;
        Container overrides with a diff that keeps matched items alive and
        creates/destroys only the changed ones. */
    virtual void reconcileSubComponents() { createSubComponents(); }

    /** Rebuild the items for the given child nodes only, keeping all others
        alive. For batch edits whose property writes require the item to be
        constructed fresh from the mutated node (e.g. parameter renames, where
        attachments only release/rebind on construction). The default rebuilds
        everything. */
    virtual void rebuildChildItems (const juce::Array<juce::ValueTree>&) { createSubComponents(); }

    /**
     This will trigger a recalculation of the children layout regardless of resized
     */
    virtual void updateLayout();

    /**
     Returns the layout type this item is managed by.
     */
    LayoutType getParentsLayoutType() const;

    /**
     Parse the values and set it to the FlexBox::Item for layouting.
     */
    void configureFlexBoxItem (const juce::ValueTree& node);

    void configurePosition (const juce::ValueTree& node);

    /**
     Calculates the position according to the parent area
     */
    juce::Rectangle<int> resolvePosition (juce::Rectangle<int> parent);
    
    void componentTransform();
    
    void referValues();
    
    /** Override to return false in headless items (LFO, Evaluate, Transport, etc.)
        that should be invisible unless explicitly set visible in the XML. */
    virtual bool isVisibleByDefault() const { return true; }

    /** Reads the static 'visible' property from the node, falling back to isVisibleByDefault(). */
    bool getStaticVisibility() const;

    /** Re-applies the correct visibility, respecting dynamic 'visibility' binding if active,
        otherwise falls back to the static 'visible' property / isVisibleByDefault().
        Use this anywhere you'd otherwise be tempted to call setVisible(true) unconditionally. */
    void refreshVisibility();

    /**
     Returns the bounds of the wrapped Component. This is the GuiItems bounds
     reduced by margin, padding and the caption, if one was set.
     */
    juce::Rectangle<int> getClientBounds() const;

    juce::String getTabCaption (const juce::String& defaultName) const;
    juce::Colour getTabColour() const;
    
#if FOLEYS_SHOW_GUI_EDITOR_PALLETTE
    static inline bool selectionToFront = true;
    void toFrontForEditing();
#endif
    
    void handleValueChanged (juce::Value& source);

    juce::FlexItem& getFlexItem() { return flexItem; }

    void itemDragEnter (const juce::DragAndDropTarget::SourceDetails& details) override;
    void itemDragExit (const juce::DragAndDropTarget::SourceDetails& details) override;
    
    void nudgeLeft ();
    void nudgeRight ();
    void nudgeUp ();
    void nudgeDown ();

    void paintOverChildren (juce::Graphics& g) override;

    /**
     Seeks recursively for a GuiItem
     */
    virtual GuiItem* findGuiItem (const juce::ValueTree& node);
    
    
    

#if FOLEYS_SHOW_GUI_EDITOR_PALLETTE

    /**
     This method sets the GUI in edit mode, that allows to drag the components around.
     */
    virtual void setEditMode (bool shouldEdit);
    virtual void setDraggable (bool selected);

    void mouseDown (const juce::MouseEvent& event) override;
    void mouseDrag (const juce::MouseEvent& event) override;
    void mouseUp (const juce::MouseEvent& event) override;
    void mouseDoubleClick (const juce::MouseEvent& event) override;

#endif

    bool isInterestedInDragSource (const juce::DragAndDropTarget::SourceDetails &dragSourceDetails) override;
    void itemDropped (const juce::DragAndDropTarget::SourceDetails &dragSourceDetails) override;
    
    /** The ValueTree node this item was built from. */
    const juce::ValueTree& getConfigNode() const noexcept { return configNode; }

    MagicGUIBuilder& magicBuilder;

protected:

    juce::ValueTree configNode;

    Decorator       decorator;

    juce::FlexItem  flexItem { juce::FlexItem (*this).withFlex (1.0f) };

    std::vector<std::pair<juce::String, int>> colourTranslation;

private:

    class BorderDragger : public juce::ResizableBorderComponent
    {
    public:
        BorderDragger (juce::Component* component, juce::ComponentBoundsConstrainer* constrainer = nullptr) : juce::ResizableBorderComponent (component, constrainer) {}
        std::function<void()> onDragStart, onDragging, onDragEnd;

        void mouseDown (const juce::MouseEvent& event) override
        {
            if (onDragStart) onDragStart();
            juce::ResizableBorderComponent::mouseDown (event);
        }

        void mouseDrag (const juce::MouseEvent& event) override
        {
            juce::ResizableBorderComponent::mouseDrag (event);
            if (onDragging) onDragging();
        }

        void mouseUp (const juce::MouseEvent& event) override
        {
            juce::ResizableBorderComponent::mouseUp (event);
            if (onDragEnd) onDragEnd();
        }
        
        void paint (juce::Graphics&) override {}

    private:
        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BorderDragger)
    };
    std::unique_ptr<BorderDragger>          borderDragger;
    std::unique_ptr<juce::ComponentDragger> componentDragger;
    
    float diffX;
    float diffY;
    float diffWidth;
    float diffHeight;
    float glowRadius = 0.0f;
    float glowDistance = 0.0f;
    float glowAngle = 0.0f;
    float glowOpacity = 1.0f;
    juce::Colour shadowColour = juce::Colours::black;
    bool shadowEnable = false;
    bool redrawAll = false;
    bool blurNeedsRepaint = true;
#if FOLEYS_SHOW_GUI_EDITOR_PALLETTE
    int  lastOriginLocalX  { 0 };
    int  lastOriginLocalY  { 0 };
    bool  hasOriginOffset       { false };
    bool  isDraggingOrigin      { false };
    int   originDragStartY      { 0 };
    float originDragStartRotate { 0.0f };
#endif
    
    juce::Value     visibility { true };
    juce::Value     scaleValue { 1.0f };
    juce::Value     widthScaleValue { 1.0f };
    juce::Value     heightScaleValue { 1.0f };
    juce::Value     horizontalValue { 0.0f };
    juce::Value     verticalValue { 0.0f };
    juce::Value     rotateValue { 0.0f };
    juce::Value     opacityValue { 0.0f };
    
    juce::AffineTransform transform;
    
    juce::String    highlight;

    void valueChanged (juce::Value& source) override;

    void valueTreePropertyChanged (juce::ValueTree&, const juce::Identifier&) override;

    void valueTreeChildAdded (juce::ValueTree&, juce::ValueTree&) override;

    void valueTreeChildRemoved (juce::ValueTree&, juce::ValueTree&, int) override;

    void valueTreeChildOrderChanged (juce::ValueTree&, int, int) override;

    void valueTreeParentChanged (juce::ValueTree&) override;

    /**
     This will get the necessary information from the stylesheet, using inheritance
     of nodes if needed, to set specific properties for the wrapped component.
     */
    void configureComponent();

    struct Position
    {
        bool   absolute = true;
        double value = 0.0;
    };
    Position posX, posY, posWidth, posHeight;

    void configurePosition (const juce::var& v, Position& p, double d);
    void savePosition ();
    juce::Rectangle<int> mouseDownBounds;
    enum class DragAxis { None, Horizontal, Vertical };
    DragAxis lockedDragAxis = DragAxis::None;
    bool hasDuplicatedOnDrag = false;
    juce::String dragStartPosX;
    juce::String dragStartPosY;
    
    juce::int64 mouseDownTime = 0;
    static constexpr juce::int64 dragDelayMs = 150;
    
    melatonin::CachedBlur blur { 8 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GuiItem)
};

} // namespace foleys
