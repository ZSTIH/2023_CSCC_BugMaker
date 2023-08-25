#define _GNU_SOURCE
#include <stdio.h>
#include <dlfcn.h>
#include <spdk/bdev.h>
#include <spdk/bdev_module.h>
#include <spdk/string.h>
#include <stdlib.h>
#include <sys/time.h>
#include "sump_spdk.h"

// TAILQ_HEAD 定义队列头
// TAILQ_ENTRY 队列实体定义
// TAILQ_INIT 初始化队列
// TAILQ_FOREACH 对队列进行遍历操作
// TAILQ_INSERT_BEFORE 在指定元素之前插入元素
// TAILQ_INSERT_TAIL 在队列尾部插入元素
// TAILQ_EMPTY 检查队列是否为空
// TAILQ_REMOVE 从队列中移除元素

/* 总的 ump_bdev 队列 */
struct ump_bdev_manage
{
    TAILQ_HEAD(, ump_bdev)
    ump_bdev_list;
};

/* 多路径聚合后的bdev */
struct ump_bdev
{
    struct spdk_bdev bdev;  // 原本的bdev必须放前面，这样之后调用属性时才可以按照原来的调用方式
    TAILQ_HEAD(, spdk_list_bdev)
    spdk_bdev_list;         // 每个ump_bdev里有一个bdev队列（指向uuid相同）
    TAILQ_ENTRY(ump_bdev)
    tailq;
};

/* spdk_bdev缺少TAILQ成员，无法使用链表，对spdk_bdev封装一层，方便使用链表 */
struct spdk_list_bdev
{
    struct spdk_bdev *bdev;
    TAILQ_ENTRY(spdk_list_bdev)
    tailq;
};


/* iopath队列 */ 
struct ump_bdev_channel
{
    struct nvme_io_path			*current_io_path;           // 填充用
	enum bdev_nvme_multipath_policy		mp_policy;          // 填充用
	enum bdev_nvme_multipath_selector	mp_selector;        // 填充用
	uint32_t				rr_min_io;                      // 填充用
	uint32_t				rr_counter;                     // 填充用
	STAILQ_HEAD(, nvme_io_path)		io_path_list;           // 填充用
	TAILQ_HEAD(retry_io_head, spdk_bdev_io)	retry_io_list;  // 填充用
	struct spdk_poller			*retry_io_poller;           // 填充用
    // 以上是struct nvme_bdev_channel中的内容，由于fail时，ump_channel与 nvme_bdev_channel 重合，为避免ump_bdev_channel的成员被无辜修改，故先填充 nvme_bdev_channel的成员

    TAILQ_HEAD(, ump_bdev_iopath)
    iopath_list;
    TAILQ_ENTRY(ump_bdev_channel) tailq;                
    uint64_t max_id;                                    // iopath_list 中最大的iopath->id(未使用的)
};

/* 保存过去各个io时间 */
struct io_queue
{
    TAILQ_HEAD(, io_queue_ele) time_list;
    unsigned int len;
    uint64_t io_time_all;
    uint64_t io_time_avg;
	uint64_t io_size_all;
	double io_rate_avg;
};
/* io时间 */
struct io_queue_ele
{
    TAILQ_ENTRY(io_queue_ele) tailq;
    uint64_t io_time;
	uint64_t io_size;
};

/* ump_bdev逻辑路径结构 */
struct ump_bdev_iopath
{
    struct spdk_io_channel *io_channel;
    struct spdk_bdev *bdev;
    bool available;                         // 是否可用的标志，用于实现重连
    TAILQ_ENTRY(ump_bdev_iopath)
    tailq;

    bool reconnecting;						// 是否正在重连
	struct spdk_poller *reconnect_poller;
	struct io_queue io_read_queue;
	struct io_queue io_write_queue;
    uint64_t id;
    uint64_t io_incomplete;                 // 未完成的io请求数
	double read_rate;
	double write_rate;
};

/* 参数上下文，用于保留必要变量并传递给回调函数 */
struct ump_bdev_io_completion_ctx
{
    struct spdk_io_channel *ch;
    void *real_caller_ctx;
    spdk_bdev_io_completion_cb real_completion_cb;
    struct ump_bdev_iopath *iopath; 
};
struct ump_failback_ctx
{
    struct ump_bdev_iopath *iopath;     
    struct spdk_bdev_io *bdev_io;                       // 用于发出io请求
};



/* 全局变量，组织所有ump_bdev设备 */
extern struct ump_bdev_manage ump_bdev_manage;
/* 全局变量，保存真正处理设备注册的函数指针 */
extern int (*real_spdk_bdev_register)(struct spdk_bdev *bdev);
/* 全局变量，劫持spdk_nvme_ctrlr_reconnect_poll_async函数，用于实现failback */
extern uint64_t count[16];



