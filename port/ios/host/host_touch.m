/* UIKit controls feed a standard SDL gamepad. The first hardware controller
   shares player one with touch; additional hardware controllers keep their ports. */
#import <UIKit/UIKit.h>
#import <CoreHaptics/CoreHaptics.h>
#include <SDL3/SDL.h>
#include "ios_host.h"
#import "host_orientation.h"
#include <math.h>
#include "host_touch_replay.h"
#include "touch_math.h"

static struct host_touch_replay_gate replay_gate;
static void touch_replay_ui_update(void);

static int pause_pending;
void host_ios_touch_pause(void) { if (!host_touch_replay_blocked(&replay_gate)) pause_pending=1; }
static SDL_Joystick *touch_joystick;
static SDL_JoystickID touch_id, primary_hardware;

static void button_state(int button, BOOL down) {
    if(touch_joystick) SDL_SetJoystickVirtualButton(touch_joystick,button,
        host_touch_replay_blocked(&replay_gate)?NO:down);
}
static void axis_state(int axis, float value) {
    if(host_touch_replay_blocked(&replay_gate))value=axis>=SDL_GAMEPAD_AXIS_LEFT_TRIGGER?-1:0;
    if(touch_joystick) SDL_SetJoystickVirtualAxis(touch_joystick,axis,(Sint16)(fmaxf(-1,fminf(1,value))*32767));
}
void host_ios_touch_initialize(void) {
    SDL_VirtualJoystickDesc desc;
    SDL_INIT_INTERFACE(&desc);
    desc.type=SDL_JOYSTICK_TYPE_GAMEPAD;
    desc.naxes=SDL_GAMEPAD_AXIS_COUNT;
    desc.nbuttons=SDL_GAMEPAD_BUTTON_COUNT;
    desc.axis_mask=(1u<<SDL_GAMEPAD_AXIS_COUNT)-1;
    desc.button_mask=(1u<<SDL_GAMEPAD_BUTTON_COUNT)-1;
    desc.name="Halo Touch Controls";
    touch_id=SDL_AttachVirtualJoystick(&desc);
    touch_joystick=SDL_OpenJoystick(touch_id);
    if(!touch_joystick)host_fatal("Could not initialize touch controls: %s",SDL_GetError());
    axis_state(SDL_GAMEPAD_AXIS_LEFT_TRIGGER,-1);
    axis_state(SDL_GAMEPAD_AXIS_RIGHT_TRIGGER,-1);
}
int host_ios_gamepads(uint32_t *out,int capacity) {
    int count=0,used=0;SDL_JoystickID *ids=SDL_GetGamepads(&count);
    primary_hardware=0;
    if(capacity>0 && touch_id)out[used++]=touch_id;
    for(int i=0;i<count;i++) {
        if(ids[i]==touch_id)continue;
        if(!primary_hardware) {
            primary_hardware=ids[i];
            if(!SDL_GetGamepadFromID(ids[i]))SDL_OpenGamepad(ids[i]);
        } else if(used<capacity) out[used++]=ids[i];
    }
    SDL_free(ids);return used;
}
int host_ios_gamepad_type(SDL_Gamepad *pad) {
    /* Keep the shared touch/hardware controller in the first recognized port. */
    return SDL_GetGamepadID(pad)==touch_id?SDL_GAMEPAD_TYPE_XBOX360:SDL_GetGamepadType(pad);
}
int host_ios_gamepad_axis(SDL_Gamepad *pad,int axis) {
    int value=SDL_GetGamepadAxis(pad,axis);
    if(SDL_GetGamepadID(pad)==touch_id && host_touch_replay_blocked(&replay_gate))
        value=0;
    if(SDL_GetGamepadID(pad)==touch_id && primary_hardware) {
        int physical=SDL_GetGamepadAxis(SDL_GetGamepadFromID(primary_hardware),axis);
        if(abs(physical)>abs(value))value=physical;
    }
    return value;
}
int host_ios_gamepad_button(SDL_Gamepad *pad,int button) {
    if(button==SDL_GAMEPAD_BUTTON_START && SDL_GetGamepadID(pad)==touch_id && pause_pending && !host_touch_replay_blocked(&replay_gate)) {
        pause_pending=0;return 1;
    }
    return (!(SDL_GetGamepadID(pad)==touch_id && host_touch_replay_blocked(&replay_gate)) && SDL_GetGamepadButton(pad,button)) ||
        (SDL_GetGamepadID(pad)==touch_id && primary_hardware &&
         SDL_GetGamepadButton(SDL_GetGamepadFromID(primary_hardware),button));
}
void host_ios_touch_reset(void) {
    pause_pending=0;
    for(int i=0;i<SDL_GAMEPAD_BUTTON_COUNT;i++)button_state(i,NO);
    for(int i=0;i<SDL_GAMEPAD_AXIS_COUNT;i++)axis_state(i,i>=SDL_GAMEPAD_AXIS_LEFT_TRIGGER?-1:0);
}

