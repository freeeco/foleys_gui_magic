/*
  ==============================================================================

    ios_FileDropContainer.h
    
    An iOS-specific singleton class that can be used for
    transfering native Drag & Drop interaction callbacks to FileDragAndDropTarget components.

    Author:  Ievgen Ivanchenko

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>


namespace juce
{

#if JUCE_IOS

class FileDropContainer : public DeletedAtShutdown
                                
{
public:
    using Targets = Array<WeakReference<Component>>;
    
    JUCE_DECLARE_SINGLETON (FileDropContainer, false)
    
    void setParentComponent (Component* parentComponent);
    
private:
     
    FileDropContainer();
    ~FileDropContainer();

    Component::SafePointer<Component> parentComponent;
    
    struct Pimpl : public ComponentListener
    {
        virtual ~Pimpl() {}
    };
    std::unique_ptr<Pimpl> pimpl;
    
    class FileDropNativeImpl;
    friend class FileDropNativeImpl;
};

#endif

}