/* sump.c */
void __attribute__((constructor)) ump_init(void);
int spdk_bdev_register(struct spdk_bdev *bdev);
int ump_bdev_construct(struct spdk_bdev *bdev);
int ump_bdev_add_bdev(struct ump_bdev *mbdev, struct spdk_bdev *bdev);



/* sump_data.c */
struct spdk_io_channel *ump_bdev_get_io_channel(void *ctx);
void ump_bdev_submit_request(struct spdk_io_channel *ch, struct spdk_bdev_io *bdev_io);
int ump_bdev_destruct(void *ctx);
bool ump_bdev_io_type_supported(void *ctx, enum spdk_bdev_io_type io_type);
int ump_bdev_dump_info_json(void *ctx, struct spdk_json_write_ctx *w);
void ump_bdev_write_config_json(struct spdk_bdev *bdev, struct spdk_json_write_ctx *w);
uint64_t ump_bdev_get_spin_time(struct spdk_io_channel *ch);



/* sump_ctrl.c */
void ump_bdev_io_completion_cb(struct spdk_bdev_io *bdev_io, bool success, void *cb_arg);
struct ump_bdev_iopath *ump_bdev_find_iopath(struct ump_bdev_channel *ump_channel, struct spdk_bdev_io *bdev_io);
// 路径选择算法
struct ump_bdev_iopath *ump_find_iopath_round_robin(struct ump_bdev_channel *ump_channel);
struct ump_bdev_iopath *ump_find_iopath_service_time(struct ump_bdev_channel *ump_channel, struct spdk_bdev_io *bdev_io);
struct ump_bdev_iopath *ump_find_iopath_queue_length(struct ump_bdev_channel *ump_channel);
struct ump_bdev_iopath *ump_find_iopath_random(struct ump_bdev_channel *ump_channel);
struct ump_bdev_iopath *ump_find_iopath_random_weight_static(struct ump_bdev_channel *ump_channel);
struct ump_bdev_iopath *ump_find_iopath_hash(struct ump_bdev_channel *ump_channel, struct spdk_bdev_io *bdev_io);
struct ump_bdev_iopath *ump_find_iopath_rate(struct ump_bdev_channel *ump_channel, struct spdk_bdev_io *bdev_io);
struct ump_bdev_iopath *ump_find_iopath_last_average_rate(struct ump_bdev_channel *ump_channel, struct spdk_bdev_io *bdev_io);
// 动态负载均衡更新模块
void update_iopath(struct ump_bdev_iopath *iopath, struct spdk_bdev_io *bdev_io);
void update_iopath_detail(struct io_queue_ele *time_ele, struct io_queue *q);
int ump_io_count_reset_fn(); // 测试各路径的io次数并重置时延
// io channel 相关
void ump_bdev_channel_clear_all_iopath(struct ump_bdev_channel *ump_channel);
int ump_bdev_channel_create_cb(void *io_device, void *ctx_buf);
void ump_bdev_channel_destroy_cb(void *io_device, void *ctx_buf);
// 故障恢复
void ump_failback(struct ump_bdev_iopath *iopath, struct spdk_bdev_io *bdev_io);
void ump_failback_io_completion_cb(struct spdk_bdev_io *bdev_io, bool success, void *cb_arg);
int ump_failback_io_fn(void *arg1);



/* sump_util.c*/
void sump_printf(const char *fmt, ...);
struct ump_bdev *get_ump_bdev_by_uuid(struct spdk_uuid *uuid);
void ump_io_queue_init(struct io_queue *t_queue);
void ump_iopath_init(struct ump_bdev_iopath *iopath);


/***************************************************************************************
*  spdk中的块设备后端的函数表（spdk_bdev结构体的fn_table成员）提供了一组允许与后端通信的API，
*  为了截取I/O请求，以自己的逻辑实现路径下发，进而实现路径选择和I/O加速等功能。
*  我们实现了自己的函数表umplib_fn_table，并在设备构造时将其赋值给spdk_bdev结构体的fn_table成员。
***************************************************************************************/
static const struct spdk_bdev_fn_table umplib_fn_table = {
    .destruct = ump_bdev_destruct,
    // 重点：提交请求
    .submit_request = ump_bdev_submit_request,
    .io_type_supported = ump_bdev_io_type_supported,
    .get_io_channel = ump_bdev_get_io_channel,
    .dump_info_json = ump_bdev_dump_info_json,
    .write_config_json = ump_bdev_write_config_json,
    .get_spin_time = ump_bdev_get_spin_time,
};