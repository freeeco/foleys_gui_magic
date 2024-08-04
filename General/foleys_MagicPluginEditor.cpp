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

#include "foleys_MagicPluginEditor.h"
#include "foleys_StringDefinitions.h"
#include "../State/foleys_MagicProcessorState.h"


namespace foleys
{

MagicPluginEditor::MagicPluginEditor (MagicProcessorState& stateToUse, std::unique_ptr<MagicGUIBuilder> builderToUse)
  : juce::AudioProcessorEditor (*stateToUse.getProcessor()),
    processorState (stateToUse),
    builder (std::move (builderToUse))
{
#if JUCE_MODULE_AVAILABLE_juce_opengl && FOLEYS_ENABLE_OPEN_GL_CONTEXT && JUCE_WINDOWS
    oglContext.attachTo (*this);
#endif

    if (builder.get() == nullptr)
    {
        builder = std::make_unique<MagicGUIBuilder>(processorState);
        builder->registerJUCEFactories();
        builder->registerJUCELookAndFeels();
    }

    auto guiTree = processorState.getGuiTree();
    if (guiTree.isValid())
        setConfigTree (guiTree);

    // Get and store initial window size and Windows renderer the first time plugin is opened
    
    if (!processorState.getWindowSizeInitialized()){
        
        const auto rootNode = builder->getGuiRootNode();
        
#if defined DEFAULT_WINDOW_WIDTH
        int width = rootNode.getProperty (IDs::width, DEFAULT_WINDOW_WIDTH);
#else
        int width = rootNode.getProperty (IDs::width, 600);
#endif
        
#if defined DEFAULT_WINDOW_HEIGHT
        int height = rootNode.getProperty (IDs::height, DEFAULT_WINDOW_HEIGHT);
#else
        int height = rootNode.getProperty (IDs::height, 400);
#endif
        
        auto settingsFile = processorState.getApplicationSettingsFile();
        auto stream = settingsFile.createInputStream();
        if (stream.get() != nullptr)
        {
            auto tree = juce::ValueTree::fromXml (stream->readEntireStreamAsString());
            if (tree.isValid())
            {
                auto sizeNode = tree.getChildWithName (foleys::IDs::lastSize);
                if (sizeNode.hasProperty (foleys::IDs::width) && sizeNode.hasProperty (foleys::IDs::height) )
                {
                    width  = sizeNode.getProperty (foleys::IDs::width);
                    height = sizeNode.getProperty (foleys::IDs::height);
                }
#if JUCE_WINDOWS && JUCE_VERSION >= 0x80000
                auto guiNode = tree.getChildWithName ("gui");
                if (guiNode.hasProperty ("windows-renderer"))
                {
                    if (guiNode.getProperty("windows-renderer").toString() == "1") {
                        renderer = 1;
                    }
                    else {
                        renderer = 0;
                    }
                }
#endif
            }
        }
        processorState.setLastEditorSize (width, height);
        
#if JUCE_WINDOWS && JUCE_VERSION >= 0x80000
        processorState.setRenderer (renderer);
#endif
    }

    updateSize();

#if FOLEYS_SHOW_GUI_EDITOR_PALLETTE
    if (!guiTree.isValid() && processorState.getValueTree().isValid())
        processorState.getValueTree().addChild (builder->getConfigTree(), -1, nullptr);

    builder->attachToolboxToWindow (*this);
#endif
    

#if !JUCE_IOS
    startTimerHz(40);
#endif
    
#if JUCE_IOS
    dropContainer.setParentComponent(getTopLevelComponent());
#endif
}

MagicPluginEditor::~MagicPluginEditor()
{
#if JUCE_MODULE_AVAILABLE_juce_opengl && FOLEYS_ENABLE_OPEN_GL_CONTEXT && JUCE_WINDOWS
    oglContext.detach();
#endif
}

void MagicPluginEditor::updateSize()
{
    int width, height;
    if(!processorState.getLastEditorSize (width, height))
        return;

    bool resizable = builder->getStyleProperty (IDs::resizable, builder->getGuiRootNode());
    bool resizeCorner = builder->getStyleProperty (IDs::resizeCorner, builder->getGuiRootNode());

#if JUCE_IOS
    resizable = false;
    resizeCorner = false;
#endif

    if (resizable)
    {
        
        const auto rootNode = builder->getGuiRootNode();
        
        auto maximalBounds = juce::Desktop::getInstance().getDisplays().getTotalBounds (true);
        int minWidth = rootNode.getProperty (IDs::minWidth, 10);
        int minHeight = rootNode.getProperty (IDs::minHeight, 10);
        int maxWidth = rootNode.getProperty (IDs::maxWidth, maximalBounds.getWidth());
        int maxHeight = rootNode.getProperty (IDs::maxHeight, maximalBounds.getHeight());
        double aspect = rootNode.getProperty (IDs::aspect, 0.0);
#if JUCE_WINDOWS && JUCE_VERSION >= 0x80000
        // Disable corner resizer in Ablton Live if using Direct2D to avoid freezes
        auto hostType = juce::PluginHostType();
        processorState.getRenderer (renderer);
        if (renderer == 1 && hostType.isAbletonLive()){
            setResizable (false, false);
        }
        else{
            setResizable (resizable, resizeCorner);
        }
#else
        setResizable (resizable, resizeCorner);
#endif
        setResizeLimits (minWidth, minHeight, maxWidth, maxHeight);
        if (aspect > 0.0)
            getConstrainer()->setFixedAspectRatio (aspect);
    }

    setSize (width, height);
}

void MagicPluginEditor::setConfigTree (const juce::ValueTree& config)
{
    jassert (config.isValid() && config.hasType(IDs::magic));

    // Set default values
    auto rootNode = config.getChildWithName (IDs::view);

    if (rootNode.isValid())
    {
        auto& undo = builder->getUndoManager();
        if (!rootNode.hasProperty(IDs::resizable)) rootNode.setProperty (IDs::resizable, true, &undo);
        if (!rootNode.hasProperty(IDs::resizeCorner)) rootNode.setProperty (IDs::resizeCorner, true, &undo);
    }

    builder->createGUI (*this);
    updateSize();
}

MagicGUIBuilder& MagicPluginEditor::getGUIBuilder()
{
    // This shouldn't be possible, since the builder instance is created if none was supplied...
    jassert (builder);

    return *builder;
}

void MagicPluginEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colours::black);
}

void MagicPluginEditor::resized()
{
    builder->updateLayout (getLocalBounds());
    processorState.setLastEditorSize (getWidth(), getHeight());
}

#if JUCE_WINDOWS && JUCE_VERSION >= 0x80000
void MagicPluginEditor::parentHierarchyChanged()
{
    processorState.getRenderer (renderer);
    if (auto peer = getPeer()){
        if (renderer == 1){
            peer->setCurrentRenderingEngine(1);
#if JUCE_MODULE_AVAILABLE_juce_opengl && FOLEYS_ENABLE_OPEN_GL_CONTEXT && JUCE_WINDOWS
            oglContext.detach();
#endif
        }
        else{
            peer->setCurrentRenderingEngine(0);
#if JUCE_MODULE_AVAILABLE_juce_opengl && FOLEYS_ENABLE_OPEN_GL_CONTEXT && JUCE_WINDOWS
            oglContext.attachTo (*this);
#endif
        }
    }
}
#endif

#if !JUCE_IOS
void MagicPluginEditor::timerCallback()
{
    if (processorState.getWindowNeedsUpdate()){
        updateSize();
        builder->updateLayout (getLocalBounds());
        processorState.setLastEditorSize (getWidth(), getHeight());
    }
}
#endif

} // namespace foleys
