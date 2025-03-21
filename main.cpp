#include "main.h"
#include "Disk.h"

Request request[MAX_REQUEST_NUM];
Object object[MAX_OBJECT_NUM];

int T, M, N, V, G;
// int disk[MAX_DISK_NUM][MAX_DISK_SIZE];
// int disk_point[MAX_DISK_NUM];

int timestamp_action() {
    int timestamp;
    scanf("%*s%d", &timestamp);
    printf("TIMESTAMP %d\n", timestamp);

    fflush(stdout);
    return timestamp;
}


void delete_action(Disk &my_disk) {
    int n_delete; // 代表这一时间片被删除的对象个数
    int abort_num = 0; // 代表这一秒被取消的读取请求的数量
    static int _id[MAX_OBJECT_NUM]; // 要删除的id列表 队列
    // 判题器删除数据输入
    scanf("%d", &n_delete); // 有几个删除的对象
    for (int i = 1; i <= n_delete; i++) {
        scanf("%d", &_id[i]); // 要删除的对象编号
    }


    for (int i = 1; i <= n_delete; i++) {
        int id = _id[i]; // 要删除的id列表
        int current_id = object[id].last_request_point;
        while (current_id != 0) {
            if (request[current_id].is_done == false) {
                abort_num++;
            }
            current_id = request[current_id].prev_id;
        }
    }


    // 删除操作输出 ：读取请求删除
    printf("%d\n", abort_num);

    for (int i = 1; i <= n_delete; i++) {
        int id = _id[i];
        int current_id = object[id].last_request_point;
        while (current_id != 0) {
            if (request[current_id].is_done == false) {
                printf("%d\n", current_id);
            }
            current_id = request[current_id].prev_id;
        }

        // 删除对象的三个副本
        // TODO: 自己Demo中磁盘的写删除代码

        for (int j = 1; j <= REP_NUM; j++) {
            my_disk.delete_object_from_disk(object[id].unit[j], object[id].replica[j], object[id].size, object[id].tag);
        }

        // 标记对象为已经删除
        object[id].is_delete = true;
    }

    fflush(stdout);
}


void write_action(Disk &my_disk) {
    int n_write;
    scanf("%d", &n_write);
    for (int i = 1; i <= n_write; i++) {
        int id, size, tag;
        scanf("%d%d%d", &id, &size, &tag);

        object[id].last_request_point = 0;
        object[id].blocks = new Block[size + 1];
        for (int k = 1; k <= size; k++) {
            object[id].blocks[k].object_id = id;
            object[id].blocks[k].last_request_id = 0;
            object[id].blocks[k].tag = tag;
            object[id].blocks[k].block_id = k;
        }
        object[id].tag = tag;
        object[id].size = size;
        object[id].is_delete = false;


        //找到当前存储该标签最少的硬盘
        int primary_disk = 1;
        int min_count = INT_MAX;
        for (int d = 1; d <= N; d++) {
            
            if (my_disk.disk_object_count[d][tag] < min_count) {
                min_count = my_disk.disk_object_count[d][tag];
                primary_disk = d;
            }
        }

        object[id].replica[1] = primary_disk;
        my_disk.disk_object_count[primary_disk][tag] += size;   //更新存储计数

        //标记数组，防止副本存入相同的硬盘
        vector<bool> used_disks(N + 1, false);
        used_disks[primary_disk] = true;

        //选择副本存储的硬盘
        int replica_1 = 0, replica_2 = 0;
        min_count = INT_MAX;
        for (int d = 1; d <= N; d++) {
            if (!used_disks[d] && my_disk.disk_object_count[d][tag] < min_count) {
                min_count = my_disk.disk_object_count[d][tag];
                replica_1 = d;
            }
        }
        object[id].replica[2] = replica_1;
        used_disks[replica_1] = true;
        my_disk.disk_object_count[replica_1][tag] += size;

        min_count = INT_MAX;
        for (int d = 1; d <= N; d++) {
            if (!used_disks[d] && my_disk.disk_object_count[d][tag] < min_count) {
                min_count = my_disk.disk_object_count[d][tag];
                replica_2 = d;
            }
        }
        object[id].replica[3] = replica_2;
        my_disk.disk_object_count[replica_2][tag] += size;

        //从分区存储对象
        for (int j = 1; j <= REP_NUM; j++) {
            object[id].unit[j] = new int[size + 1];
            // TODO: 自己Demo中磁盘的写入代码
            my_disk.write_object_to_disk(object[id].unit[j], object[id].replica[j], size, tag, V, object[id].blocks);
        }

        printf("%d\n", id);
        for (int j = 1; j <= REP_NUM; j++) {
            printf("%d", object[id].replica[j]);
            for (int k = 1; k <= size; k++) {
                printf(" %d", object[id].unit[j][k]);
            }
            printf("\n");
        }
    }

    fflush(stdout);
}

