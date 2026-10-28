#import "host_qr.h"

CIImage *host_qr_code(NSString *text, NSUInteger quiet_zone) {
    if (!text.length) return nil;
    CIFilter *filter = [CIFilter filterWithName:@"CIQRCodeGenerator"];
    [filter setValue:[text dataUsingEncoding:NSUTF8StringEncoding] forKey:@"inputMessage"];
    [filter setValue:@"M" forKey:@"inputCorrectionLevel"];
    CIImage *output = filter.outputImage;
    if (!output) return nil;
    /* CIQRCodeGenerator includes one white module on each edge. Extend that
       border over opaque white without resampling the code itself. */
    CGRect extent = CGRectInset(output.extent, 1 - (CGFloat)quiet_zone, 1 - (CGFloat)quiet_zone);
    CIImage *white = [[CIImage imageWithColor:[CIColor colorWithRed:1 green:1 blue:1]] imageByCroppingToRect:extent];
    return [[output imageByCompositingOverImage:white] imageByCroppingToRect:extent];
}

#if !TARGET_OS_OSX
UIImage *host_qr_image(NSString *text, NSUInteger scale, NSUInteger quiet_zone) {
    if (!scale) return nil;
    CIImage *output = host_qr_code(text, quiet_zone);
    if (!output) return nil;
    CGImageRef image = [[CIContext context] createCGImage:output fromRect:output.extent];
    if (!image) return nil;
    UIImage *source = [UIImage imageWithCGImage:image];
    CGSize size = CGSizeMake(CGImageGetWidth(image) * scale, CGImageGetHeight(image) * scale);
    UIGraphicsImageRendererFormat *format = [UIGraphicsImageRendererFormat preferredFormat];
    format.scale = 1;
    format.opaque = YES;
    UIGraphicsImageRenderer *renderer = [[UIGraphicsImageRenderer alloc] initWithSize:size format:format];
    UIImage *result = [renderer imageWithActions:^(UIGraphicsImageRendererContext *context) {
        CGContextSetInterpolationQuality(context.CGContext, kCGInterpolationNone);
        CGContextSetShouldAntialias(context.CGContext, NO);
        [source drawInRect:(CGRect){CGPointZero, size}];
    }];
    CGImageRelease(image);
    return result;
}
#endif
