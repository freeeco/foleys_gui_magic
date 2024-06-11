/*
  ==============================================================================

    ios_FileDropContainer.mm
    
    An iOS-specific singleton class that can be used for
    transfering native Drag & Drop interaction callbacks to FileDragAndDropTarget components.
    
    Author:  Ievgen Ivanchenko

  ==============================================================================
*/




#import <UIKit/UIKit.h>
#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>

#include "ios_FileDropContainer.h"

#include <../../JUCE/modules/juce_core/native/juce_BasicNativeHeaders.h>
#include <../../JUCE/modules/juce_core/native/juce_mac_ObjCHelpers.h>
#include <../../JUCE/modules/juce_graphics/native/juce_mac_CoreGraphicsHelpers.h>



namespace juce
{

#if JUCE_IOS

static NSString * const DropItemUTTypeIdentifier = (NSString *)kUTTypeAudio;
static const String bookmarkStringPrefix = "BMARK";

inline String convertBookmarkDataToString (const NSData* nsBookmark)
{
    return bookmarkStringPrefix + Base64::toBase64 ([nsBookmark bytes], static_cast<size_t>([nsBookmark length]));
}

class FileDropContainer::FileDropNativeImpl : public FileDropContainer::Pimpl
{
public:
    
    FileDropNativeImpl() = default;
    ~FileDropNativeImpl()
    {
        if (view && dropInteraction)
            [view removeInteraction:dropInteraction.get()];
    }
    
    void componentParentHierarchyChanged (Component& parent) override
    {
        if (auto* componentPeer = parent.getPeer())
        {
            parentComponent = &parent;
            peer = componentPeer;
            view = (UIView *)componentPeer->getNativeHandle();
            
            if (view)
            {
                static DropInteractionDelegateClass delegateClass;
                JUCE_BEGIN_IGNORE_WARNINGS_GCC_LIKE ("-Wobjc-method-access")
                NSObject<UIDropInteractionDelegate>* delegate = [delegateClass.createInstance() initWithContainer:this];
                JUCE_END_IGNORE_WARNINGS_GCC_LIKE
                
                dropInteraction.reset([[UIDropInteraction alloc] initWithDelegate:delegate]);
                dropInteraction.get().allowsSimultaneousDropSessions = NO;
                
                [view addInteraction: dropInteraction.get()];
                
                fileCoordinator.reset ([[NSFileCoordinator alloc] initWithFilePresenter:nil]);
            }
        }
    }
    
    void updateArrayOfFiles (NSArray<UIDragItem *>* items)
    {
        draggedItems.clearQuick();
        for (UIDragItem* item in items)
            draggedItems.add(nsStringToJuce([[item itemProvider] suggestedName]));
    }
    
private:
    
    class DropInteractionDelegateClass : public ObjCClass<NSObject<UIDropInteractionDelegate>>
    {
    public:
        DropInteractionDelegateClass() : ObjCClass<NSObject<UIDropInteractionDelegate>> ("DropInteractionDelegateClass_")
        {
            addIvar<FileDropContainer::FileDropNativeImpl*>("nativeContainer");
            
            JUCE_BEGIN_IGNORE_WARNINGS_GCC_LIKE ("-Wundeclared-selector")
            addMethod (@selector (initWithContainer:), initWithContainer);
            JUCE_END_IGNORE_WARNINGS_GCC_LIKE
            
            addMethod (@selector(dropInteraction:canHandleSession:), canHandleSession);
            addMethod (@selector(dropInteraction:sessionDidEnter:),  sessionDidEnter);
            addMethod (@selector(dropInteraction:sessionDidUpdate:), sessionDidUpdate);
            addMethod (@selector(dropInteraction:sessionDidExit:),   sessionDidExit);
            addMethod (@selector(dropInteraction:performDrop:),      performDrop);
            
            registerClass();
        }
        
        static id initWithContainer (id _self, SEL, FileDropContainer::FileDropNativeImpl* container)
        {
            NSObject* self = sendSuperclassMessage<NSObject*> (_self, @selector (init));
            object_setInstanceVariable (self, "nativeContainer", container);
            return self;
        }
        
        static bool canHandleSession (id self, SEL, UIDropInteraction*, id<UIDropSession> session)
        {
            return [session.items.firstObject.itemProvider hasItemConformingToTypeIdentifier:DropItemUTTypeIdentifier];
        }
        
        static void sessionDidEnter (id self, SEL, UIDropInteraction* interaction, id<UIDropSession> session)
        {
            auto* const container = getIvar<FileDropContainer::FileDropNativeImpl*> (self, "nativeContainer");
            container->location = roundToIntPoint([session locationInView:container->view]);
            
            container->updateArrayOfFiles ([session items]);
            container->peer->handleDragMove ({ container->draggedItems, {}, container->location });
        }
        
