/*
 * VitaArchive - File Archiver & Browser for PS Vita
 * Created by theheroGAC.
 * Special thanks to TheFloW, Rinnegatamante, SKGleba, and all developers, hackers,
 * and contributors of the PlayStation Vita homebrew scene.
 */


#include <psp2/ctrl.h>
#include <psp2/kernel/processmgr.h>
#include <psp2/power.h>
#include <psp2/io/fcntl.h>
#include <psp2/io/dirent.h>
#include <psp2/io/stat.h>
#include <psp2/sysmodule.h>
#include <psp2/common_dialog.h>
#include <psp2/ime_dialog.h>
#include <psp2/io/devctl.h>

#include <vita2d.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <psp2/sysmodule.h>
#include <psp2/promoterutil.h>
#include <psp2/appmgr.h>

#include <sys/types.h>
#include <errno.h>
#include <sys/statvfs.h>

#include "globals.h"
#include "gui.h"
#include "settings.h"
#include "clipboard.h"
#include "hash.h"

pid_t waitpid(pid_t pid, int *status, int options) {
    errno = ENOSYS;
    return -1;
}

int dup2(int oldfd, int newfd) {
    errno = ENOSYS;
    return -1;
}

int execvp(const char *file, char *const argv[]) {
    errno = ENOSYS;
    return -1;
}

static void load_sce_paf() {
    uint32_t args[] = {
        0x00400000,
        0x0000ea60,
        0x00040000,
        0x00000000,
        0x00000001,
        0x00000000,
    };

    int result = 0xDEADBEEF;

    SceSysmoduleOpt opt = {0};
    opt.result = &result;

    sceSysmoduleLoadModuleInternalWithArg(SCE_SYSMODULE_INTERNAL_PAF, sizeof(args), args, &opt);
}

static void utf8_to_utf16(uint16_t *dst, uint32_t dst_max_chars, const uint8_t *src) {
    uint32_t i = 0;
    while (*src && i + 1 < dst_max_chars) {
        uint32_t cp = 0;
        if ((*src & 0x80) == 0) {
            cp = *src++;
        } else if ((*src & 0xE0) == 0xC0) {
            cp  = (*src++ & 0x1F) << 6;
            cp |= (*src++ & 0x3F);
        } else if ((*src & 0xF0) == 0xE0) {
            cp  = (*src++ & 0x0F) << 12;
            cp |= (*src++ & 0x3F) << 6;
            cp |= (*src++ & 0x3F);
        } else {
            src++;
            continue;
        }
        dst[i++] = (uint16_t)cp;
    }
    dst[i] = 0;
}

static void utf16_to_utf8(uint8_t *dst, uint32_t dst_max_bytes, const uint16_t *src) {
    uint32_t i = 0;
    while (*src) {
        uint32_t cp = *src++;
        if (cp < 0x80) {
            if (i + 1 >= dst_max_bytes) break;
            dst[i++] = (uint8_t)cp;
        } else if (cp < 0x800) {
            if (i + 2 >= dst_max_bytes) break;
            dst[i++] = 0xC0 | (uint8_t)(cp >> 6);
            dst[i++] = 0x80 | (uint8_t)(cp & 0x3F);
        } else {
            if (i + 3 >= dst_max_bytes) break;
            dst[i++] = 0xE0 | (uint8_t)(cp >> 12);
            dst[i++] = 0x80 | (uint8_t)((cp >> 6) & 0x3F);
            dst[i++] = 0x80 | (uint8_t)(cp & 0x3F);
        }
    }
    dst[i] = 0;
}

#include "zip.h"
#include "filebrowser.h"
#include "language.h"
#include "rar.h"
#include "archive7z.h"
#include "vpk.h"
#include "psarc.h"
#include "ftp.h"

#define SFO_MAGIC 0x46535000

typedef struct SfoHeader {
    uint32_t magic;
    uint32_t version;
    uint32_t keyofs;
    uint32_t valofs;
    uint32_t count;
} __attribute__((packed)) SfoHeader;

typedef struct SfoEntry {
    uint16_t nameofs;
    uint8_t  alignment;
    uint8_t  type;
    uint32_t valsize;
    uint32_t totalsize;
    uint32_t dataofs;
} __attribute__((packed)) SfoEntry;

AppMode current_mode = MODE_BROWSER;
AppMode preview_return_mode = MODE_ARCHIVE_VIEW;
FileBrowser browser;
ArchiveInfo archive_info;
int extract_progress = 0;
int scroll_offset = 0;
int archive_scroll = 0;

char preview_lines[MAX_PREVIEW_LINES][MAX_PREVIEW_LINE_LEN];
int preview_line_count = 0;
int preview_scroll = 0;
char preview_filename[256] = {0};
int archive_selected = 0;

char archive_stack[MAX_ARCHIVE_STACK][MAX_PATH];
int archive_stack_selected[MAX_ARCHIVE_STACK];
int archive_stack_depth = 0;
int install_success = 0;
Language lang;
vita2d_pgf *font = NULL;
int settings_selected = 0;
int compress_level = 1; 
int settings_item_selected = 0; 
int compress_format_selected = 0;
char extraction_dest_path[MAX_PATH];
InstallMode current_install_mode = INSTALL_MODE_VPK;
static int get_ime_input(char *output, int max_len, const char *title, const char *initial_text);
int extract_selected_only = 0;
int extract_file_index = -1;

char clipboard_paths[CLIPBOARD_MAX_FILES][1024];
int clipboard_file_count = 0;
int clipboard_is_move = 0;

int archive_selection_mask[512] = {0};
int actions_menu_selected = 0;
int archive_actions_selected = 0;

uint64_t worker_processed_bytes = 0;
uint64_t worker_total_bytes = 0;
uint64_t worker_start_time = 0;

SceUID worker_thread_id = -1;
int worker_running = 0;
int worker_result = 0;
WorkerArgs worker_args;

char toast_msg[160] = {0};
uint64_t toast_expire = 0;
uint32_t toast_color = 0;

char ftp_ip[32] = {0};
unsigned short ftp_port = 0;

uint32_t hex_offset = 0;
uint64_t hex_file_size = 0;
char hex_filepath[1024] = {0};

char hash_md5[33] = {0};
char hash_sha256[65] = {0};
char hash_filepath[1024] = {0};
volatile int hash_progress = 0;
volatile int hash_cancel = 0;


char prop_filepath[1024] = {0};
SceIoStat prop_stat;
int prop_selected_row = 0;
int prop_checkboxes[6] = {0};

char preview_filepath[1024] = {0};
int preview_selected_line = 0;
int preview_is_sfo = 0;
void *sfo_buffer = NULL;
int sfo_buffer_size = 0;





static void load_preview_file(const char *path) {
    preview_line_count = 0;
    preview_scroll = 0;
    preview_selected_line = 0;
    preview_is_sfo = 0;
    strncpy(preview_filepath, path, sizeof(preview_filepath) - 1);
    preview_filepath[sizeof(preview_filepath) - 1] = '\0';
    
    SceUID fd = sceIoOpen(path, SCE_O_RDONLY, 0);
    if (fd < 0) return;
    
    char buf[2048];
    int bytes_read;
    int line_len = 0;
    char line[MAX_PREVIEW_LINE_LEN];
    memset(line, 0, sizeof(line));
    
    while ((bytes_read = sceIoRead(fd, buf, sizeof(buf))) > 0 && preview_line_count < MAX_PREVIEW_LINES) {
        for (int i = 0; i < bytes_read && preview_line_count < MAX_PREVIEW_LINES; i++) {
            char c = buf[i];
            if (c == '\n') {
                line[line_len] = '\0';
                strncpy(preview_lines[preview_line_count], line, MAX_PREVIEW_LINE_LEN - 1);
                preview_lines[preview_line_count][MAX_PREVIEW_LINE_LEN - 1] = '\0';
                preview_line_count++;
                
                line_len = 0;
                memset(line, 0, sizeof(line));
            } else if (c == '\t') {
                for (int space = 0; space < 4 && line_len < MAX_PREVIEW_LINE_LEN - 1; space++) {
                    line[line_len++] = ' ';
                }
            } else if (c != '\r') {
                if (line_len < MAX_PREVIEW_LINE_LEN - 1) {
                    line[line_len++] = c;
                }
            }
        }
    }
    
    if (line_len > 0 && preview_line_count < MAX_PREVIEW_LINES) {
        line[line_len] = '\0';
        strncpy(preview_lines[preview_line_count], line, MAX_PREVIEW_LINE_LEN - 1);
        preview_lines[preview_line_count][MAX_PREVIEW_LINE_LEN - 1] = '\0';
        preview_line_count++;
    }
    
    sceIoClose(fd);
    if (strncmp(path, VPK_TEMP_DIR, strlen(VPK_TEMP_DIR)) == 0) {
        sceIoRemove(path);
    }
}

