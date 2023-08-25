#include "sump.h"
/* 全局变量，保存所有的ump_channel */
extern TAILQ_HEAD(, ump_bdev_channel) g_ump_bdev_channels;
// 添加两个变量，用于统计每条路径的io次数
uint64_t count[16];
struct spdk_poller * io_count_poller;


/********************************************************
* Function name:    ump_bdev_io_completion_cb
* Description:      I/O完成时的回调函数，I/O请求成功时，执行真正的回调函数；失败时进行故障处理，如故障切换等等
* Parameter:
*   @bdev_io        spdk_bdev_io结构体指针，包含本次I/O的信息
*   @success        是否成功的标识
*   @cb_arg         从I/O请求处传递过来的参数
* Return:           无        
**********************************************************/
void ump_bdev_io_completion_cb(struct spdk_bdev_io *bdev_io, bool success, void *cb_arg)
{
    struct ump_bdev_io_completion_ctx *completion_ctx = cb_arg;
    struct ump_bdev_iopath *iopath = completion_ctx->iopath;

    if (success)
    {
        // 负载均衡算法：更新模块
        update_iopath(iopath, bdev_io);                            
        // 调用上层回调函数
        completion_ctx->real_completion_cb(bdev_io, success, completion_ctx->real_caller_ctx);
        free(completion_ctx);
    }
    else
    {
        /* 失败io处理 */
        sump_printf("io complete failed.\n");
        struct spdk_io_channel *ch = completion_ctx->ch;
        struct ump_bdev_iopath *iopath = completion_ctx->iopath;
        struct ump_bdev_channel *ump_channel = spdk_io_channel_get_ctx(ch);
        // 将路径置为不可用
        iopath->available = false;
        // 例询以实现故障恢复
        ump_failback(iopath, bdev_io);

        // 重新请求 failover
        bdev_io->internal.cb = completion_ctx->real_completion_cb;
        bdev_io->internal.caller_ctx = completion_ctx->real_caller_ctx;
        bdev_io->internal.submit_tsc = spdk_get_ticks();            // 重置初始时间戳
        free(completion_ctx);
        ump_bdev_submit_request(ch, bdev_io);
    }
}

/**********************************************************************************/
/****************************** load balancing begin ******************************/
/**********************************************************************************/

/********************************************************
* Function name:    ump_bdev_find_iopath
* Description:      根据指定的负载均衡算法从ump_channel中寻找一条I/O路径
* Parameter:
*   @ump_channel    ump_bedv_channel结构体指针，保存了I/O路径队列
* Return:           若找到路径，则返回ump_bdev_iopath结构体指针，否则返回空指针        
**********************************************************/
struct ump_bdev_iopath *ump_bdev_find_iopath(struct ump_bdev_channel *ump_channel, struct spdk_bdev_io *bdev_io)
{
    if (TAILQ_EMPTY(&ump_channel->iopath_list))
    {
        sump_printf("TAILQ_EMPTY\n");
        return NULL;
    }

    struct ump_bdev_iopath *iopath;
#ifdef TIME
    iopath = ump_find_iopath_service_time(ump_channel, bdev_io);
#elif ROBIN
    iopath = ump_find_iopath_round_robin(ump_channel);
#elif QUEUE
    iopath = ump_find_iopath_queue_length(ump_channel);
#elif RANDOM
    iopath = ump_find_iopath_random(ump_channel);
#elif WEIGHT
    iopath = ump_find_iopath_random_weight_static(ump_channel);
#elif HASH
    iopath = ump_find_iopath_hash(ump_channel, bdev_io);
#elif RATE
    iopath = ump_find_iopath_rate(ump_channel, bdev_io);
#elif AVG_RATE
    iopath = ump_find_iopath_last_average_rate(ump_channel, bdev_io);
#else
    iopath = ump_find_iopath_queue_length(ump_channel);
#endif
    sump_printf("%s's IO channel is chosen\n", iopath->bdev->name);
    return iopath;
}

