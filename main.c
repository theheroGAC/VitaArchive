#include <psp2/ctrl.h>
#include <psp2/kernel/processmgr.h>
#include <psp2/io/fcntl.h>
#include <psp2/io/dirent.h>
#include <psp2/io/stat.h>
#include <psp2/sysmodule.h>
#include <psp2/common_dialog.h>
#include <psp2/ime_dialog.h>

#include <vita2d.h>
#include <stdio.h>
#include <string.h>

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


typedef enum {
    MODE_BROWSER,
    MODE_ARCHIVE_VIEW,
    MODE_EXTRACTING,
    MODE_INSTALLING,
    MODE_INFO,
    MODE_SETTINGS,
    MODE_DEST_BROWSER,
    MODE_ZIP_CREATION,
    MODE_SMART_INSTALL_CONFIRM
} AppMode;

typedef enum {
    INSTALL_MODE_VPK,
    INSTALL_MODE_APP
} InstallMode;

static AppMode current_mode = MODE_BROWSER;
static FileBrowser browser;
static ArchiveInfo archive_info;
static int extract_progress = 0;
static int scroll_offset = 0;
static int archive_scroll = 0;
static int archive_selected = 0;
static int install_success = 0;
static Language lang;
static vita2d_pgf *font = NULL;
static int settings_selected = 0;
static char extraction_dest_path[MAX_PATH];
static InstallMode current_install_mode = INSTALL_MODE_VPK;

void save_settings();
void load_settings();

static int archive_can_smart_install(const ArchiveInfo *info) {
    return info && info->is_open && is_zip_file(info->archive_path) && archive_contains_homebrew(info);
}

static void get_file_visuals(const char *path, int is_directory, const char **icon, uint32_t *icon_color) {
    if (is_directory) {
        *icon = "[DIR]";
        *icon_color = RGBA8(241, 196, 15, 255);
        return;
    }

    if (is_vpk_file(path)) {
        *icon = "[VPK]";
        *icon_color = RGBA8(46, 204, 113, 255);
    } else if (is_zip_file(path)) {
        *icon = "[ZIP]";
        *icon_color = RGBA8(52, 152, 219, 255);
    } else if (is_tar_file(path)) {
        *icon = "[TAR]";
        *icon_color = RGBA8(230, 126, 34, 255);
    } else if (is_gzip_file(path)) {
        *icon = "[GZ]";
        *icon_color = RGBA8(26, 188, 156, 255);
    } else if (is_bzip2_file(path)) {
        *icon = "[BZ2]";
        *icon_color = RGBA8(241, 196, 15, 255);
    } else if (is_rar_file(path)) {
        *icon = "[RAR]";
        *icon_color = RGBA8(231, 76, 60, 255);
    } else if (is_7z_file(path)) {
        *icon = "[7Z]";
        *icon_color = RGBA8(155, 89, 182, 255);
    } else {
        *icon = "[FILE]";
        *icon_color = RGBA8(110, 118, 129, 255);
    }
}

static const char *get_file_type_label(const char *path, int is_directory) {
    if (is_directory) {
        return language_get(&lang, STR_FOLDER);
    }
    if (is_vpk_file(path)) {
        return "VPK";
    }
    if (is_zip_file(path)) {
        return "ZIP";
    }
    if (is_tar_file(path)) {
        return "TAR";
    }
    if (is_gzip_file(path)) {
        return "GZIP";
    }
    if (is_bzip2_file(path)) {
        return "BZIP2";
    }
    if (is_rar_file(path)) {
        return "RAR";
    }
    if (is_7z_file(path)) {
        return "7Z";
    }
    return language_get(&lang, STR_FILE);
}

void draw_progress_bar(int x, int y, int width, int height, int progress) {
    vita2d_draw_rectangle(x, y, width, height, 0xFFFFFFFF);
    int filled_width = (width * progress) / 100;
    vita2d_draw_rectangle(x, y, filled_width, height, 0x00FF00FF);
}

void format_size(char *buf, size_t size) {
    if (size < 1024) {
        sprintf(buf, "%lu B", (unsigned long)size);
    } else if (size < 1024 * 1024) {
        sprintf(buf, "%.1f KB", size / 1024.0);
    } else if (size < 1024 * 1024 * 1024) {
        sprintf(buf, "%.1f MB", size / (1024.0 * 1024.0));
    } else {
        sprintf(buf, "%.1f GB", size / (1024.0 * 1024.0 * 1024.0));
    }
}

