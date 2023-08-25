#include "sump.h"

/***************************************************************************/
/****************************** fn_table begin *****************************/
/***************************************************************************/

/* 获取 I/O channel */
struct spdk_io_channel *ump_bdev_get_io_channel(void *ctx)
{
    return spdk_get_io_channel(ctx);
}

/********************************************************
* Function name:    ump_bdev_submit_request
* Description:      处理I/O请求（是函数表中的函数，由上层调用）
* Parameter:
*   @ch             spdk_io_channel结构体指针，代表访问I/O设备的线程通道
*   @bdev_io        spdk_bdev_io结构体指针，保存块设备I/O信息
* Return:           无        
**********************************************************/
void ump_bdev_submit_request(struct spdk_io_channel *ch, struct spdk_bdev_io *bdev_io)
{
    struct ump_bdev_channel *ump_channel = NULL;
    struct ump_bdev_iopath *iopath = NULL;
    struct spdk_bdev *bdev = NULL;
    struct ump_bdev *mbdev = NULL;
    struct ump_bdev_io_completion_ctx *ump_completion_ctx = NULL;

    // sump_printf("ump_bdev_submit_request\n");
    
    ump_completion_ctx = calloc(1, sizeof(struct ump_bdev_io_completion_ctx));
    if (ump_completion_ctx == NULL)
    {
        fprintf(stderr, "calloc for ump_completion_ctx failed.\n");
        goto err;
    }

    /* 替换io完成的回调，方便后续实现故障处理等功能 */
    ump_completion_ctx->real_caller_ctx = bdev_io->internal.caller_ctx;
    ump_completion_ctx->real_completion_cb = bdev_io->internal.cb;
    ump_completion_ctx->ch = ch;
    bdev_io->internal.cb = ump_bdev_io_completion_cb;
    bdev_io->internal.caller_ctx = ump_completion_ctx;

    ump_channel = spdk_io_channel_get_ctx(ch);
    // sump_printf("ump_bdev_submit_request: spdk_io_channel_get_ctx\n");
    mbdev = spdk_io_channel_get_io_device(ch);

    /* 寻找I/O路径 */
    iopath = ump_bdev_find_iopath(ump_channel, bdev_io);
    if (iopath == NULL)
    {
        fprintf(stderr, "mbdev(%s) don't has any iopath.\n", mbdev->bdev.name);
        goto err;
    }
    // 统计
    count[iopath->id] += (bdev_io->u.bdev.num_blocks) * spdk_bdev_get_block_size(bdev_io->bdev);

    /* 设置路径 */
    ump_completion_ctx->iopath = iopath;
    bdev = iopath->bdev;
    // sump_printf("before bdev->fn_table->submit_request\n");
    /* 提交I/O请求 */
    void* ctxt_tmp = bdev_io->bdev->ctxt;
    bdev_io->bdev->ctxt = bdev->ctxt;
    bdev->fn_table->submit_request(iopath->io_channel, bdev_io);
    bdev_io->bdev->ctxt = ctxt_tmp;
    // sump_printf("after bdev->fn_table->submit_request\n");

err:
    return;
}

/* 销毁后端块设备对象 */
int ump_bdev_destruct(void *ctx)
{
    sump_printf("ump_bdev_destruct\n");
    return 0;
}

/* 判断是否支持某种I/O类型 */
bool ump_bdev_io_type_supported(void *ctx, enum spdk_bdev_io_type io_type)
{
    struct ump_bdev *mbdev = ctx;
    struct spdk_list_bdev *list_bdev = TAILQ_FIRST(&mbdev->spdk_bdev_list);
    struct spdk_bdev *bdev = list_bdev->bdev;

    return bdev->fn_table->io_type_supported(bdev->ctxt, io_type);
}

/* 将特定驱动程序的信息输出到JSON流（可选） */
int ump_bdev_dump_info_json(void *ctx, struct spdk_json_write_ctx *w)
{
    struct ump_bdev *mbdev = ctx;
    struct spdk_list_bdev *list_bdev = TAILQ_FIRST(&mbdev->spdk_bdev_list);
    struct spdk_bdev *bdev = list_bdev->bdev;

    return bdev->fn_table->dump_info_json(bdev->ctxt, w);
}

/* 将特定bdev的RPC配置输出到JSON流（可选） */
void ump_bdev_write_config_json(struct spdk_bdev *bdev, struct spdk_json_write_ctx *w)
{
    sump_printf("ump_bdev_write_config_json\n");
}

/* 获取每个I/O通道的spin时间（以微秒为单位，可选） */
uint64_t ump_bdev_get_spin_time(struct spdk_io_channel *ch)
{
    sump_printf("ump_bdev_get_spin_time\n");
    return 0;
}

/***************************************************************************/
/******************************* fn_table end ******************************/
/***************************************************************************/