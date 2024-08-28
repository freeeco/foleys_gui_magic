Changes in this fork (develop branch)
=====================================

Horizontal Meter
----------------

Edited foleys_MagicLevelMeter.cpp so that the meter is displayed horizontally or vertically depending on it's dimensions.

Also foleys_MagicLevelMeter.cpp has a new methods:

'setBarCorner' to change the corner radius of the bar
'setPeakLineThickness' to change the thickness of the peak-hold bar

Setting for these added to the meter's PGM factory

Added a 'flip' propety

Oscilloscope Plot
-----------------

foleys_MagicPlotComponent.cpp - Added a field in PGM editor for the plot component: 'line-width'. Sets the line width of the plot. Adding "%" to the end of the value will set the line to be a percentage of the height of the component.

foleys_MagicOscilloscope.cpp has a new method 'setRate' to change the frequency of the scope.

Added 'Always Plot' setting for RNBO buffers etc

Added a property to scale the plot slightly by the line width to avoid clipping at the edges

Added a property to round corners

Auto Save Frequency
-------------------

Set timer frequency in foleys_ToolBox.cpp. Use preprocessor definition AUTO_SAVE_MINUTES

Default Window Size
-------------------

Added DEFAULT_WINDOW_WIDTH & DEFAULT_WINDOW_HEIGHT directives to foleys_MagicPluginEditor.cpp as some hosts will use these values when first initialising a VST3 and a default preset is not loaded (patchwork & bitwig).


Slider filmstrip maintain aspect ratio
--------------------------------------

Modified foleys_AutoOrientationSlider.h so that filmstrips maintains thier aspect ratio when resized.
Use preprocessor definitions SLIDER_FILMSTRIP_HORIZONTALLY_CENTERED and SLIDER_FILMSTRIP_VERTICALLY_CENTERED to set horizontal and vertical centered justification.
Also added high resampling quality to slider component drawing

GuiItem Opacity Setting
-----------------------

Added an opacity setting to the GuiItems in foleys_MagicJUCEFactories.cpp

Fixed aspect ratio for 'View' items
-----------------------------------

Added 'view-aspect' property to the editor for 'View' items, if set then the items contained in the view will be resized while maintaining a fixed aspect ratio. Edits made to foleys_PropertiesEditor.cpp, foleys_StringDefinitions.h & foleys_Container.cpp

Sidechain The Oscilloscope
--------------------------

Added setSidechainChannel method to foleys_MagicOscilloscope so that the a channel of the buffer sent to the oscilloscope can be used as a 'sync signal' to reset it. Edited foleys_MagicOscilloscope.cpp & foleys_MagicOscilloscope.h

Tooltips Window Access
----------------------

Added getTooltipWindow & setTooltipWindow to foleys_MagicGUIBuilder.cpp (initialised in foleys_RootItem.cpp).

MIDI Learn
----------

foleys_GuiItem.cpp -> Changed the drag and drop hightlighting mode to a semi-transparent circle over the target control

Exposed unmapAllMidiController in foleys_MagicProcessorState.cpp to use when clearing all midi mappings from the plugin.

Application Settings
--------------------
Added stopTimer(), stops hanging when midi learning controls

Settings File / Set Window Size
-------------------------------

Added getFileName() method to foleys_ApplicationSettings.cpp and getApplicationSettingsFile() to foleys_MagicGUIState.cpp

Added settings file loading to foleys_MagicPluginEditor.cpp in order to set the plugin window size from the settings file when the plugin loads.

Changed getOrCreateChildWithName to getChildWithName in a couple of places in foleys_MidiParameterMapper.cpp to avoid spurious entries in the settings file

XML with missing parameters
---------------------------

foleys_MagicProcessorState.cpp -> Changed assertion to DBG when loading xml with missing parameters

Toolbox Palette Default Size
----------------------------

Edited foleys_ToolBox.cpp to add directives for TOOLBOX_X, TOOLBOX_Y, TOOLBOX_WIDTH and TOOLBOX_HEIGHT

