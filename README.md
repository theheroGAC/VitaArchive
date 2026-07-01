# VitaArchive 

A complete, powerful, and lightweight archive manager for PlayStation Vita, created by **theheroGAC** and compatible with the unofficial VitaSDK toolchain.

Born as a focused alternative for browsing and extracting archives, VitaArchive merges the familiar usability of the homebrew scene with advanced file inspection tools.

---

## Main Features

### Archive Browser & Extractor
* **Multi-format support**: Browse and extract files from `.zip`, `.7z`, `.rar`, `.tar`, `.tar.gz`, and `.tar.bz2` archives.
* **PSARC**: Support for `.psarc` archives.
* **Selective Extraction**: Browse archive contents and extract only the files you need without unpackaging the whole archive.
* **Password Support**: Compatible with archives protected by encryption.
* **ZIP/7z Creation**: Compress files and folders on the fly directly from the console.
* **VPK Installation**: Automatic detection and direct installation of `.vpk` packages.
* **Smart Install**: Detects the presence of `eboot.bin` files inside common ZIP archives to install them on the fly as homebrew applications.

### Advanced Tools (New!)
* **Asynchronous Hash Calculation**: Calculate MD5 and SHA-256 codes in the background without freezing the GUI. Saves results to `ux0:/data/VitaArchive/hash_report.txt`.
* **Hex Viewer**: Inspect files byte-by-byte with an aligned ASCII column. Reads in 240-byte pages to guarantee instant speed even on multi-Gigabyte files.
* **Properties & Attribute Modification (Chmod)**: Modify FAT32 filesystem flags (*Read-Only*, *Hidden*, *System*) and owner Unix permissions (*Read*, *Write*, *Execute*) through a friendly interface with green checkboxes.
* **Standby Prevention**: Prevents the PS Vita OLED screen from dimming or putting the console to sleep while the app is active.
* **Total/Free Space Display**: Shows the free and total space of **all** mounted partitions (including `ux0:`, `ur0:`, `uma0:`) directly on the main screen, just like VitaShell.

---

## Controls & Navigation

* **D-Pad Up/Down**: File list navigation
* **X (Cross)**: Select / Enter / Confirm / Toggle Checkbox (in Properties)
* **O (Circle)**: Back / Exit menu / Cancel operation or save
* **Square**: Delete file/folder (in Browser) / Select single file (in compression)
* **Triangle**: Open **Actions Menu** (Copy, Cut, Paste, Rename, New Folder, Search, Calculate Hash, Hex Viewer, Properties)
* **Select**: Start the integrated FTP server
* **Start**: Open the Settings menu (select language and compression level)

---

## How to Build

The project relies on the open-source **VitaSDK** toolchain.

1. Make sure your environment variables are configured (e.g. `VITASDK` in your PATH).
2. Clone the code and start compiling:
```bash
mkdir build
cd build
cmake ..
make
```


## Credits & Special Thanks

A sincere thank you to those who made PlayStation Vita homebrew development possible:

* **TheFloW** (Andy Nguyen) for his masterpiece **VitaShell**, the Henkaku jailbreak, and his massive contribution to the entire infrastructure.
* **Rinnegatamante** for his outstanding porting work, for the `vita2d` graphics library used in this interface, and for the VitaDB portal.
* **SKGleba** for system drivers, kernel-level works, custom bootloaders, and continuous technical support to the scene.
* **All developers, hackers, and contributors of the PlayStation Vita homebrew scene** who, with passion and dedication, keep this wonderful handheld console alive and kicking.
* The **libarchive** and **miniz** communities for their solid open-source compression libraries.
