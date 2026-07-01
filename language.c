/*
 * VitaArchive - File Archiver & Browser for PS Vita
 * Created by theheroGAC.
 * Special thanks to TheFloW, Rinnegatamante, SKGleba, and all developers, hackers,
 * and contributors of the PlayStation Vita homebrew scene.
 */
#include <stdio.h>
#include <string.h>
#include <psp2/io/fcntl.h>
#include "language.h"

static const char *lang_filenames[] = {
    "lang/it.txt",
    "lang/en.txt",
    "lang/es.txt",
    "lang/fr.txt",
    "lang/de.txt",
    "lang/jpn.txt"
};

static const char *lang_codes[] = {
    "it",
    "en",
    "es",
    "fr",
    "de",
    "jpn"
};

static const char *lang_names[] = {
    "Italiano",
    "English",
    "Espanol",
    "Francais",
    "Deutsch",
    "Nihongo"
};

static void language_set_english_defaults(Language *lang) {
    strcpy(lang->strings[STR_APP_TITLE], "VitaArchive");
    strcpy(lang->strings[STR_FILE_BROWSER], "File Browser");
    strcpy(lang->strings[STR_PATH], "Path");
    strcpy(lang->strings[STR_ARCHIVE_VIEW], "Archive Contents");
    strcpy(lang->strings[STR_ARCHIVE_CONTENTS], "Archive Contents");
    strcpy(lang->strings[STR_FILES], "Files");
    strcpy(lang->strings[STR_SIZE], "Size");
    strcpy(lang->strings[STR_EXTRACTING], "Extracting");
    strcpy(lang->strings[STR_PROGRESS], "Progress");
    strcpy(lang->strings[STR_ARCHIVE_INFO], "Archive Info");
    strcpy(lang->strings[STR_TOTAL_FILES], "Total Files");
    strcpy(lang->strings[STR_UNCOMPRESSED_SIZE], "Uncompressed Size");
    strcpy(lang->strings[STR_COMPRESSED_SIZE], "Compressed Size");
    strcpy(lang->strings[STR_COMPRESSION_RATIO], "Compression Ratio");
    strcpy(lang->strings[STR_DIR], "[DIR]");
    strcpy(lang->strings[STR_SELECT_ENTER], "Select/Enter");
    strcpy(lang->strings[STR_BACK], "Back");
    strcpy(lang->strings[STR_INFO], "Info");
    strcpy(lang->strings[STR_EXIT], "Exit");
    strcpy(lang->strings[STR_EXTRACT_ALL], "Extract All");
    strcpy(lang->strings[STR_CANCEL], "Cancel");
    strcpy(lang->strings[STR_CANCEL_EXTRACTION], "Cancel Extraction");
    strcpy(lang->strings[STR_PRESS_X_EXTRACT], "Press X to extract");
    strcpy(lang->strings[STR_RAR_SUPPORT_REQUIRED], "RAR support requires libunrar");
    strcpy(lang->strings[STR_7Z_SUPPORT_REQUIRED], "7z support requires LZMA SDK");
    strcpy(lang->strings[STR_VPK_INSTALLING], "Installing VPK");
    strcpy(lang->strings[STR_EXTRACT_COMPLETE], "Extract Complete");
    strcpy(lang->strings[STR_EXTRACT_CANCELLED], "Extract Cancelled");
    strcpy(lang->strings[STR_ERROR_OPENING_ARCHIVE], "Error opening archive");
    strcpy(lang->strings[STR_ERROR_EXTRACTING], "Error extracting");
    strcpy(lang->strings[STR_INSTALL_VPK], "Installing VPK");
    strcpy(lang->strings[STR_VPK_INSTALL_COMPLETE], "VPK installed successfully");
    strcpy(lang->strings[STR_VPK_INSTALL_ERROR], "VPK install failed");
    strcpy(lang->strings[STR_PRESS_X_INSTALL_VPK], "Install VPK");
    strcpy(lang->strings[STR_SETTINGS], "Settings");
    strcpy(lang->strings[STR_LANGUAGE], "Language");
    strcpy(lang->strings[STR_SELECT_DEST_FOLDER], "Select Destination Folder");
    strcpy(lang->strings[STR_CONFIRM_DEST], "Confirm Destination");
    strcpy(lang->strings[STR_CREATE_ZIP], "Create ZIP");
    strcpy(lang->strings[STR_TOGGLE_SELECT], "Select/Deselect");
    strcpy(lang->strings[STR_EXTRACT_FILE], "Extract File");
    strcpy(lang->strings[STR_DELETE_ZIP], "Delete ZIP");
    strcpy(lang->strings[STR_DELETE], "Delete");
    strcpy(lang->strings[STR_DELETE_SELECTED_ZIP], "Delete the selected ZIP file?");
    strcpy(lang->strings[STR_ENTER_ZIP_PASSWORD], "Enter ZIP password");
    strcpy(lang->strings[STR_NAME], "Name");
    strcpy(lang->strings[STR_TYPE], "Type");
    strcpy(lang->strings[STR_FOLDER], "Folder");
    strcpy(lang->strings[STR_FILE], "File");
    strcpy(lang->strings[STR_COMPRESSED], "Compressed");
    strcpy(lang->strings[STR_EXTRACTING_FILES], "Extracting files...");
    strcpy(lang->strings[STR_COMPLETE], "Complete!");
    strcpy(lang->strings[STR_SMART_INSTALL], "Smart Install");
    strcpy(lang->strings[STR_SMART_INSTALL_DETECTED], "`eboot.bin` detected in this archive.");
    strcpy(lang->strings[STR_SMART_INSTALL_PROMPT], "Install this archive as a PSVita application?");
    strcpy(lang->strings[STR_INSTALL_APP], "Installing Application");
    strcpy(lang->strings[STR_APP_INSTALL_COMPLETE], "Application installed successfully");
    strcpy(lang->strings[STR_APP_INSTALL_ERROR], "Application install failed");
    strcpy(lang->strings[STR_ACTIONS], "Actions");
    strcpy(lang->strings[STR_COPY], "Copy");
    strcpy(lang->strings[STR_CUT], "Cut");
    strcpy(lang->strings[STR_PASTE], "Paste");
    strcpy(lang->strings[STR_RENAME], "Rename");
    strcpy(lang->strings[STR_NEW_FOLDER], "New Folder");
    strcpy(lang->strings[STR_SEARCH], "Search");
    strcpy(lang->strings[STR_SELECT_MULTIPLE], "Create Archive (ZIP/7z/...)");
    strcpy(lang->strings[STR_TEST_INTEGRITY], "Test Integrity");
    strcpy(lang->strings[STR_EXTRACT_SELECTED], "Extract Selected");
    strcpy(lang->strings[STR_TOAST_COPIED], "Copied to clipboard!");
    strcpy(lang->strings[STR_TOAST_CUT], "Cut to clipboard!");
    strcpy(lang->strings[STR_TOAST_PASTE_SUCCESS], "Operation completed!");
    strcpy(lang->strings[STR_TOAST_PASTE_FAIL], "Operation failed");
    strcpy(lang->strings[STR_TOAST_RENAME_SUCCESS], "Renamed successfully!");
    strcpy(lang->strings[STR_TOAST_RENAME_FAIL], "Rename failed");
    strcpy(lang->strings[STR_TOAST_FOLDER_SUCCESS], "Folder created!");
    strcpy(lang->strings[STR_TOAST_FOLDER_FAIL], "Folder creation failed");
    strcpy(lang->strings[STR_INTEGRITY_PASSED], "Integrity test passed!");
    strcpy(lang->strings[STR_INTEGRITY_FAILED], "Integrity test failed!");
    strcpy(lang->strings[STR_INTEGRITY_INTACT], "The archive is intact and contains no errors.");
    strcpy(lang->strings[STR_INTEGRITY_CORRUPT], "The archive is corrupted or incomplete.");
    strcpy(lang->strings[STR_ENTER_PASSWORD], "Enter Archive Password");
    strcpy(lang->strings[STR_CANNOT_OPEN], "Cannot open archive");
}

