/*
 * VitaArchive - File Archiver & Browser for PS Vita
 * Created by theheroGAC.
 * Special thanks to TheFloW, Rinnegatamante, SKGleba, and all developers, hackers,
 * and contributors of the PlayStation Vita homebrew scene.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <psp2/power.h>
#include <psp2/kernel/processmgr.h>
#include <psp2/ctrl.h>
#include <psp2/common_dialog.h>
#include <psp2/io/stat.h>
#include <psp2/io/fcntl.h>
#include <vita2d.h>
#include "globals.h"
#include "gui.h"
#include "clipboard.h"
#include "vpk.h"
#include "ftp.h"
#include "psarc.h"







int is_archive_file(const char *filename) {
    return is_zip_file(filename) || is_vpk_file(filename) || 
           is_tar_file(filename) || is_gzip_file(filename) || 
           is_bzip2_file(filename) || is_rar_file(filename) || 
           is_7z_file(filename) || is_psarc_file(filename);
}

const char *path_basename(const char *path) {
    const char *slash = strrchr(path, '/');
    return slash ? slash + 1 : path;
}

static void parse_action(const char *action, int *prefix_len, int *icon_width, const char **label, const char **btn_name) {
    *prefix_len = 0;
    *icon_width = 0;
    *label = action;
    *btn_name = NULL;

    if (strncmp(action, "X: ", 3) == 0) {
        *prefix_len = 3;
        *icon_width = 20;
        *label = action + 3;
        *btn_name = "X";
    } else if (strncmp(action, "O: ", 3) == 0) {
        *prefix_len = 3;
        *icon_width = 20;
        *label = action + 3;
        *btn_name = "O";
    } else if (strncmp(action, "SQ: ", 4) == 0) {
        *prefix_len = 4;
        *icon_width = 20;
        *label = action + 4;
        *btn_name = "SQ";
    } else if (strncmp(action, "SQUARE: ", 8) == 0) {
        *prefix_len = 8;
        *icon_width = 20;
        *label = action + 8;
        *btn_name = "SQ";
    } else if (strncmp(action, "TRI: ", 5) == 0) {
        *prefix_len = 5;
        *icon_width = 20;
        *label = action + 5;
        *btn_name = "TRI";
    } else if (strncmp(action, "SEL: ", 5) == 0) {
        *prefix_len = 5;
        *icon_width = 30;
        *label = action + 5;
        *btn_name = "SEL";
    } else if (strncmp(action, "START: ", 7) == 0) {
        *prefix_len = 7;
        *icon_width = 38;
        *label = action + 7;
        *btn_name = "START";
    }
}

int is_viewable_text_file(const char *filename) {
    const char *ext = strrchr(filename, '.');
    if (!ext) return 0;
    
    const char *text_exts[] = {
        ".txt", ".ini", ".cfg", ".xml", ".json", ".yml", ".yaml", 
        ".md", ".sfo", ".inf", ".c", ".h", ".py", ".sh", ".bat", 
        ".log", ".prop", ".csv", ".jsonld", ".lua", ".conf"
    };
    int count = sizeof(text_exts) / sizeof(text_exts[0]);
    for (int i = 0; i < count; i++) {
        if (strcasecmp(ext, text_exts[i]) == 0) {
            return 1;
        }
    }
    return 0;
}





void show_toast(const char *msg, uint32_t color) {
    strncpy(toast_msg, msg, sizeof(toast_msg) - 1);
    toast_msg[sizeof(toast_msg) - 1] = '\0';
    toast_expire = sceKernelGetProcessTimeWide() + 3500000ULL;
    toast_color = color;
}

void draw_toast() {
    if (!toast_msg[0]) return;
    uint64_t now = sceKernelGetProcessTimeWide();
    if (now > toast_expire) {
        toast_msg[0] = '\0';
        return;
    }
    
    int tw = vita2d_pgf_text_width(font, 0.8f, toast_msg);
    int tx = (960 - tw) / 2;
    int ty = 460;
    
    vita2d_draw_rectangle(tx - 15, ty - 22, tw + 30, 30, RGBA8(22, 27, 34, 240));
    vita2d_draw_rectangle(tx - 15, ty - 22, 3, 30, toast_color);
    vita2d_pgf_draw_textf(font, tx, ty - 1, RGBA8(240, 246, 252, 255), 0.8f, "%s", toast_msg);
}

void format_size(char *buf, uint64_t size) {
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

void get_file_visuals(const char *path, int is_directory, const char **icon, uint32_t *icon_color) {
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
    } else if (is_psarc_file(path)) {
        *icon = "[PSA]";
        *icon_color = RGBA8(243, 156, 18, 255);
    } else {
        const char *ext = strrchr(path, '.');
        const char *filename = strrchr(path, '/');
        if (!filename) filename = strrchr(path, '\\');
        if (!filename) filename = path;
        else filename++;

        if ((ext && strcasecmp(ext, ".self") == 0) || strcasecmp(filename, "eboot.bin") == 0) {
            *icon = "[SELF]";
            *icon_color = RGBA8(155, 89, 182, 255);
        } else {
            *icon = "[FILE]";
            *icon_color = RGBA8(110, 118, 129, 255);
        }
    }
}

const char *get_file_type_label(const char *path, int is_directory) {
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
    if (is_psarc_file(path)) {
        return "PSARC";
    }
    return language_get(&lang, STR_FILE);
}

void draw_progress_bar(int x, int y, int width, int height, int progress) {
    vita2d_draw_rectangle(x, y, width, height, RGBA8(33, 38, 45, 255));
    int filled_width = (width * progress) / 100;
    vita2d_draw_rectangle(x, y, filled_width, height, RGBA8(31, 111, 235, 255));
}

void draw_button_icon(int x, int y, const char *btn_name, float scale) {
    int cx = x + 10;
    int cy = y + 17;
    int r = 9;

    uint32_t border_color = RGBA8(240, 246, 252, 255);
    uint32_t bg_color = RGBA8(13, 17, 23, 255);

    if (strcmp(btn_name, "X") == 0) {
        border_color = RGBA8(41, 128, 185, 255);
    } else if (strcmp(btn_name, "O") == 0) {
        border_color = RGBA8(231, 76, 60, 255);
    } else if (strcmp(btn_name, "SQ") == 0) {
        border_color = RGBA8(244, 143, 177, 255);
    } else if (strcmp(btn_name, "TRI") == 0) {
        border_color = RGBA8(46, 204, 113, 255);
    }

    if (strcmp(btn_name, "SEL") != 0 && strcmp(btn_name, "START") != 0) {
        vita2d_draw_rectangle(cx - r, cy - r, r * 2, r * 2, border_color);
        vita2d_draw_rectangle(cx - (r - 1), cy - (r - 1), (r - 1) * 2, (r - 1) * 2, bg_color);
    }

    if (strcmp(btn_name, "X") == 0) {
        vita2d_draw_line(cx - 3, cy - 3, cx + 3, cy + 3, RGBA8(52, 152, 219, 255));
        vita2d_draw_line(cx + 3, cy - 3, cx - 3, cy + 3, RGBA8(52, 152, 219, 255));
    } else if (strcmp(btn_name, "O") == 0) {
        uint32_t symbol_color = RGBA8(231, 76, 60, 255);
        vita2d_draw_line(cx - 2, cy - 4, cx + 2, cy - 4, symbol_color);
        vita2d_draw_line(cx - 2, cy + 4, cx + 2, cy + 4, symbol_color);
        vita2d_draw_line(cx - 4, cy - 2, cx - 4, cy + 2, symbol_color);
        vita2d_draw_line(cx + 4, cy - 2, cx + 4, cy + 2, symbol_color);
        vita2d_draw_line(cx - 4, cy - 2, cx - 2, cy - 4, symbol_color);
        vita2d_draw_line(cx + 2, cy - 4, cx + 4, cy - 2, symbol_color);
        vita2d_draw_line(cx - 4, cy + 2, cx - 2, cy + 4, symbol_color);
        vita2d_draw_line(cx + 2, cy + 4, cx + 4, cy + 2, symbol_color);
    } else if (strcmp(btn_name, "SQ") == 0) {
        uint32_t symbol_color = RGBA8(244, 143, 177, 255);
        vita2d_draw_rectangle(cx - 4, cy - 4, 8, 8, symbol_color);
        vita2d_draw_rectangle(cx - 3, cy - 3, 6, 6, bg_color);
    } else if (strcmp(btn_name, "TRI") == 0) {
        uint32_t symbol_color = RGBA8(46, 204, 113, 255);
        vita2d_draw_line(cx, cy - 4, cx - 4, cy + 3, symbol_color);
        vita2d_draw_line(cx - 4, cy + 3, cx + 4, cy + 3, symbol_color);
        vita2d_draw_line(cx + 4, cy + 3, cx, cy - 4, symbol_color);
    } else if (strcmp(btn_name, "SEL") == 0 || strcmp(btn_name, "START") == 0) {
        int width = (strcmp(btn_name, "SEL") == 0) ? 28 : 36;
        int rx = cx - width / 2;
        int ry = cy - 6;
        vita2d_draw_rectangle(rx, ry, width, 12, RGBA8(149, 165, 166, 255));
        vita2d_draw_rectangle(rx + 1, ry + 1, width - 2, 10, bg_color);
        float tiny_scale = 0.45f;
        int text_w = vita2d_pgf_text_width(font, tiny_scale, btn_name);
        vita2d_pgf_draw_textf(font, cx - text_w / 2, cy + 4, RGBA8(201, 209, 217, 255), tiny_scale, "%s", btn_name);
    }
}

void draw_footer(const char **actions, int count) {
    vita2d_draw_rectangle(0, 510, 960, 34, RGBA8(21, 26, 33, 255));

    float scale = 0.8f;
    int spacing = 30;
    int total_width = 40;

    for (int i = 0; i < count; ++i) {
        if (actions[i] && strlen(actions[i]) > 0) {
            int prefix_len, icon_width;
            const char *label, *btn_name;
            parse_action(actions[i], &prefix_len, &icon_width, &label, &btn_name);
            
            if (btn_name) {
                total_width += icon_width + 8 + vita2d_pgf_text_width(font, scale, label);
            } else {
                total_width += vita2d_pgf_text_width(font, scale, actions[i]);
            }
            if (i < count - 1) {
                total_width += spacing;
            }
        }
    }

    if (total_width > 960) {
        scale = 0.7f;
        spacing = 20;
        total_width = 40;
        for (int i = 0; i < count; ++i) {
            if (actions[i] && strlen(actions[i]) > 0) {
                int prefix_len, icon_width;
                const char *label, *btn_name;
                parse_action(actions[i], &prefix_len, &icon_width, &label, &btn_name);
                
                if (btn_name) {
                    total_width += icon_width + 8 + vita2d_pgf_text_width(font, scale, label);
                } else {
                    total_width += vita2d_pgf_text_width(font, scale, actions[i]);
                }
                if (i < count - 1) {
                    total_width += spacing;
                }
            }
        }
    }

    if (total_width > 960) {
        scale = 0.6f;
        spacing = 15;
    }

    int x_pos = 20;
    for (int i = 0; i < count; ++i) {
        if (actions[i] && strlen(actions[i]) > 0) {
            int prefix_len, icon_width;
            const char *label, *btn_name;
            parse_action(actions[i], &prefix_len, &icon_width, &label, &btn_name);
            
            if (btn_name) {
                draw_button_icon(x_pos, 510, btn_name, scale);
                vita2d_pgf_draw_textf(font, x_pos + icon_width + 8, 532, RGBA8(201, 209, 217, 255), scale, "%s", label);
                x_pos += icon_width + 8 + vita2d_pgf_text_width(font, scale, label) + spacing;
            } else {
                vita2d_pgf_draw_textf(font, x_pos, 532, RGBA8(201, 209, 217, 255), scale, "%s", actions[i]);
                x_pos += vita2d_pgf_text_width(font, scale, actions[i]) + spacing;
            }
        }
    }
}

void draw_browser() {
    vita2d_draw_rectangle(0, 0, 960, 50, RGBA8(21, 26, 33, 255));
    vita2d_pgf_draw_textf(font, 10, 30, RGBA8(240, 246, 252, 255), 1.0f, "%s", language_get(&lang, STR_APP_TITLE));
    
    vita2d_draw_rectangle(0, 50, 960, 30, RGBA8(28, 33, 40, 255));
    vita2d_pgf_draw_textf(font, 10, 70, RGBA8(201, 209, 217, 255), 0.8f, "%s: %s", 
        language_get(&lang, STR_PATH), browser.current_path);
    
    uint64_t free_sp = 0, total_sp = 0;
    if (strcmp(browser.current_path, "/") != 0 && get_partition_free_space(browser.current_path, &free_sp, &total_sp) == 0) {
        char free_buf[32], total_buf[32], space_str[128];
        format_size(free_buf, free_sp);
        format_size(total_buf, total_sp);
        snprintf(space_str, sizeof(space_str), "%s / %s free", free_buf, total_buf);
        int tw = vita2d_pgf_text_width(font, 0.8f, space_str);
        vita2d_pgf_draw_textf(font, 950 - tw, 70, RGBA8(139, 148, 158, 255), 0.8f, "%s", space_str);
    }
    
    int header_y = 85;
    vita2d_draw_rectangle(0, header_y, 960, 25, RGBA8(33, 38, 45, 255));
    vita2d_pgf_draw_textf(font, 10, header_y + 18, RGBA8(240, 246, 252, 255), 0.85f, "%s", language_get(&lang, STR_NAME));
    vita2d_pgf_draw_textf(font, 600, header_y + 18, RGBA8(240, 246, 252, 255), 0.85f, "%s", language_get(&lang, STR_SIZE));
    vita2d_pgf_draw_textf(font, 800, header_y + 18, RGBA8(240, 246, 252, 255), 0.85f, "%s", language_get(&lang, STR_TYPE));
    int start_y = 110;
    int line_height = 22;

    if (current_mode == MODE_ZIP_CREATION) {
        LanguageCode cl = language_get_current(&lang);
        const char *frag1 = "Selection mode: press ";
        const char *frag2 = " to select files, ";
        const char *frag3 = " to create archive";
        
        if (cl == LANG_IT) {
            frag1 = "Modalita' selezione file: premi ";
            frag2 = " per selezionare, ";
            frag3 = " per creare archivio";
        } else if (cl == LANG_ES) {
            frag1 = "Modo seleccion: pulsa ";
            frag2 = " para seleccionar, ";
            frag3 = " para crear archivo";
        } else if (cl == LANG_FR) {
            frag1 = "Mode selection: ";
            frag2 = " pour selectionner, ";
            frag3 = " pour creer l'archive";
        } else if (cl == LANG_DE) {
            frag1 = "Auswahlmodus: ";
            frag2 = " zum Auswaehlen, ";
            frag3 = " zum Erstellen";
        } else if (cl == LANG_JPN) {
            frag1 = "選択モード: ";
            frag2 = " で選択、 ";
            frag3 = " でアーカイブ作成";
        }
        
        vita2d_draw_rectangle(0, start_y, 960, 22, RGBA8(180, 100, 0, 220));
        vita2d_draw_rectangle(0, start_y, 4, 22, RGBA8(255, 160, 0, 255));
        
        int w1 = vita2d_pgf_text_width(font, 0.70f, frag1);
        int w2 = 24;
        int w3 = vita2d_pgf_text_width(font, 0.70f, frag2);
        int w4 = 24;
        int w5 = vita2d_pgf_text_width(font, 0.70f, frag3);
        int total_w = w1 + w2 + w3 + w4 + w5;
        
        int hx = (960 - total_w) / 2;
        
        vita2d_pgf_draw_textf(font, hx, start_y + 15, RGBA8(255, 230, 180, 255), 0.70f, "%s", frag1);
        hx += w1;
        
        draw_button_icon(hx + 2, start_y - 6, "SQ", 0.75f);
        hx += w2;
        
        vita2d_pgf_draw_textf(font, hx, start_y + 15, RGBA8(255, 230, 180, 255), 0.70f, "%s", frag2);
        hx += w3;
        
        draw_button_icon(hx + 2, start_y - 6, "TRI", 0.75f);
        hx += w4;
        
        vita2d_pgf_draw_textf(font, hx, start_y + 15, RGBA8(255, 230, 180, 255), 0.70f, "%s", frag3);
        
        start_y += 24;
    }

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
        
        int icon_x = 10;
        int text_x = 60;
        if (current_mode == MODE_ZIP_CREATION) {
            icon_x = 36;
            text_x = 86;
            
            vita2d_draw_rectangle(10, y + 4, 14, 14, RGBA8(110, 118, 129, 255));
            vita2d_draw_rectangle(12, y + 6, 10, 10, RGBA8(13, 17, 23, 255));
            if (browser.selection_mask[idx]) {
                vita2d_draw_rectangle(12, y + 6, 10, 10, RGBA8(46, 204, 113, 255));
            }
        }
        
        vita2d_pgf_draw_textf(font, icon_x, y + 16, icon_color, 0.8f, "%s", icon);
        
        uint32_t text_color = (idx == browser.selected_index) ? RGBA8(255, 255, 255, 255) : RGBA8(201, 209, 217, 255);
        vita2d_pgf_draw_textf(font, text_x, y + 16, text_color, 0.85f, "%s", file->name);

        if (!file->is_directory) {
            char size_buf[32];
            format_size(size_buf, file->size);
            vita2d_pgf_draw_textf(font, 600, y + 16, RGBA8(139, 148, 158, 255), 0.8f, "%s", size_buf);
        } else if (strcmp(browser.current_path, "/") == 0) {
            uint64_t p_free = 0, p_total = 0;
            if (get_partition_free_space(file->name, &p_free, &p_total) == 0) {
                char free_buf[32], total_buf[32], space_str[128];
                format_size(free_buf, p_free);
                format_size(total_buf, p_total);
                snprintf(space_str, sizeof(space_str), "%s / %s", free_buf, total_buf);
                vita2d_pgf_draw_textf(font, 600, y + 16, RGBA8(139, 148, 158, 255), 0.8f, "%s", space_str);
            }
        }
        
        const char *type = get_file_type_label(file->name, file->is_directory);
        uint32_t type_color = (idx == browser.selected_index) ? RGBA8(255, 255, 255, 255) : icon_color;
        vita2d_pgf_draw_textf(font, 800, y + 16, type_color, 0.8f, "%s", type);
    }
}

void draw_browser_footer() {
    if (current_mode == MODE_ZIP_CREATION) {
        char sq_str[64], tri_str[64], o_str[64];
        snprintf(sq_str, sizeof(sq_str), "SQ: %s", language_get(&lang, STR_TOGGLE_SELECT));
        snprintf(tri_str, sizeof(tri_str), "TRI: %s", language_get(&lang, STR_CREATE_ZIP));
        snprintf(o_str, sizeof(o_str), "O: %s", language_get(&lang, STR_CANCEL));

        const char *actions[] = {sq_str, tri_str, o_str};
        draw_footer(actions, 3);
    } else {
        char x_str[64], o_str[64], sq_str[64], tri_str[64], sel_str[64], start_str[64];
        snprintf(x_str, sizeof(x_str), "X: %s", language_get(&lang, STR_SELECT_ENTER));
        snprintf(o_str, sizeof(o_str), "O: %s", language_get(&lang, STR_BACK));
        snprintf(sq_str, sizeof(sq_str), "SQ: %s", language_get(&lang, STR_DELETE));
        snprintf(tri_str, sizeof(tri_str), "TRI: %s", language_get(&lang, STR_ACTIONS));
        snprintf(sel_str, sizeof(sel_str), "SEL: FTP Server");
        snprintf(start_str, sizeof(start_str), "START: %s",
            (language_get_current(&lang) == LANG_IT) ? "Impostazioni" :
            (language_get_current(&lang) == LANG_ES) ? "Ajustes" :
            (language_get_current(&lang) == LANG_FR) ? "Paramètres" :
            (language_get_current(&lang) == LANG_DE) ? "Einstellungen" :
            (language_get_current(&lang) == LANG_JPN) ? "設定" :
            "Settings");

        const char *actions[] = {x_str, o_str, sq_str, tri_str, sel_str, start_str};
        draw_footer(actions, 6);
    }
}

void draw_dest_browser() {
    vita2d_draw_rectangle(0, 0, 960, 50, RGBA8(21, 26, 33, 255));
    vita2d_pgf_draw_textf(font, 10, 30, RGBA8(240, 246, 252, 255), 1.0f, "%s", language_get(&lang, STR_SELECT_DEST_FOLDER));

    draw_browser();

    char x_str[64], o_str[64], sq_str[64];
    snprintf(x_str, sizeof(x_str), "X: %s", language_get(&lang, STR_SELECT_ENTER));
    snprintf(o_str, sizeof(o_str), "O: %s", language_get(&lang, STR_BACK));
    snprintf(sq_str, sizeof(sq_str), "SQ: %s", language_get(&lang, STR_CONFIRM_DEST));
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
        
        int icon_x = 36;
        int text_x = 86;
        
        vita2d_draw_rectangle(10, y + 4, 14, 14, RGBA8(110, 118, 129, 255));
        vita2d_draw_rectangle(12, y + 6, 10, 10, RGBA8(13, 17, 23, 255));
        if (archive_selection_mask[idx]) {
            vita2d_draw_rectangle(12, y + 6, 10, 10, RGBA8(46, 204, 113, 255));
        }
        
        vita2d_pgf_draw_textf(font, icon_x, y + 16, icon_color, 0.8f, "%s", icon);
        
        uint32_t text_color = (idx == archive_selected) ? RGBA8(255, 255, 255, 255) : RGBA8(201, 209, 217, 255);
        vita2d_pgf_draw_textf(font, text_x, y + 16, text_color, 0.85f, "%s", file->filename);
        
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

    char x_str[64], sq_str[64], o_str[64], tri_str[64], start_str[64], sel_str[64] = "";
    int vpk_selected = archive_info.file_count > 0 &&
        is_vpk_file(archive_info.files[archive_selected].filename);
    int smart_install_available = !vpk_selected && archive_can_smart_install(&archive_info);
    int can_preview = archive_info.file_count > 0 &&
        is_viewable_text_file(archive_info.files[archive_selected].filename);
    int is_nested_arch = archive_info.file_count > 0 &&
        is_archive_file(archive_info.files[archive_selected].filename);
    
    LanguageCode curr_lang = language_get_current(&lang);
    const char *extract_sel_label = "Extract Selected";
    if (curr_lang == LANG_IT) extract_sel_label = "Estrai Selezionato";
    else if (curr_lang == LANG_ES) extract_sel_label = "Extraer Seleccionado";
    else if (curr_lang == LANG_FR) extract_sel_label = "Extraire la selection";
    else if (curr_lang == LANG_DE) extract_sel_label = "Auswahl entpacken";
    else if (curr_lang == LANG_JPN) extract_sel_label = "選択項目を展開";
    
    if (vpk_selected) {
        snprintf(x_str, sizeof(x_str), "X: %s", language_get(&lang, STR_PRESS_X_INSTALL_VPK));
    } else if (smart_install_available) {
        snprintf(x_str, sizeof(x_str), "X: %s", language_get(&lang, STR_SMART_INSTALL));
    } else {
        snprintf(x_str, sizeof(x_str), "X: %s", extract_sel_label);
    }
    
    snprintf(sq_str, sizeof(sq_str), "SQ: %s", language_get(&lang, STR_TOGGLE_SELECT));
    snprintf(tri_str, sizeof(tri_str), "TRI: %s", language_get(&lang, STR_ACTIONS));
    snprintf(o_str, sizeof(o_str), "O: %s", language_get(&lang, STR_BACK));
    
    snprintf(start_str, sizeof(start_str), "START: %s",
        (language_get_current(&lang) == LANG_IT) ? "Impostazioni" :
        (language_get_current(&lang) == LANG_ES) ? "Ajustes" :
        (language_get_current(&lang) == LANG_FR) ? "Paramètres" :
        (language_get_current(&lang) == LANG_DE) ? "Einstellungen" :
        (language_get_current(&lang) == LANG_JPN) ? "設定" :
        "Settings");
        
    if (can_preview || is_nested_arch) {
        const char *prev_label = "Preview";
        if (is_nested_arch) {
            prev_label = "Open";
            if (curr_lang == LANG_IT) prev_label = "Apri";
            else if (curr_lang == LANG_ES) prev_label = "Abrir";
            else if (curr_lang == LANG_FR) prev_label = "Ouvrir";
            else if (curr_lang == LANG_DE) prev_label = "Oeffnen";
            else if (curr_lang == LANG_JPN) prev_label = "開く";
        } else {
            if (curr_lang == LANG_IT) prev_label = "Anteprima";
            else if (curr_lang == LANG_ES) prev_label = "Vista previa";
            else if (curr_lang == LANG_FR) prev_label = "Apercu";
            else if (curr_lang == LANG_DE) prev_label = "Vorschau";
            else if (curr_lang == LANG_JPN) prev_label = "プレビュー";
        }
        snprintf(sel_str, sizeof(sel_str), "SEL: %s", prev_label);
    }
        
    const char *actions[] = {x_str, sq_str, tri_str, o_str, sel_str};
    draw_footer(actions, 5);
}

void draw_worker_stats(int panel_y) {
    if (!worker_running || worker_processed_bytes == 0) {
        vita2d_pgf_draw_textf(font, 120, panel_y + 140, RGBA8(139, 148, 158, 255), 0.85f, "Speed: -- | ETA: --");
        return;
    }
    
    uint64_t current_time = sceKernelGetSystemTimeWide();
    uint64_t elapsed_time_us = current_time - worker_start_time;
    double elapsed_secs = (double)elapsed_time_us / 1000000.0;
    
    double speed_bytes = 0.0;
    if (elapsed_secs > 0.1) {
        speed_bytes = (double)worker_processed_bytes / elapsed_secs;
    }
    
    double eta_secs = -1.0;
    if (speed_bytes > 10.0) {
        uint64_t remaining_bytes = (worker_total_bytes > worker_processed_bytes) ? (worker_total_bytes - worker_processed_bytes) : 0;
        eta_secs = (double)remaining_bytes / speed_bytes;
    }
    
    char speed_str[32];
    if (speed_bytes < 1024.0) {
        snprintf(speed_str, sizeof(speed_str), "%.0f B/s", speed_bytes);
    } else if (speed_bytes < 1024.0 * 1024.0) {
        snprintf(speed_str, sizeof(speed_str), "%.1f KB/s", speed_bytes / 1024.0);
    } else {
        snprintf(speed_str, sizeof(speed_str), "%.1f MB/s", speed_bytes / (1024.0 * 1024.0));
    }
    
    char eta_str[32];
    if (eta_secs < 0.0) {
        snprintf(eta_str, sizeof(eta_str), "--:--");
    } else if (eta_secs < 60.0) {
        snprintf(eta_str, sizeof(eta_str), "00:%02d", (int)eta_secs);
    } else if (eta_secs < 3600.0) {
        int minutes = (int)eta_secs / 60;
        int seconds = (int)eta_secs % 60;
        snprintf(eta_str, sizeof(eta_str), "%02d:%02d", minutes, seconds);
    } else {
        int hours = (int)eta_secs / 3600;
        int minutes = ((int)eta_secs % 3600) / 60;
        int seconds = (int)eta_secs % 60;
        snprintf(eta_str, sizeof(eta_str), "%02d:%02d:%02d", hours, minutes, seconds);
    }
    
    LanguageCode curr_lang = language_get_current(&lang);
    char stats_text[128];
    if (curr_lang == LANG_IT) {
        snprintf(stats_text, sizeof(stats_text), "Velocita': %s | Rimanente: %s", speed_str, eta_str);
    } else if (curr_lang == LANG_ES) {
        snprintf(stats_text, sizeof(stats_text), "Velocidad: %s | Restante: %s", speed_str, eta_str);
    } else if (curr_lang == LANG_FR) {
        snprintf(stats_text, sizeof(stats_text), "Vitesse: %s | Restant: %s", speed_str, eta_str);
    } else if (curr_lang == LANG_DE) {
        snprintf(stats_text, sizeof(stats_text), "Geschwindigkeit: %s | Restzeit: %s", speed_str, eta_str);
    } else if (curr_lang == LANG_JPN) {
        snprintf(stats_text, sizeof(stats_text), "速度: %s | 残り時間: %s", speed_str, eta_str);
    } else {
        snprintf(stats_text, sizeof(stats_text), "Speed: %s | Remaining: %s", speed_str, eta_str);
    }
    
    vita2d_pgf_draw_textf(font, 120, panel_y + 140, RGBA8(139, 148, 158, 255), 0.85f, "%s", stats_text);
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
    
    if (extract_progress < 100) {
        draw_worker_stats(panel_y);
    } else {
        const char *status = language_get(&lang, STR_COMPLETE);
        vita2d_pgf_draw_textf(font, 120, panel_y + 140, RGBA8(139, 148, 158, 255), 0.9f, "%s", status);
    }
    
    char sq_str[64], start_str[64];
    snprintf(sq_str, sizeof(sq_str), "SQ: %s", language_get(&lang, STR_CANCEL_EXTRACTION));
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

        draw_worker_stats(panel_y);
    } else {
        if (success) {
            const char *result = (current_install_mode == INSTALL_MODE_APP
                ? language_get(&lang, STR_APP_INSTALL_COMPLETE)
                : language_get(&lang, STR_VPK_INSTALL_COMPLETE));
            vita2d_pgf_draw_textf(font, 120, panel_y + 100, RGBA8(46, 204, 113, 255), 1.0f, "%s", result);
        } else {
            char err_msg[128];
            snprintf(err_msg, sizeof(err_msg), "%s (Error: 0x%08X)", 
                (current_install_mode == INSTALL_MODE_APP
                    ? language_get(&lang, STR_APP_INSTALL_ERROR)
                    : language_get(&lang, STR_VPK_INSTALL_ERROR)),
                (unsigned int)worker_result);
            vita2d_pgf_draw_textf(font, 120, panel_y + 100, RGBA8(231, 76, 60, 255), 0.9f, "%s", err_msg);
        }
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

void draw_compress_format_select() {
    vita2d_draw_rectangle(0, 0, 960, 50, RGBA8(21, 26, 33, 255));
    
    LanguageCode curr_lang = language_get_current(&lang);
    const char *title = "Select Compression Format";
    if (curr_lang == LANG_IT) title = "Seleziona Formato di Compressione";
    else if (curr_lang == LANG_ES) title = "Seleccionar Formato de Compresión";
    else if (curr_lang == LANG_FR) title = "Sélectionner le format de compression";
    else if (curr_lang == LANG_DE) title = "Kompressionsformat auswählen";
    else if (curr_lang == LANG_JPN) title = "圧縮形式の選択";

    vita2d_pgf_draw_textf(font, 10, 30, RGBA8(240, 246, 252, 255), 1.0f, "%s", title);

    int start_y = 150;
    int line_height = 40;
    const char *formats[] = {
        "ZIP (.zip)",
        "7Z (.7z)",
        "TAR (.tar)",
        "TAR.GZ (.tar.gz)",
        "TAR.BZ2 (.tar.bz2)"
    };
    int formats_count = 5;

    for (int i = 0; i < formats_count; i++) {
        int y = start_y + (i * line_height);
        if (i == compress_format_selected) {
            vita2d_draw_rectangle(100, y - 5, 760, line_height, RGBA8(31, 111, 235, 255));
        } else {
            vita2d_draw_rectangle(100, y - 5, 760, line_height, RGBA8(22, 27, 34, 255));
        }
        
        uint32_t text_color = (i == compress_format_selected) ? RGBA8(255, 255, 255, 255) : RGBA8(201, 209, 217, 255);
        vita2d_pgf_draw_textf(font, 120, y + 22, text_color, 1.0f, "%s", formats[i]);
    }

    char x_str[64], o_str[64];
    snprintf(x_str, sizeof(x_str), "X: %s", language_get(&lang, STR_SELECT_ENTER));
    snprintf(o_str, sizeof(o_str), "O: %s", language_get(&lang, STR_BACK));
    const char *actions[] = {x_str, o_str};
    draw_footer(actions, 2);
}

void draw_delete_confirm() {
    vita2d_draw_rectangle(0, 0, 960, 50, RGBA8(21, 26, 33, 255));
    vita2d_pgf_draw_textf(font, 10, 30, RGBA8(240, 246, 252, 255), 1.0f, "%s - %s",
        language_get(&lang, STR_APP_TITLE), language_get(&lang, STR_DELETE));

    int panel_y = 150;
    vita2d_draw_rectangle(100, panel_y, 760, 200, RGBA8(22, 27, 34, 255));
    vita2d_draw_rectangle(100, panel_y, 760, 1, RGBA8(31, 111, 235, 255));

    const char *selected_path = filebrowser_get_selected_path(&browser);
    const char *filename = selected_path ? strrchr(selected_path, '/') : NULL;
    filename = filename ? filename + 1 : selected_path;

    char msg1[256];
    snprintf(msg1, sizeof(msg1), "%s?", language_get(&lang, STR_DELETE));
    
    vita2d_pgf_draw_textf(font, 120, panel_y + 60, RGBA8(240, 246, 252, 255), 0.9f, "%s", msg1);
    vita2d_pgf_draw_textf(font, 120, panel_y + 110, RGBA8(201, 209, 217, 255), 0.9f, "%s", filename ? filename : "");

    char x_str[64], o_str[64];
    snprintf(x_str, sizeof(x_str), "X: %s", language_get(&lang, STR_DELETE));
    snprintf(o_str, sizeof(o_str), "O: %s", language_get(&lang, STR_CANCEL));
    const char *actions[] = {x_str, o_str};
    draw_footer(actions, 2);
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

void draw_text_preview() {
    vita2d_draw_rectangle(0, 0, 960, 50, RGBA8(21, 26, 33, 255));
    vita2d_pgf_draw_textf(font, 10, 30, RGBA8(240, 246, 252, 255), 1.0f, "%s - %s", 
        language_get(&lang, STR_APP_TITLE), preview_filename);
        
    int panel_y = 65;
    int visible_lines = (510 - panel_y) / 22;

    vita2d_draw_rectangle(15, panel_y, 930, visible_lines * 22 + 6, RGBA8(22, 27, 34, 255));
    vita2d_draw_rectangle(15, panel_y, 930, 1, RGBA8(31, 111, 235, 255));
    
    int y = panel_y + 18;
    for (int i = 0; i < visible_lines; i++) {
        int line_idx = preview_scroll + i;
        if (line_idx >= preview_line_count) break;
        
        if (line_idx == preview_selected_line) {
            vita2d_draw_rectangle(17, y + (i * 22) - 16, 926, 22, RGBA8(38, 44, 54, 255));
            vita2d_pgf_draw_textf(font, 30, y + (i * 22), RGBA8(255, 255, 255, 255), 0.85f, "%s", 
                preview_lines[line_idx]);
        } else {
            vita2d_pgf_draw_textf(font, 30, y + (i * 22), RGBA8(201, 209, 217, 255), 0.85f, "%s", 
                preview_lines[line_idx]);
        }
    }

    char scroll_str[64];
    LanguageCode cl = language_get_current(&lang);
    if (cl == LANG_IT) {
        snprintf(scroll_str, sizeof(scroll_str), "Riga: %d/%d", 
            preview_line_count > 0 ? preview_selected_line + 1 : 0, 
            preview_line_count);
    } else if (cl == LANG_ES) {
        snprintf(scroll_str, sizeof(scroll_str), "Línea: %d/%d", 
            preview_line_count > 0 ? preview_selected_line + 1 : 0, 
            preview_line_count);
    } else if (cl == LANG_FR) {
        snprintf(scroll_str, sizeof(scroll_str), "Ligne: %d/%d", 
            preview_line_count > 0 ? preview_selected_line + 1 : 0, 
            preview_line_count);
    } else if (cl == LANG_DE) {
        snprintf(scroll_str, sizeof(scroll_str), "Zeile: %d/%d", 
            preview_line_count > 0 ? preview_selected_line + 1 : 0, 
            preview_line_count);
    } else if (cl == LANG_JPN) {
        snprintf(scroll_str, sizeof(scroll_str), "行: %d/%d", 
            preview_line_count > 0 ? preview_selected_line + 1 : 0, 
            preview_line_count);
    } else {
        snprintf(scroll_str, sizeof(scroll_str), "Line: %d/%d", 
            preview_line_count > 0 ? preview_selected_line + 1 : 0, 
            preview_line_count);
    }
        
    char o_str[64];
    snprintf(o_str, sizeof(o_str), "O: %s", language_get(&lang, STR_BACK));
    
    int preview_is_local = (strncmp(preview_filepath, VPK_TEMP_DIR, strlen(VPK_TEMP_DIR)) != 0);
    if (preview_is_local && preview_line_count > 0) {
        char edit_str[64], save_str[64], add_str[64], del_str[64];
        if (cl == LANG_IT) {
            snprintf(edit_str, sizeof(edit_str), "X: Modifica");
            snprintf(save_str, sizeof(save_str), "TRI: Salva");
            snprintf(add_str, sizeof(add_str), "SQ: Inserisci");
            snprintf(del_str, sizeof(del_str), "L: Elimina");
        } else if (cl == LANG_ES) {
            snprintf(edit_str, sizeof(edit_str), "X: Editar");
            snprintf(save_str, sizeof(save_str), "TRI: Guardar");
            snprintf(add_str, sizeof(add_str), "SQ: Insertar");
            snprintf(del_str, sizeof(del_str), "L: Eliminar");
        } else if (cl == LANG_FR) {
            snprintf(edit_str, sizeof(edit_str), "X: Modifier");
            snprintf(save_str, sizeof(save_str), "TRI: Sauvegarder");
            snprintf(add_str, sizeof(add_str), "SQ: Insérer");
            snprintf(del_str, sizeof(del_str), "L: Supprimer");
        } else if (cl == LANG_DE) {
            snprintf(edit_str, sizeof(edit_str), "X: Bearbeiten");
            snprintf(save_str, sizeof(save_str), "TRI: Speichern");
            snprintf(add_str, sizeof(add_str), "SQ: Einfügen");
            snprintf(del_str, sizeof(del_str), "L: Löschen");
        } else if (cl == LANG_JPN) {
            snprintf(edit_str, sizeof(edit_str), "X: 編集");
            snprintf(save_str, sizeof(save_str), "TRI: 保存");
            snprintf(add_str, sizeof(add_str), "SQ: 挿入");
            snprintf(del_str, sizeof(del_str), "L: 削除");
        } else {
            snprintf(edit_str, sizeof(edit_str), "X: Edit");
            snprintf(save_str, sizeof(save_str), "TRI: Save");
            snprintf(add_str, sizeof(add_str), "SQ: Insert");
            snprintf(del_str, sizeof(del_str), "L: Delete");
        }
        
        if (preview_is_sfo) {
            const char *actions[] = {o_str, edit_str, save_str, scroll_str};
            draw_footer(actions, 4);
        } else {
            const char *actions[] = {o_str, edit_str, save_str, add_str, del_str, scroll_str};
            draw_footer(actions, 6);
        }
    } else {
        const char *actions[] = {o_str, scroll_str};
        draw_footer(actions, 2);
    }
}

void draw_ftp_server() {
    vita2d_draw_rectangle(0, 0, 960, 50, RGBA8(21, 26, 33, 255));
    vita2d_pgf_draw_textf(font, 10, 30, RGBA8(240, 246, 252, 255), 1.0f, "%s - %s", 
        language_get(&lang, STR_APP_TITLE), "FTP Server");
        
    int panel_y = 120;
    vita2d_draw_rectangle(150, panel_y, 660, 260, RGBA8(22, 27, 34, 255));
    vita2d_draw_rectangle(150, panel_y, 660, 1, RGBA8(31, 111, 235, 255));
    
    vita2d_pgf_draw_textf(font, 180, panel_y + 40, RGBA8(240, 246, 252, 255), 1.2f, "FTP Server Active");
    
    char buf[128];
    snprintf(buf, sizeof(buf), "IP Address: %s", ftp_ip);
    vita2d_pgf_draw_textf(font, 180, panel_y + 90, RGBA8(201, 209, 217, 255), 1.0f, "%s", buf);
    
    snprintf(buf, sizeof(buf), "Port: %u", ftp_port);
    vita2d_pgf_draw_textf(font, 180, panel_y + 125, RGBA8(201, 209, 217, 255), 1.0f, "%s", buf);
    
    vita2d_pgf_draw_textf(font, 180, panel_y + 180, RGBA8(139, 148, 158, 255), 0.85f, "Status:");
    vita2d_pgf_draw_textf(font, 180, panel_y + 210, RGBA8(241, 196, 15, 255), 0.95f, "%s", ftp_server_get_status());
    
    char o_str[64];
    LanguageCode curr_lang = language_get_current(&lang);
    const char *stop_label = "Stop FTP and Back";
    if (curr_lang == LANG_IT) stop_label = "Ferma FTP e Indietro";
    else if (curr_lang == LANG_ES) stop_label = "Detener FTP y Atrás";
    else if (curr_lang == LANG_FR) stop_label = "Arrêter FTP et Retour";
    else if (curr_lang == LANG_DE) stop_label = "FTP Stoppen und Zurück";
    else if (curr_lang == LANG_JPN) stop_label = "FTP停止 e 戻る";
    
    snprintf(o_str, sizeof(o_str), "O: %s", stop_label);
    const char *actions[] = {o_str};
    draw_footer(actions, 1);
}

void draw_settings() {
    vita2d_draw_rectangle(0, 0, 960, 50, RGBA8(21, 26, 33, 255));

    LanguageCode curr_lang = language_get_current(&lang);
    const char *title =
        (curr_lang == LANG_IT) ? "Impostazioni" :
        (curr_lang == LANG_ES) ? "Ajustes" :
        (curr_lang == LANG_FR) ? "Paramètres" :
        (curr_lang == LANG_DE) ? "Einstellungen" :
        (curr_lang == LANG_JPN) ? "設定" :
        "Settings";

    vita2d_pgf_draw_textf(font, 10, 30, RGBA8(240, 246, 252, 255), 1.0f, "%s - %s",
        language_get(&lang, STR_APP_TITLE), title);

    int panel_x = 80, panel_w = 800, panel_y = 100;
    vita2d_draw_rectangle(panel_x, panel_y, panel_w, 300, RGBA8(22, 27, 34, 255));
    vita2d_draw_rectangle(panel_x, panel_y, panel_w, 2, RGBA8(31, 111, 235, 255));

    int row0_y = panel_y + 30;
    if (settings_item_selected == 0)
        vita2d_draw_rectangle(panel_x + 10, row0_y - 8, panel_w - 20, 56, RGBA8(31, 111, 235, 40));

    const char *lang_label =
        (curr_lang == LANG_IT) ? "Lingua" :
        (curr_lang == LANG_ES) ? "Idioma" :
        (curr_lang == LANG_FR) ? "Langue" :
        (curr_lang == LANG_DE) ? "Sprache" :
        (curr_lang == LANG_JPN) ? "言語" :
        "Language";
    vita2d_pgf_draw_textf(font, panel_x + 24, row0_y + 20,
        RGBA8(201, 209, 217, 255), 1.0f, "%s", lang_label);

    char lang_val[64];
    snprintf(lang_val, sizeof(lang_val), (settings_item_selected == 0) ? "< %s >" : "%s",
        language_get_name(settings_selected));
    int lw = vita2d_pgf_text_width(font, 1.0f, lang_val);
    vita2d_pgf_draw_textf(font, panel_x + panel_w - lw - 24, row0_y + 20,
        (settings_item_selected == 0) ? RGBA8(31, 111, 235, 255) : RGBA8(240, 246, 252, 255),
        1.0f, "%s", lang_val);

    vita2d_draw_rectangle(panel_x + 10, panel_y + 100, panel_w - 20, 1, RGBA8(48, 54, 61, 255));

    int row1_y = panel_y + 130;
    if (settings_item_selected == 1)
        vita2d_draw_rectangle(panel_x + 10, row1_y - 8, panel_w - 20, 56, RGBA8(31, 111, 235, 40));

    const char *comp_label =
        (curr_lang == LANG_IT) ? "Livello di Compressione" :
        (curr_lang == LANG_ES) ? "Nivel de Compresion" :
        (curr_lang == LANG_FR) ? "Niveau de compression" :
        (curr_lang == LANG_DE) ? "Kompressionsstufe" :
        (curr_lang == LANG_JPN) ? "圧縮レベル" :
        "Compression Level";
    vita2d_pgf_draw_textf(font, panel_x + 24, row1_y + 20,
        RGBA8(201, 209, 217, 255), 1.0f, "%s", comp_label);

    const char *level_names[] = {"Store (None)", "Normal", "Best (Slow)"};
    if (curr_lang == LANG_IT) { level_names[0] = "Store (Nessuna)"; level_names[1] = "Normale"; level_names[2] = "Massima (Lenta)"; }
    else if (curr_lang == LANG_ES) { level_names[0] = "Sin compresion"; level_names[2] = "Maxima (Lenta)"; }
    else if (curr_lang == LANG_FR) { level_names[0] = "Sans compression"; level_names[2] = "Maximale (Lente)"; }
    else if (curr_lang == LANG_DE) { level_names[0] = "Ohne Kompression"; level_names[2] = "Maximum (Langsam)"; }
    int cl = (compress_level < 0) ? 0 : (compress_level > 2) ? 2 : compress_level;
    char comp_val[64];
    snprintf(comp_val, sizeof(comp_val), (settings_item_selected == 1) ? "< %s >" : "%s",
        level_names[cl]);
    int clw = vita2d_pgf_text_width(font, 1.0f, comp_val);
    vita2d_pgf_draw_textf(font, panel_x + panel_w - clw - 24, row1_y + 20,
        (settings_item_selected == 1) ? RGBA8(31, 111, 235, 255) : RGBA8(240, 246, 252, 255),
        1.0f, "%s", comp_val);

    vita2d_pgf_draw_textf(font, panel_x + 24, panel_y + 240,
        RGBA8(100, 110, 120, 255), 0.85f,
        (curr_lang == LANG_IT) ? "SU/GIU: Riga   SX/DX: Cambia valore   O: Salva e Indietro" :
        (curr_lang == LANG_ES) ? "AR/AB: Fila   IZ/DE: Cambiar valor   O: Guardar y Volver" :
        "UP/DOWN: Row   LEFT/RIGHT: Change value   O: Save & Back");

    char o_str[64];
    snprintf(o_str, sizeof(o_str), "O: %s", language_get(&lang, STR_BACK));
    const char *actions[] = {o_str};
    draw_footer(actions, 1);
}

void draw_battery_status() {
    int pct = scePowerGetBatteryLifePercent();
    int charging = scePowerIsBatteryCharging();
    
    char bat_str[16];
    snprintf(bat_str, sizeof(bat_str), "%d%%", pct);
    
    uint32_t bat_color = RGBA8(46, 204, 113, 255);
    if (charging) {
        bat_color = RGBA8(52, 152, 219, 255);
    } else if (pct <= 20) {
        bat_color = RGBA8(231, 76, 60, 255);
    } else if (pct <= 50) {
        bat_color = RGBA8(241, 196, 15, 255);
    }
    
    int x = 960 - 70;
    int y = 18;

    vita2d_draw_rectangle(x, y, 35, 1, RGBA8(139, 148, 158, 255));
    vita2d_draw_rectangle(x, y + 15, 35, 1, RGBA8(139, 148, 158, 255));
    vita2d_draw_rectangle(x, y, 1, 16, RGBA8(139, 148, 158, 255));
    vita2d_draw_rectangle(x + 34, y, 1, 16, RGBA8(139, 148, 158, 255));
    vita2d_draw_rectangle(x + 35, y + 4, 2, 8, RGBA8(139, 148, 158, 255));
    
    int fill_width = (31 * pct) / 100;
    if (fill_width > 0) {
        vita2d_draw_rectangle(x + 2, y + 2, fill_width, 12, bat_color);
    }
    
    char chg_symbol[32];
    if (charging) {
        snprintf(chg_symbol, sizeof(chg_symbol), "%s +", bat_str);
    } else {
        snprintf(chg_symbol, sizeof(chg_symbol), "%s", bat_str);
    }
    
    int text_w = vita2d_pgf_text_width(font, 0.85f, chg_symbol);
    vita2d_pgf_draw_textf(font, x - text_w - 10, y + 14, bat_color, 0.85f, "%s", chg_symbol);
}

void draw_actions_menu() {
    draw_browser();
    
    vita2d_draw_rectangle(0, 0, 960, 544, RGBA8(0, 0, 0, 100));
    
    int menu_w = 320;
    int menu_h = 440;
    int menu_x = (960 - menu_w) / 2;
    int menu_y = (544 - menu_h) / 2;
    vita2d_draw_rectangle(menu_x, menu_y, menu_w, menu_h, RGBA8(22, 27, 34, 245));
    vita2d_draw_rectangle(menu_x, menu_y, menu_w, 2, RGBA8(31, 111, 235, 255));
    
    const char *title = language_get(&lang, STR_ACTIONS);
    vita2d_pgf_draw_textf(font, menu_x + 20, menu_y + 30, RGBA8(240, 246, 252, 255), 1.0f, "%s", title);
    vita2d_draw_rectangle(menu_x + 10, menu_y + 45, menu_w - 20, 1, RGBA8(48, 54, 61, 255));
    
    const char *options[13];
    options[0] = language_get(&lang, STR_COPY);
    options[1] = language_get(&lang, STR_CUT);
    options[2] = language_get(&lang, STR_PASTE);
    options[3] = language_get(&lang, STR_RENAME);
    options[4] = language_get(&lang, STR_NEW_FOLDER);
    options[5] = language_get(&lang, STR_SEARCH);
    options[6] = language_get(&lang, STR_SELECT_MULTIPLE);
    options[7] = language_get(&lang, STR_DELETE);

    LanguageCode curr_lang = language_get_current(&lang);
    if (curr_lang == LANG_IT) {
        options[8] = "Calcola MD5/SHA-256";
        options[9] = "Visualizzatore Hex";
        options[10] = "Proprieta & Permessi";
        options[11] = "Modifica File";
        options[12] = "Installa App (Promuovi)";
    } else if (curr_lang == LANG_ES) {
        options[8] = "Calcular MD5/SHA-256";
        options[9] = "Visor Hexadecimal";
        options[10] = "Propiedades & Permisos";
        options[11] = "Editar Archivo";
        options[12] = "Instalar App (Promover)";
    } else if (curr_lang == LANG_FR) {
        options[8] = "Calculer MD5/SHA-256";
        options[9] = "Lecteur Hexadecimal";
        options[10] = "Proprietes & Permissions";
        options[11] = "Modifier Fichier";
        options[12] = "Installer App (Promouvoir)";
    } else if (curr_lang == LANG_DE) {
        options[8] = "MD5/SHA-256 berechnen";
        options[9] = "Hex-Betrachter";
        options[10] = "Eigenschaften & Berechtig.";
        options[11] = "Datei bearbeiten";
        options[12] = "App installieren (Promote)";
    } else if (curr_lang == LANG_JPN) {
        options[8] = "MD5/SHA-256\xe3\x82\x92\xe8\xa8\x88\xe7\xa6\x97";
        options[9] = "16\xe9\x80\xb2\xe6\x95\xb0\xe3\x83\x93\xe3\x83\xa5\xe3\x83\xbc\xe3\x82\xa2";
        options[10] = "\xe3\x83\x97\xe3\x83\xad\xe3\x83\xb1\xe3\x83\x86\xe3\x82\xa3\xe3\x81\xa8\xe3\x82\xa2\xe3\x82\xaf\xe3\x82\xb5\xe3\x82\xb9\xe6\xa8\xa9";
        options[11] = "\xe3\x83\x95\xe3\x82\xa1\xe3\x82\xa4\xe3\x83\xab\xe3\x82\x92\xe7\xb7\xa8\xe9\x9b\x86";
        options[12] = "\xe3\x82\xa2\xe3\x83\x97\xe3\x83\xb3\xe3\x82\x92\xe3\x82\xa4\xe3\x83\xb3\xe3\x82\xb9\xe3\x83\x88\xe3\x83\xbc\xe3\x83\xab (\xe6\x98\x87\xe6\xa0\xbc)";
    } else {
        options[8] = "Calculate MD5/SHA-256";
        options[9] = "Hex Viewer";
        options[10] = "Properties & Perms";
        options[11] = "Edit File";
        options[12] = "Install App (Promote)";
    }
    int num_options = 13;
    
    int start_y = menu_y + 60;
    int row_h = 28;
    int is_dir = (browser.file_count > 0) ? browser.files[browser.selected_index].is_directory : 0;
    const char *sel_name = (browser.file_count > 0) ? browser.files[browser.selected_index].name : "";
    const char *sel_ext = strrchr(sel_name, '.');
    int can_edit = !is_dir && (is_viewable_text_file(sel_name) || (sel_ext && strcasecmp(sel_ext, ".sfo") == 0));
    int can_promote = 0;
    if (is_dir && browser.file_count > 0) {
        char sfo_path[1024];
        snprintf(sfo_path, sizeof(sfo_path), "%s%s/sce_sys/param.sfo", browser.current_path, sel_name);
        SceIoStat sfo_stat;
        if (sceIoGetstat(sfo_path, &sfo_stat) >= 0) {
            can_promote = 1;
        }
    }

    for (int i = 0; i < num_options; i++) {
        int ry = start_y + (i * row_h);
        int is_selected = (i == actions_menu_selected);
        int is_disabled = (i == 2 && clipboard_file_count == 0) ||
                          ((i == 8 || i == 9) && is_dir) ||
                          (i == 11 && !can_edit) ||
                          (i == 12 && !can_promote);
        
        if (is_selected) {
            vita2d_draw_rectangle(menu_x + 10, ry - 4, menu_w - 20, row_h, RGBA8(31, 111, 235, 40));
        }
        
        uint32_t text_color;
        if (is_disabled) {
            text_color = RGBA8(100, 110, 120, 255);
        } else if (is_selected) {
            text_color = RGBA8(31, 111, 235, 255);
        } else {
            text_color = RGBA8(201, 209, 217, 255);
        }
        
        vita2d_pgf_draw_textf(font, menu_x + 20, ry + 18, text_color, 0.85f, "%s", options[i]);
    }
    
    char x_str[64], o_str[64];
    snprintf(x_str, sizeof(x_str), "X: %s", language_get(&lang, STR_SELECT_ENTER));
    snprintf(o_str, sizeof(o_str), "O: %s", language_get(&lang, STR_BACK));
    const char *actions[] = {x_str, o_str};
    draw_footer(actions, 2);
}

void draw_archive_actions_menu() {
    draw_archive_view();
    
    vita2d_draw_rectangle(0, 0, 960, 544, RGBA8(0, 0, 0, 100));
    
    int menu_w = 320;
    int menu_h = 180;
    int menu_x = (960 - menu_w) / 2;
    int menu_y = (544 - menu_h) / 2;
    vita2d_draw_rectangle(menu_x, menu_y, menu_w, menu_h, RGBA8(22, 27, 34, 245));
    vita2d_draw_rectangle(menu_x, menu_y, menu_w, 2, RGBA8(31, 111, 235, 255));
    
    const char *title = language_get(&lang, STR_ACTIONS);
    vita2d_pgf_draw_textf(font, menu_x + 20, menu_y + 30, RGBA8(240, 246, 252, 255), 1.0f, "%s", title);
    vita2d_draw_rectangle(menu_x + 10, menu_y + 45, menu_w - 20, 1, RGBA8(48, 54, 61, 255));
    
    const char *options[3];
    options[0] = language_get(&lang, STR_EXTRACT_SELECTED);
    options[1] = language_get(&lang, STR_EXTRACT_ALL);
    options[2] = language_get(&lang, STR_TEST_INTEGRITY);
    int num_options = 3;
    
    int start_y = menu_y + 70;
    int row_h = 28;
    for (int i = 0; i < num_options; i++) {
        int ry = start_y + (i * row_h);
        int is_selected = (i == archive_actions_selected);
        
        if (is_selected) {
            vita2d_draw_rectangle(menu_x + 10, ry - 4, menu_w - 20, row_h, RGBA8(31, 111, 235, 40));
        }
        
        uint32_t text_color = is_selected ? RGBA8(31, 111, 235, 255) : RGBA8(201, 209, 217, 255);
        vita2d_pgf_draw_textf(font, menu_x + 20, ry + 18, text_color, 0.85f, "%s", options[i]);
    }
    
    char x_str[64], o_str[64];
    snprintf(x_str, sizeof(x_str), "X: %s", language_get(&lang, STR_SELECT_ENTER));
    snprintf(o_str, sizeof(o_str), "O: %s", language_get(&lang, STR_BACK));
    const char *actions[] = {x_str, o_str};
    draw_footer(actions, 2);
}

void draw_integrity_result() {
    vita2d_draw_rectangle(0, 0, 960, 50, RGBA8(21, 26, 33, 255));
    
    const char *title = language_get(&lang, STR_TEST_INTEGRITY);
    vita2d_pgf_draw_textf(font, 10, 30, RGBA8(240, 246, 252, 255), 1.0f, "%s - %s", 
        language_get(&lang, STR_APP_TITLE), title);
        
    int panel_y = 150;
    vita2d_draw_rectangle(100, panel_y, 760, 200, RGBA8(22, 27, 34, 255));
    vita2d_draw_rectangle(100, panel_y, 760, 1, RGBA8(31, 111, 235, 255));
    
    if (worker_result == 0) {
        const char *msg1 = language_get(&lang, STR_INTEGRITY_PASSED);
        const char *msg2 = language_get(&lang, STR_INTEGRITY_INTACT);
        vita2d_pgf_draw_textf(font, 120, panel_y + 60, RGBA8(46, 204, 113, 255), 1.1f, "%s", msg1);
        vita2d_pgf_draw_textf(font, 120, panel_y + 110, RGBA8(201, 209, 217, 255), 0.9f, "%s", msg2);
    } else {
        const char *msg1 = language_get(&lang, STR_INTEGRITY_FAILED);
        const char *msg2 = language_get(&lang, STR_INTEGRITY_CORRUPT);
        char err_msg[128];
        snprintf(err_msg, sizeof(err_msg), "%s (Error: %d)", msg2, worker_result);
        
        vita2d_pgf_draw_textf(font, 120, panel_y + 60, RGBA8(231, 76, 60, 255), 1.1f, "%s", msg1);
        vita2d_pgf_draw_textf(font, 120, panel_y + 110, RGBA8(201, 209, 217, 255), 0.9f, "%s", err_msg);
    }
    
    char o_str[64];
    snprintf(o_str, sizeof(o_str), "O: %s", language_get(&lang, STR_BACK));
    const char *actions[] = {o_str};
    draw_footer(actions, 1);
}

void draw_hash_view() {
    vita2d_draw_rectangle(0, 0, 960, 50, RGBA8(21, 26, 33, 255));
    
    const char *basename = path_basename(hash_filepath);
    vita2d_pgf_draw_textf(font, 10, 30, RGBA8(240, 246, 252, 255), 1.0f, "Verifica Integrita - %s", basename);
    
    int panel_y = 100;
    vita2d_draw_rectangle(50, panel_y, 860, 380, RGBA8(22, 27, 34, 255));
    vita2d_draw_rectangle(50, panel_y, 860, 2, RGBA8(31, 111, 235, 255));
    
    LanguageCode cl = language_get_current(&lang);
    
    if (hash_progress < 100) {
        const char *loading_text = "Calcolo degli hash in corso...";
        if (cl == LANG_ES) loading_text = "Calculando hash...";
        else if (cl == LANG_FR) loading_text = "Calcul des hash...";
        else if (cl == LANG_DE) loading_text = "Hash berechnen...";
        else if (cl == LANG_JPN) loading_text = "ハッシュ計算中...";
        else loading_text = "Calculating file hashes...";
        
        vita2d_pgf_draw_textf(font, 80, panel_y + 80, RGBA8(201, 209, 217, 255), 1.0f, "%s", loading_text);
        draw_progress_bar(80, panel_y + 140, 800, 30, hash_progress);
        
        char pct_str[32];
        sprintf(pct_str, "%d%%", hash_progress);
        int pct_w = vita2d_pgf_text_width(font, 0.9f, pct_str);
        vita2d_pgf_draw_textf(font, 480 - pct_w / 2, panel_y + 200, RGBA8(240, 246, 252, 255), 0.9f, "%s", pct_str);
        
        char o_str[64];
        snprintf(o_str, sizeof(o_str), "O: %s", language_get(&lang, STR_CANCEL));
        const char *actions[] = {o_str};
        draw_footer(actions, 1);
    } else {
        vita2d_pgf_draw_textf(font, 80, panel_y + 50, RGBA8(46, 204, 113, 255), 1.0f, 
            (cl == LANG_IT) ? "Calcolo completato!" :
            (cl == LANG_ES) ? "Calculo completado!" :
            "Calculation complete!");
            
        vita2d_pgf_draw_textf(font, 80, panel_y + 100, RGBA8(31, 111, 235, 255), 0.9f, "MD5");
        vita2d_draw_rectangle(80, panel_y + 120, 800, 40, RGBA8(13, 17, 23, 255));
        vita2d_draw_rectangle(80, panel_y + 120, 800, 40, RGBA8(48, 54, 61, 255));
        vita2d_pgf_draw_textf(font, 100, panel_y + 148, RGBA8(240, 246, 252, 255), 0.85f, "%s", hash_md5);
        
        vita2d_pgf_draw_textf(font, 80, panel_y + 200, RGBA8(31, 111, 235, 255), 0.9f, "SHA-256");
        vita2d_draw_rectangle(80, panel_y + 220, 800, 40, RGBA8(13, 17, 23, 255));
        vita2d_draw_rectangle(80, panel_y + 220, 800, 40, RGBA8(48, 54, 61, 255));
        vita2d_pgf_draw_textf(font, 100, panel_y + 248, RGBA8(240, 246, 252, 255), 0.8f, "%s", hash_sha256);
        
        vita2d_pgf_draw_textf(font, 80, panel_y + 310, RGBA8(139, 148, 158, 255), 0.8f, 
            (cl == LANG_IT) ? "X: Salva report hash in 'ux0:/data/VitaArchive/hash_report.txt'" :
            (cl == LANG_ES) ? "X: Guardar informe de hash en 'ux0:/data/VitaArchive/hash_report.txt'" :
            "X: Save hash report to 'ux0:/data/VitaArchive/hash_report.txt'");
            
        char o_str[64], x_str[64];
        snprintf(o_str, sizeof(o_str), "O: %s", language_get(&lang, STR_BACK));
        if (cl == LANG_IT) snprintf(x_str, sizeof(x_str), "X: Salva Report");
        else if (cl == LANG_ES) snprintf(x_str, sizeof(x_str), "X: Guardar Reporte");
        else snprintf(x_str, sizeof(x_str), "X: Save Report");
        
        const char *actions[] = {x_str, o_str};
        draw_footer(actions, 2);
    }
}

void draw_hex_view() {
    vita2d_draw_rectangle(0, 0, 960, 50, RGBA8(21, 26, 33, 255));
    
    const char *basename = path_basename(hex_filepath);
    vita2d_pgf_draw_textf(font, 10, 30, RGBA8(240, 246, 252, 255), 1.0f, "Hex Viewer - %s", basename);
    
    char offset_str[64];
    sprintf(offset_str, "Offset: 0x%08X / 0x%08X", hex_offset, (uint32_t)hex_file_size);
    int offset_w = vita2d_pgf_text_width(font, 0.8f, offset_str);
    vita2d_pgf_draw_textf(font, 800 - offset_w, 30, RGBA8(139, 148, 158, 255), 0.8f, "%s", offset_str);
    
    int box_y = 60;
    int box_h = 440;
    vita2d_draw_rectangle(10, box_y, 940, box_h, RGBA8(22, 27, 34, 255));
    
    int header_y = box_y + 20;
    vita2d_pgf_draw_textf(font, 20, header_y, RGBA8(31, 111, 235, 255), 0.85f, "OFFSET");
    vita2d_pgf_draw_textf(font, 140, header_y, RGBA8(31, 111, 235, 255), 0.85f, "HEX DATA");
    vita2d_pgf_draw_textf(font, 700, header_y, RGBA8(31, 111, 235, 255), 0.85f, "ASCII");
    
    vita2d_draw_rectangle(20, header_y + 5, 920, 1, RGBA8(48, 54, 61, 255));
    
    SceUID fd = sceIoOpen(hex_filepath, SCE_O_RDONLY, 0);
    uint8_t buffer[240];
    int bytes_read = 0;
    if (fd >= 0) {
        sceIoLseek(fd, hex_offset, SCE_SEEK_SET);
        bytes_read = sceIoRead(fd, buffer, sizeof(buffer));
        sceIoClose(fd);
    }
    
    int start_y = header_y + 25;
    int line_h = 24;
    
    for (int i = 0; i < 15; i++) {
        uint32_t line_offset = hex_offset + i * 16;
        int y = start_y + i * line_h;
        
        if (i * 16 >= bytes_read) {
            break;
        }
        
        vita2d_pgf_draw_textf(font, 20, y, RGBA8(139, 148, 158, 255), 0.85f, "%08X:", line_offset);
        
        char hex_buf[64] = {0};
        char *p_hex = hex_buf;
        char ascii_buf[17] = {0};
        
        for (int j = 0; j < 16; j++) {
            int idx = i * 16 + j;
            if (idx < bytes_read) {
                uint8_t b = buffer[idx];
                sprintf(p_hex, "%02X ", b);
                p_hex += 3;
                if (j == 7) {
                    sprintf(p_hex, " ");
                    p_hex += 1;
                }
                
                if (b >= 32 && b <= 126) {
                    ascii_buf[j] = (char)b;
                } else {
                    ascii_buf[j] = '.';
                }
            } else {
                sprintf(p_hex, "   ");
                p_hex += 3;
                if (j == 7) {
                    sprintf(p_hex, " ");
                    p_hex += 1;
                }
                ascii_buf[j] = ' ';
            }
        }
        
        vita2d_pgf_draw_textf(font, 140, y, RGBA8(201, 209, 217, 255), 0.85f, "%s", hex_buf);
        vita2d_pgf_draw_textf(font, 700, y, RGBA8(201, 209, 217, 255), 0.85f, "%s", ascii_buf);
    }
    
    char o_str[64], nav_str[128];
    snprintf(o_str, sizeof(o_str), "O: %s", language_get(&lang, STR_BACK));
    LanguageCode cl = language_get_current(&lang);
    if (cl == LANG_IT) {
        snprintf(nav_str, sizeof(nav_str), "Su/Giu: Scorre 1 riga  L/R: Scorre 1 pagina");
    } else if (cl == LANG_ES) {
        snprintf(nav_str, sizeof(nav_str), "Arriba/Abajo: Desplazar 1 fila  L/R: Desplazar 1 pag");
    } else {
        snprintf(nav_str, sizeof(nav_str), "Up/Down: Scroll 1 line  L/R: Scroll 1 page");
    }
    
    const char *actions[] = {o_str, nav_str};
    draw_footer(actions, 2);
}

void draw_properties_view() {
    vita2d_draw_rectangle(0, 0, 960, 50, RGBA8(21, 26, 33, 255));
    
    LanguageCode cl = language_get_current(&lang);
    const char *basename = path_basename(prop_filepath);
    
    const char *title = "Properties";
    const char *lbl_name = "Name:";
    const char *lbl_path = "Path:";
    const char *lbl_type = "Type:";
    const char *lbl_size = "Size:";
    const char *val_folder = "Folder";
    const char *val_file = "File";
    
    if (cl == LANG_IT) {
        title = "Proprieta";
        lbl_name = "Nome:";
        lbl_path = "Percorso:";
        lbl_type = "Tipo:";
        lbl_size = "Dimensione:";
        val_folder = "Cartella";
        val_file = "File";
    } else if (cl == LANG_ES) {
        title = "Propiedades";
        lbl_name = "Nombre:";
        lbl_path = "Ruta:";
        lbl_type = "Tipo:";
        lbl_size = "Tama\xc3\xb1o:";
        val_folder = "Carpeta";
        val_file = "Archivo";
    } else if (cl == LANG_FR) {
        title = "Proprietes";
        lbl_name = "Nom:";
        lbl_path = "Chemin:";
        lbl_type = "Type:";
        lbl_size = "Taille:";
        val_folder = "Dossier";
        val_file = "Fichier";
    } else if (cl == LANG_DE) {
        title = "Eigenschaften";
        lbl_name = "Name:";
        lbl_path = "Pfad:";
        lbl_type = "Typ:";
        lbl_size = "Groesse:";
        val_folder = "Ordner";
        val_file = "Datei";
    } else if (cl == LANG_JPN) {
        title = "\xe3\x83\x97\xe3\x83\xad\xe3\x83\xb1\xe3\x83\x86\xe3\x82\xa3";
        lbl_name = "\xe5\x90\x8d\xe5\x89\x8d:";
        lbl_path = "\xe3\x83\x91\xe3\x82\xb9:";
        lbl_type = "\xe7\xa8\xae\xe9\xa1\x9e:";
        lbl_size = "\xe3\x82\xb5\xe3\x82\xa4\xe3\x82\xba:";
        val_folder = "\xe3\x83\x95\xe3\x82\xa9\xe3\x83\xab\xe3\x83\x80";
        val_file = "\xe3\x83\x95\xe3\x82\xa1\xe3\x82\xa4\xe3\x83\xab";
    }
    
    vita2d_pgf_draw_textf(font, 10, 30, RGBA8(240, 246, 252, 255), 1.0f, "%s - %s", title, basename);
    
    int panel_y = 65;
    vita2d_draw_rectangle(20, panel_y, 920, 435, RGBA8(22, 27, 34, 255));
    vita2d_draw_rectangle(20, panel_y, 920, 2, RGBA8(31, 111, 235, 255));
    
    int info_y = panel_y + 20;
    vita2d_pgf_draw_textf(font, 40, info_y + 15, RGBA8(31, 111, 235, 255), 0.85f, "%s", lbl_name);
    vita2d_pgf_draw_textf(font, 140, info_y + 15, RGBA8(240, 246, 252, 255), 0.85f, "%s", basename);
    
    vita2d_pgf_draw_textf(font, 40, info_y + 40, RGBA8(31, 111, 235, 255), 0.85f, "%s", lbl_path);
    vita2d_pgf_draw_textf(font, 140, info_y + 40, RGBA8(201, 209, 217, 255), 0.8f, "%s", prop_filepath);
    
    vita2d_pgf_draw_textf(font, 40, info_y + 65, RGBA8(31, 111, 235, 255), 0.85f, "%s", lbl_type);
    int is_dir = SCE_S_ISDIR(prop_stat.st_mode);
    vita2d_pgf_draw_textf(font, 140, info_y + 65, RGBA8(201, 209, 217, 255), 0.85f, "%s", 
        is_dir ? val_folder : val_file);
        
    vita2d_pgf_draw_textf(font, 40, info_y + 90, RGBA8(31, 111, 235, 255), 0.85f, "%s", lbl_size);
    char sz_str[64];
    format_size(sz_str, prop_stat.st_size);
    vita2d_pgf_draw_textf(font, 140, info_y + 90, RGBA8(201, 209, 217, 255), 0.85f, "%s (%llu B)", sz_str, (unsigned long long)prop_stat.st_size);
    
    vita2d_draw_rectangle(40, info_y + 105, 880, 1, RGBA8(48, 54, 61, 255));
    
    int check_y = info_y + 125;
    
    vita2d_pgf_draw_textf(font, 40, check_y + 20, RGBA8(31, 111, 235, 255), 0.9f, 
        (cl == LANG_IT) ? "Attributi & Permessi:" :
        (cl == LANG_ES) ? "Atributos & Permisos:" :
        "Attributes & Permissions:");
        
    const char *labels[6];
    if (cl == LANG_IT) {
        labels[0] = "Sola Lettura";
        labels[1] = "Nascosto";
        labels[2] = "Sistema";
        labels[3] = "Permesso di Lettura Proprietario";
        labels[4] = "Permesso di Scrittura Proprietario";
        labels[5] = "Permesso di Esecuzione Proprietario";
    } else if (cl == LANG_ES) {
        labels[0] = "Solo Lectura";
        labels[1] = "Oculto";
        labels[2] = "Sistema";
        labels[3] = "Permiso de Lectura";
        labels[4] = "Permiso de Escritura";
        labels[5] = "Permiso de Ejecucion";
    } else if (cl == LANG_FR) {
        labels[0] = "Lecture Seule";
        labels[1] = "Cache";
        labels[2] = "Systeme";
        labels[3] = "Permission de Lecture Proprietaire";
        labels[4] = "Permission d'Ecriture Proprietaire";
        labels[5] = "Permission d'Execution Proprietaire";
    } else if (cl == LANG_DE) {
        labels[0] = "Schreibgeschuetzt";
        labels[1] = "Versteckt";
        labels[2] = "System";
        labels[3] = "Unix-Benutzer Leseberechtigung";
        labels[4] = "Unix-Benutzer Schreibberechtigung";
        labels[5] = "Unix-Benutzer Ausfuehrungsberechtigung";
    } else if (cl == LANG_JPN) {
        labels[0] = "\xe8\xaa\xad\xe3\x81\xbf\xe5\x8f\x96\xe3\x82\x8a\xe5\xb0\x82\xe7\x94\xa8";
        labels[1] = "\xe9\x9d\x9e\xe8\xa1\xa8\xe7\xa4\xba";
        labels[2] = "\xe3\x82\xb7\xe3\x82\xb9\xe3\x83\x86\xe3\x83\xa0";
        labels[3] = "\xe6\x89\x80\xe6\x9c\x89\xe8\x80\x85\xe3\x81\xae\xe8\xaa\xad\xe3\x81\xbf\xe5\x8f\x96\xe3\x82\x8a\xe6\xa8\xa9\xe9\x99\x90";
        labels[4] = "\xe6\x89\x80\xe6\x9c\x89\xe8\x80\x85\xe3\x81\xae\xe6\x9b\xb8\xe3\x81\x8d\xe8\xbe\xbc\xe3\x81\xbf\xe6\xa8\xa9\xe9\x99\x90";
        labels[5] = "\xe6\x89\x80\xe6\x9c\x89\xe8\x80\x85\xe3\x81\xae\xe5\xae\x9f\xe8\xa1\x8c\xe6\xa8\xa9\xe9\x99\x90";
    } else {
        labels[0] = "Read-Only";
        labels[1] = "Hidden";
        labels[2] = "System";
        labels[3] = "Unix User Read Permission";
        labels[4] = "Unix User Write Permission";
        labels[5] = "Unix User Execute Permission";
    }
    
    int row_h = 32;
    for (int i = 0; i < 6; i++) {
        int ry = check_y + 45 + (i * row_h);
        int is_selected = (i == prop_selected_row);
        
        if (is_selected) {
            vita2d_draw_rectangle(40, ry - 6, 880, row_h, RGBA8(31, 111, 235, 40));
        }
        
        int box_x = 50;
        int box_w = 16;
        int box_h = 16;
        
        vita2d_draw_rectangle(box_x, ry - 3, box_w, box_h, RGBA8(110, 118, 129, 255));
        vita2d_draw_rectangle(box_x + 1, ry - 2, box_w - 2, box_h - 2, RGBA8(13, 17, 23, 255));
        
        if (prop_checkboxes[i]) {
            vita2d_draw_rectangle(box_x + 3, ry, box_w - 6, box_h - 6, RGBA8(46, 204, 113, 255));
        }
        
        uint32_t text_color = is_selected ? RGBA8(31, 111, 235, 255) : RGBA8(201, 209, 217, 255);
        vita2d_pgf_draw_textf(font, box_x + 30, ry + 10, text_color, 0.85f, "%s", labels[i]);
    }
    
    char o_str[64], x_str[64];
    snprintf(o_str, sizeof(o_str), "O: %s", language_get(&lang, STR_BACK));
    if (cl == LANG_IT) snprintf(x_str, sizeof(x_str), "X: Modifica");
    else if (cl == LANG_ES) snprintf(x_str, sizeof(x_str), "X: Cambiar");
    else snprintf(x_str, sizeof(x_str), "X: Toggle");
    
    const char *actions[] = {x_str, o_str};
    draw_footer(actions, 2);
}
