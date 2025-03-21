#include "Disk.h"
#include "main.h"
#include <numeric>
#include <cmath>
// 构造函数：初始化磁盘
Disk::Disk(int num_disk, int size_disk, int tag_disk) : disk_num(num_disk), disk_size(size_disk), disk_tag(tag_disk) {
    // 1. 初始化 storage（每个磁盘都有 disk_size 个存储单元）
    storage.resize(num_disk + 1, std::vector<Block *>(size_disk + 1, nullptr));
    // 2. 初始化磁头位置，默认磁头位置为1（可以根据需求修改为0）
    disk_point.resize(num_disk + 1); // 磁头位置初始化为1 索引开始也是1
    // 磁头离对象块最近的距离
    distances.resize(num_disk + 1, INT_MAX);

    //位置
    //disk_last_position.resize(num_disk + 1, std::vector<int>(tag_disk + 1, 0));
    //数量
    disk_object_count.resize(num_disk + 1, std::vector<int>(tag_disk + 1, 0));

    disk_partition.resize(num_disk + 1, std::vector<int>(tag_disk + 1, 0));

    partition_size.resize(tag_disk + 1, 0);

    disk_partition_start.resize(tag_disk + 1, 0);

    //fre_write.resize(tag_disk + 1, 0);

    //overflow_position.resize(num_disk + 1, static_cast<int>(disk_size * 0.9) + 1);

    tag_max_occupy.resize(tag_disk + 1, 0);

}

// 磁盘分区函数

// 计算每个磁头离可读对象块最近的距离函数
std::vector<std::pair<int, int> > Disk::calculate_distances(int timestamp, Request *request) {
    for (int i = 1; i <= disk_num; i++) {
        int min_distance = INT_MAX;
        int head_position = disk_point[i].position;
        bool found_block = false;

        // 检查当前磁头位置
        if (storage[i][head_position] != nullptr && storage[i][head_position]->last_request_id != 0) {
            if (timestamp - request[storage[i][head_position]->last_request_id].reached_time >= 105) {
                storage[i][head_position]->last_request_id = 0;
            } else {
                distances[i] = 0;
                continue; // 跳出本次循环
            }
        }

        // 检查磁头后面的对象块
        for (int block_pos = head_position + 1; block_pos <= disk_size; block_pos++) {
            if (storage[i][block_pos] != nullptr && storage[i][block_pos]->last_request_id != 0) {
                if (timestamp - request[storage[i][block_pos]->last_request_id].reached_time >= 105) {
                    storage[i][block_pos]->last_request_id = 0;
                } else {
                    min_distance = block_pos - head_position;
                    found_block = true;
                    break;
                }
            }
        }

        // 检查磁头前面的对象块
        if (!found_block) {
            for (int block_pos = 1; block_pos < head_position; block_pos++) {
                if (storage[i][block_pos] != nullptr && storage[i][block_pos]->last_request_id != 0) {
                    if (timestamp - request[storage[i][block_pos]->last_request_id].reached_time >= 105) {
                        storage[i][block_pos]->last_request_id = 0;
                    } else {
                        int distance = (disk_size - head_position) + block_pos;
                        min_distance = distance;
                        break;
                    }
                }
            }
        }

        distances[i] = min_distance;
    }

    // 按照从小到大排序
    std::vector<std::pair<int, int> > valid_distances;
    for (int i = 1; i <= disk_num; i++) {
        valid_distances.emplace_back(distances[i], i);
    }
    std::sort(valid_distances.begin(), valid_distances.end());
    return valid_distances;
}


// 找到磁头最近的对象块
std::pair<int, int> Disk::find_distances(int disk_id, int head_pos, int timestamp, Request *request) {
    // 检查当前磁头位置
    if (storage[disk_id][head_pos] != nullptr && storage[disk_id][head_pos]->last_request_id != 0) {
        if (timestamp - request[storage[disk_id][head_pos]->last_request_id].reached_time >= 105) {
            storage[disk_id][head_pos]->last_request_id = 0;
        } else {
            return {0, head_pos};
        }
    }

    // 检查磁头后面的位置
    for (int i = head_pos + 1; i <= disk_size; i++) {
        if (storage[disk_id][i] != nullptr && storage[disk_id][i]->last_request_id != 0) {
            if (timestamp - request[storage[disk_id][i]->last_request_id].reached_time >= 105) {
                storage[disk_id][i]->last_request_id = 0;
            } else {
                return {i - head_pos, i};
            }
        }
    }

    // 检查磁头前面的位置
    for (int i = 1; i < head_pos; i++) {
        if (storage[disk_id][i] != nullptr && storage[disk_id][i]->last_request_id != 0) {
            if (timestamp - request[storage[disk_id][i]->last_request_id].reached_time >= 105) {
                storage[disk_id][i]->last_request_id = 0;
            } else {
                return {(disk_size - head_pos) + i, i};
            }
        }
    }

    // 没找到
    return {-1, -1};
}


