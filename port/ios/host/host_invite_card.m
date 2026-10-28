/* The invite over the game's window, including visionOS theater mode.
   p2p.c sends changes from its locked network thread; only copied strings
   cross onto the main thread, never guest pointers or synchronous UIKit work. */
#import <UIKit/UIKit.h>
#include <TargetConditionals.h>
#include <SDL3/SDL.h>
#include <stdlib.h>
#include <string.h>
#include "ios_host.h"
#include "posix.h"
#import "host_qr.h"

/* Match host_benchmark_menu.m's palette and buttons, and the report overlay's
   rounded panel in host_benchmark_page.m. */
static UIColor *inviteColor(unsigned rgb) {
    return [UIColor colorWithRed:((rgb >> 16) & 255) / 255.0 green:((rgb >> 8) & 255) / 255.0
        blue:(rgb & 255) / 255.0 alpha:1];
}

#if TARGET_OS_TV
/* Like HaloTVMenuButton, keep the menu colors instead of tvOS's white plate. */
@interface HaloTVInviteButton : UIButton
@end
@implementation HaloTVInviteButton
- (CGSize)intrinsicContentSize {
    CGSize size = [super intrinsicContentSize];
    /* HaloTVMenuButton's 14-by-32 padding without deprecated contentEdgeInsets. */
    return CGSizeMake(size.width + 64, size.height + 28);
}
- (void)updateInviteAppearance {
    self.backgroundColor = inviteColor(self.focused ? 0x3fd6ff : 0x1d3640);
    UIColor *ink = inviteColor(self.focused ? 0x081216 : 0xe8f6fa);
    [self setTitleColor:ink forState:UIControlStateNormal];
    [self setTitleColor:ink forState:UIControlStateFocused];
}
- (void)layoutSubviews {
    [super layoutSubviews];
    self.layer.cornerRadius = self.bounds.size.height / 2;
    [self updateInviteAppearance];
}
- (void)didUpdateFocusInContext:(UIFocusUpdateContext *)context withAnimationCoordinator:(UIFocusAnimationCoordinator *)coordinator {
    [super didUpdateFocusInContext:context withAnimationCoordinator:coordinator];
    [coordinator addCoordinatedAnimations:^{
        [self updateInviteAppearance];
        self.transform = self.focused && !UIAccessibilityIsReduceMotionEnabled() ? CGAffineTransformMakeScale(1.05, 1.05) : CGAffineTransformIdentity;
        self.layer.shadowOpacity = self.focused ? 0.4 : 0;
        self.layer.shadowRadius = 12;
        self.layer.shadowOffset = CGSizeMake(0, 8);
    } completion:nil];
}
@end
#endif

@interface HaloInviteCardController : UIViewController
@property(nonatomic, copy) NSString *link, *code;
@property(nonatomic, strong) UIImageView *qr;
@property(nonatomic, strong) UILabel *codeLabel;
@property(nonatomic, strong) UIButton *doneButton;
@property(nonatomic, strong) UIStackView *stack;
@property(nonatomic) BOOL preview;
@property(nonatomic, strong) UIScene *scene;
- (void)refresh;
- (void)done;
@end

static UIWindow *invite_window;

