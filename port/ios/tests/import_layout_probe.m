/* Apache-2.0. Asset-free UIKit regression app for the production import layout. */
#import <UIKit/UIKit.h>
#import <QuartzCore/QuartzCore.h>
#include <stdarg.h>
#include "../host/host_import.m"

/* host_orientation.m adds categories to SDL classes; the game is absent here. */
@interface SDL_uikitviewcontroller : UIViewController @end
@implementation SDL_uikitviewcontroller @end
@interface SDLLaunchScreenController : UIViewController @end
@implementation SDLLaunchScreenController @end
@interface SDLUIKitSceneDelegate : NSObject @end
@implementation SDLUIKitSceneDelegate @end
void host_logf(int priority, const char *format, ...) { (void)priority; (void)format; }
void host_fatal(const char *format, ...) { (void)format; abort(); }
int xiso_maps_ready(const char *path, char *error, size_t size) { return 0; }
int xiso_extract_maps(const char *path, const char *destination, xiso_progress_proc callback,
    void *context, char *error, size_t size) { abort(); }
void host_astc_store_prepare_import(const char *root, host_astc_progress callback, void *context) { abort(); }
int host_texture_upscale_prepare_import(const char *root, host_texture_upscale_progress callback, void *context) { abort(); }
int host_texture_upscale_first_run_phase(const char *root) { return 0; }
void host_texture_upscale_init(const char *root) {}
int host_texture_upscale_enabled(void) { return 0; }

/* A real simulator window supplies its own notch. Explicit safe guides let the same
   production controller exercise other supported screens without that notch leaking in. */
@interface ProbeViewport : UIView
@property(nonatomic,strong) UILayoutGuide *viewportGuide;
@property(nonatomic,strong) NSArray<NSLayoutConstraint *> *insetConstraints;
@property(nonatomic) UIEdgeInsets testedInsets;
@end
@implementation ProbeViewport
- (instancetype)initWithFrame:(CGRect)frame {
    if ((self=[super initWithFrame:frame])) {
        self.viewportGuide=[UILayoutGuide new];[self addLayoutGuide:self.viewportGuide];
        self.insetConstraints=@[[self.viewportGuide.topAnchor constraintEqualToAnchor:self.topAnchor],
            [self.viewportGuide.leadingAnchor constraintEqualToAnchor:self.leadingAnchor],
            [self.viewportGuide.bottomAnchor constraintEqualToAnchor:self.bottomAnchor],
            [self.viewportGuide.trailingAnchor constraintEqualToAnchor:self.trailingAnchor]];
        [NSLayoutConstraint activateConstraints:self.insetConstraints];
    }
    return self;
}
- (UILayoutGuide *)safeAreaLayoutGuide { return self.viewportGuide?:super.safeAreaLayoutGuide; }
- (UIEdgeInsets)safeAreaInsets { return self.testedInsets; }
- (void)setTestedInsets:(UIEdgeInsets)value {
    _testedInsets=value;
    self.insetConstraints[0].constant=value.top;self.insetConstraints[1].constant=value.left;
    self.insetConstraints[2].constant=-value.bottom;self.insetConstraints[3].constant=-value.right;
    [self setNeedsLayout];
}
@end

@interface ProbeController : HaloImportController @end
@implementation ProbeController
- (void)loadView { self.view=[ProbeViewport new]; }
/* The layout is production code; suppress only the game's scene rotation request. */
- (void)viewDidAppear:(BOOL)animated { (void)animated; }
@end