void host_touch_replay_active(int active) {
    if(host_touch_replay_set(&replay_gate,active,host_ios_touch_reset))
        dispatch_async(dispatch_get_main_queue(),^{ touch_replay_ui_update(); });
}

static touch_rect touch_rect_from_cg(CGRect rect) {
    return (touch_rect){rect.origin.x,rect.origin.y,rect.size.width,rect.size.height};
}

static void touch_button_haptic(void) {
    static UIImpactFeedbackGenerator *feedback;
    if(!CHHapticEngine.capabilitiesForHardware.supportsHaptics)return;
    if(!feedback)feedback=[[UIImpactFeedbackGenerator alloc]initWithStyle:UIImpactFeedbackStyleLight];
    [feedback impactOccurred];[feedback prepare];
}

@interface HaloPaddedButton : UIButton
@end
@implementation HaloPaddedButton
- (BOOL)pointInside:(CGPoint)point withEvent:(UIEvent *)event {
    return CGRectContainsPoint(CGRectInset(self.bounds,-7,-7),point);
}
@end

@interface HaloButton : HaloPaddedButton
@property(nonatomic) int gameButton;
@property(nonatomic) int gameAxis;
@property(nonatomic) BOOL pressed;
@end
@implementation HaloButton
- (void)sendDown:(BOOL)down {
    if(host_touch_replay_blocked(&replay_gate))down=NO;
    if(down && !self.pressed)touch_button_haptic();
    self.pressed=down;self.highlighted=down;
    if(self.gameAxis>=0)axis_state(self.gameAxis,down?1:-1);
    else button_state(self.gameButton,down);
}
- (BOOL)beginTrackingWithTouch:(UITouch *)touch withEvent:(UIEvent *)event {
    [super beginTrackingWithTouch:touch withEvent:event];[self sendDown:YES];return YES;
}
- (void)endTrackingWithTouch:(UITouch *)touch withEvent:(UIEvent *)event {
    [super endTrackingWithTouch:touch withEvent:event];[self sendDown:NO];
}
- (void)cancelTrackingWithEvent:(UIEvent *)event {
    [super cancelTrackingWithEvent:event];[self sendDown:NO];
}
- (BOOL)accessibilityActivate {
    [self sendDown:YES];
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW,150*NSEC_PER_MSEC),dispatch_get_main_queue(),^{[self sendDown:NO];});
    return YES;
}
@end

