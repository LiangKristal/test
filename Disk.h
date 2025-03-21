#ifndef DISK_H
#define DISK_H

#include <tuple>
#include <vector>
#include <utility>
#include <unordered_map>
#include <climits>
#include <algorithm>

struct Object;
struct Block;
struct Request;

class Disk {
public:
    // 基础信息
    int disk_num; // 磁盘个数
    int disk_size; // 每个磁盘的存储单元个数
    int disk_tag;   //标签数量

    struct Partition {
        int tag; // 对应的标签编号（1-based）
        int start; // 起始存储单元（含）
        int end; // 结束存储单元（含）

        Partition(int t, int s, int e)
            : tag(t), start(s), end(e) {
        }
    };

    struct head_point {
        int position = 1;
        char last_command = ' ';
        int last_token = 0;
    };

    // 磁盘
    // storage[d][pos]：第 d 号磁盘 的第 pos 号存储单元信息
    std::vector<std::vector<Block *> > storage; // 磁盘存储
    // 磁头
    std::vector<head_point> disk_point; // 磁头位置（从1开始）
    // 磁头离对象块最近的距离
    std::vector<int> distances;
    // 每个磁盘的分区列表
    std::vector<std::vector<Partition> > partitions; // partitions[d] 是该磁盘上的所有分区

    //std::vector<std::vector<int>> disk_last_position;

    std::vector<std::vector<int>> disk_object_count;          //记录每个标签在对应硬盘上的标签对象块数量

    std::vector<std::vector<int>> disk_partition;           //维护每个硬盘上每个标签的分区起始位置

    std::vector<int> partition_size;                        //每个标签的分区大小

    std::vector<int>disk_partition_start;                   //每个标签的存储起始位置

    //std::vector<int> fre_write;                             //每个标签的写入大小

    //std::vector<int> overflow_position;                     //每个硬盘额外存储空间起始位置

    std::vector<std::vector<int>> tag_write;                //写入数据
    std::vector<std::vector<int>> tag_delete;               //删除数据
    std::vector<int> tag_max_occupy;                        //每个标签的最大净占空间


public:
    // 构造函数（创建磁盘）
    Disk(int num_disks, int disk_size, int disk_tag);

    // 计算可读对象块距离
    std::vector<std::pair<int, int> > calculate_distances(int timestamp, Request *request);

    // 寻找磁头最近的对象块
    std::pair<int, int> find_distances(int disk_id, int head_pos, int timestamp, Request *request);

    // 计算连续 read 策略要消耗的token函数
    std::tuple<int, int, int, bool> calculate_read_cost(int target_pos, int token_remaining, head_point head, int V);

    // 磁盘分区函数

    // 删除操作函数
    void delete_object_from_disk(const int *object_unit, int disk_unit, int size, int tag);

    // 写入操作函数
    void write_object_to_disk(int* object_unit, int disk_unit, int size, int tag, int V, Block* blocks);

    //预定义分区
    void initialize_dynamic_partitions(int N, int M, int V);

    void calculate_max_occupy(int slice_num);

    // 读取操作函数（根据磁头移动编写）
};
#endif //DISK_H
