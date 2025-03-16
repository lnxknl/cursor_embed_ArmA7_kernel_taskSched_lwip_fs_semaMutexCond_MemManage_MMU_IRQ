#ifndef FATFS_H
#define FATFS_H

#include <stdint.h>
#include <stdbool.h>

/* File system type definitions */
typedef uint8_t     BYTE;
typedef uint16_t    WORD;
typedef uint32_t    DWORD;
typedef uint64_t    QWORD;
typedef WORD        WCHAR;

/* File system object structure */
typedef struct {
    BYTE    fs_type;    /* FAT sub-type */
    BYTE    drv;        /* Physical drive number */
    BYTE    csize;      /* Sectors per cluster */
    BYTE    n_fats;     /* Number of FAT copies */
    WORD    n_rootdir;  /* Number of root directory entries */
    DWORD   n_fatent;   /* Number of FAT entries */
    DWORD   fatbase;    /* FAT start sector */
    DWORD   dirbase;    /* Root directory start sector */
    DWORD   database;   /* Data start sector */
    DWORD   winsect;    /* Current sector appearing in the win[] */
    BYTE    win[512];   /* Disk access window for Directory/FAT */
} FATFS;

/* File object structure */
typedef struct {
    FATFS*  fs;         /* Pointer to the related file system object */
    WORD    id;         /* Owner file system mount ID */
    BYTE    flag;       /* Status flags */
    BYTE    err;        /* Abort flag */
    DWORD   fptr;       /* File read/write pointer */
    DWORD   fsize;      /* File size */
    DWORD   sclust;     /* File start cluster */
    DWORD   clust;      /* Current cluster */
    DWORD   dsect;      /* Current data sector */
} FIL;

/* Directory object structure */
typedef struct {
    FATFS*  fs;         /* Pointer to the owner file system object */
    WORD    id;         /* Owner file system mount ID */
    WORD    index;      /* Current read/write index number */
    DWORD   sclust;     /* Table start cluster */
    DWORD   clust;      /* Current cluster */
    DWORD   sect;       /* Current sector */
    BYTE*   dir;        /* Pointer to the current SFN entry in the win[] */
    BYTE*   fn;         /* Pointer to the SFN buffer */
} DIR;

/* File information structure */
typedef struct {
    DWORD   fsize;      /* File size */
    WORD    fdate;      /* Last modified date */
    WORD    ftime;      /* Last modified time */
    BYTE    fattrib;    /* Attribute */
    WCHAR   fname[13];  /* Short file name */
    WCHAR   lfname[256];/* Long file name */
} FILINFO;

/* File access control and file status flags (FIL.flag) */
#define FA_READ         0x01
#define FA_WRITE        0x02
#define FA_OPEN_EXISTING    0x00
#define FA_CREATE_NEW      0x04
#define FA_CREATE_ALWAYS   0x08
#define FA_OPEN_ALWAYS     0x10

/* File attribute bits for directory entry */
#define AM_RDO  0x01    /* Read only */
#define AM_HID  0x02    /* Hidden */
#define AM_SYS  0x04    /* System */
#define AM_VOL  0x08    /* Volume label */
#define AM_LFN  0x0F    /* LFN entry */
#define AM_DIR  0x10    /* Directory */
#define AM_ARC  0x20    /* Archive */

/* Error codes */
typedef enum {
    FR_OK = 0,              /* (0) Succeeded */
    FR_DISK_ERR,           /* (1) A hard error occurred in the low level disk I/O layer */
    FR_INT_ERR,            /* (2) Assertion failed */
    FR_NOT_READY,          /* (3) The physical drive cannot work */
    FR_NO_FILE,            /* (4) Could not find the file */
    FR_NO_PATH,            /* (5) Could not find the path */
    FR_INVALID_NAME,       /* (6) The path name format is invalid */
    FR_DENIED,             /* (7) Access denied due to prohibited access or directory full */
    FR_EXIST,              /* (8) Access denied due to prohibited access */
    FR_INVALID_OBJECT,     /* (9) The file/directory object is invalid */
    FR_WRITE_PROTECTED,    /* (10) The physical drive is write protected */
    FR_INVALID_DRIVE,      /* (11) The logical drive number is invalid */
    FR_NOT_ENABLED,        /* (12) The volume has no work area */
    FR_NO_FILESYSTEM,      /* (13) There is no valid FAT volume */
    FR_MKFS_ABORTED,       /* (14) The f_mkfs() aborted due to any problem */
    FR_TIMEOUT,            /* (15) Could not get a grant to access the volume within defined period */
    FR_LOCKED,             /* (16) The operation is rejected according to the file sharing policy */
    FR_NOT_ENOUGH_CORE,    /* (17) LFN working buffer could not be allocated */
    FR_TOO_MANY_OPEN_FILES /* (18) Number of open files > FF_FS_LOCK */
} FRESULT;

/* Function prototypes */
FRESULT f_mount (FATFS* fs, const TCHAR* path, BYTE opt);
FRESULT f_open (FIL* fp, const TCHAR* path, BYTE mode);
FRESULT f_close (FIL* fp);
FRESULT f_read (FIL* fp, void* buff, UINT btr, UINT* br);
FRESULT f_write (FIL* fp, const void* buff, UINT btw, UINT* bw);
FRESULT f_lseek (FIL* fp, DWORD ofs);
FRESULT f_truncate (FIL* fp);
FRESULT f_sync (FIL* fp);
FRESULT f_opendir (DIR* dp, const TCHAR* path);
FRESULT f_closedir (DIR* dp);
FRESULT f_readdir (DIR* dp, FILINFO* fno);
FRESULT f_mkdir (const TCHAR* path);
FRESULT f_unlink (const TCHAR* path);
FRESULT f_rename (const TCHAR* path_old, const TCHAR* path_new);
FRESULT f_stat (const TCHAR* path, FILINFO* fno);
FRESULT f_chmod (const TCHAR* path, BYTE attr, BYTE mask);
FRESULT f_utime (const TCHAR* path, const FILINFO* fno);
FRESULT f_getfree (const TCHAR* path, DWORD* nclst, FATFS** fatfs);
FRESULT f_mkfs (const TCHAR* path, BYTE opt, DWORD au, void* work, UINT len);

#endif 