void draw_browser() {
    vita2d_draw_rectangle(0, 0, 960, 50, RGBA8(21, 26, 33, 255));
    vita2d_pgf_draw_textf(font, 10, 30, RGBA8(240, 246, 252, 255), 1.0f, "%s", language_get(&lang, STR_APP_TITLE));
    
    vita2d_draw_rectangle(0, 50, 960, 30, RGBA8(28, 33, 40, 255));
    vita2d_pgf_draw_textf(font, 10, 70, RGBA8(201, 209, 217, 255), 0.8f, "%s: %s", 
        language_get(&lang, STR_PATH), browser.current_path);
    
    int header_y = 85;
    vita2d_draw_rectangle(0, header_y, 960, 25, RGBA8(33, 38, 45, 255));
    vita2d_pgf_draw_textf(font, 10, header_y + 18, RGBA8(240, 246, 252, 255), 0.85f, "%s", language_get(&lang, STR_NAME));
    vita2d_pgf_draw_textf(font, 600, header_y + 18, RGBA8(240, 246, 252, 255), 0.85f, "%s", language_get(&lang, STR_SIZE));
    vita2d_pgf_draw_textf(font, 800, header_y + 18, RGBA8(240, 246, 252, 255), 0.85f, "%s", language_get(&lang, STR_TYPE));
    int start_y = 110;
    int line_height = 22;
    int visible_files = (510 - start_y) / line_height;
    
    for (int i = 0; i < browser.file_count && i < visible_files; i++) {
        int idx = scroll_offset + i;
        if (idx >= browser.file_count) break;
        
        FileInfo *file = &browser.files[idx];
        int y = start_y + 5 + (i * line_height);
        
        if (idx == browser.selected_index) {
            vita2d_draw_rectangle(0, y, 960, line_height, RGBA8(31, 111, 235, 255));
        } else if (i % 2 != 0) {
            vita2d_draw_rectangle(0, y, 960, line_height, RGBA8(22, 27, 34, 255));
        }
        
        const char *icon = NULL;
        uint32_t icon_color = 0;
        get_file_visuals(file->name, file->is_directory, &icon, &icon_color);
        vita2d_pgf_draw_textf(font, 10, y + 16, icon_color, 0.8f, "%s", icon);
        
        uint32_t text_color = (idx == browser.selected_index) ? RGBA8(255, 255, 255, 255) : RGBA8(201, 209, 217, 255);
        vita2d_pgf_draw_textf(font, 60, y + 16, text_color, 0.85f, "%s", file->name);
        
        if (current_mode == MODE_ZIP_CREATION && browser.selection_mask[idx]) {
            vita2d_pgf_draw_textf(font, 40, y + 16, RGBA8(46, 204, 113, 255), 0.8f, "*");
        }

        if (!file->is_directory) {
            char size_buf[32];
            format_size(size_buf, file->size);
            vita2d_pgf_draw_textf(font, 600, y + 16, RGBA8(139, 148, 158, 255), 0.8f, "%s", size_buf);
        }
        
        const char *type = get_file_type_label(file->name, file->is_directory);
        uint32_t type_color = (idx == browser.selected_index) ? RGBA8(255, 255, 255, 255) : icon_color;
        vita2d_pgf_draw_textf(font, 800, y + 16, type_color, 0.8f, "%s", type);
    }
}

void draw_footer(const char **actions, int count) {
    vita2d_draw_rectangle(0, 510, 960, 34, RGBA8(21, 26, 33, 255));

    int x_pos = 20;
    const int spacing = 30;

    for (int i = 0; i < count; ++i) {
        if (actions[i] && strlen(actions[i]) > 0) {
            vita2d_pgf_draw_textf(font, x_pos, 532, RGBA8(201, 209, 217, 255), 0.8f, "%s", actions[i]);
            x_pos += vita2d_pgf_text_width(font, 0.8f, actions[i]) + spacing;
        }
    }
}

