// Copyright (C) 2012 - Will Glozer.  All rights reserved.

#include <stdlib.h>
#include <stdio.h>
#include <strings.h>
#include <inttypes.h>

#include "units.h"
#include "aprintf.h"

typedef struct {
    int scale;   //
    char *base;
    char *units[];  // 数组指针，指向char 数组的指针
} units;


//定义units类型

units time_units_us = {
    .scale = 1000,
    .base  = "us",
    .units = { "ms", "s", NULL }
};
//时间单位， us ms s ，每个单位的倍数是1000  初始化数组用{}表示 。

units time_units_s = {
    .scale = 60,
    .base  = "s",
    .units = { "m", "h", NULL }
};
//小时分钟单位 s m h ，乘积是60

units binary_units = {
    .scale = 1024,
    .base  = "",
    .units = { "K", "M", "G", "T", "P", NULL }
};
//二进制单位 KMGTP ，进阶是1024

units metric_units = {
    .scale = 1000,
    .base  = "",
    .units = { "k", "M", "G", "T", "P", NULL }
};
// 度量单位， 1000进阶



static char *format_units(long double n, units *m, int p) {
    long double amt = n, scale; //赋值加定义
    char *unit = m->base;   // char类型 指针指向 m的base 成员
    char *msg = NULL;

    scale = m->scale * 0.85;

    for (int i = 0; m->units[i+1] && amt >= scale; i++) {
        amt /= m->scale;
        unit = m->units[i];
    }

    aprintf(&msg, "%.*Lf%s", p, amt, unit);

    return msg;
}
//


static int scan_units(char *s, uint64_t *n, units *m) {
    uint64_t base, scale = 1;
    char unit[3] = { 0, 0, 0 };
    int i, c;

    if ((c = sscanf(s, "%"SCNu64"%2s", &base, unit)) < 1) return -1;
//匹配这个模式 保存在&base unit这两个变量中


/*
头文件：#include <string.h>

定义函数：int strcasecmp (const char *s1, const char *s2);

函数说明：strcasecmp()用来比较参数s1 和s2 字符串，比较时会自动忽略大小写的差异。

返回值：若参数s1 和s2 字符串相同则返回0。s1 长度大于s2 长度则返回大于0 的值，s1 长度若小于s2
长度则返回小于0 的值。

范例

    #include <string.h>
    main(){
        char *a = "aBcDeF";
        char *b = "AbCdEf";
        if(!strcasecmp(a, b))
        printf("%s=%s\n", a, b);
    }


执行结果：
aBcDeF=AbCdEf

头文件：#include <string.h>

定义函数：int strncasecmp(const char *s1, const char *s2, size_t n);

函数说明：strncasecmp()用来比较参数s1 和s2 字符串前n个字符，比较时会自动忽略大小写的差异。

返回值：若参数s1 和s2 字符串相同则返回0。s1 若大于s2 则返回大于0 的值，s1 若小于s2 则返回小于0 的值。

范例

    #include <string.h>
    main(){
        char *a = "aBcDeF";
        char *b = "AbCdEf";
        if(!strncasecmp(a, b, 3))
        printf("%s =%s\n", a, b);
    }


执行结果：
aBcDef=AbCdEf

*/

    if (c == 2 && strncasecmp(unit, m->base, 3)) {
        for (i = 0; m->units[i] != NULL; i++) {
            scale *= m->scale;
            if (!strncasecmp(unit, m->units[i], 3)) break;
        }
        if (m->units[i] == NULL) return -1;
    }

    *n = base * scale;
    return 0;
}
//



char *format_binary(long double n) {
    return format_units(n, &binary_units, 2);
}

char *format_metric(long double n) {
    return format_units(n, &metric_units, 2);
}

char *format_time_us(long double n) {
    units *units = &time_units_us;
    if (n >= 1000000.0) {
        n /= 1000000.0;
        units = &time_units_s;
    }
    return format_units(n, units, 2);
}

char *format_time_s(long double n) {
    return format_units(n, &time_units_s, 0);
}



int scan_metric(char *s, uint64_t *n) {
    return scan_units(s, n, &metric_units);
}

int scan_time(char *s, uint64_t *n) {
    return scan_units(s, n, &time_units_s);
}
