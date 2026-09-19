
#include <TargetConditionals.h>
#if TARGET_OS_IOS == 1 || TARGET_OS_VISION == 1
#import <UIKit/UIKit.h>
#else
#import <Cocoa/Cocoa.h>
#endif

#define IPLUG_AUVIEWCONTROLLER IPlugAUViewController_vSpectralDistortion
#define IPLUG_AUAUDIOUNIT IPlugAUAudioUnit_vSpectralDistortion
#import <SpectralDistortionAU/IPlugAUViewController.h>
#import <SpectralDistortionAU/IPlugAUAudioUnit.h>

//! Project version number for SpectralDistortionAU.
FOUNDATION_EXPORT double SpectralDistortionAUVersionNumber;

//! Project version string for SpectralDistortionAU.
FOUNDATION_EXPORT const unsigned char SpectralDistortionAUVersionString[];

@class IPlugAUViewController_vSpectralDistortion;
