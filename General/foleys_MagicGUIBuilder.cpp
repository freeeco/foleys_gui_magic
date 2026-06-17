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

#include "foleys_MagicGUIBuilder.h"
#include "../Layout/foleys_RootItem.h"
#include "../Layout/foleys_Container.h"
#include "../Helpers/foleys_DefaultGuiTrees.h"
#include "../LookAndFeels/foleys_JuceLookAndFeels.h"
#include "../LookAndFeels/foleys_LookAndFeel.h"
#include "../LookAndFeels/foleys_Skeuomorphic.h"

#if FOLEYS_SHOW_GUI_EDITOR_PALLETTE
#include "../Editor/foleys_ToolBox.h"
#endif

namespace foleys
{

#if JUCE_IOS

class MagicGUIBuilder::IOSTooltipOverlay  : public juce::Component
{
public:
    class CloseButton  : public juce::Button
    {
    public:
        CloseButton()
            : juce::Button ("CLOSE")
        {
        }

        void paintButton (juce::Graphics& g,
                          bool shouldDrawButtonAsHighlighted,
                          bool shouldDrawButtonAsDown) override
        {
            juce::ignoreUnused (shouldDrawButtonAsHighlighted, shouldDrawButtonAsDown);

            auto bounds = getLocalBounds().toFloat().reduced (0.5f);

            g.setColour (juce::Colour (0xff151515));
            g.fillRoundedRectangle (bounds, 7.0f);

            auto font = [] (float h)
            {
               #ifdef DEFAULT_FONT_NAME
                int dataSize = 0;

                if (auto* data = BinaryData::getNamedResource (DEFAULT_FONT_NAME, dataSize))
                {
                    auto typeface = juce::Typeface::createSystemTypefaceFor (data, static_cast<size_t> (dataSize));

                    if (typeface != nullptr)
                        return juce::Font (juce::FontOptions (typeface).withHeight (h));
                }
               #endif

                return juce::Font (juce::FontOptions (h));
            } (18.0f);

            g.setFont (font);
            g.setColour (juce::Colours::white.withAlpha (0.92f));
            g.drawFittedText ("CLOSE",
                              getLocalBounds().reduced (3, 0),
                              juce::Justification::centred,
                              1);
        }
    };

    explicit IOSTooltipOverlay (MagicGUIBuilder& builderToUse)
        : builder (builderToUse)
    {
        setInterceptsMouseClicks (true, true);

        addAndMakeVisible (closeButton);

        closeButton.onClick = [this]
        {
            setActive (false);
        };

        setVisible (false);
    }

    void setActive (bool shouldBeActive)
    {
        active = shouldBeActive;
        setVisible (active);

        if (auto* tooltip = builder.getTooltipWindow())
            tooltip->hideTip();

        if (active)
            toFront (false);

        repaint();
    }

    bool isActive() const
    {
        return active;
    }

    void resized() override
    {
        closeButton.setBounds (getWidth() - 86, 8, 76, 32);
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (juce::Colours::white.withAlpha (0.10f));
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        if (! active)
            return;

        showTipAtScreenPosition (e.getScreenPosition());
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (! active)
            return;

        showTipAtScreenPosition (e.getScreenPosition());
    }

private:
    MagicGUIBuilder& builder;
    CloseButton closeButton;

    bool active = false;

    void showTipAtScreenPosition (juce::Point<int> screenPos)
    {
        auto* root = builder.getRootItem();

        if (root == nullptr)
            return;

        juce::Component* target = nullptr;
        juce::String tip;

        {
            // While we're querying the component underneath us, hide ourselves
            // from hit-testing. JUCE's Component::reallyContains() (used both
            // directly by tooltip clients and internally by helpers such as
            // KeyboardComponentBase::getNoteAndVelocityAtPosition) walks the
            // sibling stack via getComponentAt(); because this overlay sits in
            // front of everything and intercepts mouse clicks, those checks
            // would otherwise treat the real target as "obscured" and bail out.
            //
            // The in-flight touch is already routed to us by JUCE's mouse
            // capture, so toggling interceptsMouseClicks for the duration of
            // the lookup does not disrupt the current event sequence.
            ScopedInterceptsOff transparentForHitTest { *this };

            target = findDeepestComponentAtScreenPos (*root, screenPos, this);
            tip    = findTooltipFor (target);
        }

        if (auto* tooltip = builder.getTooltipWindow())
        {
            if (tip.isNotEmpty())
            {
                tooltip->displayTip (screenPos.translated (0, -20), tip);

                // Keep the manually displayed tooltip above the modal help overlay.
                // This is harmless if TooltipWindow is already implemented as a floating layer,
                // and important if it is parented as a normal component inside the root.
                tooltip->toFront (false);
            }
            else
            {
                tooltip->hideTip();
            }
        }
    }