@interface HaloStick : UIControl
@property(nonatomic) int firstAxis;
@property(nonatomic) CGPoint position;
@property(nonatomic) CGPoint restCenter;
@property(nonatomic) BOOL floating;
@property(nonatomic) touch_rect moveZone;
@property(nonatomic,copy) NSString *caption;
@end
@implementation HaloStick
- (void)updateTouch:(UITouch *)touch {
    if(host_touch_replay_blocked(&replay_gate)){[self reset];return;}
    CGPoint p=[touch locationInView:self];CGFloat radius=MIN(self.bounds.size.width,self.bounds.size.height)/2;
    CGFloat x=p.x-self.bounds.size.width/2,y=p.y-self.bounds.size.height/2;
    touch_point output=touch_stick(x,y,radius,TOUCH_STICK_DEAD_ZONE,TOUCH_STICK_EXPONENT);
    /* The knob follows the thumb, while the game receives the curved output. */
    CGFloat length=hypot(x,y),travel=radius*.6;
    if(length>travel){x*=travel/length;y*=travel/length;}
    self.position=CGPointMake(x,y);axis_state(self.firstAxis,output.x);axis_state(self.firstAxis+1,output.y);
    [self setNeedsDisplay];
}
- (BOOL)beginTrackingWithTouch:(UITouch *)touch withEvent:(UIEvent *)event {
    if(host_touch_replay_blocked(&replay_gate))return NO;
    if(self.floating) {
        CGPoint p=[touch locationInView:self.superview];
        touch_point base=touch_floating_base((touch_point){p.x,p.y},self.moveZone,self.bounds.size.width/2);
        self.center=CGPointMake(base.x,base.y);
    }
    [self updateTouch:touch];return YES;
}
- (BOOL)continueTrackingWithTouch:(UITouch *)touch withEvent:(UIEvent *)event {[self updateTouch:touch];return YES;}
- (void)reset {if(self.floating)self.center=self.restCenter;self.position=CGPointZero;axis_state(self.firstAxis,0);axis_state(self.firstAxis+1,0);[self setNeedsDisplay];}
- (void)endTrackingWithTouch:(UITouch *)touch withEvent:(UIEvent *)event {[self reset];}
- (void)cancelTrackingWithEvent:(UIEvent *)event {[self reset];}
- (void)drawRect:(CGRect)rect {
    CGRect circle=CGRectInset(self.bounds,2,2);
    [[UIColor colorWithWhite:0 alpha:.2]setFill];UIBezierPath *path=[UIBezierPath bezierPathWithOvalInRect:circle];[path fill];
    [[UIColor colorWithWhite:1 alpha:.5]setStroke];path.lineWidth=1.5;[path stroke];
    CGFloat size=self.bounds.size.width,knobSize=size*(42.0/136.0);
    CGRect knob=CGRectMake(size/2+self.position.x-knobSize/2,size/2+self.position.y-knobSize/2,knobSize,knobSize);
    [[UIColor colorWithWhite:1 alpha:.25]setFill];[[UIBezierPath bezierPathWithOvalInRect:knob]fill];
    NSDictionary *attrs=@{NSFontAttributeName:[UIFont systemFontOfSize:10*size/136 weight:UIFontWeightSemibold],NSForegroundColorAttributeName:[UIColor colorWithWhite:1 alpha:.8]};
    CGSize text=[self.caption sizeWithAttributes:attrs];
    [self.caption drawAtPoint:CGPointMake((size-text.width)/2,size-20*size/136) withAttributes:attrs];
}
@end

