#include "ff.h"
#include "diskio.h"
#include <string.h>

/* FAT sub-types */
#define FS_FAT12    1
#define FS_FAT16    2
#define FS_FAT32    3

/* File system mount ID */
static WORD FsID = 0;

/* Mount/Unmount a logical drive */
FRESULT f_mount(FATFS* fs, const TCHAR* path, BYTE opt)
{
    BYTE vol;
    FRESULT res;
    
    /* Get logical drive number */
    vol = path[0] - '0';
    if (vol >= FF_VOLUMES) return FR_INVALID_DRIVE;
    
    /* Register the file system object */
    if (fs) {
        fs->fs_type = 0;    /* Clear new file system object */
        res = mount_volume(&path[0], fs, opt);  /* Mount the volume */
        if (res == FR_OK) {
            fs->id = ++FsID;    /* Update mount ID */
        }
    } else {
        res = unmount_volume(vol);  /* Unmount the volume */
    }
    
    return res;
}

/* Open or create a file */
FRESULT f_open(FIL* fp, const TCHAR* path, BYTE mode)
{
    FRESULT res;
    DIR dj;
    BYTE *dir;
    FATFS *fs;
    
    if (!fp) return FR_INVALID_OBJECT;
    
    /* Get logical drive */
    res = find_volume(&path, &fs, mode);
    if (res != FR_OK) return res;
    
    /* Follow the file path */
    res = follow_path(&dj, path);
    
    /* Create or Open a file */
    if (mode & (FA_CREATE_ALWAYS | FA_OPEN_ALWAYS | FA_CREATE_NEW)) {
        if (res != FR_OK) {    /* No file, create new */
            if (res == FR_NO_FILE)
                res = dir_register(&dj);
            mode |= FA_CREATE_ALWAYS;
        } else {                /* The file exists */
            if (mode & FA_CREATE_NEW)
                res = FR_EXIST;
        }
        if (res == FR_OK && (mode & FA_CREATE_ALWAYS)) {
            /* Set file change flag */
            fs->wflag = 1;
            /* Create new entry */
            res = dir_register(&dj);
        }
    }
    
    /* Open the file if it is exist */
    if (res == FR_OK) {
        if (dj.dir[DIR_Attr] & AM_DIR) {
            res = FR_NO_FILE;
        } else {
            fp->obj.fs = fs;
            fp->obj.sclust = ld_clust(fs, dj.dir);
            fp->obj.fsize = ld_dword(dj.dir + DIR_FileSize);
            fp->obj.fptr = 0;
            fp->flag = mode;
            fp->err = 0;
            fp->sect = 0;
        }
    }
    
    return res;
}

/* Close an open file object */
FRESULT f_close(FIL* fp)
{
    FRESULT res;
    FATFS *fs;
    
    if (!fp) return FR_INVALID_OBJECT;
    fs = fp->obj.fs;
    if (!fs) return FR_INVALID_OBJECT;
    
    res = f_sync(fp);        /* Flush cached data */
    if (res == FR_OK) {
#if !FF_FS_READONLY
        if (fp->flag & FA_MODIFIED) {
            /* Update directory entry */
            res = dir_update(fp);
        }
#endif
        fp->obj.fs = 0;    /* Discard file object */
    }
    
    return res;
} 