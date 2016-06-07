/* zmalloc - total amount of allocated memory aware version of malloc()
 *
 * Copyright (c) 2009-2010, Salvatore Sanfilippo <antirez at gmail dot com>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *   * Redistributions of source code must retain the above copyright notice,
 *     this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *   * Neither the name of Redis nor the names of its contributors may be used
 *     to endorse or promote products derived from this software without
 *     specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include "config.h"
#include "zmalloc.h"

#ifdef HAVE_MALLOC_SIZE
#define PREFIX_SIZE (0)
#else
#if defined(__sun) || defined(__sparc) || defined(__sparc__)
#define PREFIX_SIZE (sizeof(long long))
#else
#define PREFIX_SIZE (sizeof(size_t))
#endif
#endif

/* Explicitly override malloc/free etc when using tcmalloc. */
#if defined(USE_TCMALLOC)
#define malloc(size) tc_malloc(size)
#define calloc(count,size) tc_calloc(count,size)
#define realloc(ptr,size) tc_realloc(ptr,size)
#define free(ptr) tc_free(ptr)
#elif defined(USE_JEMALLOC)
#define malloc(size) je_malloc(size)
#define calloc(count,size) je_calloc(count,size)
#define realloc(ptr,size) je_realloc(ptr,size)
#define free(ptr) je_free(ptr)
#endif

#define update_zmalloc_stat_alloc(__n,__size) do { \
    size_t _n = (__n); \
    if (_n&(sizeof(long)-1)) _n += sizeof(long)-(_n&(sizeof(long)-1)); \
    if (zmalloc_thread_safe) { \
        pthread_mutex_lock(&used_memory_mutex);  \
        used_memory += _n; \
        pthread_mutex_unlock(&used_memory_mutex); \
    } else { \
        used_memory += _n; \
    } \
} while(0)

#define update_zmalloc_stat_free(__n) do { \
    size_t _n = (__n); \
    if (_n&(sizeof(long)-1)) _n += sizeof(long)-(_n&(sizeof(long)-1)); \
    if (zmalloc_thread_safe) { \
        pthread_mutex_lock(&used_memory_mutex);  \
        used_memory -= _n; \
        pthread_mutex_unlock(&used_memory_mutex); \
    } else { \
        used_memory -= _n; \
    } \
} while(0)

static size_t used_memory = 0;
static int zmalloc_thread_safe = 0;
pthread_mutex_t used_memory_mutex = PTHREAD_MUTEX_INITIALIZER;

static void zmalloc_oom(size_t size) {
    fprintf(stderr, "zmalloc: Out of memory trying to allocate %zu bytes\n",
        size);
    fflush(stderr);
    abort();
}

void *zmalloc(size_t size) {
    void *ptr = malloc(size+PREFIX_SIZE);

    if (!ptr) zmalloc_oom(size);
#ifdef HAVE_MALLOC_SIZE
    update_zmalloc_stat_alloc(zmalloc_size(ptr),size);
    return ptr;
#else
    *((size_t*)ptr) = size;
    update_zmalloc_stat_alloc(size+PREFIX_SIZE,size);
    return (char*)ptr+PREFIX_SIZE;
#endif
}

void *zcalloc(size_t size) {
    void *ptr = calloc(1, size+PREFIX_SIZE);

    if (!ptr) zmalloc_oom(size);
#ifdef HAVE_MALLOC_SIZE
    update_zmalloc_stat_alloc(zmalloc_size(ptr),size);
    return ptr;
#else
    *((size_t*)ptr) = size;
    update_zmalloc_stat_alloc(size+PREFIX_SIZE,size);
    return (char*)ptr+PREFIX_SIZE;
#endif
}

