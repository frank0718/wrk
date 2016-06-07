#ifndef STATS_H
#define STATS_H

#include <stdbool.h>
/*
C语言的标准函数库中的头文件stdbool.h包含了四个用于布尔型的预定义宏。本头文件是在C99标准中引入的。

IEEE 1003.1-2001标准中的宏定义包括:

    bool，会扩展为_Bool
    true，会扩展为1
    false，会扩展为0
    __bool_true_false_are_defined，会扩展为1

如下例所示:

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

int main(void) {
    bool keep_going = true;  //也可以是`bool keep_going = 1;`
    while(keep_going) {
        printf("本程序会在keep_going为真时持续运行.\n");
        keep_going = false;    // 也可以是`keep_going = 0;`
    }
    printf("停止运行!\n");
    return EXIT_SUCCESS;
}
*/

#include <stdint.h>

#define MAX(X, Y) ((X) > (Y) ? (X) : (Y))
#define MIN(X, Y) ((X) < (Y) ? (X) : (Y))

typedef struct {
    uint32_t connect;
    uint32_t read;
    uint32_t write;
    uint32_t status;
    uint32_t timeout;
} errors;

typedef struct {
    uint64_t count;
    uint64_t limit;
    uint64_t min;
    uint64_t max;
    uint64_t data[];
} stats;

//返回一个指向stats类型的指针
stats *stats_alloc(uint64_t);

void stats_free(stats *);

int stats_record(stats *, uint64_t);
void stats_correct(stats *, int64_t);

long double stats_mean(stats *);
long double stats_stdev(stats *stats, long double);
long double stats_within_stdev(stats *, long double, long double, uint64_t);
uint64_t stats_percentile(stats *, long double);

uint64_t stats_popcount(stats *);
uint64_t stats_value_at(stats *stats, uint64_t, uint64_t *);

#endif /* STATS_H */