Keyboard
--------

In foleys_MagicJUCEFactories.cpp changed keyboard up & down button width for iOS, added key-width setting

Window Resizing
---------------

Added getWindowNeedsUpdate and setWindowNeedsUpdate flag to foleys_MagicGUIState.h to trigger refresh of the window if the window size has been changed from within plugin, the flag is chedcked by a timer in foleys_MagicPluginEditor.cpp

MIDI Keyboard Styling
---------------------

Added 'text-label-color' to the Keyboard GUIItem 

Slider
---------------------

Added 'sensitivity' to the Slider GUIItem 

Added:
```
slider.setWantsKeyboardFocus(false);
slider.setMouseClickGrabsKeyboardFocus(false);
```
to prevent grabbing keyboard focus from on-screen keyboard.

Parameters
---------------------

Names of mssing paramters are printed to console


View Drawing Recursion
----------------------

In foleys_Container.cpp, the line

```
//            childItem->createSubComponents();
```
in Container::createSubComponents

is removed to avoid extra recursion that was slowing down refresing of the GUI

-->

```
    for (auto childNode : configNode)
    {
        auto childItem = magicBuilder.createGuiItem (childNode);
        if (childItem)
        {
            containerBox.addAndMakeVisible (childItem.get());
//            childItem->createSubComponents(); // <--- REMOVE THIS LINE

            children.push_back (std::move (childItem));
        }
    }
```

Also the comment out the last 2 lines of Container::updateLayout() in foleys_Container.cpp at line 324, doing this speeds up opening and resising window and doesn't seem to break anything

```
//    for (auto& child : children)
//        child->updateLayout();
```

createSubComponents() parses the tree and calls createGuiItem() for each child node, which in turn calls createSubComponents() if the child is a view, therefor it seems that the gui is built up recursively like this.

createSubComponents() also calls updateLayout() so removing the lines above avoids unnecessary additional recursion.

Velocity Mode For Sliders
-------------------------

added dynamic-knobs to slider GUI Item in foleys_MagicJUCEFactories.cpp


FileDropContainer for iOS
-------------------------