void read_action(Disk &my_disk, int timestamp) {
    int n_read;
    int request_id, object_id;
    scanf("%d", &n_read);
    for (int i = 1; i <= n_read; i++) {
        scanf("%d%d", &request_id, &object_id);
        request[request_id].object_id = object_id;
        request[request_id].prev_id = object[object_id].last_request_point;
        object[object_id].last_request_point = request_id;
        request[request_id].is_done = false;
        request[request_id].is_submit = false;
        request[request_id].reached_time = timestamp;
        request[request_id].remaining_blocks = object[object_id].size;
        request[request_id].block_status = new int[object[object_id].size + 1]{0};
        for (int j = 1; j <= object[object_id].size; j++) {
            object[object_id].blocks[j].last_request_id = request_id;
        }
    }

    vector<int> completed_requests; // 存储本时间片完成的请求id
    vector<string> actions(N + 1, "#"); // 存储每个磁盘的动作序列，初始为空操作

    // 先计算对象块任务离磁头最近的磁盘
    vector<pair<int, int> > distances_list = my_disk.calculate_distances(timestamp, request);
    // 哪个磁头离任务越近先读哪个
    for (const auto &[distance,disk_id]: distances_list) {
        if (distance == INT_MAX) continue; // 无任务，跳过

        // 初始化参数
        int token_remaining = G; // token数量
        Disk::head_point &head = my_disk.disk_point[disk_id]; // 磁头
        bool jumped = false; // 是否执行"j"
        string action = ""; // 行动

        // TODO 按照贪心策略每次只找最近的对象块 （然后怎么让分数尽可能高？）
        while (token_remaining > 0) {
            // 找最近可读的块
            auto [dist_to_block, target_pos] = my_disk.find_distances(disk_id, head.position, timestamp, request);
            if (dist_to_block == -1) break; // 没有可读块跳出循环

            bool is_read = false; // 是否执行了可读操作
            int pass_cost = dist_to_block; // Pass 需要消耗的 Token
            int read_cost = 0;

            // 对象块距离大于0，说明前一次不是r操作，所以也是64
            read_cost = (dist_to_block > 0 || head.last_command != 'r') ? 64 : max(16, (head.last_token * 8 + 9) / 10);

            // 1. 先判断是否 Jump
            if (dist_to_block > G && action.empty() && token_remaining == G) {
                // TODO Jump 是否要考虑跳到分数最高的还是最近的 ？
                // TODO Jump 而且 jump 会导致副本其他三个一起 jump 尝试优化后效果如何 改成dist_to_block >= G pass同理 如果 pass 过去抵达目标块 则把这个目标块任务留给该磁盘下一个时间片运行（包括JUMP）
                // Pass 超出最大 Token，必须 Jump
                // 不能在非开头 Jump
                action += "j " + to_string(target_pos);
                token_remaining = 0;
                head.position = target_pos;
                head.last_command = 'j';
                head.last_token = G;
                jumped = true;
                break;
            }
#if 0
            // 2. Pass + Read 是否可行 TODO 貌似这个不用判断
            else if (pass_cost + read_cost > G && dist_to_block <= G && action.empty()) {
                // 只能 Pass 过去，但不能 Read
                action += string(pass_cost, 'p');
                token_remaining -= pass_cost;
                head.position = target_pos;
                head.last_command = 'p';
                head.last_token = 1;
                break;
            }
#endif
            // 3. 执行 pass + read 或者连续 read 策略
            else {
                // 计算连续 read 的令牌消耗
                auto [continuous_read_cost, continuous_steps, current_read_cost, read_target] = my_disk.
                        calculate_read_cost(target_pos, token_remaining, head, V);

                // 4. 比较连续 read 和 pass+read 的总消耗，优先选择消耗少的
                if (read_target
                    && continuous_read_cost <= pass_cost + read_cost
                    && continuous_read_cost <= token_remaining
                ) {
                    // 连续 read 策略
                    action += string(continuous_steps, 'r');
                    token_remaining -= continuous_read_cost;
                    head.position = (target_pos % V) + 1;
                    head.last_command = 'r';
                    head.last_token = current_read_cost; // 最后一次Read的消耗
                    is_read = true;
                } else if (continuous_read_cost > pass_cost + read_cost && pass_cost + read_cost <= token_remaining) {
                    //  执行 Pass + Read 策略
                    action += string(pass_cost, 'p') + 'r';
                    token_remaining -= pass_cost + read_cost;
                    head.last_command = 'r';
                    head.last_token = read_cost;
                    head.position = (target_pos % V) + 1;
                    is_read = true;
                } else {
                    // 如果都不满足条件 则尽可能利用token接近对象块 然后跳出循环
                    if (pass_cost + read_cost > token_remaining && pass_cost <= token_remaining) {
                        int steps = min(pass_cost, token_remaining);
                        action += string(steps, 'p');
                        token_remaining -= steps;
                        head.position = (head.position + steps - 1) % V + 1;
                        head.last_command = 'p';
                        head.last_token = 1;
                    }
                    break; // 退出 while 循环，避免陷入死循环
                }
            }

            // 记录已经read的对象块 并上传已完成的请求
            if (is_read) {
                Block *blk = my_disk.storage[disk_id][target_pos];
                if (blk && blk->last_request_id != 0) {
                    int rid = blk->last_request_id;

                    while (rid > 0) {
                        if (request[rid].block_status[blk->block_id] == 0 && request[rid].is_done == false) {
                            request[rid].block_status[blk->block_id] = 1;
                            request[rid].remaining_blocks--;
                        }
                        if (request[rid].remaining_blocks == 0 && request[rid].is_done == false) {
                            completed_requests.push_back(rid);
                            request[rid].is_done = true;
                            request[rid].is_submit = true;
                        }
                        rid = request[rid].prev_id;
                        if (request[rid].is_done == true && request[rid].is_submit == true) break;
                    }
                }
                blk->last_request_id = 0;
            }
        }
        if (jumped) {
            actions[disk_id] = action;
        } else {
            actions[disk_id] = action + '#';
        }
    }

    for (int d = 1; d <= N; d++) {
        cout << actions[d] << "\n";
    }

    int done_count = (int) completed_requests.size();
    cout << done_count << "\n";
    for (int rid: completed_requests) {
        cout << rid << "\n";
    }

    fflush(stdout);
}