@interface HaloControls : UIView
@property(nonatomic,strong) HaloStick *moveStick;
@property(nonatomic,strong) HaloStick *lookStick;
@property(nonatomic,strong) NSMutableArray<HaloButton *> *buttons;
@property(nonatomic,strong) UIButton *toggle;
@property(nonatomic) CGRect layoutSafeArea;
@end
@implementation HaloControls
- (HaloButton *)addButton:(NSString *)title label:(NSString *)label button:(int)button axis:(int)axis {
    HaloButton *b=[HaloButton buttonWithType:UIButtonTypeCustom];b.gameButton=button;b.gameAxis=axis;
    [b setTitle:title forState:UIControlStateNormal];b.titleLabel.font=[UIFont systemFontOfSize:14 weight:UIFontWeightSemibold];
    b.backgroundColor=[UIColor colorWithWhite:0 alpha:.28];b.layer.borderColor=[UIColor colorWithWhite:1 alpha:.5].CGColor;b.layer.borderWidth=1;
    b.accessibilityLabel=label;b.exclusiveTouch=NO;[self addSubview:b];[self.buttons addObject:b];return b;
}
- (instancetype)initWithFrame:(CGRect)frame {
    if(!(self=[super initWithFrame:frame]))return nil;
    self.autoresizingMask=UIViewAutoresizingFlexibleWidth|UIViewAutoresizingFlexibleHeight;
    self.multipleTouchEnabled=YES;self.buttons=[NSMutableArray array];
    self.moveStick=[[HaloStick alloc]init];self.moveStick.floating=YES;self.moveStick.firstAxis=SDL_GAMEPAD_AXIS_LEFTX;self.moveStick.caption=@"MOVE";
    self.lookStick=[[HaloStick alloc]init];self.lookStick.firstAxis=SDL_GAMEPAD_AXIS_RIGHTX;self.lookStick.caption=@"LOOK";
    for(HaloStick *s in @[self.moveStick,self.lookStick]){s.backgroundColor=UIColor.clearColor;s.isAccessibilityElement=YES;s.accessibilityLabel=s.caption;[self addSubview:s];}
    [self addButton:@"A" label:@"A — Jump or select" button:SDL_GAMEPAD_BUTTON_SOUTH axis:-1];
    [self addButton:@"B" label:@"B — Melee or back" button:SDL_GAMEPAD_BUTTON_EAST axis:-1];
    [self addButton:@"X" label:@"X — Reload or use" button:SDL_GAMEPAD_BUTTON_WEST axis:-1];
    [self addButton:@"Y" label:@"Y — Switch weapon" button:SDL_GAMEPAD_BUTTON_NORTH axis:-1];
    [self addButton:@"FIRE" label:@"Fire" button:0 axis:SDL_GAMEPAD_AXIS_RIGHT_TRIGGER];
    [self addButton:@"GRENADE" label:@"Throw grenade" button:0 axis:SDL_GAMEPAD_AXIS_LEFT_TRIGGER];
    [self addButton:@"↑" label:@"Menu up" button:SDL_GAMEPAD_BUTTON_DPAD_UP axis:-1];
    [self addButton:@"↓" label:@"Menu down" button:SDL_GAMEPAD_BUTTON_DPAD_DOWN axis:-1];
    [self addButton:@"←" label:@"Menu left" button:SDL_GAMEPAD_BUTTON_DPAD_LEFT axis:-1];
    [self addButton:@"→" label:@"Menu right" button:SDL_GAMEPAD_BUTTON_DPAD_RIGHT axis:-1];
    [self addButton:@"PAUSE" label:@"Pause or start" button:SDL_GAMEPAD_BUTTON_START axis:-1];
    [self addButton:@"CROUCH" label:@"Crouch" button:SDL_GAMEPAD_BUTTON_LEFT_STICK axis:-1];
    [self addButton:@"ZOOM" label:@"Zoom" button:SDL_GAMEPAD_BUTTON_RIGHT_STICK axis:-1];
    [self addButton:@"LIGHT" label:@"Flashlight" button:SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER axis:-1];
    [self addButton:@"SWAP G" label:@"Switch grenade" button:SDL_GAMEPAD_BUTTON_LEFT_SHOULDER axis:-1];
    self.toggle=[HaloPaddedButton buttonWithType:UIButtonTypeSystem];[self.toggle setTitle:@"Hide controls" forState:UIControlStateNormal];
    [self.toggle setTitleColor:UIColor.whiteColor forState:UIControlStateNormal];self.toggle.titleLabel.font=[UIFont systemFontOfSize:12];
    self.toggle.backgroundColor=[UIColor colorWithWhite:0 alpha:.3];self.toggle.layer.cornerRadius=12;
    [self.toggle addTarget:self action:@selector(buttonHaptic) forControlEvents:UIControlEventTouchDown];
    [self.toggle addTarget:self action:@selector(toggleControls) forControlEvents:UIControlEventTouchUpInside];[self addSubview:self.toggle];
    [[NSNotificationCenter defaultCenter]addObserver:self selector:@selector(reset) name:UIApplicationWillResignActiveNotification object:nil];
    return self;
}
- (void)reset {
    host_ios_touch_reset();[self.moveStick cancelTrackingWithEvent:nil];[self.lookStick cancelTrackingWithEvent:nil];
    for(HaloButton *button in self.buttons)[button cancelTrackingWithEvent:nil];
    [self.toggle cancelTrackingWithEvent:nil];
}
- (void)buttonHaptic {if(!host_touch_replay_blocked(&replay_gate))touch_button_haptic();}
- (void)toggleControls {
    if(host_touch_replay_blocked(&replay_gate))return;
    [self reset];BOOL hidden=!self.moveStick.hidden;
    self.moveStick.hidden=self.lookStick.hidden=hidden;for(UIView *b in self.buttons)b.hidden=hidden;
    [self.toggle setTitle:hidden?@"Show controls":@"Hide controls" forState:UIControlStateNormal];
}
- (void)layoutSubviews {
    [super layoutSubviews];CGRect r=UIEdgeInsetsInsetRect(self.bounds,self.safeAreaInsets);
    /* A highlight/title update can request layout while a thumb is held down. */
    if(CGRectEqualToRect(r,self.layoutSafeArea))return;
    self.layoutSafeArea=r;
    /* Preserve the existing arrangement in scaled design points. */
    CGFloat scale=touch_scale(r.size.width,r.size.height);
    CGFloat left=r.origin.x+16*scale,right=CGRectGetMaxX(r)-16*scale,bottom=CGRectGetMaxY(r)-12*scale,top=r.origin.y+10*scale;
    CGFloat stick=136*scale,s=44*scale,gap=4*scale;
    [self reset];
    self.moveStick.frame=CGRectMake(left,bottom-stick,stick,stick);
    self.moveStick.restCenter=self.moveStick.center;
    self.moveStick.moveZone=touch_move_zone(touch_rect_from_cg(self.bounds),touch_rect_from_cg(r));
    self.lookStick.frame=CGRectMake(right-3*s-20*scale-stick,bottom-stick,stick,stick);
    // Four face buttons in the traditional diamond.
    CGPoint face[4]={{right-2*s-gap,bottom-s},{right-s,bottom-2*s-gap},{right-3*s-2*gap,bottom-2*s-gap},{right-2*s-gap,bottom-3*s-2*gap}};
    for(int i=0;i<4;i++)self.buttons[i].frame=CGRectMake(face[i].x,face[i].y,s,s);
    self.buttons[4].frame=CGRectMake(right-84*scale,top+55*scale,84*scale,48*scale);
    self.buttons[5].frame=CGRectMake(left,top+55*scale,90*scale,48*scale);
    CGFloat d=34*scale,dx=left+stick+12*scale,dy=bottom-3*d;
    self.buttons[6].frame=CGRectMake(dx+d,dy,d,d);
    self.buttons[7].frame=CGRectMake(dx+d,dy+2*d,d,d);
    self.buttons[8].frame=CGRectMake(dx,dy+d,d,d);
    self.buttons[9].frame=CGRectMake(dx+2*d,dy+d,d,d);
    self.buttons[10].frame=CGRectMake(CGRectGetMidX(r)-38*scale,top,76*scale,34*scale);
    self.buttons[11].frame=CGRectMake(left,bottom-stick-42*scale,74*scale,34*scale);
    self.buttons[12].frame=CGRectMake(right-3*s-20*scale-stick,bottom-stick-42*scale,74*scale,34*scale);
    self.buttons[13].frame=CGRectMake(right-180*scale,top+55*scale,80*scale,36*scale);
    self.buttons[14].frame=CGRectMake(left+104*scale,top+55*scale,80*scale,36*scale);
    for(UIButton *b in self.buttons){b.layer.cornerRadius=MIN(b.bounds.size.width,b.bounds.size.height)/2;b.titleLabel.font=[UIFont systemFontOfSize:14*scale weight:UIFontWeightSemibold];b.titleLabel.adjustsFontSizeToFitWidth=YES;}
    self.toggle.titleLabel.font=[UIFont systemFontOfSize:12*scale];self.toggle.layer.cornerRadius=12*scale;
    self.toggle.frame=CGRectMake(right-108*scale,top,108*scale,32*scale);
}
/* Resolve all targets together instead of letting subview order decide overlaps.
   Returning the control itself also admits touches outside its visible bounds. */
