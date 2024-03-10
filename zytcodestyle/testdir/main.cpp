#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <queue>
#include <string>
#include <thread>

struct task_info {
    std::string id;
    std::string target;
    std::string path;
};

struct pipeline_info {
    std::queue<task_info> task_queue;
    std::condition_variable cv_task_queue;
    std::mutex mtx_task_queue;
    uint8_t result{0U};
    std::mutex mtx_result;
};

static pipeline_info pipeline_soc;
static pipeline_info pipeline_sail;
static pipeline_info pipeline_vip;
static pipeline_info pipeline_switch;
static pipeline_info* ptr_pipelines[]{
    &pipeline_soc,
    &pipeline_sail,
    &pipeline_vip,
    &pipeline_switch,
};
static volatile bool withdraw_mode = false;

class my_time_stamp_printer
{
public:
    my_time_stamp_printer(std::string const& start_tag, std::string const& end_tag)
    {
        _end_tag = end_tag;
        _start_point = std::chrono::system_clock::now();
        std::time_t now_time = std::chrono::system_clock::to_time_t(_start_point);
        std::string log = start_tag + "(" + std::string(std::ctime(&now_time));
        log.erase(log.end() - 1);
        log += ")\n";
        std::cout << log;
    }
    ~my_time_stamp_printer()
    {
        auto later = std::chrono::system_clock::now();
        std::time_t later_time = std::chrono::system_clock::to_time_t(later);
        std::chrono::duration<double> diff = later - _start_point;
        std::string log = _end_tag + "(" + std::string(std::ctime(&later_time));
        log.erase(log.end() - 1);
        log += ") 耗时 " + std::to_string(diff.count()) + " 秒\n";
        std::cout << log;
    }

private:
    std::chrono::system_clock::time_point _start_point;
    std::string _end_tag;
};

static void print_timestamp(const std::string& tag)
{
    auto now = std::chrono::system_clock::now().time_since_epoch();
    auto timestamp = std::chrono::duration_cast<std::chrono::seconds>(now);
    std::time_t current_time = timestamp.count();
    std::tm* time_info = std::localtime(&current_time);
    std::ostringstream oss;
    oss << "[" << std::put_time(time_info, "%Y-%m-%d %H:%M:%S") << "] " << tag;
    std::cout << oss.str() << std::endl;
}

void master_transpond(task_info const& task)
{
    if (task.target == "soc") {
        auto _ = my_time_stamp_printer("开始往soc转发小包" + task.id, "往soc转发小包" + task.id + "成功");
        std::this_thread::sleep_for(std::chrono::seconds(1));
        std::cout << "已将" << task.id << "分别转发给soc1和soc2" << std::endl;
        if ("sail" == task.id) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
            std::cout << "已在soc1将" << task.id << "挂载成功" << std::endl;
            {
                std::unique_lock<std::mutex> lock(pipeline_sail.mtx_task_queue);
                task_info new_task{task.id, "sail", ""};
                pipeline_sail.task_queue.push(new_task);
                std::cout << "已创建往" << new_task.target << "转发" << new_task.id << "小包的任务" << std::endl;
            }
            pipeline_sail.cv_task_queue.notify_all();
        }

        if ("vip" == task.id) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
            std::cout << "已在soc1将" << task.id << "挂载成功" << std::endl;
            {
                std::unique_lock<std::mutex> lock(pipeline_vip.mtx_task_queue);
                task_info new_task{task.id, "vip", ""};
                pipeline_vip.task_queue.push(new_task);
                std::cout << "已创建往" << new_task.target << "转发" << new_task.id << "小包的任务" << std::endl;
            }
            pipeline_vip.cv_task_queue.notify_all();
        }

        if ("rootfs" == task.id) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
            std::cout << "已在soc1将" << task.id << "挂载成功" << std::endl;
            {
                std::unique_lock<std::mutex> lock(pipeline_switch.mtx_task_queue);
                task_info new_task{task.id, "switch", ""};
                pipeline_switch.task_queue.push(new_task);
                std::cout << "已创建往" << new_task.target << "转发" << new_task.id << "小包的任务" << std::endl;
            }
            pipeline_switch.cv_task_queue.notify_all();
        }
        std::cout << "已删除临时存储在" << task.target << "上的小包" << task.id << std::endl;
        {
            std::unique_lock<std::mutex> lock(pipeline_switch.mtx_result);
            pipeline_soc.result = 1U;
        }
    } else if (task.target == "sail") {
        auto _ = my_time_stamp_printer("开始往sail转发小包" + task.id, "往sail转发小包" + task.id + "成功");
        std::this_thread::sleep_for(std::chrono::seconds(5));
        {
            std::unique_lock<std::mutex> lock(pipeline_switch.mtx_result);
            pipeline_sail.result = 1U;
        }
    } else if (task.target == "vip") {
        auto _ = my_time_stamp_printer("开始往vip转发小包" + task.id, "往vip转发小包" + task.id + "成功");
        std::this_thread::sleep_for(std::chrono::seconds(10));
        {
            std::unique_lock<std::mutex> lock(pipeline_switch.mtx_result);
            pipeline_vip.result = 1U;
        }
    } else if (task.target == "switch") {
        auto _ = my_time_stamp_printer("开始往switch转发小包" + task.id, "往switch转发小包" + task.id + "成功");
        std::this_thread::sleep_for(std::chrono::seconds(15));
        {
            std::unique_lock<std::mutex> lock(pipeline_switch.mtx_result);
            pipeline_switch.result = 1U;
        }
    } else {
        std::cout << "未知目标UE，无法转发" << std::endl;
    }
}