/********************************************************
* Function name:    ump_find_iopath_round_robin
* Description:      按照设定的顺序选择路径并返回
* Parameter:
*   @ump_channel    ump_bedv_channel结构体指针，保存了I/O路径队列
* Return:           若找到路径，则返回ump_bdev_iopath结构体指针，否则返回空指针        
**********************************************************/
struct ump_bdev_iopath *ump_find_iopath_round_robin(struct ump_bdev_channel *ump_channel)
{
    static int turn = 0;
    struct ump_bdev_iopath *iopath;

    int idx = 0;
    TAILQ_FOREACH(iopath, &ump_channel->iopath_list, tailq)
    {
        if (iopath->available && idx == turn)
        {
            turn++;
            return iopath;
        }
        else if (!(iopath->available) && idx == turn)
        {
            turn++;
            idx++;
        }
        else
        {
            idx++;
        }
    }
    turn = 1;
    TAILQ_FOREACH(iopath, &ump_channel->iopath_list, tailq)
    {
        if(!iopath->available)
            turn++;
        else
        {
            return iopath;
        }
    }
    return NULL;
}

/********************************************************
* Function name:    ump_find_iopath_service_time
* Description:      根据读写类型，选择平均IO时延最小的路径并返回
* Parameter:
*   @ump_channel    ump_bedv_channel结构体指针，保存了I/O路径队列
*   @bdev_io        spdk_bdev_io结构体指针，保存了I/O操作的相关消息
* Return:           若找到路径，则返回ump_bdev_iopath结构体指针，否则返回空指针        
**********************************************************/
struct ump_bdev_iopath *ump_find_iopath_service_time(struct ump_bdev_channel *ump_channel, struct spdk_bdev_io *bdev_io)
{
    
    uint64_t min_time = UINT64_MAX;             // 初始化为最大
    struct ump_bdev_iopath *iopath, *iopath_chosen;
    if (bdev_io->type == SPDK_BDEV_IO_TYPE_READ)
    {
        TAILQ_FOREACH(iopath, &ump_channel->iopath_list, tailq)
        {
            // sump_printf("%s's delay %ld\n", iopath->bdev->name, iopath->io_read_queue.io_time_avg);
            if (iopath->available && iopath->io_read_queue.io_time_avg < min_time)
            {
                min_time = iopath->io_read_queue.io_time_avg;
                iopath_chosen = iopath;
            }
        }
    }
    else
    {
        TAILQ_FOREACH(iopath, &ump_channel->iopath_list, tailq)
        {
            if (iopath->available && iopath->io_write_queue.io_time_avg < min_time)
            {
                min_time = iopath->io_write_queue.io_time_avg;
                iopath_chosen = iopath;
            }
        }
    }
    return iopath_chosen;
}

/********************************************************
* Function name:    ump_find_iopath_last_average_rate
* Description:      根据读写类型，选择平均IO速度最大的路径并返回
* Parameter:
*   @ump_channel    ump_bedv_channel结构体指针，保存了I/O路径队列
*   @bdev_io        spdk_bdev_io结构体指针，保存了I/O操作的相关消息
* Return:           若找到路径，则返回ump_bdev_iopath结构体指针，否则返回空指针        
**********************************************************/
struct ump_bdev_iopath *ump_find_iopath_last_average_rate(struct ump_bdev_channel *ump_channel, struct spdk_bdev_io *bdev_io)
{
    double max_rate = 0;             // 初始化为最小
    struct ump_bdev_iopath *iopath, *iopath_chosen;

    iopath_chosen = TAILQ_FIRST(&ump_channel->iopath_list);

