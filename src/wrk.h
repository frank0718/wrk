#ifndef WRK_H
#define WRK_H
// 在c语言中，对同一个变量或者函数进行多次声明是不会报错的。所以如果h文件里只是进行了声明工作，
// 即使不使用# ifndef宏定义，一个c文件多次包含同一个h文件也不会报错。　
// 使用#ifndef可以避免下面这种错误：如果在h文件中定义了全局变量，一个c文件包含同一个h文件多次，
// 如果不加#ifndef宏定义，会出现变量重复定义的错误；如果加了#ifndef，则不会出现这种错.

#include "config.h"

#include <pthread.h>
/*
posix 线程库
gcc xyz.c -o xyz -lpthread

Here,

gcc is compiler command (compiler name)
xyz.c is a source file name.
-o is an option to create objcect file.
xyz is the name of object (binary) file.
-lpthread is an option for pthread.h
*/

#include <inttypes.h>
/*
inttypes.h是C标准函数库中的头文件，提供了各种位宽的整数类型输入输出时的转换标志宏
#include <inttypes.h>
#include <wchar.h>
int main(void)
{
  uintmax_t i = UINTMAX_MAX; // this type always exists
  wprintf(L"The largest integer value is %020" PRIxMAX "\n", i);
  return 0;
}
*/


#include <sys/types.h>
/*
sys/types.h，在应用程序源文件中包含 <sys/types.h> 以访问 _LP64 和 _ILP32 的定义。
所有这些类型在 ILP32 编译环境中保持为 32 位值，并会在 LP64 编译环境中增长为 64 位值。
*/

#include <netdb.h>
/*
netdb.h - definitions for network database operations
*/


#include <sys/socket.h>
//sys/socket.h - main sockets header


#include <openssl/ssl.h>
#include <openssl/err.h>
#include <luajit-2.0/lua.h>
// 会解压到目录

#include "stats.h"
#include "ae.h"


#include "http_parser.h"


//接收buffer 8192byte

#define RECVBUF  8192

#define MAX_THREAD_RATE_S   10000000
#define SOCKET_TIMEOUT_MS   2000
#define RECORD_INTERVAL_MS  100

extern const char *VERSION;

typedef struct {
    pthread_t thread;
    aeEventLoop *loop;
    struct addrinfo *addr;
    uint64_t connections;
    uint64_t complete;
    uint64_t requests;
    uint64_t bytes;
    uint64_t start;
    lua_State *L;
    errors errors;
    struct connection *cs;
} thread;
//  线程结构体


typedef struct {
    char  *buffer;
    size_t length;
    char  *cursor;
} buffer;
// buffer结构体


typedef struct connection {
    thread *thread;
    http_parser parser;
    enum {
        FIELD, VALUE
    } state;
    int fd;
    SSL *ssl;
    bool delayed;
    uint64_t start;
    char *request;
    size_t length;
    size_t written;
    uint64_t pending;
    buffer headers;
    buffer body;
    char buf[RECVBUF];
} connection;
//连接结构体


#endif /* WRK_H */
