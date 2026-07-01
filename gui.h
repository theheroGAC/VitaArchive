/*
 * VitaArchive - File Archiver & Browser for PS Vita
 * Created by theheroGAC.
 * Special thanks to TheFloW, Rinnegatamante, SKGleba, and all developers, hackers,
 * and contributors of the PlayStation Vita homebrew scene.
 */

#ifndef GUI_H
#define GUI_H

#include <stdint.h>

void show_toast(const char *msg, uint32_t color);
void draw_toast();
void format_size(char *buf, uint64_t size);
void get_file_visuals(const char *path, int is_directory, const char **icon, uint32_t *icon_color);
const char *get_file_type_label(const char *path, int is_directory);
int is_archive_file(const char *filename);
const char *path_basename(const char *path);
int is_viewable_text_file(const char *filename);

void draw_progress_bar(int x, int y, int width, int height, int progress);
void draw_button_icon(int x, int y, const char *btn_name, float scale);
void draw_footer(const char **actions, int count);
void draw_browser();
void draw_browser_footer();
void draw_dest_browser();
void draw_archive_view();
void draw_worker_stats(int panel_y);
void draw_extracting();
void draw_installing(int success);
void draw_smart_install_confirm();
void draw_compress_format_select();
void draw_delete_confirm();
void draw_info();
void draw_text_preview();
void draw_ftp_server();
void draw_settings();
void draw_battery_status();
void draw_actions_menu();
void draw_archive_actions_menu();
void draw_integrity_result();
void draw_hash_view();
void draw_hex_view();
void draw_properties_view();

#endif