static NSMutableArray *failures, *results;
static NSString *folder, *caseName;
static void exceptionReport(NSException *exception) {
    NSDictionary *report=@{@"cases":results?:@[],@"passed":@NO,@"failures":@[exception.description?:@"Uncaught exception"]};
    [[NSJSONSerialization dataWithJSONObject:report options:NSJSONWritingPrettyPrinted error:nil]
        writeToFile:[folder stringByAppendingPathComponent:@"report.json"] atomically:YES];
}
static void check(BOOL condition, NSString *message) {
    if (!condition) [failures addObject:[NSString stringWithFormat:@"%@: %@",caseName,message]];
}
static BOOL visible(UIView *view) {
    for (UIView *ancestor=view; ancestor; ancestor=ancestor.superview)
        if (ancestor.hidden || ancestor.alpha<0.01) return NO;
    return YES;
}
static void collect(UIView *view, NSMutableArray *items) {
    if (!visible(view)) return;
    if ([view isKindOfClass:UIButton.class] || [view isKindOfClass:UILabel.class] ||
        [view isKindOfClass:UIImageView.class] || [view isKindOfClass:UIProgressView.class]) {
        [items addObject:view];
        if ([view isKindOfClass:UIButton.class] || [view isKindOfClass:UIImageView.class]) return;
        if ([view isKindOfClass:UIProgressView.class]) {
            for (UIView *child in view.subviews) if ([child isKindOfClass:HaloUpscaleTick.class]) collect(child,items);
            return;
        }
    }
    for (UIView *child in view.subviews) collect(child,items);
}
static UIScrollView *findScroll(UIView *view) {
    if ([view isKindOfClass:UIScrollView.class]) return (id)view;
    for (UIView *child in view.subviews) { UIScrollView *found=findScroll(child); if (found) return found; }
    return nil;
}
static void snapshot(UIView *view, NSString *suffix) {
    UIGraphicsImageRendererFormat *format=[UIGraphicsImageRendererFormat defaultFormat];format.scale=2;
    UIGraphicsImageRenderer *renderer=[[UIGraphicsImageRenderer alloc] initWithSize:view.bounds.size format:format];
    UIImage *image=[renderer imageWithActions:^(UIGraphicsImageRendererContext *context) {
        [view.layer renderInContext:context.CGContext];
    }];
    check([UIImagePNGRepresentation(image) writeToFile:[folder stringByAppendingPathComponent:
        [caseName stringByAppendingFormat:@"%@.png",suffix]] atomically:YES],@"write PNG");
}
static void inspect(ProbeController *controller, BOOL large) {
    UIView *root=controller.view; [root setNeedsLayout]; [root layoutIfNeeded];
    UIScrollView *scroll=findScroll(root);check(scroll!=nil,@"scroll view exists");
    NSMutableArray<UIView *> *items=[NSMutableArray new];collect(root,items);
    BOOL scrolling=scroll.contentSize.height>scroll.bounds.size.height+1;
    check(large || !scrolling,@"default text must fit without scrolling");
    check(UIEdgeInsetsEqualToEdgeInsets(scroll.adjustedContentInset,UIEdgeInsetsZero),@"scroll view does not inherit the host simulator notch");
    CGRect content=CGRectMake(0,0,scroll.contentSize.width,scroll.contentSize.height);
    NSMutableArray *frames=[NSMutableArray new];
    for (UIView *item in items) {
        CGRect rect=[item convertRect:item.bounds toView:scroll];
        NSString *name=item.accessibilityIdentifier?:([item isKindOfClass:UILabel.class]?((UILabel *)item).text:NSStringFromClass(item.class));
        check(rect.size.width>0 && rect.size.height>0,[name stringByAppendingString:@" has area"]);
        check(CGRectContainsRect(CGRectInset(content,-1,-1),rect),[name stringByAppendingString:@" lies inside scroll content"]);
        UILabel *label=[item isKindOfClass:UILabel.class]?(id)item:nil;
        if (label || [item isKindOfClass:UIButton.class])
            check(!item.hasAmbiguousLayout,[name stringByAppendingString:@" layout is unambiguous"]);
        if (label && label.text.length) {
            CGSize needed=[label sizeThatFits:CGSizeMake(label.numberOfLines==1?CGFLOAT_MAX:label.bounds.size.width,CGFLOAT_MAX)];
            check(needed.height<=label.bounds.size.height+1,[name stringByAppendingString:@" text height fits"]);
            check(needed.width<=label.bounds.size.width+1,[name stringByAppendingString:@" text width fits"]);
        }
        if ([item isKindOfClass:UIButton.class]) {
            UIButton *button=(id)item; UILabel *title=button.titleLabel;
            CGSize needed=[title sizeThatFits:CGSizeMake(title.numberOfLines==1?CGFLOAT_MAX:title.bounds.size.width,CGFLOAT_MAX)];
            check(needed.width<=title.bounds.size.width+1,@"button title width fits");
            check(needed.height<=title.bounds.size.height+1,@"button title height fits");
            check(CGRectContainsRect(CGRectInset(button.bounds,-1,-1),[title convertRect:title.bounds toView:button]),@"button title lies inside button");
        }
        [frames addObject:@{@"name":name?:@"",@"rect":NSStringFromCGRect(rect)}];
        if (label && rect.size.height>scroll.bounds.size.height) {
            /* A paragraph may span pages; both ends must be reachable without clipping. */
            for (NSNumber *edge in @[@NO,@YES]) {
                CGRect end=CGRectMake(rect.origin.x,[edge boolValue]?CGRectGetMaxY(rect)-1:rect.origin.y,rect.size.width,1);
                [scroll scrollRectToVisible:end animated:NO];[root layoutIfNeeded];
                check(CGRectContainsRect(CGRectInset(scroll.bounds,-1,-1),end),[name stringByAppendingString:@" text end is reachable"]);
            }
        } else {
            [scroll scrollRectToVisible:rect animated:NO];[root layoutIfNeeded];
            CGRect reached=[item convertRect:item.bounds toView:scroll];
            check(CGRectContainsRect(CGRectInset(scroll.bounds,-1,-1),reached),[name stringByAppendingString:@" is reachable by scrolling"]);
        }
    }
    /* Internal button labels and decorative tick lines are not independent controls. */
    for (NSUInteger i=0;i<items.count;i++) for (NSUInteger j=i+1;j<items.count;j++) {
        UIView *a=items[i],*b=items[j];
        if ([a isDescendantOfView:b] || [b isDescendantOfView:a]) continue;
        CGRect overlap=CGRectIntersection([a convertRect:a.bounds toView:root],[b convertRect:b.bounds toView:root]);
        check(CGRectIsNull(overlap) || overlap.size.width<1 || overlap.size.height<1,
            [NSString stringWithFormat:@"%@ overlaps %@",frames[i][@"name"],frames[j][@"name"]]);
    }
    scroll.contentOffset=CGPointZero;[root layoutIfNeeded];snapshot(root,@"");
    if (scrolling) {
        CGFloat bottom=MAX(0,scroll.contentSize.height-scroll.bounds.size.height),step=MAX(1,scroll.bounds.size.height-32);
        for (CGFloat offset=step;offset<bottom;offset+=step) {
            scroll.contentOffset=CGPointMake(0,offset);[root layoutIfNeeded];
            snapshot(root,[NSString stringWithFormat:@"-scroll-%.0f",offset]);
        }
        scroll.contentOffset=CGPointMake(0,bottom);[root layoutIfNeeded];snapshot(root,@"-bottom");
    }
    [results addObject:@{@"case":caseName,@"scrolling":@(scrolling),@"safeInsets":NSStringFromUIEdgeInsets(root.safeAreaInsets),@"statusFontSize":@(controller.statusLabel.font.pointSize),
        @"textCategory":controller.traitCollection.preferredContentSizeCategory,@"frames":frames}];
}