void *zrealloc(void *ptr, size_t size) {
#ifndef HAVE_MALLOC_SIZE
    void *realptr;
#endif
    size_t oldsize;
    void *newptr;

    if (ptr == NULL) return zmalloc(size);
#ifdef HAVE_MALLOC_SIZE
    oldsize = zmalloc_size(ptr);
    newptr = realloc(ptr,size);
    if (!newptr) zmalloc_oom(size);

    update_zmalloc_stat_free(oldsize);
    update_zmalloc_stat_alloc(zmalloc_size(newptr),size);
    return newptr;
#else
    realptr = (char*)ptr-PREFIX_SIZE;
    oldsize = *((size_t*)realptr);
    newptr = realloc(realptr,size+PREFIX_SIZE);
    if (!newptr) zmalloc_oom(size);

    *((size_t*)newptr) = size;
    update_zmalloc_stat_free(oldsize);
    update_zmalloc_stat_alloc(size,size);
    return (char*)newptr+PREFIX_SIZE;
#endif
}

/* Provide zmalloc_size() for systems where this function is not provided by
 * malloc itself, given that in that case we store an header with this
 * information as the first bytes of every allocation. */
#ifndef HAVE_MALLOC_SIZE
size_t zmalloc_size(void *ptr) {
    void *realptr = (char*)ptr-PREFIX_SIZE;
    size_t size = *((size_t*)realptr);
    /* Assume at least that all the allocations are padded at sizeof(long) by
     * the underlying allocator. */
    if (size&(sizeof(long)-1)) size += sizeof(long)-(size&(sizeof(long)-1));
    return size+PREFIX_SIZE;
}
#endif

void zfree(void *ptr) {
#ifndef HAVE_MALLOC_SIZE
    void *realptr;
    size_t oldsize;
#endif

    if (ptr == NULL) return;
#ifdef HAVE_MALLOC_SIZE
    update_zmalloc_stat_free(zmalloc_size(ptr));
    free(ptr);
#else
    realptr = (char*)ptr-PREFIX_SIZE;
    oldsize = *((size_t*)realptr);
    update_zmalloc_stat_free(oldsize+PREFIX_SIZE);
    free(realptr);
#endif
}

char *zstrdup(const char *s) {
    size_t l = strlen(s)+1;
    char *p = zmalloc(l);

    memcpy(p,s,l);
    return p;
}

size_t zmalloc_used_memory(void) {
    size_t um;

    if (zmalloc_thread_safe) pthread_mutex_lock(&used_memory_mutex);
    um = used_memory;
    if (zmalloc_thread_safe) pthread_mutex_unlock(&used_memory_mutex);
    return um;
}

void zmalloc_enable_thread_safeness(void) {
    zmalloc_thread_safe = 1;
}

/* Get the RSS information in an OS-specific way.
 *
 * WARNING: the function zmalloc_get_rss() is not designed to be fast
 * and may not be called in the busy loops where Redis tries to release
 * memory expiring or swapping out objects.
 *
 * For this kind of "fast RSS reporting" usages use instead the
 * function RedisEstimateRSS() that is a much faster (and less precise)
 * version of the funciton. */

#if defined(HAVE_PROCFS)
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

size_t zmalloc_get_rss(void) {
    int page = sysconf(_SC_PAGESIZE);
    size_t rss;
    char buf[4096];
    char filename[256];
    int fd, count;
    char *p, *x;

    snprintf(filename,256,"/proc/%d/stat",getpid());
    if ((fd = open(filename,O_RDONLY)) == -1) return 0;
    if (read(fd,buf,4096) <= 0) {
        close(fd);
        return 0;
    }
    close(fd);

    p = buf;
    count = 23; /* RSS is the 24th field in /proc/<pid>/stat */
    while(p && count--) {
        p = strchr(p,' ');
        if (p) p++;
    }
    if (!p) return 0;
    x = strchr(p,' ');
    if (!x) return 0;
    *x = '\0';

    rss = strtoll(p,NULL,10);
    rss *= page;
    return rss;
}
#elif defined(HAVE_TASKINFO)
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/sysctl.h>
#include <mach/task.h>
#include <mach/mach_init.h>

size_t zmalloc_get_rss(void) {
    task_t task = MACH_PORT_NULL;
    struct task_basic_info t_info;
    mach_msg_type_number_t t_info_count = TASK_BASIC_INFO_COUNT;

    if (task_for_pid(current_task(), getpid(), &task) != KERN_SUCCESS)
        return 0;
    task_info(task, TASK_BASIC_INFO, (task_info_t)&t_info, &t_info_count);

    return t_info.resident_size;
}
#else
size_t zmalloc_get_rss(void) {
    /* If we can't get the RSS in an OS-specific way for this system just
     * return the memory usage we estimated in zmalloc()..
     *
     * Fragmentation will appear to be always 1 (no fragmentation)
     * of course... */
    return zmalloc_used_memory();
}
#endif