- (UIView *)controlAtPoint:(CGPoint)point {
    if(host_touch_replay_blocked(&replay_gate))return nil;
    NSMutableArray<UIControl *> *controls=[NSMutableArray array];
    for(UIControl *view in self.subviews) {
        if([view isKindOfClass:UIControl.class] && !view.hidden && view.userInteractionEnabled && view.enabled && !view.tracking)
            [controls addObject:view];
    }
    touch_target targets[MAX((NSUInteger)1,controls.count)];
    for(NSUInteger i=0;i<controls.count;i++) {
        UIControl *view=controls[i];
        targets[i]=(touch_target){touch_rect_from_cg(view.frame),[view isKindOfClass:HaloStick.class]?18:7};
    }
    int index=touch_hit_test((touch_point){point.x,point.y},targets,controls.count);
    if(index>=0)return controls[index];
    if(!self.moveStick.hidden && !self.moveStick.tracking) {
        touch_target zone={self.moveStick.moveZone,0};
        if(touch_hit_test((touch_point){point.x,point.y},&zone,1)==0)return self.moveStick;
    }
    return nil;
}
- (BOOL)pointInside:(CGPoint)point withEvent:(UIEvent *)event {
    return [self controlAtPoint:point]!=nil;
}
- (UIView *)hitTest:(CGPoint)point withEvent:(UIEvent *)event {
    if(self.hidden || !self.userInteractionEnabled || self.alpha<.01)return nil;
    return [self controlAtPoint:point];
}
@end