@interface ProbeScene : UIResponder <UIWindowSceneDelegate>
@property(nonatomic,strong) UIWindow *window;
@property(nonatomic,strong) UIViewController *container;
@property(nonatomic,strong) NSArray *cases;
@property(nonatomic,strong) ProbeController *probe;
@property(nonatomic) BOOL initiallyHidesRetry;
@property(nonatomic) NSUInteger index;
@end
@implementation ProbeScene
- (void)scene:(UIScene *)scene willConnectToSession:(UISceneSession *)session options:(UISceneConnectionOptions *)options {
    failures=[NSMutableArray new];results=[NSMutableArray new];
    BOOL accessibility=[NSProcessInfo.processInfo.arguments containsObject:@"accessibility"];
    folder=[[NSSearchPathForDirectoriesInDomains(NSDocumentDirectory,NSUserDomainMask,YES).firstObject stringByAppendingPathComponent:@"layout"]
        stringByAppendingPathComponent:accessibility?@"accessibility":@"default"];
    [NSFileManager.defaultManager createDirectoryAtPath:folder withIntermediateDirectories:YES attributes:nil error:nil];
    NSSetUncaughtExceptionHandler(exceptionReport);
    self.window=[[UIWindow alloc] initWithWindowScene:(UIWindowScene *)scene];
    self.container=[UIViewController new];self.window.rootViewController=self.container;[self.window makeKeyAndVisible];
    NSMutableArray *cases=[NSMutableArray new];
    NSArray *sizes=@[@[@"se-portrait",@375,@667,@20,@0,@0],@[@"se-landscape",@667,@375,@0,@0,@0],
        @[@"17pro-portrait",@402,@874,@62,@34,@0],@[@"17pro-landscape",@874,@402,@0,@21,@62],
        @[@"mini-landscape",@812,@375,@0,@21,@50],
        @[@"ipad",@768,@1024,@24,@20,@0],@[@"ipad-compact",@320,@480,@24,@20,@0]];
    for (NSArray *size in sizes) for (NSNumber *large in @[@(accessibility)])
        for (NSString *state in @[@"choose",@"importing",@"upscale-menu",@"upscale-skip",@"refusal-both",@"ready"])
            [cases addObject:@{@"size":size,@"large":large,@"state":state}];
    self.cases=cases;dispatch_async(dispatch_get_main_queue(),^{[self next];});
}
- (void)next {
    if (self.index==self.cases.count) {
        NSDictionary *report=@{@"cases":results,@"failures":failures,@"passed":@(failures.count==0),
            @"viewportMethod":@"Production UIKit controller in virtual viewport bounds with an explicit safe-area layout guide; host-notch scroll adjustment disabled",
            @"systemVersion":UIDevice.currentDevice.systemVersion,@"simulatorModel":NSProcessInfo.processInfo.environment[@"SIMULATOR_MODEL_IDENTIFIER"]?:@"unknown"};
        [[NSJSONSerialization dataWithJSONObject:report options:NSJSONWritingPrettyPrinted error:nil]
            writeToFile:[folder stringByAppendingPathComponent:@"report.json"] atomically:YES];return;
    }
    NSDictionary *entry=self.cases[self.index++];NSArray *size=entry[@"size"];BOOL large=[entry[@"large"] boolValue];
    NSString *state=entry[@"state"];caseName=[NSString stringWithFormat:@"%@-%@-%@",size[0],large?@"accessibility":@"default",state];
    check([UIApplication.sharedApplication.preferredContentSizeCategory isEqual:large?UIContentSizeCategoryAccessibilityExtraExtraExtraLarge:UIContentSizeCategoryLarge],
        @"system Dynamic Type category matches test");
    BOOL first=self.probe==nil;
    if (first) {self.probe=[ProbeController new];[self.container addChildViewController:self.probe];}
    ProbeController *controller=self.probe;
    UITraitCollection *traits=[UITraitCollection traitCollectionWithTraitsFromCollections:@[
        [UITraitCollection traitCollectionWithPreferredContentSizeCategory:large?UIContentSizeCategoryAccessibilityExtraExtraExtraLarge:UIContentSizeCategoryLarge],
        [UITraitCollection traitCollectionWithUserInterfaceIdiom:[size[0] hasPrefix:@"ipad"]?UIUserInterfaceIdiomPad:UIUserInterfaceIdiomPhone],
        [UITraitCollection traitCollectionWithHorizontalSizeClass:([size[0] hasPrefix:@"ipad"] && [size[1] doubleValue]>=600)?UIUserInterfaceSizeClassRegular:UIUserInterfaceSizeClassCompact]]];
    [self.container setOverrideTraitCollection:traits forChildViewController:controller];
    controller.view.frame=CGRectMake(0,0,[size[1] doubleValue],[size[2] doubleValue]);
    if (first) {[self.container.view addSubview:controller.view];[controller didMoveToParentViewController:self.container];}
    if(first)self.initiallyHidesRetry=controller.retryButton.hidden;
    findScroll(controller.view).contentInsetAdjustmentBehavior=UIScrollViewContentInsetAdjustmentNever;
    /* Reuse the controller through states, text changes, and rotations. */
    ((ProbeViewport *)controller.view).testedInsets=UIEdgeInsetsMake([size[3] doubleValue],[size[5] doubleValue],
        [size[4] doubleValue],[size[5] doubleValue]);
    controller.importing=NO;controller.upscaling=NO;controller.ready=NO;controller.lastProgressTime=0;
    controller.introLabel.hidden=NO;controller.chooseButton.hidden=NO;
    controller.cancelButton.configuration=nil;[controller.cancelButton setTitle:@"Cancel import" forState:UIControlStateNormal];
    controller.cancelButton.hidden=YES;controller.cancelButton.alpha=1;controller.cancelButton.enabled=YES;
    controller.cancelButton.accessibilityElementsHidden=NO;controller.retryButton.hidden=self.initiallyHidesRetry;
    controller.retryButton.alpha=0;controller.retryButton.enabled=NO;controller.retryButton.accessibilityElementsHidden=YES;
    controller.progress.hidden=YES;controller.progress.alpha=1;[controller.tick setPosition:-1];
    controller.statusLabel.attributedText=nil;
    controller.statusLabel.text=@"Original Xbox PAL or NTSC-US · .iso or .xiso\nNo game files are included.";
    [controller.spinner stopAnimating];
    if (![state isEqual:@"choose"]) {
        controller.introLabel.hidden=YES;controller.chooseButton.hidden=YES;controller.cancelButton.hidden=NO;
        controller.importing=YES;controller.progress.hidden=NO;
        if ([state isEqual:@"importing"]) import_progress((__bridge void *)controller,"a10.map",45,100);
        else {
            /* The normal import prepares device textures before entering upscaling. */
            texture_progress((__bridge void *)controller,100,100);controller.lastProgressTime=0;
            struct host_texture_upscale_import_status status={.fraction=.63,.tick=.28,.menu_phase=[state isEqual:@"upscale-menu"]};
            if (status.menu_phase)status.fraction=.12;
            if ([state isEqual:@"refusal-both"]) {status.message="Not enough storage to upscale textures";
                status.detail="Free up space on this device, then try again. You can start playing now.";status.can_retry=1;}
            if ([state isEqual:@"ready"]) {status.fraction=1;controller.ready=YES;}
            upscale_progress((__bridge void *)controller,&status);
        }
    }
    dispatch_async(dispatch_get_main_queue(),^{
        [controller.view setNeedsLayout];[controller.view layoutIfNeeded];
        check(visible(controller.chooseButton)==[state isEqual:@"choose"],@"Choose XISO visibility matches state");
        check(visible(controller.cancelButton)==(![state isEqual:@"choose"] && ![state isEqual:@"upscale-menu"]),@"Cancel or Start playing visibility matches state");
        check(visible(controller.retryButton)==[state isEqual:@"refusal-both"],@"Try Again visibility matches state");
        if(visible(controller.cancelButton) && ![state isEqual:@"importing"])
            check([controller.cancelButton.titleLabel.text isEqual:@"Start playing"],@"upscale action says Start playing");
        UIEdgeInsets actual=controller.view.safeAreaInsets;
        check(fabs(actual.top-[size[3] doubleValue])<1 && fabs(actual.bottom-[size[4] doubleValue])<1 &&
            fabs(actual.left-[size[5] doubleValue])<1 && fabs(actual.right-[size[5] doubleValue])<1,@"viewport safe insets match test case");
        inspect(controller,large);[self next];
    });
}
@end
@interface ProbeAppDelegate : UIResponder <UIApplicationDelegate> @end
@implementation ProbeAppDelegate @end
int main(int argc,char **argv) { @autoreleasepool { return UIApplicationMain(argc,argv,nil,NSStringFromClass(ProbeAppDelegate.class)); } }
