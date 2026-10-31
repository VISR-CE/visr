/* Apache-2.0. The upscaling bar's "ready to start" mark (host_upscale_tick.h). */
#import "host_upscale_tick.h"

UIColor *halo_upscale_bar_blue(void) { return [UIColor colorWithRed:0.247 green:0.839 blue:1 alpha:1]; }

@implementation HaloUpscaleTick {
    UIView *line;
    UILabel *label;
    double position;
}
+ (instancetype)tickOnBar:(UIProgressView *)bar {
    HaloUpscaleTick *tick=[[HaloUpscaleTick alloc] initWithFrame:bar.bounds];
    tick.autoresizingMask=UIViewAutoresizingFlexibleWidth|UIViewAutoresizingFlexibleHeight;
    tick.userInteractionEnabled=NO;tick.clipsToBounds=NO;tick.hidden=YES;
    tick->position=-1;
    tick->line=[UIView new];tick->line.backgroundColor=halo_upscale_bar_blue();
    tick->line.layer.cornerRadius=1;[tick addSubview:tick->line];
    tick->label=[UILabel new];tick->label.text=@"ready to start";tick->label.textColor=halo_upscale_bar_blue();
#if TARGET_OS_TV
    tick->label.font=[UIFont preferredFontForTextStyle:UIFontTextStyleCaption1];
#else
    tick->label.font=[UIFont preferredFontForTextStyle:UIFontTextStyleCaption2];
    tick->label.adjustsFontForContentSizeCategory=YES;
#endif
    [tick addSubview:tick->label];
    bar.clipsToBounds=NO;[bar addSubview:tick];
    return tick;
}
- (void)setPosition:(double)value {
    if(value==position)return;
    position=value;self.hidden=value<0;
    [self setNeedsLayout];
}
- (CGFloat)minimumBottomSpacing {
#if TARGET_OS_TV
    return 10+10+label.font.lineHeight+10;
#else
    return 5+6+label.font.lineHeight+6;
#endif
}
- (void)layoutSubviews {
    [super layoutSubviews];
    if(position<0)return;
    CGRect bounds=self.bounds;CGFloat scale=self.traitCollection.displayScale?:2;
#if TARGET_OS_TV
    CGFloat width=4,over=10,gap=10;
#else
    CGFloat width=2,over=5,gap=6;
#endif
    /* (on a pixel boundary, so the line stays sharp as it moves) */
    CGFloat x=round((bounds.size.width*MIN(MAX(position,0),1)-width/2)*scale)/scale;
    x=MIN(MAX(x,0),bounds.size.width-width);
    line.frame=CGRectMake(x,-over,width,bounds.size.height+2*over);
    [label sizeToFit];
    CGSize size=label.bounds.size;
    CGFloat left=MIN(MAX(x+width/2-size.width/2,0),MAX(bounds.size.width-size.width,0));
    label.frame=CGRectMake(round(left*scale)/scale,bounds.size.height+over+gap,size.width,size.height);
}
@end