@implementation HaloInviteCardController
#if !TARGET_OS_TV
- (UIInterfaceOrientationMask)supportedInterfaceOrientations { return UIInterfaceOrientationMaskLandscape; }
- (UIInterfaceOrientation)preferredInterfaceOrientationForPresentation { return UIInterfaceOrientationLandscapeRight; }
- (BOOL)prefersInterfaceOrientationLocked {
    return UIInterfaceOrientationIsLandscape(self.view.window.windowScene.interfaceOrientation);
}
#endif
- (UILabel *)label:(UIFontTextStyle)style color:(UIColor *)color {
    UILabel *label = [UILabel new];
    label.font = [UIFont preferredFontForTextStyle:style];
    label.textColor = color;
    label.textAlignment = NSTextAlignmentCenter;
    label.numberOfLines = 0;
    return label;
}
- (UIButton *)button:(NSString *)title action:(SEL)action {
#if TARGET_OS_TV
    UIButton *button = [HaloTVInviteButton buttonWithType:UIButtonTypeCustom];
#else
    UIButton *button = [UIButton buttonWithType:UIButtonTypeSystem];
    BOOL primary = action == @selector(share);
    UIButtonConfiguration *configuration = primary ? UIButtonConfiguration.filledButtonConfiguration : UIButtonConfiguration.borderedButtonConfiguration;
    configuration.baseBackgroundColor = inviteColor(primary ? 0x3fd6ff : 0x1d3640);
    configuration.baseForegroundColor = inviteColor(primary ? 0x081216 : 0xe8f6fa);
    configuration.background.backgroundColor = configuration.baseBackgroundColor;
    configuration.background.strokeColor = inviteColor(0x1d3640);
    configuration.background.strokeWidth = 1;
    configuration.background.visualEffect = nil;
    configuration.contentInsets = NSDirectionalEdgeInsetsMake(14, 32, 14, 32);
    configuration.titleTextAttributesTransformer = ^NSDictionary<NSAttributedStringKey,id> *(NSDictionary<NSAttributedStringKey,id> *attributes) {
        NSMutableDictionary *result = attributes.mutableCopy;
        result[NSFontAttributeName] = [UIFont preferredFontForTextStyle:UIFontTextStyleHeadline];
        return result;
    };
    button.configuration = configuration;
#endif
    button.tintColor = inviteColor(0x3fd6ff);
    button.titleLabel.font = [UIFont preferredFontForTextStyle:UIFontTextStyleHeadline];
    [button setTitle:title forState:UIControlStateNormal];
    [button addTarget:self action:action forControlEvents:UIControlEventPrimaryActionTriggered];
    [button.heightAnchor constraintGreaterThanOrEqualToConstant:44].active = YES;
    return button;
}
- (void)viewDidLoad {
    [super viewDidLoad];
    self.overrideUserInterfaceStyle = UIUserInterfaceStyleDark;
    self.view.backgroundColor = inviteColor(0x081216);
    for (NSNotificationName name in @[UISceneDidEnterBackgroundNotification, UISceneDidDisconnectNotification])
        [NSNotificationCenter.defaultCenter addObserver:self selector:@selector(sceneEnded:) name:name object:nil];
    self.qr = [UIImageView new];
    self.qr.backgroundColor = UIColor.whiteColor;
    self.qr.contentMode = UIViewContentModeScaleAspectFit;
    self.qr.layer.magnificationFilter = kCAFilterNearest;
    self.qr.layer.minificationFilter = kCAFilterNearest;
    self.qr.isAccessibilityElement = YES;
    self.qr.accessibilityLabel = @"Invite QR code";
    UIStackView *details = [UIStackView new];
    details.axis = UILayoutConstraintAxisVertical;
    details.alignment = UIStackViewAlignmentFill;
    details.spacing = 16;
    UILabel *title = [self label:UIFontTextStyleTitle1 color:inviteColor(0xe8f6fa)];
    title.text = @"Invite";
    [details addArrangedSubview:title];
    self.codeLabel = [self label:UIFontTextStyleTitle2 color:inviteColor(0xe8f6fa)];
    self.codeLabel.font = [UIFont monospacedSystemFontOfSize:self.codeLabel.font.pointSize weight:UIFontWeightMedium];
    [self.codeLabel setContentCompressionResistancePriority:UILayoutPriorityRequired forAxis:UILayoutConstraintAxisVertical];
    UIStackView *invite = [[UIStackView alloc] initWithArrangedSubviews:@[self.qr, self.codeLabel]];
    invite.axis = UILayoutConstraintAxisVertical;
    invite.alignment = UIStackViewAlignmentFill;
    invite.spacing = 12;
#if !TARGET_OS_TV
    [details addArrangedSubview:[self button:@"Share…" action:@selector(share)]];
#endif
    [details addArrangedSubview:[self button:@"Copy link" action:@selector(copyLink)]];
    self.doneButton = [self button:@"Done" action:@selector(done)];
    [details addArrangedSubview:self.doneButton];
    self.stack = [[UIStackView alloc] initWithArrangedSubviews:@[invite, details]];
    self.stack.axis = UILayoutConstraintAxisHorizontal;
    self.stack.alignment = UIStackViewAlignmentCenter;
    self.stack.spacing = 24;
    self.stack.backgroundColor = [UIColor.blackColor colorWithAlphaComponent:0.78];
    self.stack.layer.cornerRadius = 10;
    self.stack.layoutMargins = UIEdgeInsetsMake(10, 12, 10, 12);
    self.stack.layoutMarginsRelativeArrangement = YES;
    self.stack.translatesAutoresizingMaskIntoConstraints = NO;
    [self.view addSubview:self.stack];
    UILayoutGuide *safe = self.view.safeAreaLayoutGuide;
    NSLayoutConstraint *preferredSize = [self.qr.widthAnchor constraintEqualToConstant:400];
    preferredSize.priority = UILayoutPriorityDefaultHigh;
    [NSLayoutConstraint activateConstraints:@[
        [self.stack.centerXAnchor constraintEqualToAnchor:safe.centerXAnchor],
        [self.stack.centerYAnchor constraintEqualToAnchor:safe.centerYAnchor],
        [self.stack.leadingAnchor constraintGreaterThanOrEqualToAnchor:safe.leadingAnchor constant:20],
        [self.stack.trailingAnchor constraintLessThanOrEqualToAnchor:safe.trailingAnchor constant:-20],
        [self.stack.topAnchor constraintGreaterThanOrEqualToAnchor:safe.topAnchor constant:20],
        [self.stack.bottomAnchor constraintLessThanOrEqualToAnchor:safe.bottomAnchor constant:-20],
        [self.qr.widthAnchor constraintEqualToAnchor:self.qr.heightAnchor],
        [self.qr.widthAnchor constraintLessThanOrEqualToAnchor:safe.widthAnchor multiplier:0.5 constant:-32],
        [self.qr.heightAnchor constraintLessThanOrEqualToAnchor:safe.heightAnchor constant:-110],
        [invite.heightAnchor constraintLessThanOrEqualToAnchor:safe.heightAnchor constant:-40],
        preferredSize]];
    [self refresh];
}
- (void)dealloc { [NSNotificationCenter.defaultCenter removeObserver:self]; }
- (void)sceneEnded:(NSNotification *)note { if (note.object == self.scene) [self done]; }
- (NSArray<id<UIFocusEnvironment>> *)preferredFocusEnvironments { return @[self.doneButton]; }
- (void)refresh {
    if (!self.isViewLoaded) return;
    self.qr.image = host_qr_image(self.link.uppercaseString, 12, 4);
    self.codeLabel.text = self.code.length ? self.code : @"Join code unavailable";
}
- (void)share {
    posix_share_link(self.link.UTF8String);
}
- (void)copyLink {
    SDL_SetClipboardText(self.link.UTF8String);
}
- (void)done {
    if (invite_window.rootViewController != self) return;
    /* A share sheet's previous key window is this card. Put it away first
       so it cannot restore focus to a card whose host has stopped. */
    host_dismiss_share_sheet();
    UIWindow *window = invite_window;
    invite_window = nil;
    host_dismiss_window(window);
    host_present_input_update();
}
- (NSArray<UIKeyCommand *> *)keyCommands {
    return @[[UIKeyCommand keyCommandWithInput:UIKeyInputEscape modifierFlags:0 action:@selector(done)]];
}
- (void)pressesBegan:(NSSet<UIPress *> *)presses withEvent:(UIPressesEvent *)event {
    for (UIPress *press in presses)
        if (press.type == UIPressTypeMenu) { [self done]; return; }
    [super pressesBegan:presses withEvent:event];
}
@end

