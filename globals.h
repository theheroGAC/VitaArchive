/*
 * VitaArchive - File Archiver & Browser for PS Vita
 * Created by theheroGAC.
 * Special thanks to TheFloW, Rinnegatamante, SKGleba, and all developers, hackers,
 * and contributors of the PlayStation Vita homebrew scene.
 */
#ifndef GLOBALS_H
#define GLOBALS_H

#include <psp2/types.h>
#include <psp2/kernel/threadmgr.h>
#include <vita2d.h>
#include "filebrowser.h"
#include "zip.h"
#include "language.h"

#include <psp2/io/stat.h>

typedef enum {
    MODE_BROWSER,
    MODE_ARCHIVE_VIEW,
    MODE_EXTRACTING,
    MODE_INSTALLING,
    MODE_INFO,
    MODE_SETTINGS,
    MODE_DEST_BROWSER,
    MODE_ZIP_CREATION,
    MODE_SMART_INSTALL_CONFIRM,
    MODE_DELETE_CONFIRM,
    MODE_COMPRESS_FORMAT_SELECT,
    MODE_TEXT_PREVIEW,
    MODE_FTP_SERVER,
    MODE_ACTIONS_MENU,
    MODE_ARCHIVE_ACTIONS_MENU,
    MODE_INTEGRITY_RESULT,
    MODE_HASH_VIEW,
    MODE_HEX_VIEW,
    MODE_PROPERTIES_VIEW
} AppMode;

typedef enum {
    INSTALL_MODE_VPK,
    INSTALL_MODE_APP
} InstallMode;

typedef struct {
    int mode; 
    char dest[512];
    int index;
} WorkerArgs;

#define MAX_PREVIEW_LINES 1000
#define MAX_PREVIEW_LINE_LEN 96
#define MAX_ARCHIVE_STACK 8
#define CLIPBOARD_MAX_FILES 256

extern AppMode current_mode;
extern FileBrowser browser;
extern ArchiveInfo archive_info;
extern int extract_progress;
extern int scroll_offset;
extern int archive_scroll;

extern char preview_lines[MAX_PREVIEW_LINES][MAX_PREVIEW_LINE_LEN];
extern int preview_line_count;
extern int preview_scroll;
extern char preview_filename[256];
extern int archive_selected;

extern char archive_stack[MAX_ARCHIVE_STACK][MAX_PATH];
extern int archive_stack_selected[MAX_ARCHIVE_STACK];
extern int archive_stack_depth;
extern int install_success;
extern Language lang;
extern vita2d_pgf *font;
extern int settings_selected;
extern int compress_level;
extern int settings_item_selected;
extern int compress_format_selected;
extern char extraction_dest_path[MAX_PATH];
extern InstallMode current_install_mode;
extern int extract_selected_only;
extern int extract_file_index;

extern char clipboard_paths[CLIPBOARD_MAX_FILES][1024];
extern int clipboard_file_count;
extern int clipboard_is_move;

extern int archive_selection_mask[512];
extern int actions_menu_selected;
extern int archive_actions_selected;

extern uint64_t worker_processed_bytes;
extern uint64_t worker_total_bytes;
extern uint64_t worker_start_time;

extern SceUID worker_thread_id;
extern int worker_running;
extern int worker_result;
extern WorkerArgs worker_args;

extern char toast_msg[160];
extern uint64_t toast_expire;
extern uint32_t toast_color;

extern char ftp_ip[32];
extern unsigned short ftp_port;


extern uint32_t hex_offset;
extern uint64_t hex_file_size;
extern char hex_filepath[1024];


extern char hash_md5[33];
extern char hash_sha256[65];
extern char hash_filepath[1024];
extern volatile int hash_progress;
extern volatile int hash_cancel;


extern char prop_filepath[1024];
extern SceIoStat prop_stat;
extern int prop_selected_row;
extern int prop_checkboxes[6];

extern char preview_filepath[1024];
extern int preview_selected_line;
extern int preview_is_sfo;

#endif
