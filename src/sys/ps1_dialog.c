#include <stdio.h>
#include <string.h>

#include "sys/ps1_dialog.h"

#ifdef _WIN32

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commdlg.h>

int ps1_dialog_open_file(const char* title, const char* filter_desc,
                         const char* filter_ext, const char* initial_dir,
                         char* out, u32 out_cap) {
    OPENFILENAMEA ofn;
    char buf[1024];
    char filter[256];
    if (!out || out_cap == 0) return 0;

    buf[0] = 0;
    snprintf(filter, sizeof filter, "%s%c%s%c%c",
             filter_desc ? filter_desc : "Files", 0,
             filter_ext ? filter_ext : "*.*", 0, 0);

    memset(&ofn, 0, sizeof ofn);
    ofn.lStructSize = sizeof ofn;
    ofn.hwndOwner = NULL;
    ofn.lpstrFilter = filter;
    ofn.lpstrFile = buf;
    ofn.nMaxFile = (DWORD)sizeof buf;
    ofn.lpstrTitle = title;
    ofn.lpstrInitialDir = (initial_dir && initial_dir[0]) ? initial_dir : NULL;
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_HIDEREADONLY | OFN_PATHMUSTEXIST;
    if (!GetOpenFileNameA(&ofn)) return 0;

    snprintf(out, out_cap, "%s", ofn.lpstrFile);
    return 1;
}

int ps1_dialog_save_file(const char* title, const char* filter_desc,
                         const char* filter_ext, const char* initial_dir,
                         char* out, u32 out_cap) {
    OPENFILENAMEA ofn;
    char buf[1024];
    char filter[256];
    size_t len;
    if (!out || out_cap == 0) return 0;

    buf[0] = 0;
    snprintf(filter, sizeof filter, "%s%c%s%c%c",
             filter_desc ? filter_desc : "Files", 0,
             filter_ext ? filter_ext : "*.*", 0, 0);

    memset(&ofn, 0, sizeof ofn);
    ofn.lStructSize = sizeof ofn;
    ofn.hwndOwner = NULL;
    ofn.lpstrFilter = filter;
    ofn.lpstrFile = buf;
    ofn.nMaxFile = (DWORD)sizeof buf;
    ofn.lpstrTitle = title;
    ofn.lpstrInitialDir = (initial_dir && initial_dir[0]) ? initial_dir : NULL;
    ofn.lpstrDefExt = filter_ext;
    ofn.Flags = OFN_OVERWRITEPROMPT | OFN_HIDEREADONLY | OFN_PATHMUSTEXIST;
    if (!GetSaveFileNameA(&ofn)) return 0;

    len = strlen(buf);
    if (filter_ext && filter_ext[0] == '*' && len > 0) {
        if (buf[len - 1] == '.' ) buf[len - 1] = 0;
    }
    snprintf(out, out_cap, "%s", buf);
    return 1;
}

#else

int ps1_dialog_open_file(const char* title, const char* filter_desc,
                         const char* filter_ext, const char* initial_dir,
                         char* out, u32 out_cap) {
    (void)title;
    (void)filter_desc;
    (void)filter_ext;
    (void)initial_dir;
    (void)out;
    (void)out_cap;
    return 0;
}

int ps1_dialog_save_file(const char* title, const char* filter_desc,
                         const char* filter_ext, const char* initial_dir,
                         char* out, u32 out_cap) {
    (void)title;
    (void)filter_desc;
    (void)filter_ext;
    (void)initial_dir;
    (void)out;
    (void)out_cap;
    return 0;
}

#endif