void draw_browser_footer() {
    if (current_mode == MODE_ZIP_CREATION) {
        char sq_str[64], tri_str[64], o_str[64];
        snprintf(sq_str, sizeof(sq_str), "SQUARE: %s", language_get(&lang, STR_TOGGLE_SELECT));
        snprintf(tri_str, sizeof(tri_str), "TRIANGLE: %s", language_get(&lang, STR_CREATE_ZIP));
        snprintf(o_str, sizeof(o_str), "O: %s", language_get(&lang, STR_CANCEL));

        const char *actions[] = {sq_str, tri_str, o_str};
        draw_footer(actions, 3);
    } else {
        char x_str[64], o_str[64], tri_str[64], sel_str[64], start_str[64];
        snprintf(x_str, sizeof(x_str), "X: %s", language_get(&lang, STR_SELECT_ENTER));
        snprintf(o_str, sizeof(o_str), "O: %s", language_get(&lang, STR_BACK));
        snprintf(tri_str, sizeof(tri_str), "TRIANGLE: %s", language_get(&lang, STR_CREATE_ZIP));
        snprintf(sel_str, sizeof(sel_str), "SELECT: %s", language_get(&lang, STR_LANGUAGE));
        snprintf(start_str, sizeof(start_str), "START: %s", language_get(&lang, STR_EXIT));

        const char *actions[] = {x_str, o_str, tri_str, sel_str, start_str};
        draw_footer(actions, 5);
    }
}

void draw_dest_browser() {
    vita2d_draw_rectangle(0, 0, 960, 50, RGBA8(21, 26, 33, 255));
    vita2d_pgf_draw_textf(font, 10, 30, RGBA8(240, 246, 252, 255), 1.0f, "%s", language_get(&lang, STR_SELECT_DEST_FOLDER));

    draw_browser();

    char x_str[64], o_str[64], sq_str[64];
    snprintf(x_str, sizeof(x_str), "X: %s", language_get(&lang, STR_SELECT_ENTER));
    snprintf(o_str, sizeof(o_str), "O: %s", language_get(&lang, STR_BACK));
    snprintf(sq_str, sizeof(sq_str), "SQUARE: %s", language_get(&lang, STR_CONFIRM_DEST));
    const char* actions[] = {x_str, o_str, sq_str};
    draw_footer(actions, 3);
}

void draw_archive_view() {
    vita2d_draw_rectangle(0, 0, 960, 50, RGBA8(21, 26, 33, 255));
    vita2d_pgf_draw_textf(font, 10, 30, RGBA8(240, 246, 252, 255), 1.0f, "%s - %s", 
        language_get(&lang, STR_APP_TITLE), language_get(&lang, STR_ARCHIVE_CONTENTS));
    
    char size_buf[32];
    format_size(size_buf, archive_info.total_size);
    vita2d_draw_rectangle(0, 50, 960, 30, RGBA8(28, 33, 40, 255));
    char info_text[128];
    snprintf(info_text, sizeof(info_text), "%s: %d | %s: %s", 
        language_get(&lang, STR_FILES), archive_info.file_count,
        language_get(&lang, STR_SIZE), size_buf);
    vita2d_pgf_draw_textf(font, 10, 70, RGBA8(201, 209, 217, 255), 0.8f, "%s", info_text);
    
    int header_y = 85;
    vita2d_draw_rectangle(0, header_y, 960, 25, RGBA8(33, 38, 45, 255));
    vita2d_pgf_draw_textf(font, 10, header_y + 18, RGBA8(240, 246, 252, 255), 0.85f, "%s", language_get(&lang, STR_NAME));
    vita2d_pgf_draw_textf(font, 500, header_y + 18, RGBA8(240, 246, 252, 255), 0.85f, "%s", language_get(&lang, STR_SIZE));
    vita2d_pgf_draw_textf(font, 650, header_y + 18, RGBA8(240, 246, 252, 255), 0.85f, "%s", language_get(&lang, STR_COMPRESSED));
    vita2d_pgf_draw_textf(font, 800, header_y + 18, RGBA8(240, 246, 252, 255), 0.85f, "%s", language_get(&lang, STR_TYPE));
    int start_y = 110;
    int line_height = 22;
    int visible_files = (510 - start_y) / line_height;
    
    for (int i = 0; i < archive_info.file_count && i < visible_files; i++) {
        int idx = archive_scroll + i;
        if (idx >= archive_info.file_count) break;
        
        ArchiveFile *file = &archive_info.files[idx];
        int y = start_y + 5 + (i * line_height);
        
        if (idx == archive_selected) {
            vita2d_draw_rectangle(0, y, 960, line_height, RGBA8(31, 111, 235, 255));
        } else if (i % 2 != 0) {
            vita2d_draw_rectangle(0, y, 960, line_height, RGBA8(22, 27, 34, 255));
        }
        
        const char *icon = NULL;
        uint32_t icon_color = 0;
        get_file_visuals(file->filename, file->is_directory, &icon, &icon_color);
        vita2d_pgf_draw_textf(font, 10, y + 16, icon_color, 0.8f, "%s", icon);
        
        uint32_t text_color = (idx == archive_selected) ? RGBA8(255, 255, 255, 255) : RGBA8(201, 209, 217, 255);
        vita2d_pgf_draw_textf(font, 60, y + 16, text_color, 0.85f, "%s", file->filename);
        
        if (!file->is_directory) {
            format_size(size_buf, file->uncompressed_size);
            vita2d_pgf_draw_textf(font, 500, y + 16, RGBA8(139, 148, 158, 255), 0.8f, "%s", size_buf);
            
            format_size(size_buf, file->compressed_size);
            vita2d_pgf_draw_textf(font, 650, y + 16, RGBA8(139, 148, 158, 255), 0.8f, "%s", size_buf);
        }
        
        const char *type = get_file_type_label(file->filename, file->is_directory);
        uint32_t type_color = (idx == archive_selected) ? RGBA8(255, 255, 255, 255) : icon_color;
        vita2d_pgf_draw_textf(font, 800, y + 16, type_color, 0.8f, "%s", type);
    }
    char x_str[64], sq_str[64], o_str[64], tri_str[64], start_str[64];
    int vpk_selected = archive_info.file_count > 0 &&
        is_vpk_file(archive_info.files[archive_selected].filename);
    int smart_install_available = !vpk_selected && archive_can_smart_install(&archive_info);

    if (vpk_selected) {
        snprintf(x_str, sizeof(x_str), "X: %s", language_get(&lang, STR_PRESS_X_INSTALL_VPK));
    } else if (smart_install_available) {
        snprintf(x_str, sizeof(x_str), "X: %s", language_get(&lang, STR_SMART_INSTALL));
        snprintf(sq_str, sizeof(sq_str), "SQUARE: %s", language_get(&lang, STR_EXTRACT_ALL));
    } else {
        snprintf(x_str, sizeof(x_str), "X: %s", language_get(&lang, STR_EXTRACT_ALL));
    }
    snprintf(o_str, sizeof(o_str), "O: %s", language_get(&lang, STR_BACK));
    snprintf(tri_str, sizeof(tri_str), "TRIANGLE: %s", language_get(&lang, STR_INFO));
    snprintf(start_str, sizeof(start_str), "START: %s", language_get(&lang, STR_EXIT));

    if (smart_install_available) {
        const char *actions[] = {x_str, sq_str, o_str, tri_str, start_str};
        draw_footer(actions, 5);
    } else {
        const char *actions[] = {x_str, o_str, tri_str, start_str};
        draw_footer(actions, 4);
    }
}