static NSHashTable<HaloControls *> *attached_controls;
static void touch_replay_ui_update(void) {
    BOOL blocked=host_touch_replay_blocked(&replay_gate);
    for(HaloControls *controls in attached_controls) {
        [controls reset];
        controls.hidden=blocked;
        controls.userInteractionEnabled=!blocked;
    }
}

void host_ios_touch_attach(SDL_Window *window) {
    UIWindow *native=(__bridge UIWindow *)SDL_GetPointerProperty(SDL_GetWindowProperties(window),SDL_PROP_WINDOW_UIKIT_WINDOW_POINTER,NULL);
    /* Retire SDL's temporary launch window before entering the non-returning
       game loop. Its fade animation can otherwise keep accessibility/input
       attached to the launch controller on recent scene-based iOS versions. */
    for(UIWindow *candidate in native.windowScene.windows) {
        if(candidate!=native && [NSStringFromClass(candidate.rootViewController.class) hasPrefix:@"SDLLaunch"])
            candidate.hidden=YES;
    }
    [native makeKeyAndVisible];
    host_ios_require_landscape(native);
    /* Keep the shared controller bridge active while Mac input uses no touch overlay. */
    if(NSProcessInfo.processInfo.isMacCatalystApp || NSProcessInfo.processInfo.isiOSAppOnMac)return;
    UIView *root=native.rootViewController.view;
    HaloControls *controls=[[HaloControls alloc]initWithFrame:root.bounds];
    [root addSubview:controls];
    if(!attached_controls)attached_controls=[NSHashTable weakObjectsHashTable];
    [attached_controls addObject:controls];
    touch_replay_ui_update();
    host_logf(HOST_LOG_INFO,"touch controls attached, %.0fx%.0f",root.bounds.size.width,root.bounds.size.height);
}