/* Fragmentation = RSS / allocated-bytes */
float zmalloc_get_fragmentation_ratio(void) {
    return (float)zmalloc_get_rss()/zmalloc_used_memory();
}


/*

#include <stdio.h>
#include <stdlib.h>

//将这个函数定义在"zmalloc.h"之前就是为了避免调用jemallor或者是其他库的标准操作器，
而且调用原始C语言函数库"libc"中的free()函数
void zlibc_free(void *ptr)
{
    free(ptr);
}

#include <string.h>
#include <pthread.h>
#include "config.h"
#include "zmalloc.h"

//PREFIX_SIZE用来记录malloc已分配到的内存大小
//tc_malloc/je_malloc/Mac平台分别采用tc_malloc_size/malloc_size/je_malloc_usable_size（
//不需要单独开空间计算得到的内存大小，PREFIX_SIZE值置为0）
//linux和sun平台分别采用sizeof(size_t)=8字节和sizeof(long long)定长字段记录，所以要记录分配空间的大小
#ifdef HAVE_MALLOC_SIZE
#define PREFIX_SIZE (0)
#else
#if defined(__sun) || defined(__sparc) || defined(__sparc__)
#define PREFIX_SIZE (sizeof(long long))
#else
#define PREFIX_SIZE (sizeof(size_t))
#endif
#endif

//当使用tcmalloc库/jemalloc库的时候，显式覆盖malloc/calloc/realloc/free的方法
#if defined(USE_TCMALLOC)
#define malloc(size) tc_malloc(size)
#define calloc(count,size) tc_malloc(count,size)
#define realloc(ptr,size) tc_realloc(ptr,size)
#define free(ptr) tc_free(ptr)
#elif defined(USE_JEMALLOC)
#define malloc(size) je_malloc(size)
#define calloc(count,size) je_calloc(count,size)
#define realloc(ptr,size) je_realloc(ptr,size)
#define free(prt) je_free(ptr)
#endif

#ifdef HAVE_ATOMIC
#define update_zmalloc_stat_add(__n) __sync_add_and_fetch(&used_mempry,(__n))
#define update_zmalloc_stat_sub(__n) __sync_sub_and_fetch(&used_memory,(__n))
#else
#define update_zmalloc_stat_add(__n) do{ \
    pthread_mutex_lock(&used_memory_mutex); \
    used_memory += (__n); \
    pthread_mutex_unlock(&used_memory_mutex); \
} while(0)   //只执行1次

#define  update_zmalloc_stat_sub(__n) do{ \
    //进程中存在多线程的话，需要加锁进行变量的修改
    pthread_mutex_lock(&used_memory_mutex); \
    used_memory -= (__n); \
    pthread_mutex_unlock(&used_memory_mutex); \
} while(0)
#endif

//定义在分配和释放内存的时候更新used_memory的值（用到上面内存分配的原子操作add和sub函数）
//if结构判断语句中返回的是对_n末两位进行截断的结果：sizeof(long)在32位系统下的二进制表现形式为：100
//if结构成立证明当前分配的内存大小有碎片（即_n值不是内存分配单元的整数倍），所以后面有个+=sizeof(long)...的过程就是为了补齐
#define  update_zmalloc_stat_alloc(__n) do{ \
    size_t _n = (__n); \
    if(_n&(sizeof(long)-1))\
        _n += sizeof(long) -(_n&(sizeof(long)-1)); \
    if(zmalloc_thread_safe){ \
        update_zmalloc_stat_add(_n); \
    }else{ \
        used_memory += _n; \
    } \
    } while(0)
#define  update_zmalloc_stat_free(__n) do{ \
    size_t _n = (__n); \
    if(_n&(sizeof(long)-1))
        _n += sizeof(long) -(_n&(sizeof(long)-1)); \
    if(zmalloc_thread_safe){ \
        update_zmalloc_stat_sub(_n); \
    }else{ \
        used_memory -= _n; \
    } \
    } while(0)

static size_t used_memory = 0;   //定义当前进程使用的内存总量
static int zmalloc_thread_safe = 0;
pthread_mutex_t used_memory_mutex = PTHREAD_MUTEX_INITIALIZER;

//异常处理函数
static void zmalloc_default_oom(size_t size)
{
    fprintf(stderr,"zmalloc：Out of memoty trying to allocate %zu bytes\n",size);
    fflush(stderr);
    abort();
}

static void (*zmalloc_oom_handler)(size_t) = zmalloc_default_oom;

void *zmalloc(size_t size){
    //PREFIX_SIZE代表是否需要存储额外的变量prefix所占的内存字节数：Linux下为sizeof(size_t)=8
    void *ptr = malloc(size+PREFIX_SIZE);
    if(!ptr)  zmalloc_oom_handler(size);
#ifdef HAVE_MALLOC_SIZE    //如果在有size函数能够返回当前内存分配量情况下
    update_zmalloc_stat_alloc(zmalloc_size(ptr));   //此时PREFIX_SIZE=0，更新used_memory
    return ptr;
#else
    //在只知道内存分配单元大小，没有size函数来获取当前使用的内存大小情况下（如Linux+sun平台等）
    *((size_t*)ptr) = size;     //给当前分配空间的起始位置先存储变量size
    update_zmalloc_stat_alloc(size+PREFIX_SIZE);     //更新used_memory
    //将分配指针位置向后移动PREFIX_SIZE尺寸，因为先保存size_t size，其占空间为PREFIX_SIZE大小，移动后返回的是能用空间的起始位置
    return (char*)ptr+PREFIX_SIZE;
#endif
}

void *zcalloc(size_t size)
{
    void *ptr = calloc(1,size+PREFIX_SIZE);  //分配1个长度为size+PREFIX_SIZE大小的连续空间

    if(!ptr) zmalloc_oom_handler(size);
#ifdef HAVE_MALLOC_SIZE
    update_zmalloc_stat_alloc(zmalloc_size(ptr));
    return ptr;
#else
    *((size_t*)ptr) = size;
    update_zmalloc_stat_alloc(size+PREFIX_SIZE);
    return (char*)ptr+PREFIX_SIZE;
#endif
}

//给一个内存块分配一个新的长度的连续空间
void *zrealloc(void *ptr,size_t size)
{
#ifndef HAVE_MALLOC_SIZE
    void *realptr;
#endif
    size_t oldsize;
    void *newptr;
#ifdef HAVE_MALLOC_SIZE
    oldsize = zmalloc_size(ptr);
    newptr = realloc(ptr,size);
    if(!newptr) zmalloc_oom_handler(size);

    update_zmalloc_stat_free(oldsize);
    update_zmalloc_stat_alloc(zmalloc_size(newptr));
    return newptr;
#else
    //内存块向前移动PREFIX_SIZE字节的位置（存储的是内存分配的size大小值）
    realptr = (char*)ptr-PREFIX_SIZE;
    oldsize = *((size_t*)realptr)；
    newptr = realloc(realptr,size+PREFIX_SIZE);
    if(!newptr) zmalloc_oom_handler(size);

    //给新的内存块设值数据
        *((size_t*)newptr) = size;
    update_zmalloc_stat_free(oldsize);
    update_zmalloc_stat_alloc(size);      //这里为什么不要加上PREFIX_SIZE？
    return (char*)newptr+PREFIX_SIZE;
#endif
}

//返回当前分配内存块的大小
#ifndef HAVE_MALLOC_SIZE
size_t zmalloc_size(void *ptr)
{
    void *realptr = (char*)ptr - PREFIX_SIZE;
    size_t size = *((size_t*)realptr);
    if(size&(sizeof(long)-1))
        size += sizeof(long) - (size&(sizeof(long)-1));
    return size+PREFIX_SIZE;
}
#endif

//释放指定的内存空间
void zfree(void *ptr)
{
#ifndef HAVE_MALLOC_SIZE
    void *realptr;
    size_t oldsize;
#endif
    if(ptr == NULL) return;
#ifdef HAVE_MALLOC_SIZE
    update_zmalloc_stat_free(zmalloc_size(ptr));
    free(ptr);
#else
    realptr = (char*)ptr - PREFIX_SIZE;
    oldsize = *((size_t*)realptr);      //前面这两行是否位置反了？
    update_zmalloc_stat_free(oldsize+PREFIX_SIZE);
    free(realptr);
#endif
}

//复制一个字符串
char *zstrdup(const char *s)
{
    size_t l = strlen(s)+1;    //memcpy不会遇到‘/0’停止，增加额外的一个字节存储'/0'的内容
    char *p = zmalloc(l);
    memcpy(p,s,l);
    return p;
}
//返回当前已经使用的内存大小
size_t zmalloc_used_memory(void)
{
    size_t um;
    if(zmalloc_thread_safe){
#ifdef HAVE_ATOMIC
        um = __sync_add_and_fetch(&used_memory,0);
#else
        pthread_mutex_lock(&used_memory_mutex);
        um = used_memory;
        pthread_mutex_unlock(&used_memory_mutex);
#endif
    }
    else{
        um = used_memory;
    }
    return um;
}

void zmalloc_enable_thread_safeness(void)
{
    zmalloc_thread_safe = 1;
}

//设置异常处理的函数
void zmalloc_set_oom_handler(void(*oom_handler)(size_t))
{
    zmalloc_oom_handler = oom_handler;
}


*/


