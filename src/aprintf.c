// Copyright (C) 2012 - Will Glozer.  All rights reserved.

#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

char *aprintf(char **s, const char *fmt, ...) {
    char *c = NULL;
    int n, len;
    va_list ap;

    va_start(ap, fmt);
    n = vsnprintf(NULL, 0, fmt, ap) + 1;
    va_end(ap);

  /*
  头文件:
#include <stdarg.h>
函数声明:
int _vsnprintf(char* str, size_t size, const char* format, va_list ap);
参数说明:

    char *str [out],把生成的格式化的字符串存放在这里.
    size_t size [in], str可接受的最大字符数[1]  (非字节数，UNICODE一个字符两个字节),防止产生数组越界.
    const char *format [in], 指定输出格式的字符串，它决定了你需要提供的可变参数的类型、个数和顺序。
    va_list ap [in], va_list变量. va:variable-argument:可变参数

函数功能：将可变参数格式化输出到一个字符数组。
用法类似于vsprintf，不过加了size的限制，防止了内存溢出（size为str所指的存储空间的大小）。
返回值：执行成功，返回写入到字符数组str中的字符个数（不包含终止符），最大不超过size；执行失败，返回负值，
并置errno
*/


    len = *s ? strlen(*s) : 0;

//    C语言 realloc() 函数位于 stdlib.h 头文件中，其原型为：
//void *realloc(void *ptr, size_t size);
//    realloc() 会将 ptr 所指向的内存块的大小修改为 size，并将新的内存指针返回。
    if ((*s = realloc(*s, (len + n) * sizeof(char)))) {
        c = *s + len;
        va_start(ap, fmt);
        vsnprintf(c, n, fmt, ap);
        va_end(ap);
    }

    return c;
}

/*



可变参数应该有以下步骤:


　　1)首先在函数里定义一个va_list型的变量,这里是arg_ptr,这个变量是指向参数的指针.
　　2)然后用va_start宏初始化变量arg_ptr,这个宏的第二个参数是第一个可变参数的前一个参数,是一个固定的参数.
　　3)然后用va_arg返回可变的参数,并赋值给整数j. va_arg的第二个参数是你要返回的参数的类型,这里是int型.
　　4)最后用va_end宏结束可变参数的获取.然后你就可以在函数里使用第二个参数了.如果函数有多个可变参数的,依次调用va_arg获取各个参数.
*/
