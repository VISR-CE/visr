/* Apache-2.0. Remove only an imported image owned by the app's private Documents folder. */
#ifndef HALO_HOST_IMPORT_SOURCE_H
#define HALO_HOST_IMPORT_SOURCE_H
#import <Foundation/Foundation.h>
#include <sys/stat.h>
#include "xiso.h"

enum host_import_source_result {
    HOST_IMPORT_SOURCE_KEPT,
    HOST_IMPORT_SOURCE_REMOVED,
    HOST_IMPORT_SOURCE_REMOVE_FAILED
};
/* private_documents is the system's private Documents directory, or nil on Mac platforms where the data root may
   belong to the player. Both spellings must be inside it: an external picker URL linking inward is still external. */
static enum host_import_source_result host_import_remove_source(NSURL *source, NSString *root,
    NSString *private_documents, BOOL imported, BOOL (^canceled)(void)) {
    if(!imported||!source.isFileURL||!root.isAbsolutePath||!private_documents.isAbsolutePath||!canceled||canceled())
        return HOST_IMPORT_SOURCE_KEPT;
    NSString *documents=root.stringByStandardizingPath;
    NSString *resolved=documents.stringByResolvingSymlinksInPath;
    if(![resolved isEqualToString:private_documents.stringByStandardizingPath.stringByResolvingSymlinksInPath])
        return HOST_IMPORT_SOURCE_KEPT;
    NSString *path=source.path.stringByStandardizingPath;
    if(![path hasPrefix:[documents stringByAppendingString:@"/"]]||
        ![path.stringByResolvingSymlinksInPath hasPrefix:[resolved stringByAppendingString:@"/"]])
        return HOST_IMPORT_SOURCE_KEPT;
    struct stat info;
    /* A directory or a symlink is never a disc image we own. Ancestor links must also stay inside Documents. */
    if(lstat(path.fileSystemRepresentation,&info)||!S_ISREG(info.st_mode))return HOST_IMPORT_SOURCE_KEPT;
    NSString *maps=[root stringByAppendingPathComponent:@"maps"];
    if(!xiso_maps_ready(maps.fileSystemRepresentation,NULL,0)||canceled())return HOST_IMPORT_SOURCE_KEPT;
    return [NSFileManager.defaultManager removeItemAtPath:path error:nil]?
        HOST_IMPORT_SOURCE_REMOVED:HOST_IMPORT_SOURCE_REMOVE_FAILED;
}
#endif
