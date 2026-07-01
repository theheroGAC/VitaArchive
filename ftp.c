/*
 * VitaArchive - File Archiver & Browser for PS Vita
 * Created by theheroGAC.
 * Special thanks to TheFloW, Rinnegatamante, SKGleba, and all developers, hackers,
 * and contributors of the PlayStation Vita homebrew scene.
 */
#include <psp2/sysmodule.h>
#include <psp2/net/net.h>
#include <psp2/net/netctl.h>
#include <stdlib.h>
#include <string.h>
#include <ftpvita.h>
#include "ftp.h"

static int g_net_init = 0;
static int g_ftp_active = 0;
static char g_ftp_status[256] = "Waiting for connection...";

static void ftp_info_log_cb(const char *msg) {
    strncpy(g_ftp_status, msg, sizeof(g_ftp_status) - 1);
    g_ftp_status[sizeof(g_ftp_status) - 1] = '\0';
}

static int net_init(void) {
    if (g_net_init) return 1;

    sceSysmoduleLoadModule(SCE_SYSMODULE_NET);

    SceNetInitParam param;
    memset(&param, 0, sizeof(param));
    param.memory = malloc(128 * 1024);
    param.size = 128 * 1024;
    param.flags = 0;

    int ret = sceNetInit(&param);
    if (ret < 0) { 
        free(param.memory); 
        return 0; 
    }

    ret = sceNetCtlInit();
    if (ret < 0) { 
        sceNetTerm(); 
        free(param.memory); 
        return 0; 
    }

    g_net_init = 1;
    return 1;
}

static void net_term(void) {
    if (g_net_init) {
        sceNetCtlTerm();
        sceNetTerm();
        g_net_init = 0;
    }
}

int ftp_server_start(char *ip, unsigned short *port) {
    if (g_ftp_active) return 1;

    if (!net_init()) return 0;

    ftpvita_set_info_log_cb(ftp_info_log_cb);

    *port = 1337;
    if (ftpvita_init(ip, port) < 0) {
        return 0;
    }

    ftpvita_add_device("ux0:");
    ftpvita_add_device("ur0:");
    ftpvita_add_device("uma0:");
    
    g_ftp_active = 1;
    strcpy(g_ftp_status, "Waiting for PC connection...");
    return 1;
}

void ftp_server_stop(void) {
    if (g_ftp_active) {
        ftpvita_fini();
        net_term();
        g_ftp_active = 0;
        strcpy(g_ftp_status, "Server stopped.");
    }
}

const char *ftp_server_get_status(void) {
    return g_ftp_status;
}
