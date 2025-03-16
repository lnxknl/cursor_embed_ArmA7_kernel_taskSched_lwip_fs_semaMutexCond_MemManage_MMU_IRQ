#ifndef __FS_H__
#define __FS_H__

#include <stdint.h>
#include <stdbool.h>

// 文件系统魔数
#define FS_MAGIC            0xEF53
#define FS_VERSION          0x0001

// 文件类型
#define FT_REGULAR         1  // 普通文件
#define FT_DIRECTORY      2  // 目录
#define FT_CHARDEV        3  // 字符设备
#define FT_BLOCKDEV       4  // 块设备
#define FT_FIFO           5  // 管道
#define FT_SOCKET         6  // 套接字
#define FT_SYMLINK        7  // 符号链接

// 文件权限
#define FM_EXEC           0x001
#define FM_WRITE          0x002
#define FM_READ           0x004
#define FM_USER_MASK      0x007
#define FM_GROUP_MASK     0x038
#define FM_OTHER_MASK     0x1C0

// 文件系统参数
#define BLOCK_SIZE        4096
#define MAX_FILENAME      255
#define MAX_PATH          1024
#define MAX_OPEN_FILES    128
#define MAX_MOUNT_POINTS  16

// inode结构
typedef struct {
    uint16_t mode;           // 文件类型和权限
    uint16_t uid;            // 用户ID
    uint16_t gid;            // 组ID
    uint32_t size;           // 文件大小
    uint32_t atime;          // 访问时间
    uint32_t mtime;          // 修改时间
    uint32_t ctime;          // 创建时间
    uint32_t blocks;         // 块数
    uint32_t direct[12];     // 直接块指针
    uint32_t indirect;       // 一级间接块
    uint32_t double_indirect;// 二级间接块
    uint32_t triple_indirect;// 三级间接块
} inode_t;

// 目录项结构
typedef struct {
    uint32_t inode;          // inode号
    uint16_t rec_len;        // 记录长度
    uint8_t name_len;        // 名称长度
    uint8_t file_type;       // 文件类型
    char name[MAX_FILENAME]; // 文件名
} dir_entry_t;

// 超级块结构
typedef struct {
    uint32_t magic;          // 文件系统魔数
    uint32_t version;        // 版本号
    uint32_t block_size;     // 块大小
    uint32_t blocks_count;   // 总块数
    uint32_t free_blocks;    // 空闲块数
    uint32_t inodes_count;   // inode总数
    uint32_t free_inodes;    // 空闲inode数
    uint32_t first_data_block;// 第一个数据块
    uint32_t first_inode;    // 第一个inode
    uint32_t inode_size;     // inode大小
    uint32_t block_group_nr; // 块组数
    uint32_t blocks_per_group;// 每组块数
    uint32_t inodes_per_group;// 每组inode数
    uint32_t mtime;          // 最后挂载时间
    uint32_t wtime;          // 最后写入时间
    uint16_t mount_count;    // 挂载次数
    uint16_t max_mount_count;// 最大挂载次数
    uint16_t state;          // 文件系统状态
    uint16_t errors;         // 错误处理方式
    uint32_t checksum;       // 校验和
} superblock_t;

// 文件描述符结构
typedef struct {
    uint32_t inode;          // inode号
    uint32_t pos;            // 文件位置
    uint32_t flags;          // 打开标志
    uint32_t count;          // 引用计数
} file_desc_t;

// 挂载点结构
typedef struct {
    char device[MAX_PATH];   // 设备名
    char mount_point[MAX_PATH];// 挂载点
    superblock_t *sb;        // 超级块
    bool mounted;            // 是否已挂载
} mount_point_t;

// 缓存块结构
typedef struct cache_block {
    uint32_t block_no;       // 块号
    uint8_t *data;          // 数据
    bool dirty;             // 是否脏
    uint32_t ref_count;     // 引用计数
    struct cache_block *next;// 下一个块
    struct cache_block *prev;// 前一个块
} cache_block_t;

// 文件系统接口函数
int fs_init(void);
int fs_mount(const char *device, const char *mount_point);
int fs_unmount(const char *mount_point);
int fs_open(const char *path, int flags);
int fs_close(int fd);
int fs_read(int fd, void *buf, uint32_t count);
int fs_write(int fd, const void *buf, uint32_t count);
int fs_seek(int fd, int offset, int whence);
int fs_stat(const char *path, struct stat *buf);
int fs_mkdir(const char *path, uint16_t mode);
int fs_rmdir(const char *path);
int fs_unlink(const char *path);
int fs_rename(const char *oldpath, const char *newpath);
int fs_truncate(const char *path, uint32_t size);
int fs_sync(void);

// 目录操作函数
DIR *fs_opendir(const char *path);
struct dirent *fs_readdir(DIR *dir);
int fs_closedir(DIR *dir);

// 缓存管理函数
cache_block_t *cache_get_block(uint32_t block_no);
void cache_release_block(cache_block_t *block);
void cache_sync(void);
void cache_invalidate(void);

#endif 