connected FileDropContainer to peer in constructor of foleys_MagicPluginEditor.cpp for drag and drop in iOS (see https://forum.juce.com/t/drag-and-drop-files-to-juce-component-ios-auv3/42322/18)


WebBrowser GUIItem
------------------

Added url property to WebBrowserItem in foleys_MagicJUCEFactories.cpp


Direct2D renderer selection
----------------------------

Added to timer callback --> 'windows needs updating' in foleys_MagicPluginEditor.cpp


Default margin, padding, radius and background colour
-----------------------------------------------------

Edited defaults in foleys_Decorator.cpp and foleys_Decorator.h


Key commands to nudge position
------------------------------

Added key commands for arrow keys to: foleys_ToolBox.cpp (hold shift when using arrow keys)
Added methods for nudging left, right and up, down to foleys_GuiItem.cpp



Set default windows renderer
----------------------------

In foleys_MagicPluginEditor.cpp add parentHierarchyChanged() method -->

```
#if JUCE_WINDOWS && JUCE_VERSION >= 0x80000
void MagicPluginEditor::parentHierarchyChanged()
{
    if (auto peer = getPeer()){
        if (renderer = 1){
            peer->setCurrentRenderingEngine(1);
        }
        else{
            peer->setCurrentRenderingEngine(0);
        }
    }
}
#endif
```

and in constructor -->

```
#if JUCE_WINDOWS && JUCE_VERSION >= 0x80000
                auto guiNode = tree.getChildWithName ("gui");
                if (guiNode.hasProperty ("windows-renderer"))
                {
                    renderer = guiNode.getProperty ("windows-renderer");
                }
#endif
```


Turn Off OpenGL on Mac / iOS (as native renderer is better)
----------------------------------------------------------

in foleys_MagicPluginEditor.cpp

append after all FOLEYS_ENABLE_OPEN_GL_CONTEXT directives '&& JUCE_WINDOWS'  e.g. -->

```
#if JUCE_MODULE_AVAILABLE_juce_opengl && FOLEYS_ENABLE_OPEN_GL_CONTEXT && JUCE_WINDOWS
    oglContext.attachTo (*this);
#endif
```


Add pass-mouse-clicks propery to container / views
--------------------------------------------------

in foleys_Container.cpp

add to Container::updateLayout() -->

```
    if (magicBuilder.getStyleProperty (IDs::passMouseClicks, configNode)){
        containerBox.setInterceptsMouseClicks(false, true);
    }
```
    
also 
    
to foleys_StringDefinitions.h -->
    
```  
    static juce::Identifier passMouseClicks  { "pass-mouse-clicks" };
```   
     
to foleys_PropertiesEditor.cpp -->

```
    array.add (new StyleBoolPropertyComponent (builder, IDs::passMouseClicks, styleItem));
```



Add buffer-to-image propery to container / views
------------------------------------------------

in foleys_Container.cpp

add to Container::updateLayout() -->

```
    if (magicBuilder.getStyleProperty (IDs::bufferToImage, configNode)){
        containerBox.setBufferedToImage(true);
    }
```
    
also 
    
to foleys_StringDefinitions.h -->
    
```  
    static juce::Identifier bufferToImage  { "buffer-to-image" };
```   
     
to foleys_PropertiesEditor.cpp -->

```
    array.add (new StyleBoolPropertyComponent (builder, IDs::bufferToImage, styleItem));
```


Added single image and SVGs to slider GUI Item
----------------------------------------------
added single image and SVGs to slider GUI Item in foleys_MagicJUCEFactories




Added parameter property to change tabs in 'View'
-------------------------------------------------
Added to Container::updateLayout in foleys_Container.cpp

+

Added     

```
std::unique_ptr<juce::ParameterAttachment> attachment; 
```

to -->
foleys_Container.h



Added value attachments to Midi Drumpad
---------------------------------------
Added to -->

foleys_MagicJUCEFactories.cpp
foleys_MidiDrumpadComponent.cpp
foleys_MidiDrumpadComponent.h



Added svg support to decorator background images
------------------------------------------------
Added backgroundImageSvg -->

```
    if (backgroundImageSvg)
    {
        juce::Graphics::ScopedSaveState save (g);
        g.setOpacity (backgroundAlpha);
        backgroundImageSvg->drawWithin(g, boundsf, backgroundPlacement ,1.0f);
    }
    
    if (backgroundImage.isNull()){
        auto backgroundImageSvgName = stylesheet.getBackgroundImageSvg (node);
        if (backgroundImageSvgName.isNotEmpty()){
            int dataSize = 0;
            const char* data = BinaryData::getNamedResource (backgroundImageSvgName.toRawUTF8(), dataSize);
            if (data != nullptr){
                backgroundImageSvg = juce::Drawable::createFromImageData (data, dataSize);
            }
        }
```

to -->

foleys_Decorator.cpp
foleys_Decorator.h

And added -->

```
juce::String getBackgroundImageSvg (const juce::ValueTree& node) const; 
```

to -->

foleys_Stylesheet.cpp
foleys_Stylesheet.h



Fixed bug when re-ordering items in the GUI Tree browser
--------------------------------------------------------

Items would end up being 1 index to high when reordering by dragging items downwards, fix -->

At line 222 in foleys_GUITreeEditor.cpp replace -->

```
        builder.draggedItemOnto (selectedNode, itemNode, index);
```

with

```
        if (selectedNode.getParent() == itemNode && selectedNode.getParent().indexOf (selectedNode) < index) {
                builder.draggedItemOnto (selectedNode, itemNode, index - 1);
        } else {
            builder.draggedItemOnto (selectedNode, itemNode, index);
        }
```



Added more key-commands
-----------------------

ToolBox::keyPressed At line 328 in foleys_ToolBox.cpp change to -->

```
bool ToolBox::keyPressed (const juce::KeyPress& key)
{
    if ((key.isKeyCode (juce::KeyPress::backspaceKey) || key.isKeyCode (juce::KeyPress::deleteKey)) && !key.getModifiers().isCommandDown())
    {
        auto selected = builder.getSelectedNode();
        if (selected.isValid())
        {
            auto p = selected.getParent();
            if (p.isValid())
                p.removeChild (selected, &undo);
        }

        return true;
    }
    
    if ((key.isKeyCode (juce::KeyPress::backspaceKey) || key.isKeyCode (juce::KeyPress::deleteKey)) && key.getModifiers().isCommandDown())
    {
        auto selected = builder.getSelectedNode();
        if (selected.isValid())
        {
            selected.removeAllChildren (&undo);
        }

        return true;
    }

    if (key.isKeyCode ('Z') && key.getModifiers().isCommandDown())
    {
        if (key.getModifiers().isShiftDown())
            undo.redo();
        else
            undo.undo();

        return true;
    }

    if (key.isKeyCode ('X') && key.getModifiers().isCommandDown())
    {
        auto selected = builder.getSelectedNode();
        if (selected.isValid())
        {
            juce::SystemClipboard::copyTextToClipboard (selected.toXmlString());
            auto p = selected.getParent();
            if (p.isValid())
                p.removeChild (selected, &undo);
        }
        
        return true;
    }

    if (key.isKeyCode ('C') && key.getModifiers().isCommandDown())
    {
        auto selected = builder.getSelectedNode();
        if (selected.isValid())
            juce::SystemClipboard::copyTextToClipboard (selected.toXmlString());

        return true;
    }

    if (key.isKeyCode ('V') && key.getModifiers().isCommandDown())
    {
        auto paste = juce::ValueTree::fromXml (juce::SystemClipboard::getTextFromClipboard());
        auto selected = builder.getSelectedNode();
        if (paste.isValid() && selected.isValid())
            builder.draggedItemOnto (paste, selected);

        return true;
    }
    
    if (key.isKeyCode ('D') && key.getModifiers().isCommandDown())
    {
        auto selected = builder.getSelectedNode();
        auto paste = juce::ValueTree::fromXml (selected.toXmlString());
        if (paste.isValid() && selected.isValid())
            builder.draggedItemOnto (paste, selected.getParent(), selected.getParent().indexOf (selected) + 1);

        return true;
    }
     
    if (key.isKeyCode ('-') && key.getModifiers().isCommandDown())
    {
        auto selected = builder.getSelectedNode();
        if (selected.isValid())
            builder.draggedItemOnto (selected, selected.getParent(), selected.getParent().indexOf (selected) - 1);

        return true;
    }
         
    if (key.isKeyCode ('=') && key.getModifiers().isCommandDown())
    {
        auto selected = builder.getSelectedNode();
        if (selected.isValid())
            builder.draggedItemOnto (selected, selected.getParent(), selected.getParent().indexOf (selected) + 1);

        return true;
    }
    
   if (key.isKeyCode (juce::KeyPress::leftKey))
   {
       auto selected = builder.getSelectedNode();
       auto item = builder.findGuiItem(selected);
       if (item){
           item->nudgeLeft();
           return true;
       }
       else{
           return false;
       }
   }
    
    if (key.isKeyCode (juce::KeyPress::rightKey))
    {
        auto selected = builder.getSelectedNode();
        auto item = builder.findGuiItem(selected);
        if (item){
            item->nudgeRight();
            return true;
        }
        else{
            return false;
        }
    }
             
    if (key.isKeyCode (juce::KeyPress::upKey))
    {
        auto selected = builder.getSelectedNode();
        auto item = builder.findGuiItem(selected);
        if (item){
            item->nudgeUp();
            return true;
        }
        else{
            return false;
        }
    }
             
    if (key.isKeyCode (juce::KeyPress::downKey))
    {
        auto selected = builder.getSelectedNode();
        auto item = builder.findGuiItem(selected);
        if (item){
            item->nudgeDown();
            return true;
        }
        else{
            return false;
        }
    }

    return false;
}
```


All Keyboard-Commands
---------------------

Cut = cmd+X
Copy - cmd+C
Paste - cmd+V
Duplicate - cmd+D
Delete = delete key
Delete Children = cmd+delete key
Move Up Layer - cmd+'='
Move Down Layer' - cmd+'-'
Nudge - shift+arrow keys
Coarse Nudge control+arrow keys
Constrain Vertical - hold shift and drag
Constrain Horizontal - hold control and drag


Fixed visibility property so it is preserved after resizing or closing the GUI
------------------------------------------------------------------------------

in Layout/foleys_GuiItem.h add this at line 263 -->

```
    bool hasVisibilityProperty = false;
``` 
    
in Layout/foleys_GuiItem.cpp change this at line 153 -->

```
    if (! visibilityNode.isVoid()){
        visibility.referTo (magicBuilder.getMagicState().getPropertyAsValue (visibilityNode.toString()));
        hasVisibilityProperty = true;
    } else {
        hasVisibilityProperty = false;
    }
``` 

and this at line 271 at the last line of GuiItem::configureFlexBoxItem -->

``` 
if (hasVisibilityProperty)
        setVisible (visibility.getValue());
``` 

Added 'auto-saved' folder to keep files auto-saved files organised
------------------------------------------------------------------

in /Users/davidalexander/Documents/GitHub/foleys_gui_magic/Editor/foleys_ToolBox.cpp

changed line 530 -->

``` 
 autoSaveFile = lastLocation.getParentDirectory()
                               .getNonexistentChildFile (file.getFileNameWithoutExtension() + ".sav", ".xml");
``` 

to

``` 
    auto autoSaveFileDirectory = lastLocation.getParentDirectory()
            .getChildFile ("auto-saved");
        i
        if (!autoSaveFileDirectory.exists()) {
            const auto result = autoSaveFileDirectory.createDirectory();
            if (result.failed()) {
                DBG("Could not create auto-saved file directory: " + result.getErrorMessage());
                jassertfalse;
            }
        }
        
        autoSaveFile = autoSaveFileDirectory.getNonexistentChildFile (file.getFileNameWithoutExtension() + ".sav", ".xml");
``` 





foleys_gui_magic
===============

This module allows to create GUIs without any coding. It is created with a DOM model
that provides a hierarchical information, and a CSS cascading stylesheet to define
rules for the appearance of the GUI.

There is a drag and drop editor to add GUI elements, and to connect to
parameters of your AudioProcessor. Also an editor in the style of FireBug
to investigate the individual properties, and how they were obtained/calculated.


Support
-------

For the modules of Foleys Finest Audio there is a new forum where you can search for information 
and ask questions: https://forum.foleysfinest.com
All feedback is welcome.

Setup
-----

To use the WYSWYG plugin editor, add this module via Projucer or CMake to your JUCE project.

Instead of inheriting from juce::AudioProcessor inherit foleys::MagicProcessor.
Get rid of those methods:
- bool hasEditor()
- juce::PluginEditor* createEditor()
- void setStateInformation() and void getStateInformation() (optional, a default saving and loading method is provided)

The foleys::MagicProcessor will provide a `foleys::MagicProcessorState magicState` (protected visibility) 
to add visualisations or other objects to expose to the GUI.

The floating editor has an auto save, which needs to know the location of your source files. To activate that
you need to copy this macro into your foleys::MagicProcessor constructor:
```cpp
FOLEYS_SET_SOURCE_PATH(__FILE__);
```
Otherwise autosave will start working once you loaded or saved the state, since then the editor has a location to use.

It is also possible to use the module in an Application. In that case you add a `MagicGuiState` and a `MagicGUIBuilder` yourself.
There is an example available in the examples repository called PlayerExample.


Add Visualisations
------------------

To add visualisations like an Analyser or Oscilloscope to your plugin, add them in the constructor:

```
// Member:
foleys::MagicPlotSource* analyser = nullptr;

// Constructor
analyser = magicState.createAndAddObject<foleys::MagicAnalyser>("input");

// prepareToPlay
analyser->prepareToPlay (sampleRate, samplesPerBlockExpected);

// e.g. in processBlock send the samples to the analyser:
analyser->pushSamples (buffer);
```

In your editor, drag a `Plot` component into the UI and select the name you supplied as `source`, in this
case "input".


Saving and restoring the plugin
-------------------------------

The `foleys::MagicProcessor` takes care of saving and restoring the parameters and properties.
You can add your own values to the ValueTree in the `magicState`.
There is a callback after restoring that you can override, in case you need to do some additional action.

Additionally to saving the values in the plugin state you can have settings across all instances of your plugin.
To use that you need to setup the path to a file. After that the settings are synchronised in the
settings of the magicState:
```
// in constructor
magicState.setApplicationSettingsFile (juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                                       .getChildFile (ProjectInfo::companyName)
                                       .getChildFile (ProjectInfo::projectName + juce::String (".settings")));

// use it like
presetNode = magicState.getSettings().getOrCreateChildWithName ("presets", nullptr);
presetNode.setProperty ("foo", 100, nullptr);
```

Other than the juce::ApplicationSettings this is not a flat list but can deal with hierarchical data in form of ValueTrees.
PluginGuiMagic will deal with interprocess safety and update if the file has changed.

Bake in the xml
---------------

To bake in the GUI save the XML from the PGM panel and add it to the BinaryResources via Projucer.
In the constructor you set the XML file:
```
magicState.setGuiValueTree (BinaryData::magic_xml, BinaryData::magic_xmlSize);
```


Removing the WYSWYG editor
--------------------------

The WYSWYG editor panel (attached outside the PluginEditor) is switched on or off with the config
in Projucer named `FOLEYS_SHOW_GUI_EDITOR_PALLETTE`. An effective way is to add `FOLEYS_SHOW_GUI_EDITOR_PALLETTE=0`
into extra preprocessor defines of the release build, that way you can design in debug mode, and avoid
the WYSWYG editor from being included in the release build for deployment.


Currently available Components
------------------------------

It is completely possible to register your own bespoke Components into the builder. These Components
are already available:

- Slider (attachable to parameters)
- ComboBox ( -"- )
- ToggleButton ( -"- )
- TextButton ( -"- )
- XYDragComponent (attachable to two parameters)
- Plot (displays various 2-d data)
- LevelMeter (displays different RMS / Max levels)
- Label
- MidiKeyboardComponent
- MidiLearn
- ListBox
- WebBrowserComponent

All Components have the option to add margins/paddings and a border also with rounded corners.
The View component serves as container, that has the option to layer all child components on top of each other
or to layout them using flex-box.


Demo projects
-------------

There is a repository with demo projects, that includes this module as git submodule:
[PluginGuiMagic examples](https://github.com/ffAudio/PluginGuiMagic)


Contributing
------------

Everybody is welcome and encouraged to contribute. This is supposed to be helpful for as 
many people as possible, so please give your ideas as github issues and send pull requests.

We might ask you to change things in your pull requests to keep the style consistent and
to keep the API as concise as possible.

We have a repository containing example projects, that are our reference what must not break.
These will be built on our CI for OSX and Windows 10, so we catch hopefully any breakage 
early on. 
Clone that repository using:
```
git clone --recurse-submodules https://github.com/ffAudio/PluginGuiMagic.git
```
To update:
```
git pull origin master
git submodule update
```


Brighton, UK - started Sept. 2019