static void load_sfo_preview(const char *path) {
    preview_line_count = 0;
    preview_scroll = 0;
    preview_selected_line = 0;
    preview_is_sfo = 1;
    strncpy(preview_filepath, path, sizeof(preview_filepath) - 1);
    preview_filepath[sizeof(preview_filepath) - 1] = '\0';
    
    if (sfo_buffer) {
        free(sfo_buffer);
        sfo_buffer = NULL;
    }
    
    SceUID fd = sceIoOpen(path, SCE_O_RDONLY, 0);
    if (fd < 0) return;
    
    sfo_buffer_size = sceIoLseek(fd, 0, SCE_SEEK_END);
    sceIoLseek(fd, 0, SCE_SEEK_SET);
    
    if (sfo_buffer_size <= (int)sizeof(SfoHeader)) {
        sceIoClose(fd);
        if (strncmp(path, VPK_TEMP_DIR, strlen(VPK_TEMP_DIR)) == 0) {
            sceIoRemove(path);
        }
        return;
    }
    
    sfo_buffer = malloc(sfo_buffer_size);
    if (!sfo_buffer) {
        sceIoClose(fd);
        if (strncmp(path, VPK_TEMP_DIR, strlen(VPK_TEMP_DIR)) == 0) {
            sceIoRemove(path);
        }
        return;
    }
    
    if (sceIoRead(fd, sfo_buffer, sfo_buffer_size) != sfo_buffer_size) {
        free(sfo_buffer);
        sfo_buffer = NULL;
        sceIoClose(fd);
        if (strncmp(path, VPK_TEMP_DIR, strlen(VPK_TEMP_DIR)) == 0) {
            sceIoRemove(path);
        }
        return;
    }
    sceIoClose(fd);
    if (strncmp(path, VPK_TEMP_DIR, strlen(VPK_TEMP_DIR)) == 0) {
        sceIoRemove(path);
    }
    
    SfoHeader *header = (SfoHeader*)sfo_buffer;
    SfoEntry *entries = (SfoEntry*)((uintptr_t)sfo_buffer + sizeof(SfoHeader));
    
    if (header->magic != SFO_MAGIC) {
        free(sfo_buffer);
        sfo_buffer = NULL;
        return;
    }
    
    for (int i = 0; i < (int)header->count && preview_line_count < MAX_PREVIEW_LINES; i++) {
        const char *key = (const char *)sfo_buffer + header->keyofs + entries[i].nameofs;
        const char *val = (const char *)sfo_buffer + header->valofs + entries[i].dataofs;
        
        char line_buf[MAX_PREVIEW_LINE_LEN];
        if (entries[i].type == 2) {
            snprintf(line_buf, sizeof(line_buf), "%s: %s", key, val);
        } else if (entries[i].type == 4) {
            uint32_t int_val = *(uint32_t*)val;
            snprintf(line_buf, sizeof(line_buf), "%s: %u (0x%08X)", key, int_val, int_val);
        } else {
            snprintf(line_buf, sizeof(line_buf), "%s: [Binary]", key);
        }
        
        strncpy(preview_lines[preview_line_count], line_buf, MAX_PREVIEW_LINE_LEN - 1);
        preview_lines[preview_line_count][MAX_PREVIEW_LINE_LEN - 1] = '\0';
        preview_line_count++;
    }
}



static int try_open_archive(const char *path, ArchiveInfo *info) {
    if (is_zip_file(path) || is_vpk_file(path) ||
        is_tar_file(path) || is_gzip_file(path) ||
        is_bzip2_file(path)) {
        return zip_open(path, info);
    } else if (is_rar_file(path)) {
        return rar_open(path, info);
    } else if (is_7z_file(path)) {
        return archive7z_open(path, info);
    } else if (is_psarc_file(path)) {
        return psarc_open(path, info);
    }
    return -1;
}

static int try_open_archive_with_password(const char *path, ArchiveInfo *info) {
    int open_res = try_open_archive(path, info);
    while (open_res == -5) {
        char pass_buf[128] = {0};
        const char *prompt_title = language_get(&lang, STR_ENTER_PASSWORD);
        
        if (get_ime_input(pass_buf, 120, prompt_title, NULL) == 0 && strlen(pass_buf) > 0) {
            strncpy(info->password, pass_buf, sizeof(info->password) - 1);
            open_res = try_open_archive(path, info);
        } else {
            open_res = -1;
            break;
        }
    }
    return open_res;
}

static void get_name_without_extension(char *out, const char *filename) {
    const char *base = strrchr(filename, '/');
    if (!base) base = strrchr(filename, '\\');
    if (!base) base = filename;
    else base++;
    
    strcpy(out, base);
    char *ext = strrchr(out, '.');
    if (ext && ext != out) {
        *ext = '\0';
    }
}

static int get_ime_input(char *output, int max_len, const char *title, const char *initial_text) {
    SceImeDialogParam param;
    sceImeDialogParamInit(&param);

    uint16_t title_utf16[64];
    utf8_to_utf16(title_utf16, 64, (const uint8_t *)title);

    uint16_t initial_text_utf16[128] = {0};
    if (initial_text) {
        utf8_to_utf16(initial_text_utf16, 128, (const uint8_t *)initial_text);
    }
    uint16_t output_utf16[max_len + 1];
    memset(output_utf16, 0, sizeof(output_utf16));
    if (initial_text) {
        utf8_to_utf16(output_utf16, max_len, (const uint8_t *)initial_text);
    }

    param.sdkVersion = 0x03150021;
    param.supportedLanguages = 0;
    param.languagesForced = SCE_FALSE;
    param.type = SCE_IME_TYPE_DEFAULT;
    param.option = SCE_IME_OPTION_NO_ASSISTANCE;
    param.title = title_utf16;
    param.maxTextLength = max_len;
    param.initialText = initial_text_utf16;
    param.inputTextBuffer = output_utf16;

    int res = sceImeDialogInit(&param);
    if (res < 0) return -1;

    while (1) {
        vita2d_start_drawing();
        vita2d_clear_screen();
        if (current_mode == MODE_TEXT_PREVIEW) {
            draw_text_preview();
        } else {
            draw_browser();
            draw_browser_footer();
        }
        vita2d_end_drawing();
        vita2d_common_dialog_update();
        vita2d_swap_buffers();

        SceCommonDialogStatus status = sceImeDialogGetStatus();
        if (status == SCE_COMMON_DIALOG_STATUS_FINISHED) {
            SceImeDialogResult result;
            memset(&result, 0, sizeof(SceImeDialogResult));
            sceImeDialogGetResult(&result);
            if (result.button == SCE_IME_DIALOG_BUTTON_ENTER) {
                utf16_to_utf8((uint8_t *)output, (uint32_t)max_len, output_utf16);
            }
            sceImeDialogTerm();
            return (result.button == SCE_IME_DIALOG_BUTTON_ENTER) ? 0 : -1;
        }
    }
}

void archive_close_custom(ArchiveInfo *info) {
    if (!info || !info->is_open) return;
    if (is_zip_file(info->archive_path) || is_vpk_file(info->archive_path) ||
        is_tar_file(info->archive_path) || is_gzip_file(info->archive_path) ||
        is_bzip2_file(info->archive_path)) {
        zip_close(info);
    } else if (is_rar_file(info->archive_path)) {
        rar_close(info);
    } else if (is_7z_file(info->archive_path)) {
        archive7z_close(info);
    } else if (is_psarc_file(info->archive_path)) {
        psarc_close(info);
    }
}

void archive_cancel_custom(ArchiveInfo *info) {
    if (!info || !info->is_open) return;
    if (is_zip_file(info->archive_path) || is_vpk_file(info->archive_path) ||
        is_tar_file(info->archive_path) || is_gzip_file(info->archive_path) ||
        is_bzip2_file(info->archive_path)) {
        zip_cancel(info);
    } else if (is_rar_file(info->archive_path)) {
        rar_cancel(info);
    } else if (is_7z_file(info->archive_path)) {
        archive7z_cancel(info);
    } else if (is_psarc_file(info->archive_path)) {
        psarc_cancel(info);
    }
}

int archive_extract_all_custom(const char *dest, ArchiveInfo *info, int *progress) {
    if (!info || !info->is_open) return -1;
    if (is_zip_file(info->archive_path) || is_vpk_file(info->archive_path) ||
        is_tar_file(info->archive_path) || is_gzip_file(info->archive_path) ||
        is_bzip2_file(info->archive_path)) {
        return zip_extract_all(dest, info, progress);
    } else if (is_rar_file(info->archive_path)) {
        return rar_extract_all(dest, info, progress);
    } else if (is_7z_file(info->archive_path)) {
        return archive7z_extract_all(dest, info, progress);
    } else if (is_psarc_file(info->archive_path)) {
        return psarc_extract_all(dest, info, progress);
    }
    return -1;
}

int archive_extract_file_custom(const char *dest, int file_index, ArchiveInfo *info) {
    if (!info || !info->is_open) return -1;
    if (is_zip_file(info->archive_path) || is_vpk_file(info->archive_path) ||
        is_tar_file(info->archive_path) || is_gzip_file(info->archive_path) ||
        is_bzip2_file(info->archive_path)) {
        return zip_extract_file(dest, file_index, info);
    } else if (is_rar_file(info->archive_path)) {
        return rar_extract_file(dest, file_index, info);
    } else if (is_7z_file(info->archive_path)) {
        return archive7z_extract_file(dest, file_index, info);
    } else if (is_psarc_file(info->archive_path)) {
        return psarc_extract_file(dest, file_index, info);
    }
    return -1;
}