    if (bdev_io->type == SPDK_BDEV_IO_TYPE_READ)
    {
        max_rate = iopath_chosen->io_read_queue.io_rate_avg;
        TAILQ_FOREACH(iopath, &ump_channel->iopath_list, tailq)
        {
            // sump_printf("%s's read rate %lf\n", iopath->bdev->name, iopath->io_read_queue.io_rate_avg);
            if (iopath->available && iopath->io_read_queue.io_rate_avg > max_rate)
            {
                max_rate = iopath->io_read_queue.io_rate_avg;
                iopath_chosen = iopath;
            }
        }
    }
    else
    {
        max_rate = iopath_chosen->io_write_queue.io_rate_avg;
        TAILQ_FOREACH(iopath, &ump_channel->iopath_list, tailq)
        {
            // sump_printf("%s's write rate %lf\n", iopath->bdev->name, iopath->io_write_queue.io_rate_avg);
            if (iopath->available && iopath->io_write_queue.io_rate_avg > max_rate)
            {
                max_rate = iopath->io_write_queue.io_rate_avg;
                iopath_chosen = iopath;
            }
        }
    }
    return iopath_chosen;
}

/********************************************************
* Function name:    ump_find_iopath_queue_length
* Description:      根据读写类型，选择未完成任务数最低的路径并返回
* Parameter:
*   @ump_channel    ump_bedv_channel结构体指针，保存了I/O路径队列
* Return:           若找到路径，则返回ump_bdev_iopath结构体指针，否则返回空指针        
**********************************************************/
struct ump_bdev_iopath *ump_find_iopath_queue_length(struct ump_bdev_channel *ump_channel)
{
    uint64_t min_io_count = UINT64_MAX;             // 最小未完成请求数 初始化为最大
    struct ump_bdev_iopath *iopath, *iopath_chosen;
    TAILQ_FOREACH(iopath, &ump_channel->iopath_list, tailq)
    {
        // sump_printf("%s's IO channel is visited, incomplete task:%ld\n", iopath->bdev->name, iopath->io_incomplete);
        if (iopath->available && iopath->io_incomplete < min_io_count)
        {
            min_io_count = iopath->io_incomplete;
            iopath_chosen = iopath;
        }
    }
    iopath_chosen->io_incomplete++;
    return iopath_chosen;
}

/********************************************************
* Function name:    ump_find_iopath_queue_random
* Description:      随机选择一条路径并返回
* Parameter:
*   @ump_channel    ump_bedv_channel结构体指针，保存了I/O路径队列
* Return:           若找到路径，则返回ump_bdev_iopath结构体指针，否则返回空指针        
**********************************************************/
struct ump_bdev_iopath *ump_find_iopath_random(struct ump_bdev_channel *ump_channel)
{
    struct ump_bdev_iopath *iopath;
    int turn, count;
    while (1)
    {
        // 随机选择一个
        struct timeval start;
        gettimeofday( &start, NULL );
        srand((unsigned)(1000000*start.tv_sec + start.tv_usec));    // 精确到毫秒的随机种子
        turn = rand() % ump_channel->max_id;

        count = 0;

        TAILQ_FOREACH(iopath, &ump_channel->iopath_list, tailq)
        {
            if (!iopath->available)
                count++;
            if (iopath->id == turn)
            {
                if (iopath->available)
                    return iopath;
                else
                {
                    if (count == ump_channel->max_id)   // 所有路径均不可用
                        return NULL;
                    break;
                }
            }

        }
    }

}

/********************************************************
* Function name:    ump_find_iopath_random_weight_static
* Description:      在随机法的基础之上，为每条路径分配一个权重。权重越高，这条路径被选中的概率越大
* Parameter:
*   @ump_channel    ump_bedv_channel结构体指针，保存了I/O路径队列
* Return:           若找到路径，则返回ump_bdev_iopath结构体指针，否则返回空指针        
**********************************************************/
struct ump_bdev_iopath *ump_find_iopath_random_weight_static(struct ump_bdev_channel *ump_channel)
{
    struct ump_bdev_iopath *iopath;
    int factor1, chose_id, i;
    float factor2, *p;
    
