#ifndef FILEBROWSER_H
#define FILEBROWSER_H

#define MAX_FILES 256
#define MAX_PATH 512

typedef struct {
    char name[256];
    int is_directory;
    SceOff size;
} FileInfo;

typedef struct {
    FileInfo files[MAX_FILES];
    int file_count;
    int selected_index;
    char current_path[MAX_PATH];
    int selection_mask[MAX_FILES];
} FileBrowser;

int filebrowser_init(FileBrowser *fb, const char *start_path);
int filebrowser_refresh(FileBrowser *fb);
void filebrowser_navigate_up(FileBrowser *fb);
int filebrowser_navigate_down(FileBrowser *fb);
int filebrowser_enter(FileBrowser *fb);
void filebrowser_navigate_back(FileBrowser *fb);
const char *filebrowser_get_selected_path(FileBrowser *fb);

#endif
