/* Native symbols stay out of the guest's pointer arena. Only bounded white
   coverage crosses back; the game supplies placement, clipping, and color. */
#import <Foundation/Foundation.h>
#import <CoreGraphics/CoreGraphics.h>
#import <dispatch/dispatch.h>
#if HALO_NATIVE_MAC
#import <AppKit/AppKit.h>
#define HostGlyphImage NSImage
#else
#import <UIKit/UIKit.h>
#define HostGlyphImage UIImage
#endif
#include "../../linux/include/halo_keyboard_symbols.h"
#include <string.h>

/* Resolve against the running SDK's symbol catalog, and log each missing
   name once. A guessed square variant is never sent straight to drawing. */
static HostGlyphImage *glyph_image(NSString *name)
{
#if HALO_NATIVE_MAC
    HostGlyphImage *image = [NSImage imageWithSystemSymbolName:name accessibilityDescription:nil];
#else
    HostGlyphImage *image = [UIImage systemImageNamed:name];
#endif
    if (!image) {
        static NSMutableSet<NSString *> *missing;
        static dispatch_once_t once;
        dispatch_once(&once, ^{ missing = [NSMutableSet set]; });
        @synchronized (missing) {
            if (![missing containsObject:name]) {
                [missing addObject:name];
                NSLog(@"VISR: missing SF Symbol %@", name);
            }
        }
    }
    return image;
}

/* The first raster request validates every menu key and the special-key
   fallback catalog. Images remain native objects and are never exported. */
static void glyph_validate_keyboard_symbols(void)
{
    static dispatch_once_t once;
    dispatch_once(&once, ^{
        glyph_image(@"square");
        for (int index = 0; index < 36; index++) {
            char key[2] = {index < 26 ? 'a' + index : '0' + index - 26, 0};
            char square[96]; const char *overlay;
            halo_keyboard_symbol_plan(key, square, sizeof(square), &overlay);
            glyph_image([NSString stringWithUTF8String:square]);
        }
        for (unsigned int index = 0; index < sizeof(halo_keyboard_symbols) / sizeof(halo_keyboard_symbols[0]); index++) {
            char square[96]; const char *overlay;
            halo_keyboard_symbol_plan(halo_keyboard_symbols[index].key, square, sizeof(square), &overlay);
            if (!glyph_image([NSString stringWithUTF8String:square]) && overlay)
                glyph_image([NSString stringWithUTF8String:overlay]);
        }
        for (int index = 1; index <= 24; index++)
            glyph_image([NSString stringWithFormat:@"f%d.square", index]);
    });
}

static void glyph_draw_image(HostGlyphImage *image, int width, int height, CGFloat fraction)
{
#if HALO_NATIVE_MAC
    image = [image imageWithSymbolConfiguration:[NSImageSymbolConfiguration
        configurationWithPointSize:height * .82 weight:NSFontWeightMedium]];
    NSSize size = image.size;
#else
    image = [image imageByApplyingSymbolConfiguration:[UIImageSymbolConfiguration
        configurationWithPointSize:height * .82 weight:UIImageSymbolWeightMedium]];
    image = [image imageWithTintColor:UIColor.whiteColor renderingMode:UIImageRenderingModeAlwaysOriginal];
    CGSize size = image.size;
#endif
    CGFloat scale = MIN((width - 4.0) / MAX(size.width, 1), (height - 4.0) / MAX(size.height, 1)) * fraction;
    CGRect rect = CGRectMake((width - size.width * scale) / 2, (height - size.height * scale) / 2,
        size.width * scale, size.height * scale);
#if HALO_NATIVE_MAC
    [image drawInRect:NSRectFromCGRect(rect) fromRect:NSZeroRect operation:NSCompositingOperationSourceOver
        fraction:1 respectFlipped:YES hints:nil];
#else
    [image drawInRect:rect];
#endif
}

int host_controller_glyph_raster(int kind, const char *label, int width, int height, unsigned char *pixels)
{
    if ((kind != 2 && kind != 3) || !label || !*label || strnlen(label, 96) >= 96 ||
        !pixels || width < 8 || height < 8 || width > 512 || height > 256)
        return 0;
    @autoreleasepool {
        glyph_validate_keyboard_symbols();
        const char *overlay = NULL;
        char square[96];
        if (kind == 3) {
            if (!halo_keyboard_symbol_plan(label, square, sizeof(square), &overlay)) return 0;
            label = square;
        }
        NSString *name = [NSString stringWithUTF8String:label];
        if (!name) return 0;
        HostGlyphImage *image = glyph_image(name);
        HostGlyphImage *inside = nil;
        if (!image && kind == 3) {
            image = glyph_image(@"square");
            if (overlay) inside = glyph_image([NSString stringWithUTF8String:overlay]);
            /* An unsupported key must remain visible, with a diagnostic. */
            if (!inside) inside = glyph_image(@"keyboard");
        }
        if (!image) return 0;
        memset(pixels, 0, (size_t)width * height * 4);
        CGColorSpaceRef space = CGColorSpaceCreateDeviceRGB();
        CGContextRef context = CGBitmapContextCreate(pixels, width, height, 8, width * 4, space,
            kCGImageAlphaPremultipliedLast | kCGBitmapByteOrder32Big);
        CGColorSpaceRelease(space);
        if (!context) return 0;
        CGContextTranslateCTM(context, 0, height);
        CGContextScaleCTM(context, 1, -1);
#if HALO_NATIVE_MAC
        [NSGraphicsContext saveGraphicsState];
        NSGraphicsContext.currentContext = [NSGraphicsContext graphicsContextWithCGContext:context flipped:YES];
#else
        UIGraphicsPushContext(context);
#endif
        glyph_draw_image(image, width, height, 1);
        if (inside) glyph_draw_image(inside, width, height, .52);
#if HALO_NATIVE_MAC
        [NSGraphicsContext restoreGraphicsState];
#else
        UIGraphicsPopContext();
#endif
        CGContextRelease(context);
        /* The guest tints this white coverage with the legend's HUD blue. */
        for (int index = 0; index < width * height; index++)
            pixels[index * 4] = pixels[index * 4 + 1] = pixels[index * 4 + 2] = 255;
        return 1;
    }
}
