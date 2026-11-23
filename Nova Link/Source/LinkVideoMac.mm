#include "LinkVideo.h"
#import <Foundation/Foundation.h>

bool NovaLink::LinkVideo::hostAllowsCamera()
{
    @autoreleasepool
    {
        // Without this key in the host app's Info.plist, macOS kills the app when the
        // camera is opened, so never try in that case.
        return [[NSBundle mainBundle] objectForInfoDictionaryKey: @"NSCameraUsageDescription"] != nil;
    }
}