        static UIDropProposal* sessionDidUpdate (id self, SEL, UIDropInteraction*, id<UIDropSession> session)
        {
            auto* const container = getIvar<FileDropContainer::FileDropNativeImpl*> (self, "nativeContainer");
            container->location = roundToIntPoint([session locationInView:container->view]);
            
            if (container->draggedItems.size() != [[session items] count])
                container->updateArrayOfFiles ([session items]);
            
            container->peer->handleDragMove ({ container->draggedItems, {}, container->location });
            
            return [[UIDropProposal alloc] initWithDropOperation:UIDropOperationCopy];
        }
        
        static void sessionDidExit (id self, SEL, UIDropInteraction*, id<UIDropSession> session)
        {
            auto* const container = getIvar<FileDropContainer::FileDropNativeImpl*> (self, "nativeContainer");
            container->location = roundToIntPoint([session locationInView:container->view]);
            
            container->peer->handleDragExit ({ container->draggedItems, {}, container->location });
            container->draggedItems.clearQuick();
        }
        
        static void performDrop (id self, SEL, UIDropInteraction* interaction, id<UIDropSession> session)
        {
            auto* const container = getIvar<FileDropContainer::FileDropNativeImpl*> (self, "nativeContainer");
            container->location = roundToIntPoint([session locationInView:container->view]);
            
            container->loadedDragItems = 0;
            container->droppedDragItems = (uint16)[[session items] count];
            
            dispatch_group_t group = dispatch_group_create();
            
            [[session items] enumerateObjectsUsingBlock:^(UIDragItem* item, NSUInteger index, BOOL*)
             {
                NSItemProvider* provider = [item itemProvider];
                if ([provider hasItemConformingToTypeIdentifier:DropItemUTTypeIdentifier])
                {
                    dispatch_group_enter( group );
                    
                    [provider loadItemForTypeIdentifier:DropItemUTTypeIdentifier
                                                options:nil
                                      completionHandler:^(NSURL *url, NSError *error) {
                        
                        if (error != nil)
                        {
                            [[maybe_unused]] auto desc = [error localizedDescription];
                            jassertfalse;
                            ++container->loadedDragItems;
                            return;
                        }
                        
                        dispatch_group_enter( group );
                        
                        [container->fileCoordinator.get() coordinateReadingItemAtURL: url
                                                                             options: NSFileCoordinatorReadingWithoutChanges
                                                                               error: &error
                                                                          byAccessor:^(NSURL *newURL)
                         {
                            bool securityAccessSucceeded = [newURL startAccessingSecurityScopedResource];
                            
                            NSError* error = nil;
                            NSData* bookmark = [url bookmarkDataWithOptions: 0
                                             includingResourceValuesForKeys: nil
                                                              relativeToURL: nil
                                                                      error: &error];
                            
                            if (error == nil)
                            {
                                [bookmark retain];
                                container->draggedItems.set ((int)index, convertBookmarkDataToString(bookmark));
                            }
                            else
                            {
                                [[maybe_unused]] auto desc = [error localizedDescription];
                                jassertfalse;
                            }
                            
                            if (securityAccessSucceeded)
                                [newURL stopAccessingSecurityScopedResource];
                            
                            dispatch_group_leave( group );
                        }];
                        
                        if (error != nil)
                        {
                            [[maybe_unused]] auto desc = [error localizedDescription];
                            jassertfalse;
                        }
                        
                        dispatch_group_leave( group );
                    }];
                }
                
                dispatch_group_notify (group, dispatch_get_main_queue(),
                                       ^{
                    container->peer->handleDragDrop ({ container->draggedItems, {}, container->location });
                });
                
            }];
        }
    };
    
    // Implementation
    
    Component* parentComponent;
    ComponentPeer* peer;
    UIView* view;
    
    std::unique_ptr<UIDropInteraction, NSObjectDeleter> dropInteraction;
    std::unique_ptr<NSFileCoordinator, NSObjectDeleter> fileCoordinator;
    
    // Session
    
    StringArray draggedItems;
    juce::Point<int> location;
    
    uint16 droppedDragItems;
    std::atomic<uint16> loadedDragItems;
    
    JUCE_DECLARE_NON_COPYABLE(FileDropNativeImpl)
};

//==============================================================================

JUCE_IMPLEMENT_SINGLETON (FileDropContainer)

FileDropContainer::FileDropContainer() {}
FileDropContainer::~FileDropContainer()
{
    clearSingletonInstance();
}

void FileDropContainer::setParentComponent(Component* parentComponentToUse)
{
    jassert(parentComponentToUse);
    if (parentComponent == parentComponentToUse) return;
    
    if (parentComponent && pimpl)
        parentComponentToUse->removeComponentListener (pimpl.get());
    
    pimpl.reset (new FileDropNativeImpl);
    parentComponent = parentComponentToUse;
    parentComponent->addComponentListener (pimpl.get());
}

#endif

}