/*

当我第一次阅读了这个文件的源码的时候，我笑了，忽然想起前几周阿里电话二面的时候，问到了自定义内存管理函数并处理8字节对齐问题。当时无言以对，在面试官无数次的提示下才答了出来，结果显而易见，挂掉了二面。而这份源码中函数zmalloc()和zfree()的设计思路和实现原理，正是面试官想要的答案。
源码结构

zmalloc.c文件的内容如下：

主要函数

  zmalloc()
  zfree()
  zcalloc()
  zrelloc()
  zstrdup()

字长与字节对齐
      CPU一次性能读取数据的二进制位数称为字长，也就是我们通常所说的32位系统（字长4个字节）、64位系统（字长8个字节）的由来。所谓的8字节对齐，就是指变量的起始地址是8的倍数。比如程序运行时（CPU）在读取long型数据的时候，只需要一个总线周期，时间更短，如果不是8字节对齐的则需要两个总线周期才能读完数据。
      本文中我提到的8字节对齐是针对64位系统而言的，如果是32位系统那么就是4字节对齐。实际上Redis源码中的字节对齐是软编码，而非硬编码。里面多用sizeof(long)或sizeof(size_t)来表示。size_t（gcc中其值为long unsigned int）和long的长度是一样的，long的长度就是计算机的字长。这样在未来的系统中如果字长（long的大小）不是8个字节了，该段代码依然能保证相应代码可用。

zmalloc
      辅助的函数：

  malloc()
  zmalloc_oom_handler【函数指针】
  zmalloc_default_oom()【被上面的函数指针所指向】
  update_zmalloc_stat_alloc()【宏函数】
  update_zmalloc_stat_add()【宏函数】

zmalloc()和malloc()有相同的函数接口（参数，返回值）。
zmalloc()源码
[cpp] view plain copy
在CODE上查看代码片派生到我的代码片

  void *zmalloc(size_t size) {
      void *ptr = malloc(size+PREFIX_SIZE);

      if (!ptr) zmalloc_oom_handler(size);
  #ifdef HAVE_MALLOC_SIZE
      update_zmalloc_stat_alloc(zmalloc_size(ptr));
      return ptr;
  #else
      *((size_t*)ptr) = size;
      update_zmalloc_stat_alloc(size+PREFIX_SIZE);
      return (char*)ptr+PREFIX_SIZE;
  #endif
  }

      参数size是我们需要分配的内存大小。实际上我们调用malloc实际分配的大小是size+PREFIX_SIZE。PREFIX_SIZE是一个条件编译的宏，不同的平台有不同的结果，在Linux中其值是sizeof(size_t)，所以我们多分配了一个字长(8个字节)的空间（后面代码可以看到多分配8个字节的目的是用于储存size的值）。
      如果ptr指针为NULL（内存分配失败），调用zmalloc_oom_handler（size）。该函数实际上是一个函数指针指向函数zmalloc_default_oom，其主要功能就是打印错误信息并终止程序。
[cpp] view plain copy
在CODE上查看代码片派生到我的代码片

  // oom是out of memory（内存不足）的意思
  static void zmalloc_default_oom(size_t size) {
      fprintf(stderr, "zmalloc: Out of memory trying to allocate %zu bytes\n",
          size);
      fflush(stderr);
      abort();
  }

接下来是宏的条件编译，我们聚焦在#else的部分。
[cpp] view plain copy
在CODE上查看代码片派生到我的代码片

  *((size_t*)ptr) = size;
  update_zmalloc_stat_alloc(size+PREFIX_SIZE);
  return (char*)ptr+PREFIX_SIZE;

第一行就是在已分配空间的第一个字长（前8个字节）处存储需要分配的字节大小（size）。
第二行调用了update_zmalloc_stat_alloc()【宏函数】，它的功能是更新全局变量used_memory（已分配内存的大小）的值（源码解读见下一节）。
第三行返回的（char *）ptr+PREFIX_SIZE。就是将已分配内存的起始地址向右偏移PREFIX_SIZE * sizeof(char)的长度（即8个字节），此时得到的新指针指向的内存空间的大小就等于size了。
接下来，分析一下update_zmalloc_stat_alloc的源码
update_zmalloc_stat_alloc源码
[cpp] view plain copy
在CODE上查看代码片派生到我的代码片

  #define update_zmalloc_stat_alloc(__n) do { \
      size_t _n = (__n); \
      if (_n&(sizeof(long)-1)) _n += sizeof(long)-(_n&(sizeof(long)-1)); \
      if (zmalloc_thread_safe) { \
          update_zmalloc_stat_add(_n); \
      } else { \
          used_memory += _n; \
      } \
  } while(0)

这个宏函数最外圈有一个do{...}while(0)循环看似毫无意义，实际上大有深意。这部分内容不是本文讨论的重点，这里不再赘述。具体请看网上的这篇文章 http://www.spongeliu.com/415.html。
      因为 sizeof(long) = 8 【64位系统中】，所以上面的第一个if语句，可以等价于以下代码：
[cpp] view plain copy
在CODE上查看代码片派生到我的代码片

  if(_n&7) _n += 8 - (_n&7);

       这段代码就是判断分配的内存空间的大小是不是8的倍数。如果内存大小不是8的倍数，就加上相应的偏移量使之变成8的倍数。_n&7 在功能上等价于 _n%8，不过位操作的效率显然更高。
      malloc()本身能够保证所分配的内存是8字节对齐的：如果你要分配的内存不是8的倍数，那么malloc就会多分配一点，来凑成8的倍数。所以update_zmalloc_stat_alloc函数（或者说zmalloc()相对malloc()而言）真正要实现的功能并不是进行8字节对齐（malloc已经保证了），它的真正目的是使变量used_memory精确的维护实际已分配内存的大小。
      第2个if的条件是一个整型变量zmalloc_thread_safe。顾名思义，它的值表示操作是否是线程安全的，如果不是线程安全的（else），就给变量used_memory加上n。used_memory是zmalloc.c文件中定义的全局静态变量，表示已分配内存的大小。如果是内存安全的就使用update_zmalloc_stat_add来给used_memory加上n。
      update_zmalloc_stat_add也是一个宏函数（Redis效率之高，速度之快，这些宏函数可谓功不可没）。它也是一个条件编译的宏，依据不同的宏有不同的定义，这里我们来看一下#else后面的定义的源码【zmalloc.c有多处条件编译的宏，为了把精力都集中在内存管理的实现算法上，这里我只关注Linux平台下使用glibc的malloc的情况】。
[cpp] view plain copy
在CODE上查看代码片派生到我的代码片

  #define update_zmalloc_stat_add(__n) do { \
      pthread_mutex_lock(&used_memory_mutex); \
      used_memory += (__n); \
      pthread_mutex_unlock(&used_memory_mutex); \
  } while(0)

      pthread_mutex_lock()和pthread_mutex_unlock()使用互斥锁（mutex）来实现线程同步，前者表示加锁，后者表示解锁，它们是POSIX定义的线程同步函数。当加锁以后它后面的代码在多线程同时执行这段代码的时候就只会执行一次，也就是实现了线程安全。
zfree
      zfree()和free()有相同的编程接口，它负责清除zmalloc()分配的空间。
辅助函数:

  free()
  update_zmalloc_free()【宏函数】
  update_zmalloc_sub()【宏函数】
  zmalloc_size()

zfree()源码
[cpp] view plain copy
在CODE上查看代码片派生到我的代码片

  void zfree(void *ptr) {
  #ifndef HAVE_MALLOC_SIZE
      void *realptr;
      size_t oldsize;
  #endif

      if (ptr == NULL) return;
  #ifdef HAVE_MALLOC_SIZE
      update_zmalloc_stat_free(zmalloc_size(ptr));
      free(ptr);
  #else
      realptr = (char*)ptr-PREFIX_SIZE;
      oldsize = *((size_t*)realptr);
      update_zmalloc_stat_free(oldsize+PREFIX_SIZE);
      free(realptr);
  #endif
  }

重点关注#else后面的代码
[cpp] view plain copy
在CODE上查看代码片派生到我的代码片

  realptr = (char *)ptr - PREFIX_SIZE;

表示的是ptr指针向前偏移8个字节的长度，即回退到最初malloc返回的地址，这里称为realptr。然后
[cpp] view plain copy
在CODE上查看代码片派生到我的代码片

  oldsize = *((size_t*)realptr);

先进行类型转换再取指针所指向的值。通过zmalloc()函数的分析，可知这里存储着我们最初需要分配的内存大小（zmalloc中的size），这里赋值个oldsize
[cpp] view plain copy
在CODE上查看代码片派生到我的代码片

  update_zmalloc_stat_free(oldsize+PREFIX_SIZE);

update_zmalloc_stat_free()也是一个宏函数，和zmalloc中update_zmalloc_stat_alloc()大致相同，唯一不同之处是前者在给变量used_memory减去分配的空间，而后者是加上该空间大小。
最后free(realptr)，清除空间
update_zmalloc_free源码
[cpp] view plain copy
在CODE上查看代码片派生到我的代码片

  #define update_zmalloc_stat_free(__n) do { \
      size_t _n = (__n); \
      if (_n&(sizeof(long)-1)) _n += sizeof(long)-(_n&(sizeof(long)-1)); \
      if (zmalloc_thread_safe) { \
          update_zmalloc_stat_sub(_n); \
      } else { \
          used_memory -= _n; \
      } \
  } while(0)

其中的函数update_zmalloc_sub与zmalloc()中的update_zmalloc_add相对应，但功能相反，提供线程安全地used_memory减法操作。
[cpp] view plain copy
在CODE上查看代码片派生到我的代码片

  #define update_zmalloc_stat_sub(__n) do { \
      pthread_mutex_lock(&used_memory_mutex); \
      used_memory -= (__n); \
      pthread_mutex_unlock(&used_memory_mutex); \
  } while(0)

zcalloc
      zcalloc()的实现基于calloc()，但是两者编程接口不同。看一下对比：
[cpp] view plain copy
在CODE上查看代码片派生到我的代码片

  void *calloc(size_t nmemb, size_t size);
  void *zcalloc(size_t size);

calloc()的功能是也是分配内存空间，与malloc()的不同之处有两点：

  它分配的空间大小是 size * nmemb。比如calloc(10,sizoef(char)); // 分配10个字节
  calloc()会对分配的空间做初始化工作（初始化为0），而malloc()不会

辅助函数

  calloc()
  update_zmalloc_stat_alloc()【宏函数】
  update_zmalloc_stat_add()【宏函数】

zcalloc()源码
[cpp] view plain copy
在CODE上查看代码片派生到我的代码片

  void *zcalloc(size_t size) {
      void *ptr = calloc(1, size+PREFIX_SIZE);

      if (!ptr) zmalloc_oom_handler(size);
  #ifdef HAVE_MALLOC_SIZE
      update_zmalloc_stat_alloc(zmalloc_size(ptr));
      return ptr;
  #else
      *((size_t*)ptr) = size;
      update_zmalloc_stat_alloc(size+PREFIX_SIZE);
      return (char*)ptr+PREFIX_SIZE;
  #endif
  }

      zcalloc()中没有calloc()的第一个函数nmemb。因为它每次调用calloc(),其第一个参数都是1。也就是说zcalloc()功能是每次分配 size+PREFIX_SIZE 的空间，并初始化。
其余代码的分析和zmalloc()相同，也就是说：
      zcalloc()和zmalloc()具有相同的编程接口，实现功能基本相同，唯一不同之处是zcalloc()会做初始化工作，而zmalloc()不会。
zrealloc
      zrealloc()和realloc()具有相同的编程接口：
[cpp] view plain copy
在CODE上查看代码片派生到我的代码片

  void *realloc (void *ptr, size_t size);
  void *zrealloc(void *ptr, size_t size);

      realloc()要完成的功能是给首地址ptr的内存空间，重新分配大小。如果失败了，则在其它位置新建一块大小为size字节的空间，将原先的数据复制到新的内存空间，并返回这段内存首地址【原内存会被系统自然释放】。
      zrealloc()要完成的功能也类似。
辅助函数：

  zmalloc()
  zmalloc_size()
  realloc()
  zmalloc_oom_handler【函数指针】
  update_zmalloc_stat_free()【宏函数】
  update_zmalloc_stat_alloc()【宏函数】

zrealloc()源码
[cpp] view plain copy
在CODE上查看代码片派生到我的代码片

  void *zrealloc(void *ptr, size_t size) {
  #ifndef HAVE_MALLOC_SIZE
      void *realptr;
  #endif
      size_t oldsize;
      void *newptr;

      if (ptr == NULL) return zmalloc(size);
  #ifdef HAVE_MALLOC_SIZE
      oldsize = zmalloc_size(ptr);
      newptr = realloc(ptr,size);
      if (!newptr) zmalloc_oom_handler(size);

      update_zmalloc_stat_free(oldsize);
      update_zmalloc_stat_alloc(zmalloc_size(newptr));
      return newptr;
  #else
      realptr = (char*)ptr-PREFIX_SIZE;
      oldsize = *((size_t*)realptr);
      newptr = realloc(realptr,size+PREFIX_SIZE);
      if (!newptr) zmalloc_oom_handler(size);

      *((size_t*)newptr) = size;
      update_zmalloc_stat_free(oldsize);
      update_zmalloc_stat_alloc(size);
      return (char*)newptr+PREFIX_SIZE;
  #endif
  }

经过前面关于zmalloc()和zfree()的源码解读，相信您一定能够很轻松地读懂zrealloc()的源码，这里我就不赘述了。
zstrdup
      从这个函数名中，很容易发现它是string duplicate的缩写，即字符串复制。它的代码比较简单。先看一下声明：
[cpp] view plain copy
在CODE上查看代码片派生到我的代码片

  char *zstrdup(const char *s);

功能描述：复制字符串s的内容，到新的内存空间，构造新的字符串【堆区】。并将这段新的字符串地址返回。
zstrdup源码
[cpp] view plain copy
在CODE上查看代码片派生到我的代码片

  char *zstrdup(const char *s) {
      size_t l = strlen(s)+1;
      char *p = zmalloc(l);

      memcpy(p,s,l);
      return p;
  }

  首先，先获得字符串s的长度，新闻strlen()函数是不统计'\0'的，所以最后要加1。
  然后调用zmalloc()来分配足够的空间，首地址为p。
  调用memcpy来完成复制。
  然后返回p。

简单介绍一下memcpy
memcpy
      这是标准C【ANSI C】中用于内存复制的函数，在头文件<string.h>中（gcc）。声明如下：
[cpp] view plain copy
在CODE上查看代码片派生到我的代码片

  void *memcpy(void *dest, const void *src, size_t n);

dest即目的地址，src是源地址。n是要复制的字节数。
*/
