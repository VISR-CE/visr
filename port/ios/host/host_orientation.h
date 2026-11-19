#pragma once
#import <UIKit/UIKit.h>

/* Keep launch, import, Files, and SDL on the same orientation policy. */
@interface HaloLandscapeController : UIViewController
@end

@interface HaloLandscapeDocumentPicker : UIDocumentPickerViewController
@end

void host_ios_require_landscape(UIWindow *window);

/* Called by host_main.m after the device profile is installed. */
void host_ios_orientation_configure(void);
BOOL host_ios_allows_portrait(void);
/* The orientation a presented screen starts in: the game scene's current one
   when portrait is allowed, landscape otherwise. */
UIInterfaceOrientation host_ios_presentation_orientation(void);