    //==============================================================================
    /** RAII helper that temporarily disables mouse-click interception on a
        component, restoring the previous state on destruction. Used during the
        tooltip lookup so that obscured components correctly report themselves
        via getComponentAt() / reallyContains().
    */
    struct ScopedInterceptsOff
    {
        explicit ScopedInterceptsOff (juce::Component& c) noexcept
            : comp (c)
        {
            comp.getInterceptsMouseClicks (prevSelf, prevChildren);
            comp.setInterceptsMouseClicks (false, false);
        }

        ~ScopedInterceptsOff() noexcept
        {
            comp.setInterceptsMouseClicks (prevSelf, prevChildren);
        }

        juce::Component& comp;
        bool prevSelf     = true;
        bool prevChildren = true;
    };

    static juce::Component* findDeepestComponentAtScreenPos (juce::Component& root,
                                                             juce::Point<int> screenPos,
                                                             const juce::Component* componentToIgnore)
    {
        if (&root == componentToIgnore)
            return nullptr;

        if (! root.isShowing())
            return nullptr;

        auto localPoint = root.getLocalPoint (nullptr, screenPos);

        if (! root.getLocalBounds().contains (localPoint))
            return nullptr;

        for (int i = root.getNumChildComponents(); --i >= 0;)
        {
            if (auto* child = root.getChildComponent (i))
            {
                if (child == componentToIgnore)
                    continue;

                if (auto* found = findDeepestComponentAtScreenPos (*child, screenPos, componentToIgnore))
                    return found;
            }
        }

        return &root;
    }