    // 权重，暂时设定为4条路径
    float w[4] = {0.1, 0.1, 0.4, 0.4};

    // 累积和
    float s[4];
    s[0] = w[0];
    for (i = 1; i < 4; i++)
        s[i] = s[i - 1] + w[i];

    while (1)
    {
        // 随机选择一个
        struct timeval start;
        gettimeofday( &start, NULL );
        srand((unsigned)(1000000*start.tv_sec + start.tv_usec));    // 精确到毫秒的随机种子
        factor1 = (4 * ump_channel->max_id);
        factor2 = (float)(rand() % factor1);

        factor2 = factor2 / factor1;

        p = s;
        for (i = 0 ;; p++, i++)
        {
            if(factor2 < *p)
            {
                chose_id = i;
                break;
            }
        }

        TAILQ_FOREACH(iopath, &ump_channel->iopath_list, tailq)
        {
            if (iopath->available && chose_id == iopath->id)
                return iopath;
        }
    }
}

/********************************************************
* Function name:    ump_find_iopath_hash
* Description:      对 IO 请求的状态信息进行哈希运算，再与路径列表大小进行取模运算，获得要进行 IO 下发的路径
* Parameter:
*   @ump_channel    ump_bedv_channel结构体指针，保存了I/O路径队列
*   @bdev_io        spdk_bdev_io结构体指针，保存了I/O操作的相关消息
* Return:           若找到路径，则返回ump_bdev_iopath结构体指针，否则返回空指针        
**********************************************************/
struct ump_bdev_iopath *ump_find_iopath_hash(struct ump_bdev_channel *ump_channel, struct spdk_bdev_io *bdev_io)
{
    struct ump_bdev_iopath *iopath;

    // hash
    unsigned int chose_id = (bdev_io->u.bdev.offset_blocks / bdev_io->u.bdev.num_blocks) % ump_channel->max_id;
    TAILQ_FOREACH(iopath, &ump_channel->iopath_list, tailq)
    {
        if (iopath->available && chose_id == iopath->id)
            return iopath;
    }
    // 若刚好选中那个是不可用的，则选择第一个可用的
    TAILQ_FOREACH(iopath, &ump_channel->iopath_list, tailq)
    {
        if(iopath->available)
            return iopath;
    }
    return NULL;
}

/********************************************************
* Function name:    ump_find_iopath_rate
* Description:      根据读写类型，选择IO速度最大的路径并返回
* Parameter:
*   @ump_channel    ump_bedv_channel结构体指针，保存了I/O路径队列
*   @bdev_io        spdk_bdev_io结构体指针，保存了I/O操作的相关消息
* Return:           若找到路径，则返回ump_bdev_iopath结构体指针，否则返回空指针        
**********************************************************/
struct ump_bdev_iopath *ump_find_iopath_rate(struct ump_bdev_channel *ump_channel, struct spdk_bdev_io *bdev_io)
{
    uint64_t max_rate = 0;             // 初始化为最小
    struct ump_bdev_iopath *iopath, *iopath_chosen;
    iopath_chosen = TAILQ_FIRST(&ump_channel->iopath_list);
    if (bdev_io->type == SPDK_BDEV_IO_TYPE_READ)
    {
        max_rate = iopath_chosen->read_rate;
        TAILQ_FOREACH(iopath, &ump_channel->iopath_list, tailq)
        {
            sump_printf("%s's read rate %ld\n", iopath->bdev->name, iopath->read_rate);
            if (iopath->available && iopath->read_rate > max_rate)
            {
                max_rate = iopath->read_rate;
                iopath_chosen = iopath;
            }
        }
    }
    else
    {
        max_rate = iopath_chosen->write_rate;
        TAILQ_FOREACH(iopath, &ump_channel->iopath_list, tailq)
        {
            sump_printf("%s's write rate %ld\n", iopath->bdev->name, iopath->write_rate);
            if (iopath->available && iopath->write_rate > max_rate)
            {
                max_rate = iopath->write_rate;
                iopath_chosen = iopath;
            }
        }
    }
    return iopath_chosen;
}