void draw_extracting() {
    vita2d_draw_rectangle(0, 0, 960, 50, RGBA8(21, 26, 33, 255));
    vita2d_pgf_draw_textf(font, 10, 30, RGBA8(240, 246, 252, 255), 1.0f, "%s - %s", 
        language_get(&lang, STR_APP_TITLE), language_get(&lang, STR_EXTRACTING));
    
    int panel_y = 150;
    vita2d_draw_rectangle(100, panel_y, 760, 200, RGBA8(22, 27, 34, 255));
    vita2d_draw_rectangle(100, panel_y, 760, 1, RGBA8(31, 111, 235, 255));
    
    vita2d_pgf_draw_textf(font, 120, panel_y + 40, RGBA8(240, 246, 252, 255), 1.2f, "%s: %d%%", 
        language_get(&lang, STR_PROGRESS), extract_progress);
    
    vita2d_draw_rectangle(120, panel_y + 80, 720, 40, RGBA8(33, 38, 45, 255));
    int filled_width = (720 * extract_progress) / 100;
    vita2d_draw_rectangle(120, panel_y + 80, filled_width, 40, RGBA8(31, 111, 235, 255));
    
    const char *status = extract_progress < 100
        ? language_get(&lang, STR_EXTRACTING_FILES)
        : language_get(&lang, STR_COMPLETE);
    vita2d_pgf_draw_textf(font, 120, panel_y + 140, RGBA8(139, 148, 158, 255), 0.9f, "%s", status);
    
    char sq_str[64], start_str[64];
    snprintf(sq_str, sizeof(sq_str), "SQUARE: %s", language_get(&lang, STR_CANCEL_EXTRACTION));
    snprintf(start_str, sizeof(start_str), "START: %s", language_get(&lang, STR_EXIT));
    const char *actions[] = {sq_str, start_str};
    draw_footer(actions, 2);
}

