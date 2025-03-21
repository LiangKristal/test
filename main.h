#ifndef MAIN_H
#define MAIN_H

#include <cstdio>
#include <unordered_set>
#include <cassert>
#include <cstdlib>
#include <utility>
#include <iostream>
#include <string>
#include <vector>
#include <climits>

using namespace std;


#define MAX_DISK_NUM (10 + 1)
#define MAX_DISK_SIZE (16384 + 1)
#define MAX_REQUEST_NUM (30000000 + 1)
#define MAX_OBJECT_NUM (100000 + 1)
#define REP_NUM (3)
#define FRE_PER_SLICING (1800)
#define EXTRA_TIME (105)


struct Request {
    int object_id; // 表示当前请求想要读取的对象编号
    int reached_time;
    int prev_id; // 存储前一个读取同一对象的请求编号
    bool is_done; // 记录该请求的读取是否已经完成
    int *block_status; // 记录当前请求对应对象的各个块的读取状态（0: 未读, 1: 已读）
    bool is_submit; // 记录该请求是否已经提交完成
    int remaining_blocks; // 剩余未读取的块数，方便检查是否完成
};

struct Block {
    int block_id = 0; // 第object中第几个
    int object_id = 0; // 对象编号（0表示空）
    int last_request_id = 0; // 当前最新请求编号
    int tag = 0; // 标签（0表示空）
};


struct Object {
    int replica[REP_NUM + 1]; // 记录该对象3个副本分别放在哪块硬盘
    int *unit[REP_NUM + 1]; // 存储了3个副本在各自硬盘上的存储单元位置
    int size; // 表示该对象的大小
    Block *blocks;
    int tag; // 表示该对象的标签
    int last_request_point; // 最近一次访问此对象的请求编号
    bool is_delete; // 标记该对象是否已经被删除
};


#endif //MAIN_H