// 计算连续 read 策略要消耗的token函数
std::tuple<int, int, int, bool> Disk::calculate_read_cost(int target_pos, int token_remaining, head_point head, int V) {
    int continuous_read_cost = 0; // 连续阅读消耗令牌数量
    int current_read_cost = (head.last_command == 'r') ? max(16, (head.last_token * 8 + 9) / 10) : 64; // 计算第一步阅读的令牌消耗
    int continuous_steps = 0;
    bool read_target = false;

    // 模拟连续Read（包括无任务块），直到到达目标块位置
    while (token_remaining >= current_read_cost && token_remaining > 0) {
        // 消耗一次 Read 的 token
        token_remaining -= current_read_cost;
        continuous_read_cost += current_read_cost;
        continuous_steps++;

        // 移动到下一个位置
        head.position = (head.position % V) + 1;

        // 读取完当前单元块后的位置
        if (head.position == (target_pos % V) + 1) {
            read_target = true;
            break;
        }

        // 计算下次Read的消耗
        current_read_cost = max(16, (current_read_cost * 8 + 9) / 10);
    }

    // 如果没到达目标，连续read无意义，选择pass+read
    if (!read_target || token_remaining < 0) {
        continuous_read_cost = INT_MAX;
    }

    return {continuous_read_cost, continuous_steps, current_read_cost, read_target};
}


// 删除操作函数
void Disk::delete_object_from_disk(const int *object_unit, int disk_unit, int size, int tag) {
    for (int i = 1; i <= size; i++) {
        storage[disk_unit][object_unit[i]] = nullptr;
    }

    disk_object_count[disk_unit][tag] -= size;
}

// 写入操作函数
void Disk::write_object_to_disk(int* object_unit, int disk_unit, int size, int tag, int V, Block* blocks) {
    int start_position = disk_partition_start[tag];                 //分区起始位置
    int end_position = start_position + partition_size[tag] - 1;       //分区上限
    int current_write_point = 0;

    for (int i = start_position; i <= end_position && current_write_point < size; i++) {
        if (storage[disk_unit][i] == nullptr) {
            int null_block_number = 0;

            for (int j = i; j <= i + size - 1; j++) {
                if (j > V) break;
                if (storage[disk_unit][j] == nullptr) {
                    null_block_number++;
                } else {
                    break;
                }
            }

            if (null_block_number == size) {
                for (int j = i; j <= i + size - 1; j++) {
                    storage[disk_unit][j] = &blocks[++current_write_point];
                    object_unit[current_write_point] = j;
                }
            }

            if (current_write_point == size) break;
        }
    }

    if (current_write_point != size) {
        current_write_point = 0;
        for (int i = start_position; i <= end_position && current_write_point < size; i++) {
            if (storage[disk_unit][i] == nullptr) {
                storage[disk_unit][i] = &blocks[++current_write_point];
                object_unit[current_write_point] = i;
                if (current_write_point == size) break;
            }
        }

        
    }

    /*for (int i = start_position; i <= end_position && current_write_point < size; i++) {
        if (storage[disk_unit][i] == nullptr) {
            storage[disk_unit][i] = &blocks[++current_write_point];
            object_unit[current_write_point] = i;
            
        }
    }*/

    /*if (current_write_point < size) {
        for (int i = overflow_position[disk_unit]; i <= V && current_write_point < size; i++) {
            if (storage[disk_unit][i] == nullptr) {
                storage[disk_unit][i] = &blocks[++current_write_point];
                object_unit[current_write_point] = i;
                overflow_position[disk_unit]++;
            }
        }
    }*/

    if (current_write_point < size) {
        for (int i = 1; i <= V && current_write_point < size; i++) {
            if (storage[disk_unit][i] == nullptr) {
                storage[disk_unit][i] = &blocks[++current_write_point];
                object_unit[current_write_point] = i;
               
            }
        }
        
    }
   
    //assert(current_write_point == size);
}

void Disk::initialize_dynamic_partitions(int N, int M, int V)
{

    int total_write = accumulate(tag_max_occupy.begin(), tag_max_occupy.end(), 0); //计算总写入量
    
    vector<pair<int, double>> error_list;       //存储(tag，误差值)
    int allocated_sum = 0;                      //已分配存储单元总值

    int current_position = 1;       //磁盘存储起始位置
    for (int tag = 1; tag <= disk_tag; tag++) {
        double ideal_partition_size = (double)tag_max_occupy[tag] / total_write * V;        //计算每个标签的理想存储空间

        partition_size[tag] = static_cast<int>(floor(ideal_partition_size));   //计算标签的分区大小
        double error = ideal_partition_size - partition_size[tag];              //计算误差值
        error_list.emplace_back(tag, error);

        allocated_sum += partition_size[tag];                           //记录已分配空间

        disk_partition_start[tag] = current_position;           //分区起始块
        current_position += partition_size[tag];                //更新下一个标签的起始位置
    }

    //计算剩余存储单元
    int remaining_space = V - allocated_sum;


    //按误差值从大到小排序标签索引
    sort(error_list.begin(), error_list.end(), [](const pair<int, double>& a, const pair<int, double>& b) {
        return a.second > b.second;  // 误差值大的优先
        });

    //从误差最大标签开始补充存储
    for (int i = 0; i < disk_tag && remaining_space > 0; i++) {
        int tag = error_list[i].first;
        partition_size[tag] += 1;
        remaining_space--;
    }

    //重新计算 disk_partition_start
    current_position = 1;
    for (int tag = 1; tag <= disk_tag; tag++) {
        disk_partition_start[tag] = current_position;
        current_position += partition_size[tag];
    }


}

void Disk::calculate_max_occupy(int slice_num)
{
    for (int tag = 1; tag <= disk_tag; tag++) {
        int cur_space = 0;
        for (int j = 1; j <= slice_num; j++) {
            cur_space += (tag_write[tag][j] - tag_delete[tag][j]);
            cur_space = max(0, cur_space);
            tag_max_occupy[tag] = max(tag_max_occupy[tag], cur_space);
        }
    }
    
}

// 读取操作函数