int host_invite_card_up(void) { return invite_window != nil; }

int posix_show_invite(const char *link, const char *code) {
    if (!NSThread.isMainThread || !link || !*link) return 0;
    if (invite_window) {
        HaloInviteCardController *card = (HaloInviteCardController *)invite_window.rootViewController;
        card.link = @(link);
        card.code = @(code ?: "");
        card.preview = NO;
        [card refresh];
        return 1;
    }
    if (host_window_up()) return 0;
    UIWindow *window = host_present_window();
    if (!window) return 0;
    HaloInviteCardController *card = [HaloInviteCardController new];
    card.link = @(link);
    card.code = @(code ?: "");
    card.scene = window.windowScene;
    window.rootViewController = card;
    invite_window = window;
    host_present_input_update();
    return 1;
}

void posix_update_invite(const char *link, const char *code) {
    NSString *copiedLink = link ? @(link) : nil;
    NSString *copiedCode = code ? @(code) : @"";
    dispatch_async(dispatch_get_main_queue(), ^{
        HaloInviteCardController *card = (HaloInviteCardController *)invite_window.rootViewController;
        if (!card) return;
        if (!copiedLink.length) {
            if (!card.preview) [card done];
        } else if (![card.link isEqualToString:copiedLink] || ![card.code isEqualToString:copiedCode]) {
            host_dismiss_share_sheet();
            card.preview = NO;
            card.link = copiedLink;
            card.code = copiedCode;
            [card refresh];
        }
    });
}

void posix_invite_menu_ready(void) {
#ifdef HALO_DEVELOPMENT_HOOKS
    static BOOL shown;
    const char *setting = getenv("HALO_SHOW_INVITE_CARD");
    if (shown || !setting || strcmp(setting, "1")) return;
    if (posix_show_invite("halo://join/0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef", "ABCD-EFGH")) {
        ((HaloInviteCardController *)invite_window.rootViewController).preview = YES;
        shown = YES;
    }
#endif
}