void draw_installing(int success) {
    vita2d_draw_rectangle(0, 0, 960, 50, RGBA8(21, 26, 33, 255));
    const char *install_title = current_install_mode == INSTALL_MODE_APP
        ? language_get(&lang, STR_INSTALL_APP)
        : language_get(&lang, STR_INSTALL_VPK);
    vita2d_pgf_draw_textf(font, 10, 30, RGBA8(240, 246, 252, 255), 1.0f, "%s - %s",
        language_get(&lang, STR_APP_TITLE), install_title);

    int panel_y = 150;
    vita2d_draw_rectangle(100, panel_y, 760, 200, RGBA8(22, 27, 34, 255));
    vita2d_draw_rectangle(100, panel_y, 760, 1, RGBA8(31, 111, 235, 255));

    if (extract_progress < 100) {
        vita2d_pgf_draw_textf(font, 120, panel_y + 40, RGBA8(240, 246, 252, 255), 1.2f, "%s: %d%%",
            language_get(&lang, STR_PROGRESS), extract_progress);

        vita2d_draw_rectangle(120, panel_y + 80, 720, 40, RGBA8(33, 38, 45, 255));
        int filled_width = (720 * extract_progress) / 100;
        vita2d_draw_rectangle(120, panel_y + 80, filled_width, 40, RGBA8(31, 111, 235, 255));

        vita2d_pgf_draw_textf(font, 120, panel_y + 140, RGBA8(139, 148, 158, 255), 0.9f, "%s",
            install_title);
    } else {
        const char *result = success
            ? (current_install_mode == INSTALL_MODE_APP
                ? language_get(&lang, STR_APP_INSTALL_COMPLETE)
                : language_get(&lang, STR_VPK_INSTALL_COMPLETE))
            : (current_install_mode == INSTALL_MODE_APP
                ? language_get(&lang, STR_APP_INSTALL_ERROR)
                : language_get(&lang, STR_VPK_INSTALL_ERROR));
        uint32_t color = success ? RGBA8(46, 204, 113, 255) : RGBA8(231, 76, 60, 255);
        vita2d_pgf_draw_textf(font, 120, panel_y + 100, color, 1.0f, "%s", result);
    }

    char start_str[64];
    snprintf(start_str, sizeof(start_str), "START: %s", language_get(&lang, STR_EXIT));
    const char *actions[] = {start_str};
    draw_footer(actions, 1);
}

void draw_smart_install_confirm() {
    vita2d_draw_rectangle(0, 0, 960, 50, RGBA8(21, 26, 33, 255));
    vita2d_pgf_draw_textf(font, 10, 30, RGBA8(240, 246, 252, 255), 1.0f, "%s - %s",
        language_get(&lang, STR_APP_TITLE), language_get(&lang, STR_SMART_INSTALL));

    int panel_y = 150;
    vita2d_draw_rectangle(100, panel_y, 760, 200, RGBA8(22, 27, 34, 255));
    vita2d_draw_rectangle(100, panel_y, 760, 1, RGBA8(31, 111, 235, 255));

    vita2d_pgf_draw_textf(font, 120, panel_y + 60, RGBA8(240, 246, 252, 255), 0.9f, "%s",
        language_get(&lang, STR_SMART_INSTALL_DETECTED));
    vita2d_pgf_draw_textf(font, 120, panel_y + 110, RGBA8(201, 209, 217, 255), 0.9f, "%s",
        language_get(&lang, STR_SMART_INSTALL_PROMPT));

    char x_str[64], sq_str[64], o_str[64];
    snprintf(x_str, sizeof(x_str), "X: %s", language_get(&lang, STR_INSTALL_APP));
    snprintf(sq_str, sizeof(sq_str), "SQUARE: %s", language_get(&lang, STR_EXTRACT_ALL));
    snprintf(o_str, sizeof(o_str), "O: %s", language_get(&lang, STR_BACK));
    const char *actions[] = {x_str, sq_str, o_str};
    draw_footer(actions, 3);
}

