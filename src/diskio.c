#include "ff.h"
#include "diskio.h"
#include <string.h>

/* Disk Status Bits */
#define STA_NOINIT      0x01    /* Drive not initialized */
#define STA_NODISK      0x02    /* No medium in the drive */
#define STA_PROTECT     0x04    /* Write protected */

/* Definitions of physical drive number for each drive */
#define DEV_RAM     0   /* Example: Map Ramdisk to physical drive 0 */
#define DEV_MMC     1   /* Example: Map MMC/SD card to physical drive 1 */
#define DEV_USB     2   /* Example: Map USB MSD to physical drive 2 */

static volatile DSTATUS Stat[FF_VOLUMES] = {STA_NOINIT};   /* Disk status */

/* Disk IO control functions */
DSTATUS disk_initialize(BYTE pdrv)
{
    DSTATUS stat = STA_NOINIT;
    
    switch (pdrv) {
    case DEV_RAM:
        // Initialize RAM disk
        stat = RAM_disk_initialize();
        break;

    case DEV_MMC:
        // Initialize MMC/SD card
        stat = MMC_disk_initialize();
        break;

    case DEV_USB:
        // Initialize USB storage
        stat = USB_disk_initialize();
        break;
    }
    
    return stat;
}

DSTATUS disk_status(BYTE pdrv)
{
    DSTATUS stat;
    
    switch (pdrv) {
    case DEV_RAM:
        stat = RAM_disk_status();
        break;

    case DEV_MMC:
        stat = MMC_disk_status();
        break;

    case DEV_USB:
        stat = USB_disk_status();
        break;

    default:
        stat = STA_NOINIT;
    }
    
    return stat;
}

DRESULT disk_read(BYTE pdrv, BYTE* buff, DWORD sector, UINT count)
{
    DRESULT res;
    
    switch (pdrv) {
    case DEV_RAM:
        res = RAM_disk_read(buff, sector, count);
        break;

    case DEV_MMC:
        res = MMC_disk_read(buff, sector, count);
        break;

    case DEV_USB:
        res = USB_disk_read(buff, sector, count);
        break;

    default:
        res = RES_PARERR;
    }
    
    return res;
}

DRESULT disk_write(BYTE pdrv, const BYTE* buff, DWORD sector, UINT count)
{
    DRESULT res;
    
    switch (pdrv) {
    case DEV_RAM:
        res = RAM_disk_write(buff, sector, count);
        break;

    case DEV_MMC:
        res = MMC_disk_write(buff, sector, count);
        break;

    case DEV_USB:
        res = USB_disk_write(buff, sector, count);
        break;

    default:
        res = RES_PARERR;
    }
    
    return res;
}

DRESULT disk_ioctl(BYTE pdrv, BYTE cmd, void* buff)
{
    DRESULT res;
    
    switch (pdrv) {
    case DEV_RAM:
        res = RAM_disk_ioctl(cmd, buff);
        break;

    case DEV_MMC:
        res = MMC_disk_ioctl(cmd, buff);
        break;

    case DEV_USB:
        res = USB_disk_ioctl(cmd, buff);
        break;

    default:
        res = RES_PARERR;
    }
    
    return res;
} 