/********************************************************
* Function name:    update_iopath
* Description:      动态负载均衡算法的路径状态更新模块，用于更新路径状态信息
* Parameter:
*   @iopath         ump_bdev_iopath结构体指针，即要更新的路径
*   @bdev_io        spdk_bdev_io结构体指针，保存了I/O操作的相关消息
* Return:           NULL     
**********************************************************/
void update_iopath(struct ump_bdev_iopath *iopath, struct spdk_bdev_io *bdev_io)
{

    iopath->io_incomplete--;                                                    // 负载均衡算法：queue-length

    struct io_queue_ele *time_ele = calloc(1, sizeof(struct io_queue_ele));
    if (time_ele == NULL)
    {
        fprintf(stderr, "calloc for io_queue_ele failed\n");
        return;
    }
    time_ele->io_time = spdk_get_ticks() - bdev_io->internal.submit_tsc;                        // 时延（完成时间）
    time_ele->io_size = (bdev_io->u.bdev.num_blocks) * spdk_bdev_get_block_size(bdev_io->bdev); // 读写大小
    double tmp;

    if (bdev_io->type == SPDK_BDEV_IO_TYPE_READ)
    {
        update_iopath_detail(time_ele, &iopath->io_read_queue);                                 // 更新读队列

        tmp = (double)time_ele->io_size / (double)time_ele->io_time;
        iopath->read_rate = tmp * spdk_get_ticks_hz();
    }
    else
    {
        update_iopath_detail(time_ele, &iopath->io_write_queue);                                // 更新写队列

        tmp = (double)time_ele->io_size / (double)time_ele->io_time;
        iopath->write_rate = tmp * spdk_get_ticks_hz();
    }
}

/********************************************************
* Function name:    update_iopath_detail
* Description:      动态负载均衡算法的路径状态更新模块的辅助函数，用于更新 IO 操作队列（记录IO操作信息）
* Parameter:
*   @time_ele       io_queue_ele结构体指针，记录了本次 IO 操作的相关信息
*   @t_queue        io_queue结构体指针，即要更新的队列（保存了过去一段时间的 IO 操作信息）
* Return:           NULL     
**********************************************************/
void update_iopath_detail(struct io_queue_ele *time_ele, struct io_queue *t_queue)
{
    double tmp;

    TAILQ_INSERT_TAIL(&t_queue->time_list, time_ele, tailq);
    t_queue->io_time_all += time_ele->io_time;
    t_queue->io_size_all += time_ele->io_size;

    if (t_queue->len < 20) // 记录20次io的时延
    {
        t_queue->len++;
        t_queue->io_time_avg = t_queue->io_time_all / t_queue->len;                 // 平均时延
        tmp = (double)t_queue->io_size_all / (double)t_queue->io_time_all;          // 平均速度
        t_queue->io_rate_avg = tmp * spdk_get_ticks_hz();
        return;
    }
    struct io_queue_ele *tqh_first = t_queue->time_list.tqh_first;
    TAILQ_REMOVE(&t_queue->time_list, tqh_first, tailq);
    t_queue->io_time_all -= tqh_first->io_time;
    t_queue->io_size_all -= tqh_first->io_size;
    free(tqh_first);
    t_queue->io_time_avg = t_queue->io_time_all / t_queue->len;                 // 平均时延
    tmp = (double)t_queue->io_size_all / (double)t_queue->io_time_all;          // 平均速度
    t_queue->io_rate_avg = tmp * spdk_get_ticks_hz();
}

/**********************************************************************************/
/******************************* load balancing end *******************************/
/**********************************************************************************/



/*************************************************************************/
/***************************io channel begin*****************************/
/*************************************************************************/

