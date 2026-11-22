/* The saved game's writer in the host (port/runtime/save_writer.h has the rules): the guest's
   imports, and what the host itself calls around them. */
#ifndef HOST_SAVE_WRITER_H
#define HOST_SAVE_WRITER_H

/* at startup, before the game: removes what a run that ended early left in <save_root>/z, and the checkpoint
   file builds before this one kept there (save_writer_clean) */
void host_save_writer_start(const char *save_root);
/* the guest is about to unlink path: a saved game being deleted is not written again behind it */
void host_save_writer_unlinking(const char *path);
/* waits, at most seconds, for a saved game that is being written */
void host_save_writer_wait(double seconds);

/* the guest's imports (guest_host.h): paths are host paths, as the guest's own file calls pass them */
int host_save_checkpoint_store(const void *image, unsigned int size, const char *session_path, const char *target_path);
int host_save_checkpoint_load(void *image, unsigned int size);
void host_save_withdraw_folder(const char *folder_path);
int host_save_write_now(const char *session_path, const char *target_path, const void *image, unsigned int size);

#endif
