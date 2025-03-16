#include "mini_stdio.h"
#include "mini_syscall.h"

#define BUFFER_SIZE 1024

typedef struct {
    int fd;
    int flags;
    char *buf;
    int buf_pos;
    int buf_end;
} FILE;

static FILE _stdin = {0, O_RDONLY, NULL, 0, 0};
static FILE _stdout = {1, O_WRONLY, NULL, 0, 0};
static FILE _stderr = {2, O_WRONLY, NULL, 0, 0};

FILE *stdin = &_stdin;
FILE *stdout = &_stdout;
FILE *stderr = &_stderr;

static int _fill_buffer(FILE *stream)
{
    if (!stream->buf) {
        stream->buf = malloc(BUFFER_SIZE);
        if (!stream->buf)
            return EOF;
    }
    
    stream->buf_pos = 0;
    stream->buf_end = sys_read(stream->fd, stream->buf, BUFFER_SIZE);
    return stream->buf_end > 0 ? 0 : EOF;
}

static int _flush_buffer(FILE *stream)
{
    int written = 0;
    int ret;
    
    while (written < stream->buf_pos) {
        ret = sys_write(stream->fd, 
                       stream->buf + written,
                       stream->buf_pos - written);
        if (ret < 0)
            return EOF;
        written += ret;
    }
    
    stream->buf_pos = 0;
    return 0;
}

FILE *fopen(const char *path, const char *mode)
{
    int flags = 0;
    FILE *fp;
    
    if (strchr(mode, 'r')) flags |= O_RDONLY;
    if (strchr(mode, 'w')) flags |= O_WRONLY | O_CREAT | O_TRUNC;
    if (strchr(mode, 'a')) flags |= O_WRONLY | O_CREAT | O_APPEND;
    if (strchr(mode, '+')) flags = (flags & ~(O_RDONLY | O_WRONLY)) | O_RDWR;
    
    fp = malloc(sizeof(FILE));
    if (!fp)
        return NULL;
    
    fp->fd = sys_open(path, flags, 0666);
    if (fp->fd < 0) {
        free(fp);
        return NULL;
    }
    
    fp->flags = flags;
    fp->buf = NULL;
    fp->buf_pos = 0;
    fp->buf_end = 0;
    
    return fp;
}

int fclose(FILE *stream)
{
    int ret = 0;
    
    if (stream->buf_pos > 0)
        ret = _flush_buffer(stream);
    
    if (stream->buf)
        free(stream->buf);
    
    if (stream->fd > 2)
        sys_close(stream->fd);
    
    if (stream != stdin && stream != stdout && stream != stderr)
        free(stream);
    
    return ret;
}

size_t fread(void *ptr, size_t size, size_t nmemb, FILE *stream)
{
    size_t bytes = size * nmemb;
    size_t read = 0;
    char *buf = ptr;
    
    while (read < bytes) {
        if (stream->buf_pos >= stream->buf_end) {
            if (_fill_buffer(stream) == EOF)
                break;
        }
        
        size_t remain = stream->buf_end - stream->buf_pos;
        size_t to_copy = bytes - read < remain ? bytes - read : remain;
        
        memcpy(buf + read, stream->buf + stream->buf_pos, to_copy);
        stream->buf_pos += to_copy;
        read += to_copy;
    }
    
    return read / size;
}

size_t fwrite(const void *ptr, size_t size, size_t nmemb, FILE *stream)
{
    size_t bytes = size * nmemb;
    size_t written = 0;
    const char *buf = ptr;
    
    while (written < bytes) {
        if (stream->buf_pos >= BUFFER_SIZE) {
            if (_flush_buffer(stream) == EOF)
                break;
        }
        
        size_t remain = BUFFER_SIZE - stream->buf_pos;
        size_t to_copy = bytes - written < remain ? bytes - written : remain;
        
        memcpy(stream->buf + stream->buf_pos, buf + written, to_copy);
        stream->buf_pos += to_copy;
        written += to_copy;
    }
    
    if (stream == stderr || (stream->flags & O_APPEND))
        _flush_buffer(stream);
    
    return written / size;
} 