/********************************************************
* Function name:    ump_bdev_channel_clear_all_iopath
* Description:      释放所有I/O channel
* Parameter:
*   @ump_channel    ump_bedv_channel结构体指针，保存了I/O路径队列
* Return:           无        
**********************************************************/
void ump_bdev_channel_clear_all_iopath(struct ump_bdev_channel *ump_channel)
{
    struct ump_bdev_iopath *iopath = NULL;

    while (!TAILQ_EMPTY(&ump_channel->iopath_list))
    {
        iopath = TAILQ_FIRST(&ump_channel->iopath_list);
        spdk_put_io_channel(iopath->io_channel);
        TAILQ_REMOVE(&ump_channel->iopath_list, iopath, tailq);
        sump_printf("%s's io channel is removed\n", iopath->bdev->name);
        free(iopath);
    }
}

/********************************************************
* Function name:    ump_bdev_channel_create_cb
* Description:      创建完channel的回调函数（在spdk_io_device_register时指定，get_io_channel时若没有channel会调用） 通过回调函数来分配新I/O通道所需的任何资源 （这里主要是更新iopath）
* Parameter:
*   @io_device      ump_bedv结构体指针
*   @ctx_buf        ump_bdev_channel结构体指针
* Return:           0表示成功，-1表示失败        
**********************************************************/
int ump_bdev_channel_create_cb(void *io_device, void *ctx_buf)
{
    struct ump_bdev *mbdev = io_device; // io_device和mbdev的第一个参数为bdev
    struct ump_bdev_channel *ump_channel = ctx_buf;
    struct spdk_list_bdev *list_bdev = NULL;
    struct spdk_bdev *bdev = NULL;
    struct spdk_io_channel *io_channel = NULL;
    struct ump_bdev_iopath *iopath = NULL;

    TAILQ_INIT(&ump_channel->iopath_list); // 当前mbdev在当前线程的各条路径
    TAILQ_INSERT_TAIL(&g_ump_bdev_channels, ump_channel, tailq);
    ump_channel->max_id = 0;           

    // 创建poller用于统计
    io_count_poller = spdk_poller_register(ump_io_count_reset_fn, NULL, 300000); // 轮询的时间单位是微秒

    // 遍历mbdev的所有bdev
    TAILQ_FOREACH(list_bdev, &mbdev->spdk_bdev_list, tailq)
    {
        bdev = list_bdev->bdev;
        io_channel = bdev->fn_table->get_io_channel(bdev->ctxt); 
        if (io_channel == NULL)
        {
            fprintf(stderr, "ump bdev channel create get iopath channel failed.\n");
            goto err;
        }

        iopath = calloc(1, sizeof(struct ump_bdev_iopath));
        if (iopath == NULL)
        {
            fprintf(stderr, "calloc for iopath failed\n");
            spdk_put_io_channel(io_channel);
            goto err;
        }

        iopath->io_channel = io_channel;
        iopath->bdev = bdev;
        iopath->id = ump_channel->max_id++;
        // 初始化iopath
        ump_iopath_init(iopath);
        count[iopath->id] = 0;
        TAILQ_INSERT_TAIL(&ump_channel->iopath_list, iopath, tailq);
        sump_printf("%s's io channel is added\n", bdev->name);
    }

    return 0;

err:
    ump_bdev_channel_clear_all_iopath(ump_channel);
    return -1;
}

/********************************************************
* Function name:    ump_bdev_channel_destroy_cb
* Description:      释放 I/O channel 时的回调函数，在设备注册（spdk_io_device_register）时指定
* Parameter:
*   @io_device      指向I/O设备的指针
*   @ctx_buf        传递过来的参数（一个ump_bdev_channel指针）
* Return:           无        
**********************************************************/
void ump_bdev_channel_destroy_cb(void *io_device, void *ctx_buf)
{
    sump_printf("ump_bdev_channel_destroy_cb\n");
    struct ump_bdev_channel *ump_channel = ctx_buf;
    TAILQ_REMOVE(&g_ump_bdev_channels, ump_channel, tailq);
    ump_bdev_channel_clear_all_iopath(ump_channel);
}

