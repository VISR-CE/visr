/* Native symbols stay out of the guest's pointer arena. Only bounded white
   coverage crosses back; the game supplies placement, clipping, and color. */
#import <Foundation/Foundation.h>
#import <CoreGraphics/CoreGraphics.h>
#if HALO_NATIVE_MAC
#import <AppKit/AppKit.h>
#else
#import <UIKit/UIKit.h>
#endif
#include <string.h>

int host_controller_glyph_raster(int kind, const char *label, int width, int height, unsigned char *pixels)
{
    if ((kind != 2 && kind != 3) || !label || !*label || strnlen(label, 96) >= 96 ||
        !pixels || width < 8 || height < 8 || width > 512 || height > 256)
        return 0;
    @autoreleasepool {
        if (kind == 3 && !strcmp(label, "Escape")) label = "Esc";
        NSString *name = [NSString stringWithUTF8String:label];
        if (!name) return 0;
        memset(pixels, 0, (size_t)width * height * 4);
        CGColorSpaceRef space = CGColorSpaceCreateDeviceRGB();
        CGContextRef context = CGBitmapContextCreate(pixels, width, height, 8, width * 4, space,
            kCGImageAlphaPremultipliedLast | kCGBitmapByteOrder32Big);
        CGColorSpaceRelease(space);
        if (!context) return 0;
        CGContextTranslateCTM(context, 0, height);
        CGContextScaleCTM(context, 1, -1);
        int success = 1;
#if HALO_NATIVE_MAC
        [NSGraphicsContext saveGraphicsState];
        NSGraphicsContext.currentContext = [NSGraphicsContext graphicsContextWithCGContext:context flipped:YES];
        if (kind == 2) {
            NSImage *image = [NSImage imageWithSystemSymbolName:name accessibilityDescription:nil];
            if (!image) success = 0;
            else {
                image = [image imageWithSymbolConfiguration:[NSImageSymbolConfiguration
                    configurationWithPointSize:height * .82 weight:NSFontWeightMedium]];
                NSSize size = image.size;
                CGFloat scale = MIN((width - 4.0) / MAX(size.width, 1), (height - 4.0) / MAX(size.height, 1));
                NSRect rect = NSMakeRect((width - size.width * scale) / 2, (height - size.height * scale) / 2,
                    size.width * scale, size.height * scale);
                [image drawInRect:rect fromRect:NSZeroRect operation:NSCompositingOperationSourceOver fraction:1
                    respectFlipped:YES hints:nil];
            }
        } else {
            NSBezierPath *cap = [NSBezierPath bezierPathWithRoundedRect:NSMakeRect(1, 1, width-2, height-2)
                xRadius:height * .16 yRadius:height * .16];
            [NSColor.whiteColor setStroke]; cap.lineWidth = MAX(1, height * .045); [cap stroke];
            CGFloat point = height * .55;
            NSDictionary *attributes = @{NSFontAttributeName:[NSFont systemFontOfSize:point weight:NSFontWeightMedium],
                NSForegroundColorAttributeName:NSColor.whiteColor};
            NSSize size = [name sizeWithAttributes:attributes];
            if (size.width > width-8) {
                point *= (width-8) / size.width;
                attributes = @{NSFontAttributeName:[NSFont systemFontOfSize:point weight:NSFontWeightMedium],
                    NSForegroundColorAttributeName:NSColor.whiteColor};
                size = [name sizeWithAttributes:attributes];
            }
            [name drawAtPoint:NSMakePoint((width-size.width)/2, (height-size.height)/2) withAttributes:attributes];
        }
        [NSGraphicsContext restoreGraphicsState];
#else
        UIGraphicsPushContext(context);
        if (kind == 2) {
            UIImage *image = [UIImage systemImageNamed:name withConfiguration:[UIImageSymbolConfiguration
                configurationWithPointSize:height * .82 weight:UIImageSymbolWeightMedium]];
            if (!image) success = 0;
            else {
                image = [image imageWithTintColor:UIColor.whiteColor renderingMode:UIImageRenderingModeAlwaysOriginal];
                CGSize size = image.size;
                CGFloat scale = MIN((width-4.0)/MAX(size.width,1), (height-4.0)/MAX(size.height,1));
                [image drawInRect:CGRectMake((width-size.width*scale)/2, (height-size.height*scale)/2,
                    size.width*scale,size.height*scale)];
            }
        } else {
            UIBezierPath *cap = [UIBezierPath bezierPathWithRoundedRect:CGRectMake(1,1,width-2,height-2)
                cornerRadius:height*.16];
            [UIColor.whiteColor setStroke]; cap.lineWidth = MAX(1,height*.045); [cap stroke];
            CGFloat point = height * .55;
            NSDictionary *attributes = @{NSFontAttributeName:[UIFont systemFontOfSize:point weight:UIFontWeightMedium],
                NSForegroundColorAttributeName:UIColor.whiteColor};
            CGSize size = [name sizeWithAttributes:attributes];
            if (size.width > width-8) {
                point *= (width-8)/size.width;
                attributes = @{NSFontAttributeName:[UIFont systemFontOfSize:point weight:UIFontWeightMedium],
                    NSForegroundColorAttributeName:UIColor.whiteColor};
                size = [name sizeWithAttributes:attributes];
            }
            [name drawAtPoint:CGPointMake((width-size.width)/2,(height-size.height)/2) withAttributes:attributes];
        }
        UIGraphicsPopContext();
#endif
        CGContextRelease(context);
        /* SF Symbols may have palette RGB; callers tint coverage uniformly. */
        for (int index=0;index<width*height;index++)
            pixels[index*4]=pixels[index*4+1]=pixels[index*4+2]=255;
        return success;
    }
}
