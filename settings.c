/*
 * VitaArchive - File Archiver & Browser for PS Vita
 * Created by theheroGAC.
 * Special thanks to TheFloW, Rinnegatamante, SKGleba, and all developers, hackers,
 * and contributors of the PlayStation Vita homebrew scene.
 */

#include <psp2/io/fcntl.h>
#include <psp2/io/stat.h>
#include "globals.h"
#include "settings.h"

void save_settings() {
    sceIoMkdir("ux0:/data/VitaArchive", 0777);
    SceUID fd = sceIoOpen("ux0:/data/VitaArchive/settings.cfg", SCE_O_WRONLY | SCE_O_CREAT | SCE_O_TRUNC, 0777);
    if (fd >= 0) {
        int lang_code = language_get_current(&lang);
        sceIoWrite(fd, &lang_code, sizeof(lang_code));
        sceIoWrite(fd, &compress_level, sizeof(compress_level));
        sceIoClose(fd);
    }
}

void load_settings() {
    SceUID fd = sceIoOpen("ux0:/data/VitaArchive/settings.cfg", SCE_O_RDONLY, 0);
    if (fd >= 0) {
        int lang_code = 0;
        sceIoRead(fd, &lang_code, sizeof(lang_code));
        if (sceIoRead(fd, &compress_level, sizeof(compress_level)) != (int)sizeof(compress_level))
            compress_level = 1; 
        if (compress_level < 0 || compress_level > 2) compress_level = 1;
        sceIoClose(fd);
        language_set(&lang, (LanguageCode)lang_code);
    }
}