/*************************************************************************/
/******************************io channel end*****************************/
/*************************************************************************/


/*************************************************************************/
/******************************failback begin*****************************/
/*************************************************************************/

void ump_failback(struct ump_bdev_iopath *iopath, struct spdk_bdev_io *bdev_io)
{
    if (iopath->reconnecting)
        return;
    iopath->reconnecting = true;

    sump_printf("ump_failback! bdev name:%s\n", iopath->bdev->name);
    struct ump_failback_ctx *ump_failback_ctx = malloc(sizeof(struct ump_failback_ctx));

    // 参数设置
    ump_failback_ctx->bdev_io = malloc(sizeof(struct spdk_bdev_io));
    memcpy(ump_failback_ctx->bdev_io, bdev_io, sizeof(struct spdk_bdev_io));
    ump_failback_ctx->iopath = iopath;
    // 注册轮询函数ump_failback_io_fn
    iopath->reconnect_poller = spdk_poller_register_named(ump_failback_io_fn, (void *)ump_failback_ctx, 1000000, "failback"); // 每1000000微秒试一下是否已经重连
    return;
}

int ump_failback_io_fn(void *arg1)
{
    struct ump_failback_ctx *ump_failback_ctx = arg1;
    struct ump_bdev_iopath *iopath = ump_failback_ctx->iopath;
    struct spdk_bdev_io *bdev_io = ump_failback_ctx->bdev_io;

    sump_printf("ump_failback_io_fn! bdev name:%s\n", iopath->bdev->name);
    // 设置轮询IO的回调函数和参数
    bdev_io->internal.cb = ump_failback_io_completion_cb;
    bdev_io->internal.caller_ctx = ump_failback_ctx;

    /* 提交I/O请求 */
    iopath->bdev->fn_table->submit_request(iopath->io_channel, bdev_io);

    return 0;
}

void ump_failback_io_completion_cb(struct spdk_bdev_io *bdev_io, bool success, void *cb_arg)
{
    struct ump_failback_ctx *ump_failback_ctx = cb_arg;
    if (success)
    {
        sump_printf("io reconnect success.\n");
        // 轮询成功则重新初始化路径
        ump_iopath_init(ump_failback_ctx->iopath);

        struct spdk_thread *thread = spdk_get_thread();
        struct spdk_poller *poller;
        // 终止轮询
        spdk_poller_pause(ump_failback_ctx->iopath->reconnect_poller);

        free(bdev_io);
        free(ump_failback_ctx);
    }
    return;
}

/*************************************************************************/
/****************************** failback end *****************************/
/*************************************************************************/



/********************************************************
* Function name:    ump_io_count_reset_fn
* Description:      SUMP 注册的poller函数，用于输出各条路径的 IO 信息；以及用于负载均衡算法中的定期重置模块
* Parameter:        无
* Return:           0表示成功，-1表示失败        
**********************************************************/
int ump_io_count_reset_fn()
{
    // static uint64_t pre = 0;
    // uint64_t now = spdk_get_ticks();
    // // printf("poller time：%lf\n", ((double)(now - pre) / spdk_get_ticks_hz()));
    // pre = now;
    // for (int i = 0; i < 4; i++)
    // {
    //     printf("path %d %ld KB\n", i, count[i]/1024);
    // }

    // 定时重置路径状态，防止某些路径一直不被使用
    struct ump_bdev_iopath *iopath;
    struct ump_bdev_channel *ch;
    TAILQ_FOREACH(ch, &g_ump_bdev_channels, tailq)
    {
        TAILQ_FOREACH(iopath, &ch->iopath_list, tailq)
        {
            ump_io_queue_init(&iopath->io_read_queue);
            ump_io_queue_init(&iopath->io_write_queue);
        }
    }
}

