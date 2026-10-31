/* Apache-2.0. The "ready to start" mark on the import screen's upscaling bar (host_import.m, host_tv_import.m): a line
   across the bar where the main menu's textures are all made, which is where "Start playing" appears, and its label
   under the bar, in the bar's own blue. */
#import <UIKit/UIKit.h>

@interface HaloUpscaleTick : UIView
/* adds the mark over bar, which must not clip its subviews (the label sits under it) */
+ (instancetype)tickOnBar:(UIProgressView *)bar;
/* position: 0 to 1 along the bar, or negative to hide the mark (not known yet) */
- (void)setPosition:(double)position;
/* Space below the bar, including Dynamic Type's label and a gap before the next control. */
- (CGFloat)minimumBottomSpacing;
@end

/* the bar's blue when the upscaling phase may be skipped (the HUD's blue) */
UIColor *halo_upscale_bar_blue(void);
