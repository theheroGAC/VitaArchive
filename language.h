#ifndef LANGUAGE_H
#define LANGUAGE_H

#define MAX_LANG_STRINGS 64
#define MAX_LANG_LENGTH 256
#define MAX_LANG_CODE 8

typedef enum {
    LANG_IT,
    LANG_EN,
    LANG_ES,
    LANG_FR,
    LANG_DE,
    LANG_JPN,
    LANG_COUNT
} LanguageCode;

typedef enum {
    STR_APP_TITLE,
    STR_FILE_BROWSER,
    STR_PATH,
    STR_ARCHIVE_VIEW,
    STR_ARCHIVE_CONTENTS,
    STR_FILES,
    STR_SIZE,
    STR_EXTRACTING,
    STR_PROGRESS,
    STR_ARCHIVE_INFO,
    STR_TOTAL_FILES,
    STR_UNCOMPRESSED_SIZE,
    STR_COMPRESSED_SIZE,
    STR_COMPRESSION_RATIO,
    STR_DIR,
    STR_SELECT_ENTER,
    STR_BACK,
    STR_INFO,
    STR_EXIT,
    STR_EXTRACT_ALL,
    STR_CANCEL,
    STR_CANCEL_EXTRACTION,
    STR_PRESS_X_EXTRACT,
    STR_RAR_SUPPORT_REQUIRED,
    STR_7Z_SUPPORT_REQUIRED,
    STR_VPK_INSTALLING,
    STR_EXTRACT_COMPLETE,
    STR_EXTRACT_CANCELLED,
    STR_ERROR_OPENING_ARCHIVE,
    STR_ERROR_EXTRACTING,
    STR_INSTALL_VPK,
    STR_VPK_INSTALL_COMPLETE,
    STR_VPK_INSTALL_ERROR,
    STR_PRESS_X_INSTALL_VPK,
    STR_SETTINGS,
    STR_LANGUAGE,
    STR_SELECT_DEST_FOLDER,
    STR_CONFIRM_DEST,
    STR_CREATE_ZIP,
    STR_TOGGLE_SELECT,
    STR_EXTRACT_FILE,
    STR_DELETE_ZIP,
    STR_DELETE,
    STR_DELETE_SELECTED_ZIP,
    STR_ENTER_ZIP_PASSWORD,
    STR_NAME,
    STR_TYPE,
    STR_FOLDER,
    STR_FILE,
    STR_COMPRESSED,
    STR_EXTRACTING_FILES,
    STR_COMPLETE,
    STR_SMART_INSTALL,
    STR_SMART_INSTALL_DETECTED,
    STR_SMART_INSTALL_PROMPT,
    STR_INSTALL_APP,
    STR_APP_INSTALL_COMPLETE,
    STR_APP_INSTALL_ERROR,
    STR_COUNT
} StringId;

typedef struct {
    char strings[STR_COUNT][MAX_LANG_LENGTH];
    LanguageCode current_lang;
} Language;

int language_init(Language *lang);
int language_load(Language *lang, LanguageCode code);
void language_set(Language *lang, LanguageCode code);
const char *language_get(Language *lang, StringId id);
LanguageCode language_get_current(Language *lang);
const char *language_get_code(LanguageCode code);
const char *language_get_name(LanguageCode code);

#endif
