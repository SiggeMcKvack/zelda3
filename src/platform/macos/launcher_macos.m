// macOS-specific launcher functions
#import <Cocoa/Cocoa.h>

// Force the application to activate and come to front
void ActivateMacOSApp(void) {
    [NSApp activateIgnoringOtherApps:YES];
}
