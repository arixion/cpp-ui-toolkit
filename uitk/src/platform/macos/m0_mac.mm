// Milestone 0 (macOS): borderless, transparent, rounded window with system shadow.
// Compile with: clang++ -framework Cocoa -framework QuartzCore m0_mac.mm -o m0_mac
#import <Cocoa/Cocoa.h>

@interface AppDelegate : NSObject <NSApplicationDelegate>
@property (strong) NSWindow *window;
@end

@implementation AppDelegate
- (void)applicationDidFinishLaunching:(NSNotification *)n {
    NSRect frame = NSMakeRect(200, 200, 900, 600);
    NSUInteger style = NSWindowStyleMaskBorderless;
    self.window = [[NSWindow alloc] initWithContentRect:frame styleMask:style backing:NSBackingStoreBuffered defer:NO];
    self.window.opaque = NO;
    self.window.backgroundColor = [NSColor clearColor];
    self.window.hasShadow = YES;
    self.window.level = NSFloatingWindowLevel;

    NSView *content = [[NSView alloc] initWithFrame:frame];
    content.wantsLayer = YES;
    content.layer.cornerRadius = 16.0;
    content.layer.masksToBounds = YES;
    content.layer.backgroundColor = [[NSColor whiteColor] CGColor];
    self.window.contentView = content;

    [self.window makeKeyAndOrderFront:nil];
}
- (BOOL)applicationShouldTerminateAfterLastWindowClosed:(NSApplication *)sender { return YES; }
@end

int main(int argc, const char *argv[]) {
    @autoreleasepool {
        NSApplication *app = [NSApplication sharedApplication];
        AppDelegate *del = [[AppDelegate alloc] init];
        app.delegate = del;
        [app run];
    }
    return 0;
}