int language_init(Language *lang) {
    memset(lang, 0, sizeof(Language));
    language_set_english_defaults(lang);
    lang->current_lang = LANG_EN;
    return 0;
}

int language_load(Language *lang, LanguageCode code) {
    if (code >= LANG_COUNT) return -1;
    memset(lang->strings, 0, sizeof(lang->strings));
    language_set_english_defaults(lang);
    if (code == LANG_EN) {
        lang->current_lang = code;
        return 0;
    }
    
    char path[64];
    snprintf(path, sizeof(path), "app0:%s", lang_filenames[code]);
    
    SceUID fd = sceIoOpen(path, SCE_O_RDONLY, 0);
    if (fd < 0) {
        return -1;
    }
    
    char buffer[4096];
    int bytes = sceIoRead(fd, buffer, sizeof(buffer) - 1);
    sceIoClose(fd);
    
    if (bytes <= 0) return -1;
    buffer[bytes] = '\0';
    
    char *line = strtok(buffer, "\n");
    int line_num = 0;
    
    while (line && line_num < STR_COUNT) {
        char *cr = strchr(line, '\r');
        if (cr) *cr = '\0';
        
        
        if (strlen(line) > 0 && line[0] != '#') {
            strncpy(lang->strings[line_num], line, MAX_LANG_LENGTH - 1);
            lang->strings[line_num][MAX_LANG_LENGTH - 1] = '\0';
            line_num++;
        }
        line = strtok(NULL, "\n");
    }
    
    lang->current_lang = code;
    return 0;
}

void language_set(Language *lang, LanguageCode code) {
    if (code < LANG_COUNT) {
        language_load(lang, code);
    }
}

const char *language_get(Language *lang, StringId id) {
    if (id >= STR_COUNT) return "";
    return lang->strings[id];
}

LanguageCode language_get_current(Language *lang) {
    return lang->current_lang;
}

const char *language_get_code(LanguageCode code) {
    if (code >= LANG_COUNT) return "en";
    return lang_codes[code];
}

const char *language_get_name(LanguageCode code) {
    if (code >= LANG_COUNT) return "Unknown";
    return lang_names[code];
}
