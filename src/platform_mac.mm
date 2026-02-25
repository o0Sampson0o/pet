#include "platform_mac.h"

#ifdef __APPLE__
#import <Cocoa/Cocoa.h>
#import <QuartzCore/QuartzCore.h>

void setupMacWindow(void* windowHandle) {
    NSWindow* nsWindow = (__brigde NSWindow*)windowHandle;
    [nsWindow setTitleVisibility : NSWindowTitleHidden] ;
    [nsWindow setTitlebarAppearsTransparent : YES] ;
    [nsWindow setOpaque : NO] ;
    [nsWindow setBackgroundColor : [NSColor clearColor] ] ;
    [nsWindow setIgnoresMouseEvents : YES] ;
    [nsWindow setLevel : NSFloatingWindowLevel] ;
}
#endif