static int worker_thread_func(SceSize args, void *argp) {
    WorkerArgs *wargs = (WorkerArgs *)argp;
    worker_running = 1;
    worker_processed_bytes = 0;
    worker_start_time = sceKernelGetSystemTimeWide();
    
    if (wargs->mode == 0) {
        worker_total_bytes = archive_info.total_size;
        worker_result = archive_extract_all_custom(wargs->dest, &archive_info, &extract_progress);
    } else if (wargs->mode == 1) {
        if (wargs->index >= 0 && wargs->index < archive_info.file_count) {
            worker_total_bytes = archive_info.files[wargs->index].uncompressed_size;
            worker_result = archive_extract_file_custom(wargs->dest, wargs->index, &archive_info);
        } else {
            worker_result = -1;
        }
        extract_progress = 100;
    } else if (wargs->mode == 2) {
        worker_total_bytes = archive_info.total_size;
        worker_result = vpk_install_homebrew_from_archive(&archive_info, &extract_progress);
    } else if (wargs->mode == 3) {
        if (wargs->index >= 0 && wargs->index < archive_info.file_count) {
            worker_total_bytes = archive_info.files[wargs->index].uncompressed_size;
            worker_result = vpk_install_from_zip(&archive_info, wargs->index, &extract_progress);
        } else {
            worker_result = -1;
        }
        extract_progress = 100;
    } else if (wargs->mode == 4) {
        const char *files_to_add[MAX_FILES];
        int files_to_add_count = 0;
        static char file_paths[MAX_FILES][MAX_PATH];
        
        worker_total_bytes = 0;
        for (int i = 0; i < browser.file_count; i++) {
            if (browser.selection_mask[i]) {
                strncpy(file_paths[files_to_add_count], browser.current_path, MAX_PATH - 1);
                file_paths[files_to_add_count][MAX_PATH - 1] = '\0';
                strncat(file_paths[files_to_add_count], browser.files[i].name, MAX_PATH - strlen(file_paths[files_to_add_count]) - 1);
                
                SceIoStat stat;
                if (sceIoGetstat(file_paths[files_to_add_count], &stat) >= 0) {
                    worker_total_bytes += stat.st_size;
                }
                
                files_to_add[files_to_add_count] = file_paths[files_to_add_count];
                files_to_add_count++;
            }
        }
        
        worker_result = archive_create_custom_format(wargs->dest, files_to_add, files_to_add_count, wargs->index);
        extract_progress = 100;
        filebrowser_refresh(&browser);
    } else if (wargs->mode == 5) {
        worker_total_bytes = archive_info.total_size;
        worker_result = archive_test_integrity(&archive_info, &extract_progress);
    } else if (wargs->mode == 6) {
        worker_total_bytes = 0;
        for (int i = 0; i < clipboard_file_count; i++) {
            worker_total_bytes += get_path_total_size(clipboard_paths[i]);
        }
        
        worker_result = 0;
        for (int i = 0; i < clipboard_file_count; i++) {
            const char *src = clipboard_paths[i];
            const char *base = strrchr(src, '/');
            base = base ? base + 1 : src;
            
            char dest_path[2048];
            snprintf(dest_path, sizeof(dest_path), "%s/%s", wargs->dest, base);
            
            int res = copy_or_move_path_recursive(src, dest_path, clipboard_is_move);
            if (res < 0) {
                worker_result = res;
            }
        }
        if (clipboard_is_move) {
            clipboard_file_count = 0;
        }
        extract_progress = 100;
        filebrowser_refresh(&browser);
    } else if (wargs->mode == 7) {
        worker_total_bytes = 0;
        int count_to_extract = 0;
        for (int i = 0; i < archive_info.file_count; i++) {
            if (archive_selection_mask[i]) {
                worker_total_bytes += archive_info.files[i].uncompressed_size;
                count_to_extract++;
            }
        }
        
        worker_result = 0;
        int processed_files = 0;
        for (int i = 0; i < archive_info.file_count; i++) {
            if (archive_selection_mask[i]) {
                int res = archive_extract_file_custom(wargs->dest, i, &archive_info);
                if (res < 0) {
                    worker_result = res;
                }
                processed_files++;
                extract_progress = (processed_files * 100) / count_to_extract;
            }
        }
        extract_progress = 100;
    } else if (wargs->mode == 8) {
        worker_result = calculate_file_hashes(hash_filepath, hash_md5, hash_sha256, &hash_cancel, &hash_progress);
        hash_progress = 100;
    }
    
    worker_running = 0;
    return sceKernelExitDeleteThread(0);
}

static void ensure_system_folders() {
    sceIoMkdir("ux0:/app", 0777);
    sceIoMkdir("ux0:/appmeta", 0777);
    sceIoMkdir("ux0:/bgdl", 0777);
    sceIoMkdir("ux0:/data", 0777);
    sceIoMkdir("ux0:/license", 0777);
    sceIoMkdir("ux0:/license/app", 0777);
    sceIoMkdir("ux0:/patch", 0777);
    sceIoMkdir("ux0:/temp", 0777);
    sceIoMkdir("ux0:/user", 0777);
    sceIoMkdir("ux0:/user/00", 0777);
    sceIoMkdir("ux0:/user/00/savedata", 0777);
}

