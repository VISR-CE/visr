/* Synthetic map headers and source files exercise post-import cleanup without game data. */
#import <Foundation/Foundation.h>
#include <assert.h>
#include "host_import_source.h"

static void write_file(NSString *path) {
    assert([@"synthetic disc" writeToFile:path atomically:YES encoding:NSUTF8StringEncoding error:nil]);
}
static void valid_maps(NSString *root) {
    NSString *maps=[root stringByAppendingPathComponent:@"maps"];
    assert([NSFileManager.defaultManager createDirectoryAtPath:maps withIntermediateDirectories:YES attributes:nil error:nil]);
    unsigned char header[2048]={0};
    memcpy(header,"daeh",4);header[4]=5;memcpy(header+64,"01.10.12.2276",13);memcpy(header+2044,"toof",4);
    NSData *data=[NSData dataWithBytes:header length:sizeof(header)];
    for(NSString *name in @[@"ui",@"a10",@"a30",@"a50",@"b30",@"b40",@"c10",@"c20",@"c40",@"d20",@"d40"])
        assert([data writeToFile:[maps stringByAppendingPathComponent:[name stringByAppendingString:@".map"]] atomically:YES]);
}
int main(void) {
    @autoreleasepool {
        NSFileManager *files=NSFileManager.defaultManager;
        NSString *base=[NSTemporaryDirectory() stringByAppendingPathComponent:NSUUID.UUID.UUIDString];
        NSString *root=[base stringByAppendingPathComponent:@"Documents"];
        NSString *outside=[base stringByAppendingPathComponent:@"Documents-other"];
        for(NSString *folder in @[root,outside,[root stringByAppendingPathComponent:@"Inbox"]])
            assert([files createDirectoryAtPath:folder withIntermediateDirectories:YES attributes:nil error:nil]);
        valid_maps(root);
        BOOL (^active)(void)=^{return NO;};
        for(NSString *name in @[@"disc.iso",@"Inbox/disc.xiso"]) {
            NSString *path=[root stringByAppendingPathComponent:name];write_file(path);
            assert(host_import_remove_source([NSURL fileURLWithPath:path],root,root,YES,active)==HOST_IMPORT_SOURCE_REMOVED);
            assert(![files fileExistsAtPath:path]);
        }
        NSString *path=[root stringByAppendingPathComponent:@"kept.iso"];write_file(path);
        NSURL *url=[NSURL fileURLWithPath:path];
        assert(host_import_remove_source(url,root,root,NO,active)==HOST_IMPORT_SOURCE_KEPT);
        assert(host_import_remove_source(url,root,root,YES,^{return YES;})==HOST_IMPORT_SOURCE_KEPT);
        assert(host_import_remove_source(url,root,nil,YES,active)==HOST_IMPORT_SOURCE_KEPT);
        assert(host_import_remove_source(url,root,outside,YES,active)==HOST_IMPORT_SOURCE_KEPT);
        assert(host_import_remove_source(url,@"",@"",YES,active)==HOST_IMPORT_SOURCE_KEPT);
        assert(host_import_remove_source(url,@"Documents",@"Documents",YES,active)==HOST_IMPORT_SOURCE_KEPT);
        assert(host_import_remove_source([NSURL URLWithString:@"https://example.invalid/disc.iso"],root,root,YES,active)==HOST_IMPORT_SOURCE_KEPT);
        assert([files fileExistsAtPath:path]);
        NSString *external=[outside stringByAppendingPathComponent:@"disc.iso"];write_file(external);
        assert(host_import_remove_source([NSURL fileURLWithPath:external],root,root,YES,active)==HOST_IMPORT_SOURCE_KEPT);
        NSString *escape=[root stringByAppendingPathComponent:@"escape.iso"];
        assert([files createSymbolicLinkAtPath:escape withDestinationPath:external error:nil]);
        assert(host_import_remove_source([NSURL fileURLWithPath:escape],root,root,YES,active)==HOST_IMPORT_SOURCE_KEPT);
        NSString *escapeFolder=[root stringByAppendingPathComponent:@"escape-folder"];
        assert([files createSymbolicLinkAtPath:escapeFolder withDestinationPath:outside error:nil]);
        assert(host_import_remove_source([NSURL fileURLWithPath:[escapeFolder stringByAppendingPathComponent:@"disc.iso"]],root,root,YES,active)==HOST_IMPORT_SOURCE_KEPT);
        NSString *inward=[outside stringByAppendingPathComponent:@"inward.iso"];
        assert([files createSymbolicLinkAtPath:inward withDestinationPath:path error:nil]);
        assert(host_import_remove_source([NSURL fileURLWithPath:inward],root,root,YES,active)==HOST_IMPORT_SOURCE_KEPT);
        assert(host_import_remove_source([NSURL fileURLWithPath:root],root,root,YES,active)==HOST_IMPORT_SOURCE_KEPT);
        __block int checks=0;
        assert(host_import_remove_source(url,root,root,YES,^BOOL{return ++checks==2;})==HOST_IMPORT_SOURCE_KEPT);
        assert(checks==2);
        assert([files removeItemAtPath:[root stringByAppendingPathComponent:@"maps/ui.map"] error:nil]);
        assert(host_import_remove_source(url,root,root,YES,active)==HOST_IMPORT_SOURCE_KEPT);
        assert([files fileExistsAtPath:path]&&[files fileExistsAtPath:external]);
        assert([files removeItemAtPath:base error:nil]);
        puts("import source cleanup: passed");
    }
}
