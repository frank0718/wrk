#ifndef CONFIG_H
#define CONFIG_H

/*
#ifdef和#defined（）比较

　　首先比较一下这两种方法，第一种方法只能判断一个宏，如果条件比较复杂实现起来比较烦锁，用后者就比较方便。
如有两个宏MACRO_1，MACRO_2只有两个宏都定义过才会编译代码段A，分别实现如下：

　#ifdef MACRO_1
#ifdef MACRO_2
　　　 代码段　A
#endif
#endif

或者
#if defined(MACRO_1) && defined(MACRO_2)
#endif

　　同样，要实现更复杂的条件用#ifdef更麻烦，所以推荐使用后者，因为即使当前代码用的是简单的条件编译，
以后在维护、升级时可能会增加，用后者可维护性较强。旧的编译器可能没有实现#defined（）指令，
C99已经加为标准。要兼容老的编译器，还需用#ifdef指令。

*/


#if defined(__FreeBSD__) || defined(__APPLE__)
#define HAVE_KQUEUE
// 根据平台选择不同的队列

#elif defined(__linux__)
#define HAVE_EPOLL
#elif defined (__sun)
#define HAVE_EVPORT
#define _XPG6
#define __EXTENSIONS__
#include <stropts.h>
/*
编译程序时，出现了这个错误，因为linux不支持STREAMS，缺少这个文件。
stropts.h是POSIX XSR的一部分，但是linux不支持。
解决办法很简单，在/usr/include目录下创建一个空的stropts.h文件。

mac linux ; /usr/include/ 都不存在这个文件
*/


#include <sys/filio.h>
// 文件io
#include <sys/time.h>
//时间
#endif

#endif /* CONFIG_H */

/*
/usr/include缺失了怎么办？

好像将系统升级到Maverick之后就不再有/usr/include文件夹了，这里面包含了编译C/C++的文件，Apple自然不会认为每一个买了Mac的人都是拿来开发的，因此默认把它移除了，有两种方式来安装它：

1）打开终端，输入：

xcode-select --install

会弹出是否安装命令行开发工具的提示，可以点击“安装”来安装单独的工具，安装完成之后/usr/include便出现了。

2）又或者你可以选择先安装Xcode，然后再通过Xcode安装Command Line Tools。
*/