void draw_info() {
    vita2d_draw_rectangle(0, 0, 960, 50, RGBA8(21, 26, 33, 255));
    vita2d_pgf_draw_textf(font, 10, 30, RGBA8(240, 246, 252, 255), 1.0f, "%s - %s", 
        language_get(&lang, STR_APP_TITLE), language_get(&lang, STR_ARCHIVE_INFO));
    
    int panel_y = 100;
    vita2d_draw_rectangle(100, panel_y, 760, 300, RGBA8(22, 27, 34, 255));
    vita2d_draw_rectangle(100, panel_y, 760, 1, RGBA8(31, 111, 235, 255));
    
    char size_buf[32];
    char comp_size_buf[32];
    format_size(size_buf, archive_info.total_size);
    format_size(comp_size_buf, archive_info.total_compressed_size);
    
    int y = panel_y + 40;
    int line_height = 50;
    
    vita2d_pgf_draw_textf(font, 120, y, RGBA8(139, 148, 158, 255), 0.8f, "%s:", 
        language_get(&lang, STR_TOTAL_FILES));
    vita2d_pgf_draw_textf(font, 400, y, RGBA8(201, 209, 217, 255), 0.9f, "%d", archive_info.file_count);
    y += line_height;
    
    vita2d_pgf_draw_textf(font, 120, y, RGBA8(139, 148, 158, 255), 0.8f, "%s:", 
        language_get(&lang, STR_UNCOMPRESSED_SIZE));
    vita2d_pgf_draw_textf(font, 400, y, RGBA8(201, 209, 217, 255), 0.9f, "%s", size_buf);
    y += line_height;
    
    vita2d_pgf_draw_textf(font, 120, y, RGBA8(139, 148, 158, 255), 0.8f, "%s:", 
        language_get(&lang, STR_COMPRESSED_SIZE));
    vita2d_pgf_draw_textf(font, 400, y, RGBA8(201, 209, 217, 255), 0.9f, "%s", comp_size_buf);
    y += line_height;
    
    float ratio = archive_info.total_size > 0 ? 
        (100.0 * archive_info.total_compressed_size) / archive_info.total_size : 0;
    vita2d_pgf_draw_textf(font, 120, y, RGBA8(139, 148, 158, 255), 0.8f, "%s:", 
        language_get(&lang, STR_COMPRESSION_RATIO));
    vita2d_pgf_draw_textf(font, 400, y, RGBA8(201, 209, 217, 255), 0.9f, "%.1f%%", ratio);
    
    char o_str[64], start_str[64];
    snprintf(o_str, sizeof(o_str), "O: %s", language_get(&lang, STR_BACK));
    snprintf(start_str, sizeof(start_str), "START: %s", language_get(&lang, STR_EXIT));
    const char *actions[] = {o_str, start_str};
    draw_footer(actions, 2);
}

void draw_settings() {
    vita2d_draw_rectangle(0, 0, 960, 50, RGBA8(21, 26, 33, 255));
    vita2d_pgf_draw_textf(font, 10, 30, RGBA8(240, 246, 252, 255), 1.0f, "%s - %s", 
        language_get(&lang, STR_APP_TITLE), language_get(&lang, STR_LANGUAGE));

    int start_y = 110;
    int line_height = 40;

    for (int i = 0; i < LANG_COUNT; i++) {
        int y = start_y + (i * line_height);
        if (i == settings_selected) {
            vita2d_draw_rectangle(50, y - 5, 860, line_height, RGBA8(31, 111, 235, 255));
        }
        
        uint32_t text_color = (i == settings_selected) ? RGBA8(255, 255, 255, 255) : RGBA8(201, 209, 217, 255);
        vita2d_pgf_draw_textf(font, 70, y + 22, text_color, 1.0f, language_get_name(i));
    }

    char x_str[64], o_str[64];
    snprintf(x_str, sizeof(x_str), "X: %s", language_get(&lang, STR_SELECT_ENTER));
    snprintf(o_str, sizeof(o_str), "O: %s", language_get(&lang, STR_BACK));
    const char *actions[] = {x_str, o_str};
    draw_footer(actions, 2);
}

void save_settings() {
    sceIoMkdir("ux0:/data/VitaArchive", 0777);
    SceUID fd = sceIoOpen("ux0:/data/VitaArchive/settings.cfg", SCE_O_WRONLY | SCE_O_CREAT | SCE_O_TRUNC, 0777);
    if (fd >= 0) {
        int lang_code = language_get_current(&lang);
        sceIoWrite(fd, &lang_code, sizeof(lang_code));
        sceIoClose(fd);
    }
}

void load_settings() {
    SceUID fd = sceIoOpen("ux0:/data/VitaArchive/settings.cfg", SCE_O_RDONLY, 0);
    if (fd >= 0) {
        int lang_code;
        sceIoRead(fd, &lang_code, sizeof(lang_code));
        sceIoClose(fd);
        language_set(&lang, (LanguageCode)lang_code);
    }
}

