
#include <TargetConditionals.h>
#if TARGET_OS_IOS == 1 || TARGET_OS_VISION == 1
#import <UIKit/UIKit.h>
#else
#import <Cocoa/Cocoa.h>
#endif

#define IPLUG_AUVIEWCONTROLLER IPlugAUViewController_vOperateursSpectraux
#define IPLUG_AUAUDIOUNIT IPlugAUAudioUnit_vOperateursSpectraux
#import <OperateursSpectrauxAU/IPlugAUViewController.h>
#import <OperateursSpectrauxAU/IPlugAUAudioUnit.h>

//! Project version number for OperateursSpectrauxAU.
FOUNDATION_EXPORT double OperateursSpectrauxAUVersionNumber;

//! Project version string for OperateursSpectrauxAU.
FOUNDATION_EXPORT const unsigned char OperateursSpectrauxAUVersionString[];

@class IPlugAUViewController_vOperateursSpectraux;