// 任务处理函数
void pipeline_func(pipeline_info& pipeline)
{
    while (true) {
        // 收到终止升级的命令时，将任务队列清空，并 continue
        if (withdraw_mode) {
            {
                std::unique_lock<std::mutex> lock(pipeline.mtx_task_queue);
                while (!pipeline.task_queue.empty()) {
                    auto const& task = pipeline.task_queue.front();
                    std::cout << "取消任务: " << task.id << " to " << task.target << std::endl;
                    pipeline.task_queue.pop();
                }
            }
            std::this_thread::sleep_for(std::chrono::seconds(1));
            continue;
        }

        // 从任务队列中取出一个任务
        task_info task{};
        {
            std::unique_lock<std::mutex> lock(pipeline.mtx_task_queue);
            while (pipeline.task_queue.empty()) {
                pipeline.cv_task_queue.wait(lock);
            }
            task = pipeline.task_queue.front();
        }

        // 处理这个任务
        master_transpond(task);

        // 任务完成，将其从队列中删除
        {
            std::unique_lock<std::mutex> lock(pipeline.mtx_task_queue);
            pipeline.task_queue.pop();
        }
    }
}

bool wail_all_finished(uint32_t const timeout_ms)
{
    constexpr uint32_t sleep_interval{200};

    // 获取当前时间
    auto start_time = std::chrono::steady_clock::now();

    for (auto const& p : ptr_pipelines) {
        while (true) {
            {
                std::unique_lock<std::mutex> lock(p->mtx_task_queue);
                if (p->task_queue.empty()) {
                    break;
                }
            }

            auto elapsed_time = std::chrono::steady_clock::now() - start_time;
            auto elapsed_time_ms = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed_time).count();
            if (elapsed_time_ms >= (int64_t)timeout_ms) {
                return false;
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(sleep_interval));
        }
    }

    return true;
}

int main()
{
    // 创建流水线
    for (auto const& p : ptr_pipelines) {
        std::thread pipeline_thread(pipeline_func, std::ref(*p));
        pipeline_thread.detach();
    }

    // 等待输入命令
    std::string input;
    while (true) {
        std::getline(std::cin, input);

        if (input == "start") {
            {
                std::unique_lock<std::mutex> lock_soc(pipeline_soc.mtx_result);
                std::unique_lock<std::mutex> lock_sail(pipeline_sail.mtx_result);
                std::unique_lock<std::mutex> lock_vip(pipeline_vip.mtx_result);
                std::unique_lock<std::mutex> lock_switch(pipeline_switch.mtx_result);
                pipeline_soc.result = 0U;
                pipeline_sail.result = 0U;
                pipeline_vip.result = 0U;
                pipeline_switch.result = 0U;
            }
            print_timestamp("收到开始升级请求，开始停止现有升级任务，并等待流水线运行结束");
            withdraw_mode = true;
            bool all_finished = wail_all_finished(30000);
            withdraw_mode = false;
            if (all_finished) {
                std::cout << "现有升级任务已停止，准备升级成功" << std::endl;
            } else {
                std::cout << "等待现有升级任务停止超时，准备升级失败" << std::endl;
            }
            continue;
        }

        if (input == "transfer") {
            // 模拟从 update app 接收到一些小包
            std::vector<task_info> tasks{
                task_info{"description", "soc", "/tmp/update_temp_file_1"},  //
                task_info{"vip", "soc", "/tmp/update_temp_file_2"},          //
                task_info{"sail", "soc", "/tmp/update_temp_file_3"},         //
                task_info{"rootfs", "soc", "/tmp/update_temp_file_4"},       //
                task_info{"middleware", "soc", "/tmp/update_temp_file_5"},   //
            };
            for (auto const& task : tasks) {
                std::cout << "从 update app 接收到小包: " << task.id << std::endl;
                {
                    // 获取互斥锁
                    std::unique_lock<std::mutex> lock(pipeline_soc.mtx_task_queue);
                    // 生产数据
                    pipeline_soc.task_queue.push(task);
                }
                // 通知消费者开始消费
                pipeline_soc.cv_task_queue.notify_all();
            }
            continue;
        }

        if (input == "query") {
            print_timestamp("收到一致性检查请求，开始等待流水线运行结束");
            bool all_finished = wail_all_finished(30000);
            if (all_finished) {
                std::cout << "流水线运行结束" << std::endl;
                {
                    std::unique_lock<std::mutex> lock_soc(pipeline_soc.mtx_result);
                    std::unique_lock<std::mutex> lock_sail(pipeline_sail.mtx_result);
                    std::unique_lock<std::mutex> lock_vip(pipeline_vip.mtx_result);
                    std::unique_lock<std::mutex> lock_switch(pipeline_switch.mtx_result);
                    if ((1U != pipeline_soc.result) && (1U != pipeline_sail.result) && (1U != pipeline_vip.result) &&
                        (1U != pipeline_switch.result)) {
                        std::cout << "升级失败" << std::endl;
                    } else {
                        std::cout << "升级成功" << std::endl;
                    }
                }

            } else {
                std::cout << "等待流水线运行结束超时，升级失败" << std::endl;
            }
            continue;
        }

        if (input == "") {
            continue;
        }

        std::cout << "unkown cmd" << std::endl;
        continue;
    }

    return 0;
}