static int get_ime_input(char *output, int max_len, const char *title) {
    SceImeDialogParam param;
    sceImeDialogParamInit(&param);

    uint16_t title_utf16[64];
    utf8_to_utf16(title_utf16, 64, (const uint8_t *)title);

    uint16_t initial_text_utf16[2] = {0};
    uint16_t output_utf16[max_len + 1];

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
        draw_browser();
        draw_browser_footer();
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
    }
    return -1;
}

int main() {
    vita2d_init();
    vita2d_set_clear_color(RGBA8(13, 17, 23, 255));
    sceCtrlSetSamplingMode(SCE_CTRL_MODE_ANALOG);

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
    filebrowser_init(&browser, "/");
    
    int old_buttons = 0;
    
    while (1) {
        SceCtrlData pad;
        sceCtrlPeekBufferPositive(0, &pad, 1);
        int pressed = pad.buttons & ~old_buttons;
        old_buttons = pad.buttons;
        
        if (pressed & SCE_CTRL_START) break;
        
        switch (current_mode) {
            case MODE_BROWSER:
                if (pressed & SCE_CTRL_UP) {
                    filebrowser_navigate_up(&browser);
                } else if (pressed & SCE_CTRL_DOWN) {
                    filebrowser_navigate_down(&browser);
                } else if (pressed & SCE_CTRL_CROSS) {
                    int result = filebrowser_enter(&browser);
                    if (result == 1) {
                        const char *selected = filebrowser_get_selected_path(&browser);
                        if (selected) {
                            int open_res = -1;
                            if (is_zip_file(selected) || is_vpk_file(selected) ||
                                is_tar_file(selected) || is_gzip_file(selected) ||
                                is_bzip2_file(selected)) {
                                open_res = zip_open(selected, &archive_info);
                            } else if (is_rar_file(selected)) {
                                open_res = rar_open(selected, &archive_info);
                            } else if (is_7z_file(selected)) {
                                open_res = archive7z_open(selected, &archive_info);
                            }

                            if (open_res >= 0) {
                                current_mode = MODE_ARCHIVE_VIEW;
                                archive_scroll = 0;
                                archive_selected = 0;
                            }
                        }
                    }
                } else if (pressed & SCE_CTRL_CIRCLE) {
                    filebrowser_navigate_back(&browser);
                } else if (pressed & SCE_CTRL_SELECT) {
                    settings_selected = language_get_current(&lang);
                    current_mode = MODE_SETTINGS;
                } else if (pressed & SCE_CTRL_TRIANGLE) {
                    current_mode = MODE_ZIP_CREATION;
                    memset(browser.selection_mask, 0, sizeof(browser.selection_mask));
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
                if (pressed & SCE_CTRL_UP) {
                    if (archive_selected > 0) {
                        archive_selected--;
                    }
                } else if (pressed & SCE_CTRL_DOWN) {
                    if (archive_selected < archive_info.file_count - 1) {
                        archive_selected++;
                    }
                } else if (pressed & SCE_CTRL_CROSS) {
                    if (vpk_selected) {
                        current_mode = MODE_INSTALLING;
                        current_install_mode = INSTALL_MODE_VPK;
                        extract_progress = 0;
                    } else if (smart_install_available) {
                        current_mode = MODE_SMART_INSTALL_CONFIRM;
                    } else {
                        current_mode = MODE_DEST_BROWSER;
                        filebrowser_init(&browser, "ux0:/");
                        scroll_offset = 0;
                    }
                } else if (pressed & SCE_CTRL_CIRCLE) {
                    archive_close_custom(&archive_info);
                    current_mode = MODE_BROWSER;
                } else if (pressed & SCE_CTRL_SQUARE) {
                    if (smart_install_available) {
                        current_mode = MODE_DEST_BROWSER;
                        filebrowser_init(&browser, "ux0:/");
                        scroll_offset = 0;
                    } else {
                        archive_cancel_custom(&archive_info);
                    }
                } else if (pressed & SCE_CTRL_TRIANGLE) {
                    current_mode = MODE_INFO;
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
                static int extracting = 0;
                if (!extracting) {
                    extracting = 1;
                    sceIoMkdir(extraction_dest_path, 0777);
                    archive_extract_all_custom(extraction_dest_path, &archive_info, &extract_progress);
                    extracting = 0;
                    current_mode = MODE_ARCHIVE_VIEW;
                }
                
                if (pressed & SCE_CTRL_SQUARE) {
                    archive_cancel_custom(&archive_info);
                    current_mode = MODE_ARCHIVE_VIEW;
                }
                break;
            }
                
            case MODE_INSTALLING: {
                static int installing = 0;
                static int install_done_frames = 0;

                if (!installing && install_done_frames == 0) {
                    installing = 1;
                    int res = current_install_mode == INSTALL_MODE_APP
                        ? vpk_install_homebrew_from_archive(&archive_info, &extract_progress)
                        : vpk_install_from_zip(&archive_info, archive_selected, &extract_progress);
                    install_success = (res == 0);
                    if (extract_progress < 100) {
                        extract_progress = 100;
                    }
                    installing = 0;
                    install_done_frames = 90;
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
                if (pressed & SCE_CTRL_UP) {
                    if (settings_selected > 0) settings_selected--;
                } else if (pressed & SCE_CTRL_DOWN) {
                    if (settings_selected < LANG_COUNT - 1) settings_selected++;
                } else if (pressed & SCE_CTRL_CROSS) {
                    language_set(&lang, (LanguageCode)settings_selected);
                    save_settings();
                    current_mode = MODE_BROWSER;
                } else if (pressed & SCE_CTRL_CIRCLE) {
                    current_mode = MODE_BROWSER;
                }
                break;

            case MODE_DEST_BROWSER:
                if (pressed & SCE_CTRL_UP) filebrowser_navigate_up(&browser);
                if (pressed & SCE_CTRL_DOWN) filebrowser_navigate_down(&browser);
                if (pressed & SCE_CTRL_CROSS) filebrowser_enter(&browser);
                if (pressed & SCE_CTRL_CIRCLE) filebrowser_navigate_back(&browser);

                if (pressed & SCE_CTRL_SQUARE) {
                    snprintf(extraction_dest_path, sizeof(extraction_dest_path), "%s", browser.current_path);
                    current_mode = MODE_EXTRACTING;
                    extract_progress = 0;
                }

                int visible_files_dest = (510 - 110) / 22;
                if (browser.selected_index < scroll_offset) scroll_offset = browser.selected_index;
                if (browser.selected_index >= scroll_offset + visible_files_dest) scroll_offset = browser.selected_index - visible_files_dest + 1;
                break;

            case MODE_ZIP_CREATION:
                if (pressed & SCE_CTRL_UP) filebrowser_navigate_up(&browser);
                if (pressed & SCE_CTRL_DOWN) filebrowser_navigate_down(&browser);
                if (pressed & SCE_CTRL_CROSS) filebrowser_enter(&browser);
                if (pressed & SCE_CTRL_CIRCLE) current_mode = MODE_BROWSER;

                if (pressed & SCE_CTRL_SQUARE) {
                    if (browser.file_count > 0) {
                        browser.selection_mask[browser.selected_index] = !browser.selection_mask[browser.selected_index];
                    }
                }

                if (pressed & SCE_CTRL_TRIANGLE) {
                    char zip_name[256] = {0};
                    if (get_ime_input(zip_name, 250, "Enter ZIP name (.zip will be added)") == 0 && strlen(zip_name) > 0) {
                        char zip_path[MAX_PATH];
                        strncpy(zip_path, browser.current_path, MAX_PATH - 1);
                        zip_path[MAX_PATH - 1] = '\0';
                        strncat(zip_path, zip_name, MAX_PATH - strlen(zip_path) - 5);
                        strncat(zip_path, ".zip", MAX_PATH - strlen(zip_path) - 1);

                        const char *files_to_add[MAX_FILES];
                        int files_to_add_count = 0;
                        char file_paths[MAX_FILES][MAX_PATH];

                        for (int i = 0; i < browser.file_count; i++) {
                            if (browser.selection_mask[i]) {
                                strncpy(file_paths[files_to_add_count], browser.current_path, MAX_PATH - 1);
                                file_paths[files_to_add_count][MAX_PATH - 1] = '\0';
                                strncat(file_paths[files_to_add_count], browser.files[i].name, MAX_PATH - strlen(file_paths[files_to_add_count]) - 1);
                                files_to_add[files_to_add_count] = file_paths[files_to_add_count];
                                files_to_add_count++;
                            }
                        }
                        zip_create(zip_path, files_to_add, files_to_add_count);
                        current_mode = MODE_BROWSER;
                        filebrowser_refresh(&browser);
                    }
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
        }
        
        vita2d_end_drawing();
        vita2d_swap_buffers();
    }
    
    if (archive_info.is_open) archive_close_custom(&archive_info);
    save_settings();
    vpk_cleanup_dirs();
    vita2d_free_pgf(font);
    sceSysmoduleUnloadModule(SCE_SYSMODULE_IME);
    vita2d_fini();
    return 0;
}