int main() {
    vita2d_init();
    vita2d_set_clear_color(RGBA8(13, 17, 23, 255));
    sceCtrlSetSamplingMode(SCE_CTRL_MODE_ANALOG);


    load_sce_paf();
    sceSysmoduleLoadModuleInternal(SCE_SYSMODULE_INTERNAL_PROMOTER_UTIL);
    scePromoterUtilityInit();

    sceSysmoduleLoadModule(SCE_SYSMODULE_PGF);
    font = vita2d_load_default_pgf();
    sceSysmoduleLoadModule(SCE_SYSMODULE_IME);
    if (!font) {
        vita2d_fini();
        sceKernelExitProcess(1);
        return 1;
    }
    
    language_init(&lang);
    load_settings();
    ensure_system_folders();
    filebrowser_init(&browser, "/");
    
    int old_buttons = 0;
    int repeat_count = 0;
    int repeat_button = 0;
    
    while (1) {
        sceKernelPowerTick(0);
        SceCtrlData pad;
        sceCtrlPeekBufferPositive(0, &pad, 1);
        int pressed = pad.buttons & ~old_buttons;
        old_buttons = pad.buttons;
        
        int repeat_event = 0;
        int active_repeat_buttons = pad.buttons & (SCE_CTRL_UP | SCE_CTRL_DOWN);
        if (active_repeat_buttons) {
            if (active_repeat_buttons == repeat_button) {
                repeat_count++;
            } else {
                repeat_button = active_repeat_buttons;
                repeat_count = 0;
            }
            if (repeat_count > 20 && (repeat_count - 20) % 6 == 0) {
                repeat_event = repeat_button;
            }
        } else {
            repeat_button = 0;
            repeat_count = 0;
        }
        
        switch (current_mode) {
            case MODE_BROWSER:
                if ((pressed | repeat_event) & SCE_CTRL_UP) {
                    filebrowser_navigate_up(&browser);
                } else if ((pressed | repeat_event) & SCE_CTRL_DOWN) {
                    filebrowser_navigate_down(&browser);
                } else if (pressed & SCE_CTRL_CROSS) {
                    int result = filebrowser_enter(&browser);
                    if (result == 0) {
                        browser.search_query[0] = '\0';
                        scroll_offset = 0;
                    } else if (result == 1) {
                        const char *selected = filebrowser_get_selected_path(&browser);
                        if (selected) {
                            const char *file_ext = strrchr(selected, '.');
                            int is_sfo = file_ext && strcasecmp(file_ext, ".sfo") == 0;
                            int is_txt = is_viewable_text_file(selected);
                            if (is_sfo || is_txt) {
                                if (is_sfo) {
                                    load_sfo_preview(selected);
                                } else {
                                    load_preview_file(selected);
                                }
                                current_mode = MODE_TEXT_PREVIEW;
                                preview_return_mode = MODE_BROWSER;
                                strncpy(preview_filename, path_basename(selected), sizeof(preview_filename) - 1);
                                preview_filename[sizeof(preview_filename) - 1] = '\0';
                            } else {
                                int open_res = try_open_archive_with_password(selected, &archive_info);
                                if (open_res >= 0) {
                                    current_mode = MODE_ARCHIVE_VIEW;
                                    archive_scroll = 0;
                                    archive_selected = 0;
                                    archive_stack_depth = 1;
                                    strncpy(archive_stack[0], selected, MAX_PATH - 1);
                                    archive_stack[0][MAX_PATH - 1] = '\0';
                                } else {
                                    show_toast(language_get(&lang, STR_CANNOT_OPEN), RGBA8(231, 76, 60, 255));
                                }
                            }
                        }
                    }
                } else if (pressed & SCE_CTRL_CIRCLE) {
                    if (strlen(browser.search_query) > 0) {
                        browser.search_query[0] = '\0';
                        filebrowser_refresh(&browser);
                    } else {
                        filebrowser_navigate_back(&browser);
                    }
                } else if (pressed & SCE_CTRL_SELECT) {
                    if (ftp_server_start(ftp_ip, &ftp_port)) {
                        current_mode = MODE_FTP_SERVER;
                        char ftp_toast[128];
                        snprintf(ftp_toast, sizeof(ftp_toast), "FTP: %s:%d", ftp_ip, ftp_port);
                        show_toast(ftp_toast, RGBA8(52, 152, 219, 255));
                    } else {
                        LanguageCode cl = language_get_current(&lang);
                        const char *emsg =
                            (cl == LANG_IT) ? "Errore avvio FTP" :
                            (cl == LANG_ES) ? "Error al iniciar FTP" :
                            (cl == LANG_FR) ? "Erreur demarrage FTP" :
                            (cl == LANG_DE) ? "FTP-Startfehler" :
                            "FTP server failed to start";
                        show_toast(emsg, RGBA8(231, 76, 60, 255));
                    }
                } else if (pressed & SCE_CTRL_START) {
                    settings_selected = language_get_current(&lang);
                    current_mode = MODE_SETTINGS;
                } else if (pressed & SCE_CTRL_TRIANGLE) {
                    actions_menu_selected = 0;
                    current_mode = MODE_ACTIONS_MENU;
                } else if (pressed & SCE_CTRL_SQUARE) {
                    if (browser.file_count > 0) {
                        current_mode = MODE_DELETE_CONFIRM;
                    }
                }
                
                int visible_files = (510 - 110) / 22;
                if (browser.selected_index < scroll_offset) scroll_offset = browser.selected_index;
                if (browser.selected_index >= scroll_offset + visible_files) 
                    scroll_offset = browser.selected_index - visible_files + 1;
                break;
                
            case MODE_ARCHIVE_VIEW:
                {
                int vpk_selected = archive_info.file_count > 0 &&
                    is_vpk_file(archive_info.files[archive_selected].filename);
                int smart_install_available = !vpk_selected && archive_can_smart_install(&archive_info);
                if ((pressed | repeat_event) & SCE_CTRL_UP) {
                    if (archive_selected > 0) {
                        archive_selected--;
                    } else if (archive_info.file_count > 0) {
                        archive_selected = archive_info.file_count - 1;
                    }
                } else if ((pressed | repeat_event) & SCE_CTRL_DOWN) {
                    if (archive_selected < archive_info.file_count - 1) {
                        archive_selected++;
                    } else {
                        archive_selected = 0;
                    }
                } else if (pressed & SCE_CTRL_CROSS) {
                    if (vpk_selected) {
                        current_mode = MODE_INSTALLING;
                        current_install_mode = INSTALL_MODE_VPK;
                        extract_progress = 0;
                    } else if (smart_install_available) {
                        current_mode = MODE_SMART_INSTALL_CONFIRM;
                    } else {
                        extract_selected_only = 1;
                        extract_file_index = archive_selected;
                        worker_args.mode = 1;
                        current_mode = MODE_DEST_BROWSER;
                        filebrowser_init(&browser, "ux0:/");
                        scroll_offset = 0;
                    }
                } else if (pressed & SCE_CTRL_CIRCLE) {
                    if (archive_stack_depth > 1) {
                        char current_nested_path[1024];
                        strncpy(current_nested_path, archive_stack[archive_stack_depth - 1], sizeof(current_nested_path) - 1);
                        archive_stack_depth--;
                        
                        archive_close_custom(&archive_info);
                        sceIoRemove(current_nested_path);
                        
                        const char *parent_path = archive_stack[archive_stack_depth - 1];
                        int open_res = try_open_archive_with_password(parent_path, &archive_info);
                        
                        if (open_res >= 0) {
                            archive_selected = archive_stack_selected[archive_stack_depth - 1];
                            archive_scroll = 0;
                        } else {
                            archive_stack_depth = 0;
                            current_mode = MODE_BROWSER;
                            filebrowser_refresh(&browser);
                        }
                    } else {
                        archive_close_custom(&archive_info);
                        archive_stack_depth = 0;
                        current_mode = MODE_BROWSER;
                        vpk_cleanup_dirs();
                        filebrowser_refresh(&browser);
                    }
                } else if (pressed & SCE_CTRL_SQUARE) {
                    if (archive_info.file_count > 0) {
                        archive_selection_mask[archive_selected] = !archive_selection_mask[archive_selected];
                    }
                } else if (pressed & SCE_CTRL_TRIANGLE) {
                    archive_actions_selected = 0;
                    current_mode = MODE_ARCHIVE_ACTIONS_MENU;
                } else if (pressed & SCE_CTRL_SELECT) {
                    const char *filename = archive_info.files[archive_selected].filename;
                    int is_txt = is_viewable_text_file(filename);
                    int is_arch = is_archive_file(filename);
                    
                    if (is_txt || is_arch) {

                        vita2d_start_drawing();
                        vita2d_clear_screen();
                        draw_archive_view();
                        draw_battery_status();
                        
                        int box_w = 420;
                        int box_h = 100;
                        int box_x = (960 - box_w) / 2;
                        int box_y = (544 - box_h) / 2;
                        
                        vita2d_draw_rectangle(box_x, box_y, box_w, box_h, RGBA8(22, 27, 34, 255));
                        vita2d_draw_rectangle(box_x, box_y, box_w, 1, RGBA8(31, 111, 235, 255));
                        
                        const char *msg = is_arch ? "Opening..." : "Loading...";
                        LanguageCode curr_lang = language_get_current(&lang);
                        if (curr_lang == LANG_IT) {
                            msg = is_arch ? "Apertura in corso..." : "Caricamento in corso...";
                        } else if (curr_lang == LANG_ES) {
                            msg = is_arch ? "Abriendo..." : "Cargando...";
                        } else if (curr_lang == LANG_FR) {
                            msg = is_arch ? "Ouverture..." : "Chargement...";
                        } else if (curr_lang == LANG_DE) {
                            msg = is_arch ? "Öffnen..." : "Laden...";
                        } else if (curr_lang == LANG_JPN) {
                            msg = is_arch ? "開いています..." : "読み込んでいます...";
                        }
                        
                        int msg_w = vita2d_pgf_text_width(font, 1.0f, msg);
                        vita2d_pgf_draw_textf(font, box_x + (box_w - msg_w) / 2, box_y + 55, RGBA8(240, 246, 252, 255), 1.0f, "%s", msg);
                        
                        vita2d_end_drawing();
                        vita2d_swap_buffers();
                        
                        if (is_txt) {
                            char temp_dir[512];
                            snprintf(temp_dir, sizeof(temp_dir), "%s/", VPK_TEMP_DIR);
                            
   
                            sceIoMkdir(VPK_TEMP_DIR, 0777);
                            int ext_res = archive_extract_file_custom(temp_dir, archive_selected, &archive_info);
                            if (ext_res == 0) {
                                char real_file_path[1024];
                                snprintf(real_file_path, sizeof(real_file_path), "%s/%s", VPK_TEMP_DIR, filename);
                                
                                const char *file_ext = strrchr(filename, '.');
                                if (file_ext && strcasecmp(file_ext, ".sfo") == 0) {
                                    load_sfo_preview(real_file_path);
                                } else {
                                    load_preview_file(real_file_path);
                                }
                                current_mode = MODE_TEXT_PREVIEW;
                                strncpy(preview_filename, path_basename(filename), sizeof(preview_filename) - 1);
                                preview_filename[sizeof(preview_filename) - 1] = '\0';
                            }
                        } else if (is_arch) {
                            if (archive_stack_depth < MAX_ARCHIVE_STACK) {
                                char temp_dir[512];
                                snprintf(temp_dir, sizeof(temp_dir), "%s/", VPK_TEMP_DIR);
                                
                                sceIoMkdir(VPK_TEMP_DIR, 0777);
                                int ext_res = archive_extract_file_custom(temp_dir, archive_selected, &archive_info);
                                if (ext_res == 0) {
                                    char real_file_path[1024];
                                    snprintf(real_file_path, sizeof(real_file_path), "%s/%s", VPK_TEMP_DIR, filename);
                                    
                                    char nested_temp_path[1024];
                                    const char *dot = strrchr(filename, '.');
                                    snprintf(nested_temp_path, sizeof(nested_temp_path), "%s/nested_lvl%d%s", 
                                        VPK_TEMP_DIR, archive_stack_depth, dot ? dot : ".zip");
                                        
                                    sceIoRename(real_file_path, nested_temp_path);
                                    
                                    archive_stack_selected[archive_stack_depth - 1] = archive_selected;
                                    strncpy(archive_stack[archive_stack_depth], nested_temp_path, MAX_PATH - 1);
                                    archive_stack[archive_stack_depth][MAX_PATH - 1] = '\0';
                                    archive_stack_depth++;
                                    
                                    archive_close_custom(&archive_info);
                                    
                                    int open_res = try_open_archive_with_password(nested_temp_path, &archive_info);
                                    
                                    if (open_res >= 0) {
                                        archive_selected = 0;
                                        archive_scroll = 0;
                                    } else {
                                        archive_stack_depth--;
                                        sceIoRemove(nested_temp_path);
                                        
                                        const char *parent_path = archive_stack[archive_stack_depth - 1];
                                        try_open_archive_with_password(parent_path, &archive_info);
                                    }
                                }
                            }
                        }
                    }
                }

                int visible_archive_files = (510 - 110) / 22;
                if (archive_selected < archive_scroll) {
                    archive_scroll = archive_selected;
                }
                if (archive_selected >= archive_scroll + visible_archive_files) {
                    archive_scroll = archive_selected - visible_archive_files + 1;
                }
                }
                break;
                
            case MODE_EXTRACTING: {
                static int thread_started = 0;
                static int active_mode = 0;
                if (!thread_started && !worker_running) {
                    thread_started = 1;
                    extract_progress = 0;
                    active_mode = worker_args.mode;
                    
                    if (active_mode == 0 || active_mode == 1 || active_mode == 7) {
                        sceIoMkdir(extraction_dest_path, 0777);
                        if (active_mode != 7) {
                            worker_args.mode = extract_selected_only ? 1 : 0;
                        }
                        strncpy(worker_args.dest, extraction_dest_path, sizeof(worker_args.dest) - 1);
                        worker_args.dest[sizeof(worker_args.dest) - 1] = '\0';
                        worker_args.index = extract_file_index;
                    }
                    
                    worker_thread_id = sceKernelCreateThread("extract_worker", worker_thread_func, 0x10000100, 0x10000, 0, 0, NULL);
                    if (worker_thread_id >= 0) {
                        sceKernelStartThread(worker_thread_id, sizeof(WorkerArgs), &worker_args);
                    } else {
                        thread_started = 0;
                        current_mode = (active_mode == 4 || active_mode == 6) ? MODE_BROWSER : MODE_ARCHIVE_VIEW;
                    }
                } else if (thread_started && !worker_running) {
                    thread_started = 0;
                    extract_progress = 0;
                    LanguageCode cl = language_get_current(&lang);
                    
                    if (active_mode == 4) {
                        current_mode = MODE_BROWSER;
                        if (worker_result == 0) {
                            const char *ok =
                                (cl == LANG_IT) ? "Archivio creato con successo" :
                                (cl == LANG_ES) ? "Archivo creado con exito" :
                                (cl == LANG_FR) ? "Archive cree avec succes" :
                                (cl == LANG_DE) ? "Archiv erfolgreich erstellt" :
                                "Archive created successfully";
                            show_toast(ok, RGBA8(46, 204, 113, 255));
                        } else {
                            const char *err =
                                (cl == LANG_IT) ? "Errore creazione archivio" :
                                (cl == LANG_ES) ? "Error al crear el archivo" :
                                (cl == LANG_FR) ? "Erreur creation archive" :
                                (cl == LANG_DE) ? "Fehler beim Erstellen des Archivs" :
                                "Archive creation failed";
                            show_toast(err, RGBA8(231, 76, 60, 255));
                        }
                    } else if (active_mode == 5) {
                        current_mode = MODE_INTEGRITY_RESULT;
                    } else if (active_mode == 6) {
                        current_mode = MODE_BROWSER;
                        if (worker_result == 0) {
                            show_toast(language_get(&lang, STR_TOAST_PASTE_SUCCESS), RGBA8(46, 204, 113, 255));
                        } else {
                            show_toast(language_get(&lang, STR_TOAST_PASTE_FAIL), RGBA8(231, 76, 60, 255));
                        }
                    } else {
                        current_mode = MODE_ARCHIVE_VIEW;
                        if (worker_result == 0) {
                            const char *ok =
                                (cl == LANG_IT) ? "Estrazione completata" :
                                (cl == LANG_ES) ? "Extraccion completada" :
                                (cl == LANG_FR) ? "Extraction terminee" :
                                (cl == LANG_DE) ? "Entpacken abgeschlossen" :
                                "Extraction complete";
                            show_toast(ok, RGBA8(46, 204, 113, 255));
                        } else {
                            char err_msg[256];
                            if (cl == LANG_IT) {
                                snprintf(err_msg, sizeof(err_msg), "Errore estrazione: 0x%08X", worker_result);
                            } else if (cl == LANG_ES) {
                                snprintf(err_msg, sizeof(err_msg), "Error de extraccion: 0x%08X", worker_result);
                            } else if (cl == LANG_FR) {
                                snprintf(err_msg, sizeof(err_msg), "Erreur d'extraction: 0x%08X", worker_result);
                            } else if (cl == LANG_DE) {
                                snprintf(err_msg, sizeof(err_msg), "Entpackfehler: 0x%08X", worker_result);
                            } else {
                                snprintf(err_msg, sizeof(err_msg), "Extraction error: 0x%08X", worker_result);
                            }
                            show_toast(err_msg, RGBA8(231, 76, 60, 255));
                        }
                    }
                }
                
                if (pressed & SCE_CTRL_SQUARE) {
                    archive_cancel_custom(&archive_info);
                }
                break;
            }
                
            case MODE_INSTALLING: {
                static int thread_started = 0;
                static int install_done_frames = 0;

                if (!thread_started && !worker_running && install_done_frames == 0) {
                    thread_started = 1;
                    extract_progress = 0;
                    worker_args.mode = (current_install_mode == INSTALL_MODE_APP) ? 2 : 3;
                    worker_args.index = archive_selected;
                    
                    worker_thread_id = sceKernelCreateThread("install_worker", worker_thread_func, 0x10000100, 0x10000, 0, 0, NULL);
                    if (worker_thread_id >= 0) {
                        sceKernelStartThread(worker_thread_id, sizeof(WorkerArgs), &worker_args);
                    } else {
                        thread_started = 0;
                        current_mode = MODE_ARCHIVE_VIEW;
                    }
                } else if (thread_started && !worker_running && install_done_frames == 0) {
                    thread_started = 0;
                    install_success = (worker_result == 0);
                    extract_progress = 100;
                    install_done_frames = 90;
                    LanguageCode cl = language_get_current(&lang);
                    if (install_success) {
                        const char *ok =
                            (cl == LANG_IT) ? "VPK installato con successo!" :
                            (cl == LANG_ES) ? "VPK instalado con exito!" :
                            (cl == LANG_FR) ? "VPK installe avec succes!" :
                            (cl == LANG_DE) ? "VPK erfolgreich installiert!" :
                            "VPK installed successfully!";
                        show_toast(ok, RGBA8(46, 204, 113, 255));
                    } else {
                        const char *err =
                            (cl == LANG_IT) ? "Installazione VPK fallita" :
                            (cl == LANG_ES) ? "Error al instalar VPK" :
                            (cl == LANG_FR) ? "Echec installation VPK" :
                            (cl == LANG_DE) ? "VPK-Installation fehlgeschlagen" :
                            "VPK installation failed";
                        show_toast(err, RGBA8(231, 76, 60, 255));
                    }
                } else if (install_done_frames > 0) {
                    install_done_frames--;
                    if (install_done_frames == 0) {
                        current_mode = MODE_ARCHIVE_VIEW;
                    }
                }
                break;
            }

            case MODE_SMART_INSTALL_CONFIRM:
                if (pressed & SCE_CTRL_CROSS) {
                    current_mode = MODE_INSTALLING;
                    current_install_mode = INSTALL_MODE_APP;
                    extract_progress = 0;
                } else if (pressed & SCE_CTRL_SQUARE) {
                    current_mode = MODE_DEST_BROWSER;
                    filebrowser_init(&browser, "ux0:/");
                    scroll_offset = 0;
                } else if (pressed & SCE_CTRL_CIRCLE) {
                    current_mode = MODE_ARCHIVE_VIEW;
                }
                break;

            case MODE_INFO:
                if (pressed & SCE_CTRL_CIRCLE) {
                    current_mode = MODE_ARCHIVE_VIEW;
                }
                break;

            case MODE_SETTINGS:
                if ((pressed | repeat_event) & SCE_CTRL_UP) {
                    if (settings_item_selected > 0) settings_item_selected--;
                } else if ((pressed | repeat_event) & SCE_CTRL_DOWN) {
                    if (settings_item_selected < 1) settings_item_selected++;
                } else if ((pressed | repeat_event) & SCE_CTRL_LEFT) {
                    if (settings_item_selected == 0) {
                        if (settings_selected > 0) settings_selected--;
                        else settings_selected = LANG_COUNT - 1;
                        language_set(&lang, (LanguageCode)settings_selected);
                    } else {
                        if (compress_level > 0) compress_level--;
                        else compress_level = 2;
                    }
                } else if ((pressed | repeat_event) & SCE_CTRL_RIGHT) {
                    if (settings_item_selected == 0) {
                        if (settings_selected < LANG_COUNT - 1) settings_selected++;
                        else settings_selected = 0;
                        language_set(&lang, (LanguageCode)settings_selected);
                    } else {
                        if (compress_level < 2) compress_level++;
                        else compress_level = 0;
                    }
                } else if (pressed & SCE_CTRL_CIRCLE) {
                    save_settings();
                    current_mode = MODE_BROWSER;
                }
                break;

            case MODE_DEST_BROWSER:
                if ((pressed | repeat_event) & SCE_CTRL_UP) filebrowser_navigate_up(&browser);
                if ((pressed | repeat_event) & SCE_CTRL_DOWN) filebrowser_navigate_down(&browser);
                if (pressed & SCE_CTRL_CROSS) filebrowser_enter(&browser);
                if (pressed & SCE_CTRL_CIRCLE) filebrowser_navigate_back(&browser);

                if (pressed & SCE_CTRL_SQUARE) {
                    char folder_name[256] = {0};
                    if (worker_args.mode == 0 || worker_args.mode == 7) {
                        get_name_without_extension(folder_name, archive_info.archive_path);
                    } else if (worker_args.mode == 1) {
                        if (extract_file_index >= 0 && extract_file_index < archive_info.file_count) {
                            get_name_without_extension(folder_name, archive_info.files[extract_file_index].filename);
                        }
                    }
                    
                    snprintf(extraction_dest_path, sizeof(extraction_dest_path), "%s", browser.current_path);
                    if (folder_name[0] != '\0') {
                        size_t len = strlen(extraction_dest_path);
                        if (len > 0 && extraction_dest_path[len - 1] != '/') {
                            strcat(extraction_dest_path, "/");
                        }
                        strcat(extraction_dest_path, folder_name);
                        strcat(extraction_dest_path, "/");
                    }
                    
                    current_mode = MODE_EXTRACTING;
                    extract_progress = 0;
                }

                int visible_files_dest = (510 - 110) / 22;
                if (browser.selected_index < scroll_offset) scroll_offset = browser.selected_index;
                if (browser.selected_index >= scroll_offset + visible_files_dest) scroll_offset = browser.selected_index - visible_files_dest + 1;
                break;

            case MODE_DELETE_CONFIRM:
                if (pressed & SCE_CTRL_CROSS) {
                    const char *path = filebrowser_get_selected_path(&browser);
                    if (path) {
                        remove_path_recursive(path);
                        filebrowser_refresh(&browser);
                    }
                    current_mode = MODE_BROWSER;
                } else if (pressed & SCE_CTRL_CIRCLE) {
                    current_mode = MODE_BROWSER;
                }
                break;

            case MODE_ZIP_CREATION:
                if ((pressed | repeat_event) & SCE_CTRL_UP) filebrowser_navigate_up(&browser);
                if ((pressed | repeat_event) & SCE_CTRL_DOWN) filebrowser_navigate_down(&browser);
                if (pressed & SCE_CTRL_CROSS) {
                    int result = filebrowser_enter(&browser);
                    if (result == 0) {
                        browser.search_query[0] = '\0';
                        scroll_offset = 0;
                    }
                }
                if (pressed & SCE_CTRL_CIRCLE) {
                    current_mode = MODE_BROWSER;
                }

                if (pressed & SCE_CTRL_SQUARE) {
                    if (browser.file_count > 0) {
                        browser.selection_mask[browser.selected_index] = !browser.selection_mask[browser.selected_index];
                    }
                }

                if (pressed & SCE_CTRL_TRIANGLE) {
                    int has_selection = 0;
                    for (int i = 0; i < browser.file_count; i++) {
                        if (browser.selection_mask[i]) { has_selection = 1; break; }
                    }
                    if (!has_selection && browser.file_count > 0) {
                        LanguageCode cl = language_get_current(&lang);
                        const char *wmsg =
                            (cl == LANG_IT) ? "Nessun file selezionato - uso file corrente" :
                            (cl == LANG_ES) ? "Ninguno seleccionado - usando archivo actual" :
                            (cl == LANG_FR) ? "Rien selectionne - fichier actuel utilise" :
                            (cl == LANG_DE) ? "Nichts ausgewaehlt - aktuelle Datei wird verwendet" :
                            "No selection - using current file";
                        show_toast(wmsg, RGBA8(230, 126, 34, 255));
                        browser.selection_mask[browser.selected_index] = 1;
                    }
                    compress_format_selected = 0;
                    current_mode = MODE_COMPRESS_FORMAT_SELECT;
                }

                int visible_files_zip = (510 - 134) / 22;
                if (browser.selected_index < scroll_offset) scroll_offset = browser.selected_index;
                if (browser.selected_index >= scroll_offset + visible_files_zip) {
                    scroll_offset = browser.selected_index - visible_files_zip + 1;
                }
                break;

            case MODE_COMPRESS_FORMAT_SELECT:
                if ((pressed | repeat_event) & SCE_CTRL_UP) {
                    if (compress_format_selected > 0) compress_format_selected--;
                    else compress_format_selected = 4;
                } else if ((pressed | repeat_event) & SCE_CTRL_DOWN) {
                    if (compress_format_selected < 4) compress_format_selected++;
                    else compress_format_selected = 0;
                } else if (pressed & SCE_CTRL_CROSS) {
                    char archive_name[256] = {0};
                    const char *extensions[] = {".zip", ".7z", ".tar", ".tar.gz", ".tar.bz2"};
                    char prompt[128];
                    snprintf(prompt, sizeof(prompt), "Enter archive name (%s will be added)", extensions[compress_format_selected]);
                    
                    if (get_ime_input(archive_name, 250, prompt, NULL) == 0 && strlen(archive_name) > 0) {
                        char archive_path[1024];
                        char base_path[512];
                        
                        strncpy(base_path, browser.current_path, MAX_PATH - 1);
                        base_path[MAX_PATH - 1] = '\0';
                        strncat(base_path, archive_name, MAX_PATH - strlen(base_path) - 16);
                        
                        int counter = 0;
                        while (1) {
                            if (counter == 0) {
                                snprintf(archive_path, sizeof(archive_path), "%s%s", base_path, extensions[compress_format_selected]);
                            } else {
                                snprintf(archive_path, sizeof(archive_path), "%s (%d)%s", base_path, counter, extensions[compress_format_selected]);
                            }
                            
                            SceIoStat stat;
                            if (sceIoGetstat(archive_path, &stat) < 0) {
                                break;
                            }
                            counter++;
                        }
                        
                        strncpy(worker_args.dest, archive_path, sizeof(worker_args.dest) - 1);
                        worker_args.dest[sizeof(worker_args.dest) - 1] = '\0';
                        worker_args.mode = 4;
                        worker_args.index = compress_format_selected;
                        
                        current_mode = MODE_EXTRACTING;
                        extract_progress = 0;
                    } else {
                        current_mode = MODE_ZIP_CREATION;
                    }
                } else if (pressed & SCE_CTRL_CIRCLE) {
                    current_mode = MODE_ZIP_CREATION;
                }
                break;

            case MODE_TEXT_PREVIEW:
                {
                    int visible_lines = (510 - 65) / 22;
                    int preview_is_local = (strncmp(preview_filepath, VPK_TEMP_DIR, strlen(VPK_TEMP_DIR)) != 0);
                    
                    if ((pressed | repeat_event) & SCE_CTRL_UP) {
                        if (preview_selected_line > 0) {
                            preview_selected_line--;
                            if (preview_selected_line < preview_scroll) {
                                preview_scroll = preview_selected_line;
                            }
                        }
                    } else if ((pressed | repeat_event) & SCE_CTRL_DOWN) {
                        if (preview_selected_line < preview_line_count - 1) {
                            preview_selected_line++;
                            if (preview_selected_line >= preview_scroll + visible_lines) {
                                preview_scroll = preview_selected_line - visible_lines + 1;
                            }
                        }
                    } else if (pressed & SCE_CTRL_LEFT) {
                        preview_selected_line -= visible_lines;
                        if (preview_selected_line < 0) preview_selected_line = 0;
                        preview_scroll -= visible_lines;
                        if (preview_scroll < 0) preview_scroll = 0;
                    } else if (pressed & SCE_CTRL_RIGHT) {
                        preview_selected_line += visible_lines;
                        if (preview_selected_line >= preview_line_count) preview_selected_line = preview_line_count - 1;
                        if (preview_selected_line < 0) preview_selected_line = 0;
                        preview_scroll += visible_lines;
                        if (preview_scroll + visible_lines >= preview_line_count) {
                            preview_scroll = preview_line_count - visible_lines;
                            if (preview_scroll < 0) preview_scroll = 0;
                        }
                    } else if (pressed & SCE_CTRL_CIRCLE) {
                        current_mode = preview_return_mode;
                        if (sfo_buffer) {
                            free(sfo_buffer);
                            sfo_buffer = NULL;
                        }
                    } else if (pressed & SCE_CTRL_CROSS) {
                        if (preview_is_local && preview_line_count > 0) {
                            if (preview_is_sfo) {
                                SfoHeader *header = (SfoHeader*)sfo_buffer;
                                SfoEntry *entries = (SfoEntry*)((uintptr_t)sfo_buffer + sizeof(SfoHeader));
                                int idx = preview_selected_line;
                                if (idx >= 0 && idx < (int)header->count) {
                                    const char *key = (const char *)sfo_buffer + header->keyofs + entries[idx].nameofs;
                                    char *val = (char *)sfo_buffer + header->valofs + entries[idx].dataofs;
                                    
                                    if (entries[idx].type == 2) {
                                        char new_val[256];
                                        memset(new_val, 0, sizeof(new_val));
                                        if (get_ime_input(new_val, entries[idx].totalsize - 1, key, val) == 0) {
                                            strncpy(val, new_val, entries[idx].totalsize - 1);
                                            val[entries[idx].totalsize - 1] = '\0';
                                            snprintf(preview_lines[idx], MAX_PREVIEW_LINE_LEN, "%s: %s", key, val);
                                        }
                                    } else if (entries[idx].type == 4) {
                                        char new_val[32];
                                        memset(new_val, 0, sizeof(new_val));
                                        uint32_t current_int = *(uint32_t*)val;
                                        char initial_txt[32];
                                        snprintf(initial_txt, sizeof(initial_txt), "%u", current_int);
                                        if (get_ime_input(new_val, 10, key, initial_txt) == 0) {
                                            uint32_t parsed_int = (uint32_t)strtoul(new_val, NULL, 10);
                                            *(uint32_t*)val = parsed_int;
                                            snprintf(preview_lines[idx], MAX_PREVIEW_LINE_LEN, "%s: %u (0x%08X)", key, parsed_int, parsed_int);
                                        }
                                    }
                                }
                            } else {
                                char edited_line[MAX_PREVIEW_LINE_LEN];
                                memset(edited_line, 0, sizeof(edited_line));
                                if (get_ime_input(edited_line, MAX_PREVIEW_LINE_LEN - 1, "Edit Line", preview_lines[preview_selected_line]) == 0) {
                                    strncpy(preview_lines[preview_selected_line], edited_line, MAX_PREVIEW_LINE_LEN - 1);
                                    preview_lines[preview_selected_line][MAX_PREVIEW_LINE_LEN - 1] = '\0';
                                }
                            }
                        }
                    } else if (pressed & SCE_CTRL_TRIANGLE) {
                        if (preview_is_local && preview_line_count > 0) {
                            if (preview_is_sfo) {
                                SceUID fd_out = sceIoOpen(preview_filepath, SCE_O_WRONLY | SCE_O_CREAT | SCE_O_TRUNC, 0777);
                                if (fd_out >= 0) {
                                    sceIoWrite(fd_out, sfo_buffer, sfo_buffer_size);
                                    sceIoClose(fd_out);
                                    LanguageCode cl = language_get_current(&lang);
                                    const char *msg = (cl == LANG_IT) ? "File SFO salvato!" : "SFO file saved!";
                                    show_toast(msg, RGBA8(46, 204, 113, 255));
                                } else {
                                    LanguageCode cl = language_get_current(&lang);
                                    const char *msg = (cl == LANG_IT) ? "Errore salvataggio!" : "Error saving file!";
                                    show_toast(msg, RGBA8(231, 76, 60, 255));
                                }
                            } else {
                                SceUID fd_out = sceIoOpen(preview_filepath, SCE_O_WRONLY | SCE_O_CREAT | SCE_O_TRUNC, 0777);
                                if (fd_out >= 0) {
                                    for (int i = 0; i < preview_line_count; i++) {
                                        sceIoWrite(fd_out, preview_lines[i], strlen(preview_lines[i]));
                                        sceIoWrite(fd_out, "\r\n", 2);
                                    }
                                    sceIoClose(fd_out);
                                    LanguageCode cl = language_get_current(&lang);
                                    const char *msg = (cl == LANG_IT) ? "File salvato!" : "File saved!";
                                    show_toast(msg, RGBA8(46, 204, 113, 255));
                                } else {
                                    LanguageCode cl = language_get_current(&lang);
                                    const char *msg = (cl == LANG_IT) ? "Errore salvataggio!" : "Error saving file!";
                                    show_toast(msg, RGBA8(231, 76, 60, 255));
                                }
                            }
                        }
                    } else if (pressed & SCE_CTRL_SQUARE) {
                        if (preview_is_local && !preview_is_sfo && preview_line_count < MAX_PREVIEW_LINES) {
                            char new_line[MAX_PREVIEW_LINE_LEN];
                            memset(new_line, 0, sizeof(new_line));
                            if (get_ime_input(new_line, MAX_PREVIEW_LINE_LEN - 1, "Insert Line", "") == 0) {
                                int insert_idx = (preview_line_count > 0) ? preview_selected_line + 1 : 0;
                                for (int i = preview_line_count; i > insert_idx; i--) {
                                    strncpy(preview_lines[i], preview_lines[i - 1], MAX_PREVIEW_LINE_LEN - 1);
                                    preview_lines[i][MAX_PREVIEW_LINE_LEN - 1] = '\0';
                                }
                                strncpy(preview_lines[insert_idx], new_line, MAX_PREVIEW_LINE_LEN - 1);
                                preview_lines[insert_idx][MAX_PREVIEW_LINE_LEN - 1] = '\0';
                                preview_line_count++;
                                if (preview_line_count > 1) {
                                    preview_selected_line = insert_idx;
                                    if (preview_selected_line >= preview_scroll + visible_lines) {
                                        preview_scroll = preview_selected_line - visible_lines + 1;
                                    }
                                }
                            }
                        }
                    } else if (pressed & SCE_CTRL_LTRIGGER) {
                        if (preview_is_local && !preview_is_sfo && preview_line_count > 0) {
                            for (int i = preview_selected_line; i < preview_line_count - 1; i++) {
                                strncpy(preview_lines[i], preview_lines[i + 1], MAX_PREVIEW_LINE_LEN - 1);
                                preview_lines[i][MAX_PREVIEW_LINE_LEN - 1] = '\0';
                            }
                            preview_line_count--;
                            if (preview_selected_line >= preview_line_count && preview_line_count > 0) {
                                preview_selected_line = preview_line_count - 1;
                            }
                            if (preview_scroll > 0 && preview_selected_line < preview_scroll) {
                                preview_scroll = preview_selected_line;
                            }
                        }
                    }
                }
                break;

            case MODE_FTP_SERVER:
                if (pressed & SCE_CTRL_CIRCLE) {
                    ftp_server_stop();
                    current_mode = MODE_BROWSER;
                filebrowser_refresh(&browser);
                }
                break;
            
            case MODE_ACTIONS_MENU:
                if ((pressed | repeat_event) & SCE_CTRL_UP) {
                    if (actions_menu_selected > 0) actions_menu_selected--;
                    else actions_menu_selected = 12;
                } else if ((pressed | repeat_event) & SCE_CTRL_DOWN) {
                    if (actions_menu_selected < 12) actions_menu_selected++;
                    else actions_menu_selected = 0;
                } else if (pressed & SCE_CTRL_CIRCLE) {
                    current_mode = MODE_BROWSER;
                } else if (pressed & SCE_CTRL_CROSS) {
                    if (actions_menu_selected == 0) {
                        clipboard_file_count = 0;
                        int has_sel = 0;
                        for (int i = 0; i < browser.file_count; i++) {
                            if (browser.selection_mask[i]) {
                                snprintf(clipboard_paths[clipboard_file_count], sizeof(clipboard_paths[0]), "%s/%s", browser.current_path, browser.files[i].name);
                                clipboard_file_count++;
                                has_sel = 1;
                            }
                        }
                        if (!has_sel && browser.file_count > 0) {
                            snprintf(clipboard_paths[0], sizeof(clipboard_paths[0]), "%s/%s", browser.current_path, browser.files[browser.selected_index].name);
                            clipboard_file_count = 1;
                        }
                        clipboard_is_move = 0;
                        show_toast(language_get(&lang, STR_TOAST_COPIED), RGBA8(52, 152, 219, 255));
                        current_mode = MODE_BROWSER;
                    } else if (actions_menu_selected == 1) {
                        clipboard_file_count = 0;
                        int has_sel = 0;
                        for (int i = 0; i < browser.file_count; i++) {
                            if (browser.selection_mask[i]) {
                                snprintf(clipboard_paths[clipboard_file_count], sizeof(clipboard_paths[0]), "%s/%s", browser.current_path, browser.files[i].name);
                                clipboard_file_count++;
                                has_sel = 1;
                            }
                        }
                        if (!has_sel && browser.file_count > 0) {
                            snprintf(clipboard_paths[0], sizeof(clipboard_paths[0]), "%s/%s", browser.current_path, browser.files[browser.selected_index].name);
                            clipboard_file_count = 1;
                        }
                        clipboard_is_move = 1;
                        show_toast(language_get(&lang, STR_TOAST_CUT), RGBA8(52, 152, 219, 255));
                        current_mode = MODE_BROWSER;
                    } else if (actions_menu_selected == 2) {
                        if (clipboard_file_count > 0) {
                            strncpy(extraction_dest_path, browser.current_path, sizeof(extraction_dest_path) - 1);
                            extraction_dest_path[sizeof(extraction_dest_path) - 1] = '\0';
                            
                            worker_args.mode = 6;
                            strncpy(worker_args.dest, extraction_dest_path, sizeof(worker_args.dest) - 1);
                            worker_args.dest[sizeof(worker_args.dest) - 1] = '\0';
                            
                            current_mode = MODE_EXTRACTING;
                            extract_progress = 0;
                        }
                    } else if (actions_menu_selected == 3) {
                        if (browser.file_count > 0) {
                            char new_name[256] = {0};
                            const char *old_name = browser.files[browser.selected_index].name;
                            const char *title = language_get(&lang, STR_RENAME);
                            if (get_ime_input(new_name, 250, title, old_name) == 0 && strlen(new_name) > 0) {
                                char old_path[1024], new_path[1024];
                                snprintf(old_path, sizeof(old_path), "%s/%s", browser.current_path, old_name);
                                snprintf(new_path, sizeof(new_path), "%s/%s", browser.current_path, new_name);
                                int res = sceIoRename(old_path, new_path);
                                if (res >= 0) {
                                    show_toast(language_get(&lang, STR_TOAST_RENAME_SUCCESS), RGBA8(46, 204, 113, 255));
                                } else {
                                    show_toast(language_get(&lang, STR_TOAST_RENAME_FAIL), RGBA8(231, 76, 60, 255));
                                }
                                filebrowser_refresh(&browser);
                            }
                        }
                        current_mode = MODE_BROWSER;
                    } else if (actions_menu_selected == 4) {
                        char folder_name[256] = {0};
                        const char *title = language_get(&lang, STR_NEW_FOLDER);
                        if (get_ime_input(folder_name, 250, title, NULL) == 0 && strlen(folder_name) > 0) {
                            char folder_path[1024];
                            snprintf(folder_path, sizeof(folder_path), "%s/%s", browser.current_path, folder_name);
                            int res = sceIoMkdir(folder_path, 0777);
                            if (res >= 0) {
                                show_toast(language_get(&lang, STR_TOAST_FOLDER_SUCCESS), RGBA8(46, 204, 113, 255));
                            } else {
                                show_toast(language_get(&lang, STR_TOAST_FOLDER_FAIL), RGBA8(231, 76, 60, 255));
                            }
                            filebrowser_refresh(&browser);
                        }
                        current_mode = MODE_BROWSER;
                    } else if (actions_menu_selected == 5) {
                        const char *title = language_get(&lang, STR_SEARCH);
                        if (get_ime_input(browser.search_query, 120, title, NULL) == 0) {
                            filebrowser_refresh(&browser);
                        }
                        current_mode = MODE_BROWSER;
                    } else if (actions_menu_selected == 6) {
                        current_mode = MODE_ZIP_CREATION;
                        memset(browser.selection_mask, 0, sizeof(browser.selection_mask));
                    } else if (actions_menu_selected == 7) {
                        if (browser.file_count > 0) {
                            current_mode = MODE_DELETE_CONFIRM;
                        } else {
                            current_mode = MODE_BROWSER;
                        }
                    } else if (actions_menu_selected == 8) {
                        if (browser.file_count > 0) {
                            int is_dir = browser.files[browser.selected_index].is_directory;
                            if (!is_dir) {
                                snprintf(hash_filepath, sizeof(hash_filepath), "%s/%s", browser.current_path, browser.files[browser.selected_index].name);
                                hash_progress = 0;
                                hash_cancel = 0;
                                hash_md5[0] = '\0';
                                hash_sha256[0] = '\0';
                                current_mode = MODE_HASH_VIEW;
                                
                                worker_args.mode = 8;
                                worker_thread_id = sceKernelCreateThread("hash_worker", worker_thread_func, 0x10000100, 0x10000, 0, 0, NULL);
                                if (worker_thread_id >= 0) {
                                    sceKernelStartThread(worker_thread_id, sizeof(worker_args), &worker_args);
                                }
                            } else {
                                current_mode = MODE_BROWSER;
                            }
                        } else {
                            current_mode = MODE_BROWSER;
                        }
                    } else if (actions_menu_selected == 9) {
                        if (browser.file_count > 0) {
                            int is_dir = browser.files[browser.selected_index].is_directory;
                            if (!is_dir) {
                                snprintf(hex_filepath, sizeof(hex_filepath), "%s/%s", browser.current_path, browser.files[browser.selected_index].name);
                                hex_offset = 0;
                                hex_file_size = browser.files[browser.selected_index].size;
                                current_mode = MODE_HEX_VIEW;
                            } else {
                                current_mode = MODE_BROWSER;
                            }
                        } else {
                            current_mode = MODE_BROWSER;
                        }
                    } else if (actions_menu_selected == 10) {
                        if (browser.file_count > 0) {
                            snprintf(prop_filepath, sizeof(prop_filepath), "%s/%s", browser.current_path, browser.files[browser.selected_index].name);
                            memset(&prop_stat, 0, sizeof(prop_stat));
                            if (sceIoGetstat(prop_filepath, &prop_stat) >= 0) {
                                prop_selected_row = 0;
                                prop_checkboxes[0] = (prop_stat.st_attr & 0x01) ? 1 : 0;
                                prop_checkboxes[1] = (prop_stat.st_attr & 0x02) ? 1 : 0;
                                prop_checkboxes[2] = (prop_stat.st_attr & 0x04) ? 1 : 0;
                                prop_checkboxes[3] = (prop_stat.st_mode & 0400) ? 1 : 0;
                                prop_checkboxes[4] = (prop_stat.st_mode & 0200) ? 1 : 0;
                                prop_checkboxes[5] = (prop_stat.st_mode & 0100) ? 1 : 0;
                                current_mode = MODE_PROPERTIES_VIEW;
                            } else {
                                current_mode = MODE_BROWSER;
                            }
                        } else {
                            current_mode = MODE_BROWSER;
                        }
                    } else if (actions_menu_selected == 11) {
                        if (browser.file_count > 0) {
                            int is_dir = browser.files[browser.selected_index].is_directory;
                            const char *name = browser.files[browser.selected_index].name;
                            const char *ext = strrchr(name, '.');
                            int is_sfo = ext && strcasecmp(ext, ".sfo") == 0;
                            int is_txt = is_viewable_text_file(name);
                            if (!is_dir && (is_sfo || is_txt)) {
                                char filepath[1024];
                                snprintf(filepath, sizeof(filepath), "%s/%s", browser.current_path, name);
                                if (is_sfo) {
                                    load_sfo_preview(filepath);
                                } else {
                                    load_preview_file(filepath);
                                }
                                current_mode = MODE_TEXT_PREVIEW;
                                preview_return_mode = MODE_BROWSER;
                                strncpy(preview_filename, name, sizeof(preview_filename) - 1);
                                preview_filename[sizeof(preview_filename) - 1] = '\0';
                            } else {
                                current_mode = MODE_BROWSER;
                            }
                        } else {
                            current_mode = MODE_BROWSER;
                        }
                    } else if (actions_menu_selected == 12) {
                        if (browser.file_count > 0) {
                            int is_dir = browser.files[browser.selected_index].is_directory;
                            const char *name = browser.files[browser.selected_index].name;
                            int can_promote = 0;
                            if (is_dir) {
                                char sfo_path[1024];
                                snprintf(sfo_path, sizeof(sfo_path), "%s%s/sce_sys/param.sfo", browser.current_path, name);
                                SceIoStat sfo_stat;
                                if (sceIoGetstat(sfo_path, &sfo_stat) >= 0) {
                                    can_promote = 1;
                                }
                            }
                            if (can_promote) {
                                char dir_path[1024];
                                snprintf(dir_path, sizeof(dir_path), "%s%s", browser.current_path, name);
                                
                                scePromoterUtilityInit();
                                int res = scePromoterUtilityPromotePkg(dir_path, 0);
                                scePromoterUtilityExit();
                                
                                if (res >= 0) {
                                    LanguageCode cl = language_get_current(&lang);
                                    const char *msg = (cl == LANG_IT) ? "App promossa con successo!" : "App promoted successfully!";
                                    show_toast(msg, RGBA8(46, 204, 113, 255));
                                } else {
                                    LanguageCode cl = language_get_current(&lang);
                                    const char *msg = (cl == LANG_IT) ? "Errore promozione app!" : "Error promoting app!";
                                    show_toast(msg, RGBA8(231, 76, 60, 255));
                                }
                            }
                        }
                        current_mode = MODE_BROWSER;
                    }
                }
                break;
                
            case MODE_ARCHIVE_ACTIONS_MENU:
                if ((pressed | repeat_event) & SCE_CTRL_UP) {
                    if (archive_actions_selected > 0) archive_actions_selected--;
                    else archive_actions_selected = 2;
                } else if ((pressed | repeat_event) & SCE_CTRL_DOWN) {
                    if (archive_actions_selected < 2) archive_actions_selected++;
                    else archive_actions_selected = 0;
                } else if (pressed & SCE_CTRL_CIRCLE) {
                    current_mode = MODE_ARCHIVE_VIEW;
                } else if (pressed & SCE_CTRL_CROSS) {
                    if (archive_actions_selected == 0) {
                        int has_sel = 0;
                        for (int i = 0; i < archive_info.file_count; i++) {
                            if (archive_selection_mask[i]) { has_sel = 1; break; }
                        }
                        if (!has_sel && archive_info.file_count > 0) {
                            archive_selection_mask[archive_selected] = 1;
                        }
                        extract_selected_only = 2;
                        worker_args.mode = 7;
                        current_mode = MODE_DEST_BROWSER;
                        filebrowser_init(&browser, "ux0:/");
                        scroll_offset = 0;
                    } else if (archive_actions_selected == 1) {
                        extract_selected_only = 0;
                        worker_args.mode = 0;
                        current_mode = MODE_DEST_BROWSER;
                        filebrowser_init(&browser, "ux0:/");
                        scroll_offset = 0;
                    } else if (archive_actions_selected == 2) {
                        worker_args.mode = 5;
                        current_mode = MODE_EXTRACTING;
                        extract_progress = 0;
                    }
                }
                break;
            case MODE_INTEGRITY_RESULT:
                if (pressed & SCE_CTRL_CIRCLE) {
                    current_mode = MODE_ARCHIVE_VIEW;
                }
                break;
                
            case MODE_HASH_VIEW:
                if (pressed & SCE_CTRL_CIRCLE) {
                    if (hash_progress < 100) {
                        hash_cancel = 1;
                    }
                    current_mode = MODE_BROWSER;
                } else if (pressed & SCE_CTRL_CROSS) {
                    if (hash_progress == 100) {
                        sceIoMkdir("ux0:/data/VitaArchive", 0777);
                        SceUID fd = sceIoOpen("ux0:/data/VitaArchive/hash_report.txt", SCE_O_WRONLY | SCE_O_CREAT | SCE_O_TRUNC, 0777);
                        if (fd >= 0) {
                            char r_buf[2048];
                            snprintf(r_buf, sizeof(r_buf), "File: %s\r\nMD5: %s\r\nSHA-256: %s\r\n", hash_filepath, hash_md5, hash_sha256);
                            sceIoWrite(fd, r_buf, strlen(r_buf));
                            sceIoClose(fd);
                            LanguageCode cl = language_get_current(&lang);
                            if (cl == LANG_IT) show_toast("Report hash salvato!", RGBA8(46, 204, 113, 255));
                            else if (cl == LANG_ES) show_toast("Reporte de hash guardado!", RGBA8(46, 204, 113, 255));
                            else if (cl == LANG_FR) show_toast("Rapport de hash enregistre!", RGBA8(46, 204, 113, 255));
                            else if (cl == LANG_DE) show_toast("Hash-Bericht gespeichert!", RGBA8(46, 204, 113, 255));
                            else if (cl == LANG_JPN) show_toast("ハッシュレポートが保存されました！", RGBA8(46, 204, 113, 255));
                            else show_toast("Hash report saved!", RGBA8(46, 204, 113, 255));
                        } else {
                            LanguageCode cl = language_get_current(&lang);
                            if (cl == LANG_IT) show_toast("Errore salvataggio report", RGBA8(231, 76, 60, 255));
                            else if (cl == LANG_ES) show_toast("Error al guardar el reporte", RGBA8(231, 76, 60, 255));
                            else if (cl == LANG_FR) show_toast("Erreur d'enregistrement", RGBA8(231, 76, 60, 255));
                            else if (cl == LANG_DE) show_toast("Fehler beim Speichern des Berichts", RGBA8(231, 76, 60, 255));
                            else if (cl == LANG_JPN) show_toast("レポートの保存に失敗しました", RGBA8(231, 76, 60, 255));
                            else show_toast("Failed to save report", RGBA8(231, 76, 60, 255));
                        }
                    }
                }
                break;
                
            case MODE_HEX_VIEW:
                if (pressed & SCE_CTRL_CIRCLE) {
                    current_mode = MODE_BROWSER;
                } else if ((pressed | repeat_event) & SCE_CTRL_UP) {
                    if (hex_offset >= 16) {
                        hex_offset -= 16;
                    }
                } else if ((pressed | repeat_event) & SCE_CTRL_DOWN) {
                    if (hex_offset + 16 < hex_file_size) {
                        hex_offset += 16;
                    }
                } else if (pressed & SCE_CTRL_LTRIGGER) {
                    if (hex_offset >= 240) {
                        hex_offset -= 240;
                    } else {
                        hex_offset = 0;
                    }
                } else if (pressed & SCE_CTRL_RTRIGGER) {
                    if (hex_offset + 240 < hex_file_size) {
                        hex_offset += 240;
                    } else {
                        uint32_t pages = hex_file_size / 240;
                        hex_offset = pages * 240;
                    }
                }
                break;
                
            case MODE_PROPERTIES_VIEW:
                if (pressed & SCE_CTRL_CIRCLE) {
                    SceIoStat change_stat;
                    memset(&change_stat, 0, sizeof(change_stat));
                    
                    uint32_t new_attr = 0;
                    if (prop_checkboxes[0]) new_attr |= 0x01;
                    if (prop_checkboxes[1]) new_attr |= 0x02;
                    if (prop_checkboxes[2]) new_attr |= 0x04;
                    change_stat.st_attr = new_attr;
                    
                    uint32_t new_mode = prop_stat.st_mode & ~0700;
                    if (prop_checkboxes[3]) new_mode |= 0400;
                    if (prop_checkboxes[4]) new_mode |= 0200;
                    if (prop_checkboxes[5]) new_mode |= 0100;
                    change_stat.st_mode = new_mode;
                    
                    sceIoChstat(prop_filepath, &change_stat, 0x0001 | 0x0002);
                    filebrowser_refresh(&browser);
                    current_mode = MODE_BROWSER;
                } else if ((pressed | repeat_event) & SCE_CTRL_UP) {
                    if (prop_selected_row > 0) prop_selected_row--;
                    else prop_selected_row = 5;
                } else if ((pressed | repeat_event) & SCE_CTRL_DOWN) {
                    if (prop_selected_row < 5) prop_selected_row++;
                    else prop_selected_row = 0;
                } else if (pressed & SCE_CTRL_CROSS) {
                    prop_checkboxes[prop_selected_row] = !prop_checkboxes[prop_selected_row];
                }
                break;
        }
        
        vita2d_start_drawing();
        vita2d_clear_screen();
        
        switch (current_mode) {
            case MODE_BROWSER:
                draw_browser();
                draw_browser_footer();
                break;
            case MODE_ARCHIVE_VIEW:
                draw_archive_view();
                break;
            case MODE_EXTRACTING:
                draw_extracting();
                break;
            case MODE_INSTALLING:
                draw_installing(install_success);
                break;
            case MODE_INFO:
                draw_info();
                break;
            case MODE_SETTINGS:
                draw_settings();
                break;
            case MODE_DEST_BROWSER:
                draw_dest_browser();
                break;
            case MODE_ZIP_CREATION:
                draw_browser();
                draw_browser_footer();
                break;
            case MODE_SMART_INSTALL_CONFIRM:
                draw_smart_install_confirm();
                break;
            case MODE_DELETE_CONFIRM:
                draw_delete_confirm();
                break;
            case MODE_COMPRESS_FORMAT_SELECT:
                draw_compress_format_select();
                break;
            case MODE_TEXT_PREVIEW:
                draw_text_preview();
                break;
            case MODE_FTP_SERVER:
                draw_ftp_server();
                break;
            case MODE_ACTIONS_MENU:
                draw_actions_menu();
                break;
            case MODE_ARCHIVE_ACTIONS_MENU:
                draw_archive_actions_menu();
                break;
            case MODE_INTEGRITY_RESULT:
                draw_integrity_result();
                break;
            case MODE_HASH_VIEW:
                draw_hash_view();
                break;
            case MODE_HEX_VIEW:
                draw_hex_view();
                break;
            case MODE_PROPERTIES_VIEW:
                draw_properties_view();
                break;
        }
        
        draw_battery_status();
        draw_toast();
        
        vita2d_end_drawing();
        vita2d_swap_buffers();
    }
    
    if (archive_info.is_open) archive_close_custom(&archive_info);
    save_settings();
    vpk_cleanup_dirs();
    vita2d_free_pgf(font);
    sceSysmoduleUnloadModule(SCE_SYSMODULE_IME);
    scePromoterUtilityExit();
    sceSysmoduleUnloadModuleInternal(SCE_SYSMODULE_INTERNAL_PROMOTER_UTIL);
    vita2d_fini();
    return 0;
}