void clean() {
    for (auto &req: request) {
        if (req.block_status != nullptr) {
            delete[] req.block_status;
            req.block_status = nullptr;
        }
    }

    for (auto &obj: object) {
        // 释放 blocks 数组
        delete[] obj.blocks;
        obj.blocks = nullptr;

        for (int i = 1; i <= REP_NUM; i++) {
            if (obj.unit[i] == nullptr)
                continue;
            free(obj.unit[i]);
            obj.unit[i] = nullptr;
        }
    }
}

int main() {
    // ----------------全局处理阶段-----------------
    // 判题器数据输入
    scanf("%d%d%d%d%d", &T, &M, &N, &V, &G);
   
    // ----------------预处理阶段策略：预先分区-------------------
    Disk disk(N, V, M);
   

    int slice_num = (T - 1) / FRE_PER_SLICING + 1;
    // 删除操作
    disk.tag_delete.resize(M + 1, std::vector<int>(slice_num + 1, 0));
    for (int i = 1; i <= M; i++) {
        for (int j = 1; j <= (T - 1) / FRE_PER_SLICING + 1; j++) {
            scanf("%d", &disk.tag_delete[i][j]);
        }
    }
    // 写入操作
    disk.tag_write.resize(M + 1, std::vector<int>(slice_num + 1, 0));
    for (int i = 1; i <= M; i++) {
        for (int j = 1; j <= (T - 1) / FRE_PER_SLICING + 1; j++) {
            scanf("%d", &disk.tag_write[i][j]);
        }
        
    }
    // 读取操作
    for (int i = 1; i <= M; i++) {
        for (int j = 1; j <= (T - 1) / FRE_PER_SLICING + 1; j++) {
            scanf("%*d");
        }
    }
    
    disk.calculate_max_occupy(slice_num);
    disk.initialize_dynamic_partitions(N, M, V);
    // -------------------------------------------------------
 
    // 数据预处理阶段完成
    printf("OK\n");
    // 刷新输出缓存区
    fflush(stdout);
#if 0
    // 初始化所有硬盘的磁头初始位置
    for (int i = 1; i <= N; i++) {
        disk_point[i] = 1;
    }
#endif

    // ----------------每个时间片周期-----------------
    for (int t = 1; t <= T + EXTRA_TIME; t++) {
        int timestamp = timestamp_action(); // 时间戳对齐
        delete_action(disk);
        write_action(disk);
        read_action(disk, timestamp);
    }
    clean();

    return 0;
}