    static juce::String findTooltipFor (juce::Component* component)
    {
        for (auto* c = component; c != nullptr; c = c->getParentComponent())
        {
            if (auto* tooltipClient = dynamic_cast<juce::TooltipClient*> (c))
            {
                auto tip = tooltipClient->getTooltip();

                if (tip.isNotEmpty())
                    return tip;
            }
        }

        return {};
    }
};

#endif

MagicGUIBuilder::MagicGUIBuilder (MagicGUIState& state)
  : magicState (state)
{
    updateStylesheet();
    getConfigTree().addListener (this);
}

MagicGUIBuilder::~MagicGUIBuilder()
{
    getConfigTree().removeListener (this);
}

Stylesheet& MagicGUIBuilder::getStylesheet()
{
    return stylesheet;
}

juce::ValueTree& MagicGUIBuilder::getConfigTree()
{
    return magicState.getGuiTree();
}

juce::ValueTree MagicGUIBuilder::getGuiRootNode()
{
    return getConfigTree().getOrCreateChildWithName (IDs::view, &undo);
}

std::unique_ptr<GuiItem> MagicGUIBuilder::createGuiItem (const juce::ValueTree& node, bool dontUpdate)
{
    if (node.getType() == IDs::view)
    {
        auto item = (node == getGuiRootNode()) ? std::make_unique<RootItem>(*this, node)
                                               : std::make_unique<Container>(*this, node);
        item->updateInternal();
        item->createSubComponents();
        return item;
    }

    auto factory = factories.find (node.getType());
    if (factory != factories.end())
    {
        auto item = factory->second (*this, node);
        if (!dontUpdate)
            item->updateInternal();
        
        return item;
    }
    
    DBG ("No factory for: " << node.getType().toString());
    return {};
}

std::unique_ptr<RootItem> MagicGUIBuilder::createRootItem (const juce::ValueTree& node)
{
    std::unique_ptr<RootItem> item = std::make_unique<RootItem>(*this, node);
    item->updateInternal();
    item->createSubComponents();
    return item;
}

void MagicGUIBuilder::updateStylesheet()
{
    auto stylesNode = getConfigTree().getOrCreateChildWithName (IDs::styles, &undo);
    if (stylesNode.getNumChildren() == 0)
        stylesNode.appendChild (DefaultGuiTrees::createDefaultStylesheet(), &undo);

    auto selectedName = stylesNode.getProperty (IDs::selected, {}).toString();
    if (selectedName.isNotEmpty())
    {
        auto style = stylesNode.getChildWithProperty (IDs::name, selectedName);
        stylesheet.setStyle (style);
    }
    else
    {
        stylesheet.setStyle (stylesNode.getChild (0));
    }

    stylesheet.updateStyleClasses();
    stylesheet.updateValidRanges();
}

void MagicGUIBuilder::clearGUI()
{
    auto guiNode = getConfigTree().getOrCreateChildWithName (IDs::view, &undo);
    guiNode.removeAllChildren (&undo);
    guiNode.removeAllProperties (&undo);

    updateComponents();
}

void MagicGUIBuilder::prepareForTreeSwap()
{
    getConfigTree().removeListener (this);
    root.reset();
}

void MagicGUIBuilder::completeTreeSwap()
{
    getConfigTree().addListener (this);
    updateComponents();
}

void MagicGUIBuilder::setPropertyAndRelayout(const juce::String& targetId,
                                              const juce::Identifier& property,
                                              const juce::var& value)
{
    auto node = getGuiRootNode();
    std::function<juce::ValueTree(juce::ValueTree)> findNode = [&](juce::ValueTree tree) -> juce::ValueTree {
        if (tree.getProperty(IDs::id).toString().equalsIgnoreCase(targetId))
            return tree;
        for (auto child : tree) {
            auto result = findNode(child);
            if (result.isValid()) return result;
        }
        return {};
    };

    auto target = findNode(node);
    if (!target.isValid()) return;

    auto* item = findGuiItemWithId(targetId);

    getConfigTree().removeListener(this);
    if (item) item->suspendNodeListening();

    target.setProperty(property, value, nullptr);

    if (item)
    {
        item->configureFlexBoxItem(target);
        item->resumeNodeListening();

        if (auto* parentItem = item->findParentComponentOfClass<GuiItem>())
            parentItem->updateLayout();
    }
    getConfigTree().addListener(this);
}

void MagicGUIBuilder::showOverlayDialog (std::unique_ptr<juce::Component> dialog)
{
    if (parent == nullptr)
        return;

    overlayDialog = std::move (dialog);
    parent->addAndMakeVisible (overlayDialog.get());

    parent->resized();
}

void MagicGUIBuilder::closeOverlayDialog()
{
    overlayDialog.reset();
}

void MagicGUIBuilder::createGUI (juce::Component& parentToUse)
{
    parent = &parentToUse;

    updateComponents();

#if FOLEYS_SHOW_GUI_EDITOR_PALLETTE
    if (magicToolBox.get() != nullptr)
        magicToolBox->stateWasReloaded();
#endif
}

void MagicGUIBuilder::updateComponents()
{
    if (parent == nullptr)
        return;

    updateStylesheet();

#if JUCE_IOS
    if (iosTooltipOverlay != nullptr)
        if (auto* oldParent = iosTooltipOverlay->getParentComponent())
            oldParent->removeChildComponent (iosTooltipOverlay.get());
#endif

    root = createRootItem (getGuiRootNode());
    parent->addAndMakeVisible (root.get());

    root->setBounds (parent->getLocalBounds());

#if JUCE_IOS
    if (iosTooltipOverlay != nullptr)
    {
        root->addAndMakeVisible (*iosTooltipOverlay);
        iosTooltipOverlay->setBounds (root->getLocalBounds());

        if (iosTooltipOverlay->isActive())
            iosTooltipOverlay->toFront (false);
    }
#endif

#if FOLEYS_SHOW_GUI_EDITOR_PALLETTE
    if (root.get() != nullptr)
        root->setEditMode (editMode);
#endif
}

void MagicGUIBuilder::updateLayout (juce::Rectangle<int> bounds)
{
    if (parent == nullptr)
        return;

    if (root.get() != nullptr)
    {
        auto rootBounds = bounds;

#if JUCE_IOS
        const auto rootNode = getGuiRootNode();
        int minW = 0;
        int minH = 0;

        #ifdef IOS_MIN_WINDOW_WIDTH
            minW = IOS_MIN_WINDOW_WIDTH;
        #else
            minW = static_cast<int> (getStyleProperty (IDs::minWidth, rootNode));
        #endif

        #ifdef IOS_MIN_WINDOW_HEIGHT
            minH = IOS_MIN_WINDOW_HEIGHT;
        #else
            minH = static_cast<int> (getStyleProperty (IDs::minHeight, rootNode));
        #endif

        if (minW > 0) rootBounds.setWidth  (juce::jmax (rootBounds.getWidth(),  minW));
        if (minH > 0) rootBounds.setHeight (juce::jmax (rootBounds.getHeight(), minH));
#endif

        if (! stylesheet.setMediaSize (bounds.getWidth(), bounds.getHeight()))
        {
            stylesheet.updateValidRanges();
            root->updateInternal();
        }

        if (root->getBounds() == rootBounds)
            root->updateLayout();
        else
            root->setBounds (rootBounds);
    }

#if JUCE_IOS
    if (iosTooltipOverlay != nullptr && root != nullptr)
    {
        if (iosTooltipOverlay->getParentComponent() != root.get())
            root->addAndMakeVisible (*iosTooltipOverlay);

        iosTooltipOverlay->setBounds (root->getLocalBounds());

        if (iosTooltipOverlay->isActive())
            iosTooltipOverlay->toFront (false);
    }
#endif

    if (overlayDialog)
    {
        if (overlayDialog->getBounds() == bounds)
            overlayDialog->resized();
        else
            overlayDialog->setBounds (bounds);
    }

    parent->repaint();
}

void MagicGUIBuilder::updateColours()
{
    if (root)
        root->updateColours();
}

GuiItem* MagicGUIBuilder::findGuiItemWithId (const juce::String& name)
{
    if (root)
        return root->findGuiItemWithId (name);

    return nullptr;
}

GuiItem* MagicGUIBuilder::findGuiItem (const juce::ValueTree& node)
{
    if (node.isValid() && root)
        return root->findGuiItem (node);

    return nullptr;
}

void MagicGUIBuilder::registerFactory (juce::Identifier type, std::unique_ptr<GuiItem>(*factory)(MagicGUIBuilder& builder, const juce::ValueTree&))
{
    if (factories.find (type) != factories.cend())
    {
        // You tried to add two factories with the same type name!
        // That cannot work, the second factory will be ignored.
        jassertfalse;
        return;
    }

    factories [type] = factory;
}

void MagicGUIBuilder::registerFactory (juce::Identifier type, std::unique_ptr<GuiItem>(*factory)(MagicGUIBuilder& builder, const juce::ValueTree&), const juce::String& category, bool isFavourite)
{
    registerFactory (type, factory);
    factoryCategories [type] = category;

    if (isFavourite)
        factoryFavourites.insert (type);
}

juce::StringArray MagicGUIBuilder::getFactoryNames() const
{
    // Build a map of category -> sorted item names
    std::map<juce::String, juce::StringArray> categorised;
    juce::StringArray uncategorised;
    juce::StringArray favourites;

    // View is a built-in type (not in factories map) — add it manually
    favourites.add (IDs::view.toString());
    categorised["Drawing & Layout"].add (IDs::view.toString());

    for (const auto& f : factories)
    {
        auto name = f.first.toString();

        // Collect favourites
        if (factoryFavourites.count (f.first))
            favourites.add (name);

        // Place into category or uncategorised
        auto it = factoryCategories.find (f.first);
        if (it != factoryCategories.end())
            categorised[it->second].add (name);
        else
            uncategorised.add (name);
    }

    // Sort items within each category
    for (auto& [cat, items] : categorised)
        items.sortNatural();

    uncategorised.sortNatural();
    favourites.sortNatural();

    // Build final list
    juce::StringArray names;

    // Favourites first (View pinned at top)
    names.add ("Favourites:");
    names.add (IDs::view.toString());
    favourites.removeString (IDs::view.toString());
    names.addArray (favourites);

    // Categories alphabetically (std::map is already sorted by key)
    for (const auto& [cat, items] : categorised)
    {
        names.add (cat + ":");
        names.addArray (items);
    }

    // Uncategorised items at the end
    if (uncategorised.size() > 0)
    {
        names.add ("Miscellaneous:");
        names.addArray (uncategorised);
    }

    return names;
}

juce::String MagicGUIBuilder::getFactoryCategory (juce::Identifier type) const
{
    auto it = factoryCategories.find (type);
    return it != factoryCategories.end() ? it->second : juce::String();
}

void MagicGUIBuilder::registerLookAndFeel (juce::String name, std::unique_ptr<juce::LookAndFeel> lookAndFeel)
{
    stylesheet.registerLookAndFeel (name, std::move (lookAndFeel));
}

void MagicGUIBuilder::registerJUCELookAndFeels()
{
    stylesheet.registerLookAndFeel ("LookAndFeel_V1", std::make_unique<juce::LookAndFeel_V1>());
    stylesheet.registerLookAndFeel ("LookAndFeel_V2", std::make_unique<JuceLookAndFeel_V2>());
    stylesheet.registerLookAndFeel ("LookAndFeel_V3", std::make_unique<JuceLookAndFeel_V3>());
    stylesheet.registerLookAndFeel ("LookAndFeel_V4", std::make_unique<JuceLookAndFeel_V4>());
    stylesheet.registerLookAndFeel ("FoleysFinest", std::make_unique<LookAndFeel>());
    stylesheet.registerLookAndFeel ("Skeuomorphic", std::make_unique<Skeuomorphic>());
}

juce::var MagicGUIBuilder::getStyleProperty (const juce::Identifier& name, const juce::ValueTree& node) const
{
    return stylesheet.getStyleProperty (name, node);
}

void MagicGUIBuilder::removeStyleClassReferences (juce::ValueTree tree, const juce::String& name)
{
    if (tree.hasProperty (IDs::styleClass))
    {
        const auto separator = " ";
        auto strings = juce::StringArray::fromTokens (tree.getProperty (IDs::styleClass).toString(), separator, "");
        strings.removeEmptyStrings (true);
        strings.removeString (name);
        tree.setProperty (IDs::styleClass, strings.joinIntoString (separator), &undo);
    }

    for (auto child : tree)
        removeStyleClassReferences (child, name);
}

juce::StringArray MagicGUIBuilder::getColourNames (juce::Identifier type)
{
    juce::ValueTree node (type);
    if (auto item = createGuiItem (node))
        return item->getColourNames();

    return {};
}

std::function<void(juce::ComboBox&)> MagicGUIBuilder::createChoicesMenuLambda (juce::StringArray choices) const
{
    return [choices](juce::ComboBox& combo)
    {
        int index = 0;
        for (auto& choice : choices)
            combo.addItem (choice, ++index);
    };
}

std::function<void(juce::ComboBox&)> MagicGUIBuilder::createParameterMenuLambda() const
{
    return [this](juce::ComboBox& combo)
    {
        *combo.getRootMenu() = magicState.createParameterMenu();
    };
}

std::function<void(juce::ComboBox&)> MagicGUIBuilder::createPropertiesMenuLambda() const
{
    return [this](juce::ComboBox& combo)
    {
        magicState.populatePropertiesMenu (combo);
    };
}

std::function<void(juce::ComboBox&)> MagicGUIBuilder::createTriggerMenuLambda() const
{
    return [this](juce::ComboBox& combo)
    {
        *combo.getRootMenu() = magicState.createTriggerMenu();
    };
}

std::function<void(juce::ComboBox&)> MagicGUIBuilder::createPlayheadUIDMenuLambda() const
{
    return [this](juce::ComboBox& combo)
    {
        auto uids = magicState.getPlayheadUIDs();

        int index = 1;
        for (auto& uid : uids)
            combo.addItem (uid, index++);

        combo.addSeparator();

        combo.getRootMenu()->addItem (NEEDS_TRANS ("New / Edit Value"), [&combo]
        {
            combo.setEditableText (true);
            combo.showEditor();
        });
    };
}

std::function<void(juce::ComboBox&)> MagicGUIBuilder::createLFOUIDMenuLambda() const
{
    return [this](juce::ComboBox& combo)
    {
        auto uids = magicState.getLFOUIDs();

        int index = 1;
        for (auto& uid : uids)
            combo.addItem (uid, index++);

        combo.addSeparator();

        combo.getRootMenu()->addItem (NEEDS_TRANS ("New / Edit Value"), [&combo]
        {
            combo.setEditableText (true);
            combo.showEditor();
        });
    };
}

std::function<void(juce::ComboBox&)> MagicGUIBuilder::createEnvelopeUIDMenuLambda() const
{
    return [this](juce::ComboBox& combo)
    {
        auto uids = magicState.getEnvelopeUIDs();

        int index = 1;
        for (auto& uid : uids)
            combo.addItem (uid, index++);

        combo.addSeparator();

        combo.getRootMenu()->addItem (NEEDS_TRANS ("New / Edit Value"), [&combo]
        {
            combo.setEditableText (true);
            combo.showEditor();
        });
    };
}

std::function<void(juce::ComboBox&)> MagicGUIBuilder::createGeneratorUIDMenuLambda() const
{
    return [this](juce::ComboBox& combo)
    {
        auto uids = magicState.getGeneratorUIDs();

        int index = 1;
        for (auto& uid : uids)
            combo.addItem (uid, index++);

        combo.addSeparator();

        combo.getRootMenu()->addItem (NEEDS_TRANS ("New / Edit Value"), [&combo]
        {
            combo.setEditableText (true);
            combo.showEditor();
        });
    };
}

std::function<void(juce::ComboBox&)> MagicGUIBuilder::createCalculatorUIDMenuLambda() const
{
    return [this](juce::ComboBox& combo)
    {
        auto uids = magicState.getCalculatorUIDs();

        int index = 1;
        for (auto& uid : uids)
            combo.addItem (uid, index++);

        combo.addSeparator();

        combo.getRootMenu()->addItem (NEEDS_TRANS ("New / Edit Value"), [&combo]
        {
            combo.setEditableText (true);
            combo.showEditor();
        });
    };
}

std::function<void(juce::ComboBox&)> MagicGUIBuilder::createMapperUIDMenuLambda() const
{
    return [this](juce::ComboBox& combo)
    {
        auto uids = magicState.getMapperUIDs();

        int index = 1;
        for (auto& uid : uids)
            combo.addItem (uid, index++);

        combo.addSeparator();

        combo.getRootMenu()->addItem (NEEDS_TRANS ("New / Edit Value"), [&combo]
        {
            combo.setEditableText (true);
            combo.showEditor();
        });
    };
}

std::function<void(juce::ComboBox&)> MagicGUIBuilder::createArpeggiatorUIDMenuLambda() const
{
    return [this](juce::ComboBox& combo)
    {
        auto uids = magicState.getArpeggiatorUIDs();

        int index = 1;
        for (auto& uid : uids)
            combo.addItem (uid, index++);

        combo.addSeparator();

        combo.getRootMenu()->addItem (NEEDS_TRANS ("New / Edit Value"), [&combo]
        {
            combo.setEditableText (true);
            combo.showEditor();
        });
    };
}

std::function<void(juce::ComboBox&)> MagicGUIBuilder::createPlaylistUIDMenuLambda() const
{
    return [this](juce::ComboBox& combo)
    {
        auto uids = magicState.getPlaylistUIDs();
        int index = 1;
        for (auto& uid : uids)
            combo.addItem (uid, index++);
        combo.addSeparator();
        combo.getRootMenu()->addItem (NEEDS_TRANS ("New / Edit Value"), [&combo]
        {
            combo.setEditableText (true);
            combo.showEditor();
        });
    };
}

std::function<void(juce::ComboBox&)> MagicGUIBuilder::createClipUIDMenuLambda() const
{
    return [this](juce::ComboBox& combo)
    {
        auto uids = magicState.getClipUIDs();
        int index = 1;
        for (auto& uid : uids)
            combo.addItem (uid, index++);
        combo.addSeparator();
        combo.getRootMenu()->addItem (NEEDS_TRANS ("New / Edit Value"), [&combo]
        {
            combo.setEditableText (true);
            combo.showEditor();
        });
    };
}

std::function<void(juce::ComboBox&)> MagicGUIBuilder::createModifierUIDMenuLambda() const
{
    return [this](juce::ComboBox& combo)
    {
        auto uids = magicState.getModifierUIDs();
        int index = 1;
        for (auto& uid : uids)
            combo.addItem (uid, index++);
        combo.addSeparator();
        combo.getRootMenu()->addItem (NEEDS_TRANS ("New / Edit Value"), [&combo]
        {
            combo.setEditableText (true);
            combo.showEditor();
        });
    };
}

std::function<void(juce::ComboBox&)> MagicGUIBuilder::createUidMenuLambda() const
{
    return [this] (juce::ComboBox& combo)
    {
        int index = 1;

        auto addGroup = [&] (const juce::StringArray& uids)
        {
            for (auto& uid : uids)
                combo.addItem (uid, index++);
            if (! uids.isEmpty())
                combo.addSeparator();
        };

        addGroup (magicState.getLFOUIDs());
        addGroup (magicState.getGeneratorUIDs());
        addGroup (magicState.getCalculatorUIDs());
        addGroup (magicState.getMapperUIDs());
        addGroup (magicState.getArpeggiatorUIDs());
        addGroup (magicState.getPlayheadUIDs());
        addGroup (magicState.getPlaylistUIDs());
        addGroup (magicState.getClipUIDs());
        addGroup (magicState.getModifierUIDs());

        combo.getRootMenu()->addItem (NEEDS_TRANS ("New / Edit Value"), [&combo]
        {
            combo.setEditableText (true);
            combo.showEditor();
        });
    };
}

std::function<void(juce::ComboBox&)> MagicGUIBuilder::createPositionMenuLambda() const
{
    return [](juce::ComboBox& combo)
    {
        auto addBand = [&combo] (const juce::String& name, int first, int last)
        {
            juce::PopupMenu sub;
            for (int i = first; i <= last; ++i)
                sub.addItem (i + 1, juce::String (i));
            combo.getRootMenu()->addSubMenu (name, sub);
        };

        addBand (NEEDS_TRANS ("Before Triggers (0-15)"),   0,  15);
        addBand (NEEDS_TRANS ("Normal (16-31)"),          16,  31);
        addBand (NEEDS_TRANS ("After Sequencers (32-47)"), 32, 47);

        combo.addSeparator();

        combo.getRootMenu()->addItem (NEEDS_TRANS ("New / Edit Value"), [&combo]
        {
            combo.setEditableText (true);
            combo.showEditor();
        });
    };
}

std::function<void(juce::ComboBox&)> MagicGUIBuilder::createNodePropertiesMenuLambda() const
{
    auto* self = const_cast<MagicGUIBuilder*> (this);

    return [self] (juce::ComboBox& combo)
    {
        // The combo is owned by the StyleChoicePropertyComponent that edits this
        // property. Read the edited node straight off it — works in the main ToolBox
        // and the Trigger Bank Editor alike, with no plugin-class dependency.
        juce::ValueTree editedNode;
        if (auto* spc = combo.findParentComponentOfClass<StyleChoicePropertyComponent>())
            editedNode = spc->getNode();   // <-- adjust to the real accessor (see note)

        auto targetUid = editedNode.getProperty ("destination-uid").toString();
        if (targetUid.isEmpty())
            return;

        // Find the destination node: any *-uid property matching targetUid,
        // ignoring our own destination-uid pointer property and our own node.
        juce::ValueTree found;
        std::function<void (juce::ValueTree)> walk = [&] (juce::ValueTree tree)
        {
            if (found.isValid())
                return;

            if (tree != editedNode)
            {
                for (int i = 0; i < tree.getNumProperties(); ++i)
                {
                    auto propName = tree.getPropertyName (i);
                    if (propName.toString() == "destination-uid")
                        continue;

                    if (propName.toString().endsWith ("-uid")
                        && tree.getProperty (propName).toString() == targetUid)
                    {
                        found = tree;
                        return;
                    }
                }
            }

            for (auto child : tree)
                walk (child);
        };

        walk (self->getGuiRootNode());
        if (! found.isValid())
            return;

        // Bare-type temp — no uid, no engine wiring (dontUpdate = true).
        juce::ValueTree bare (found.getType());
        auto item = self->createGuiItem (bare, true);
        if (item == nullptr)
            return;

        int index = 1;
        for (auto& p : item->getSettableProperties())
        {
            auto name = p.name.toString();
            if (name.endsWith ("-uid"))
                continue;
            combo.addItem (name, index++);
        }

        combo.getRootMenu()->addItem (NEEDS_TRANS ("New / Edit Value"), [&combo]
        {
            combo.setEditableText (true);
            combo.showEditor();
        });
    };
}

juce::var MagicGUIBuilder::getPropertyDefaultValue (juce::Identifier property) const
{
    // flexbox
    if (property == IDs::flexDirection) return IDs::flexDirRow;
    if (property == IDs::flexWrap)      return IDs::flexNoWrap;
    if (property == IDs::flexAlignContent) return IDs::flexStretch;
    if (property == IDs::flexAlignItems) return IDs::flexStretch;
    if (property == IDs::flexJustifyContent) return IDs::flexStart;
    if (property == IDs::flexAlignSelf) return IDs::flexStretch;
    if (property == IDs::flexOrder) return 0;
    if (property == IDs::flexGrow) return 1.0;
    if (property == IDs::flexShrink) return 1.0;
    if (property == IDs::minWidth) return 0.0;
    if (property == IDs::minHeight) return 0.0;
    if (property == IDs::display) return IDs::flexbox;

    if (property == IDs::captionPlacement) return "centred-top";
    if (property == IDs::lookAndFeel) return "FoleysFinest";

    if (property == juce::Identifier ("font-size")) return 12.0;
    if (property == IDs::opacity) return 1.0;
    if (property == IDs::glowOpacity) return 1.0;

    return {};
}

RadioButtonManager& MagicGUIBuilder::getRadioButtonManager()
{
    return radioButtonManager;
}

void MagicGUIBuilder::changeListenerCallback (juce::ChangeBroadcaster*)
{
    if (root.get() != nullptr)
        root->updateInternal();

    root->resized();
}

void MagicGUIBuilder::valueTreeRedirected (juce::ValueTree& treeWhichHasBeenChanged)
{
    juce::ignoreUnused (treeWhichHasBeenChanged);
    updateComponents();
}

MagicGUIState& MagicGUIBuilder::getMagicState()
{
    return magicState;
}

juce::UndoManager& MagicGUIBuilder::getUndoManager()
{
    return undo;
}

juce::TooltipWindow* MagicGUIBuilder::getTooltipWindow()
{
    if (root)
        return root->getTooltipWindow();
    else
        return nullptr;
}

#if JUCE_IOS

void MagicGUIBuilder::setIOSTooltipModeEnabled (bool shouldBeEnabled)
{
    if (root == nullptr)
        return;

    if (iosTooltipOverlay == nullptr)
        iosTooltipOverlay = std::make_unique<IOSTooltipOverlay> (*this);

    if (iosTooltipOverlay->getParentComponent() != root.get())
        root->addAndMakeVisible (*iosTooltipOverlay);

    iosTooltipOverlay->setBounds (root->getLocalBounds());
    iosTooltipOverlay->setActive (shouldBeEnabled);
}

bool MagicGUIBuilder::isIOSTooltipModeEnabled() const
{
    return iosTooltipOverlay != nullptr && iosTooltipOverlay->isActive();
}

#endif

void MagicGUIBuilder::refreshColours()
{
    updateColours();
    if (parent != nullptr)
        parent->repaint();
}

#if FOLEYS_SHOW_GUI_EDITOR_PALLETTE

void MagicGUIBuilder::setEditMode (bool shouldEdit, bool shouldDeselect)
{
    editMode = shouldEdit;
    if (parent == nullptr) return;
    if (root.get() != nullptr)
        root->setEditMode (shouldEdit);

    // Only restore z-order when LEAVING edit mode
    if (!shouldEdit && GuiItem::selectionToFront && selectedNode.isValid())
        restoreZOrderForAll();

    if (!shouldEdit && shouldDeselect)
        setSelectedNode (juce::ValueTree());

    parent->repaint();
}

bool MagicGUIBuilder::isEditModeOn() const
{
    return editMode;
}

#endif

#if FOLEYS_SHOW_GUI_EDITOR_PALLETTE || USE_PROPERTY_COMPONENTS

void MagicGUIBuilder::setSelectedNode (const juce::ValueTree& node)
{
    if (selectedNode != node)
    {
#if FOLEYS_SHOW_GUI_EDITOR_PALLETTE
        if (auto* item = findGuiItem (selectedNode))
            if (!isNodeSelected (selectedNode))
                item->setDraggable (false);
       #endif

        selectedNode = node;

#if FOLEYS_SHOW_GUI_EDITOR_PALLETTE
        if (magicToolBox.get() != nullptr)
            magicToolBox->setSelectedNode (selectedNode);

        if (auto* item = findGuiItem (selectedNode))
            item->setDraggable (true);
#endif

        if (parent != nullptr)
            parent->repaint();
    }
}

const juce::ValueTree& MagicGUIBuilder::getSelectedNode() const
{
    return selectedNode;
}

juce::Array<juce::ValueTree> MagicGUIBuilder::getSelectedNodes() const
{
#if FOLEYS_SHOW_GUI_EDITOR_PALLETTE
    if (magicToolBox)
        return magicToolBox->getSelectedNodes();
#endif

    if (selectedNode.isValid())
        return { selectedNode };

    return {};
}

#endif

#if FOLEYS_SHOW_GUI_EDITOR_PALLETTE

bool MagicGUIBuilder::isNodeSelected (const juce::ValueTree& node) const
{
    if (magicToolBox)
        return magicToolBox->isNodeSelected (node);

    return selectedNode == node;
}

void MagicGUIBuilder::draggedItemOnto (juce::ValueTree dragged, juce::ValueTree target, int index)
{
    if (dragged == target)
        return;

    undo.beginNewTransaction("Drag Item Onto");

    auto targetParent  = target.getParent();
    auto draggedParent = dragged.getParent();

    if (draggedParent.isValid())
        draggedParent.removeChild (dragged, &undo);

    if (targetParent.isValid() != false && index < 0)
        index = targetParent.indexOf (target);

    juce::ValueTree actualParent;

    if (target.getType() == IDs::view)
    {
        target.addChild (dragged, index, &undo);
        actualParent = target;
    }
    else
    {
        targetParent.addChild (dragged, index, &undo);
        actualParent = targetParent;
    }

    // When the parent container uses Contents layout, give newly added children
    // default dimensions so they fill the entire view rather than having no size
    
    if (actualParent.isValid()
        && actualParent.getProperty (IDs::display).toString() == IDs::contents)
    {
        auto* guiItem = findGuiItem (dragged);
        if (guiItem == nullptr || guiItem->isVisibleByDefault())
        {
            if (! dragged.hasProperty (IDs::posX))
                dragged.setProperty (IDs::posX, "25%", &undo);
            if (! dragged.hasProperty (IDs::posY))
                dragged.setProperty (IDs::posY, "25%", &undo);
            if (! dragged.hasProperty (IDs::posWidth))
                dragged.setProperty (IDs::posWidth, "50%", &undo);
            if (! dragged.hasProperty (IDs::posHeight))
                dragged.setProperty (IDs::posHeight, "50%", &undo);
        }
    }
    
    setSelectedNode (dragged);
}

void MagicGUIBuilder::attachToolboxToWindow (juce::Component& window)
{
    juce::Component::SafePointer<juce::Component> reference (&window);

    juce::MessageManager::callAsync ([&, reference]
                                     {
                                         if (reference != nullptr)
                                         {
                                             magicToolBox = std::make_unique<ToolBox>(reference->getTopLevelComponent(), *this);
                                             magicToolBox->setLastLocation (magicState.getResourcesFolder());
                                         }
                                     });
}

ToolBox& MagicGUIBuilder::getMagicToolBox()
{
    // The magicToolBox should always be present!
    // This method wouldn't be included, if
    // FOLEYS_SHOW_GUI_EDITOR_PALLETTE was 0
    jassert (magicToolBox.get() != nullptr);

    return *magicToolBox;
}

void MagicGUIBuilder::restoreZOrderForAll()
{
    std::function<void (juce::ValueTree)> restore = [&] (juce::ValueTree node)
    {
        if (auto* item = findGuiItem (node))
        {
            if (auto* container = dynamic_cast<Container*> (item))
            {
                if (container->getLayoutMode() == LayoutType::Tabbed)
                    container->restoreFromEditing();
            }
        }

        for (int i = 0; i < node.getNumChildren(); ++i)
        {
            if (auto* child = findGuiItem (node.getChild (i)))
                child->toFront (false);
        }

        for (int i = 0; i < node.getNumChildren(); ++i)
            restore (node.getChild (i));
    };

    restore (getGuiRootNode());
}

#endif


} // namespace foleys
