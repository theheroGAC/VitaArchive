/*
 * VitaArchive - File Archiver & Browser for PS Vita
 * Created by theheroGAC.
 * Special thanks to TheFloW, Rinnegatamante, SKGleba, and all developers, hackers,
 * and contributors of the PlayStation Vita homebrew scene.
 */

#ifndef FTP_SERVER_H
#define FTP_SERVER_H

int ftp_server_start(char *ip, unsigned short *port);
void ftp_server_stop(void);
const char *ftp_server_get_status(void);

#endif
