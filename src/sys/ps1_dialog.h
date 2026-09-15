#ifndef PS1_DIALOG_H
#define PS1_DIALOG_H

#include "core/ps1_types.h"

/* Opens a native OS file-open dialog. On Windows it uses the common item
 * dialog (comdlg32) with an optional initial directory and extension filter
 * such as "*.obj". Returns 1 if the user picked a file (out filled), 0 if
 * cancelled or the platform has no dialog support. */
int ps1_dialog_open_file(const char* title, const char* filter_desc,
                         const char* filter_ext, const char* initial_dir,
                         char* out, u32 out_cap);

/* Opens a native OS save-as dialog (GetSaveFileNameA on Windows). Returns 1
 * if the user provided a path (out filled), 0 if cancelled. */
int ps1_dialog_save_file(const char* title, const char* filter_desc,
                         const char* filter_ext, const char* initial_dir,
                         char* out, u32 out_cap);

#endif