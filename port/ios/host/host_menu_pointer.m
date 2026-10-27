/* Fingers on the game's view drive the menus' pointer (host_menu_pointer.h has the rules; the guest's
   halo_ui_pointer_update reads the result through host_ui_pointer_read). The recognizer sits on the SDL
   root view beside the touch controls, takes no touch from them and cancels none. */
#import <UIKit/UIKit.h>
#include <SDL3/SDL.h>
#include "ios_host.h"
#include "host_menu_pointer.h"

#define pointer_state (*host_menu_pointer_shared())
static UISelectionFeedbackGenerator *selection_feedback;
static UIImpactFeedbackGenerator *impact_feedback;

@interface HaloMenuTouch : UIGestureRecognizer <UIGestureRecognizerDelegate>
@end

@implementation HaloMenuTouch
- (instancetype)initWithTarget:(id)target action:(SEL)action {
    if(!(self=[super initWithTarget:target action:action]))return nil;
    self.allowedTouchTypes=@[@(UITouchTypeDirect)];self.cancelsTouchesInView=NO;self.delaysTouchesBegan=NO;self.delaysTouchesEnded=NO;self.delegate=self;
    return self;
}
/* a touch that lands on a control belongs to the control; the controls' own view, left claiming every touch
   while they are put away, does not */
- (BOOL)gestureRecognizer:(UIGestureRecognizer *)recognizer shouldReceiveTouch:(UITouch *)touch {
    /* only the game's own view, or the controls' view where it claims every touch (controller in use); a button
       (the audio output picker, a control) keeps its touch */
    return touch.view==self.view || [NSStringFromClass(touch.view.class) isEqualToString:@"HaloControls"];
}
- (void)track {
    UIView *root=self.view;
    host_menu_pointer_set_blocked(&pointer_state,host_ios_touch_blocked());
    host_menu_pointer_set_surface(&pointer_state,(float)root.bounds.size.width,(float)root.bounds.size.height);
}
- (void)touchesBegan:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event {
    [self track];
    for(UITouch *touch in touches) {
        CGPoint p=[touch locationInView:self.view];
        host_menu_pointer_touch_begin(&pointer_state,(intptr_t)(__bridge void *)touch,(float)p.x,(float)p.y);
    }
}
- (void)touchesMoved:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event {
    [self track];
    for(UITouch *touch in touches) {
        CGPoint p=[touch locationInView:self.view];
        host_menu_pointer_touch_move(&pointer_state,(intptr_t)(__bridge void *)touch,(float)p.x,(float)p.y);
    }
}
- (void)touchesEnded:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event {
    [self track];
    for(UITouch *touch in touches)host_menu_pointer_touch_end(&pointer_state,(intptr_t)(__bridge void *)touch);
}
- (void)touchesCancelled:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event {
    for(UITouch *touch in touches)host_menu_pointer_touch_cancel(&pointer_state,(intptr_t)(__bridge void *)touch);
}
@end

/* a tick when the focus lands on an item, a light tap when something is pressed; the iPad has no Taptic Engine
   and these do nothing there */
static void pointer_feedback(int kind) {
    if(kind==HOST_UI_POINTER_FEEDBACK_FOCUS)[selection_feedback selectionChanged];
    else if(kind==HOST_UI_POINTER_FEEDBACK_ACTIVATE)[impact_feedback impactOccurred];
}

static void menus_changed(int menus) {
    if(menus){[selection_feedback prepare];[impact_feedback prepare];}
    host_ios_touch_menu_changed(menus);
}

void host_menu_pointer_attach(void *root_view) {
    UIView *root=(__bridge UIView *)root_view;
    if(!selection_feedback)selection_feedback=[[UISelectionFeedbackGenerator alloc]init];
    if(!impact_feedback)impact_feedback=[[UIImpactFeedbackGenerator alloc]initWithStyle:UIImpactFeedbackStyleLight];
    [root addGestureRecognizer:[[HaloMenuTouch alloc]initWithTarget:nil action:nil]];
    host_ui_pointer_install(pointer_feedback,menus_changed);
}
