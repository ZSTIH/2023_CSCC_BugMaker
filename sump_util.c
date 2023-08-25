#include "sump.h"


/**************************************************************************/
/*******************************tools begin********************************/
/**************************************************************************/

/********************************************************
* Function name:    sump_printf
* Description:      用于调试时输出打印信息
* Parameter:
*   @fmt            要打印的信息 
* Return:           无        
**********************************************************/
void sump_printf(const char *fmt, ...)
{
#ifdef DEBUG_OUT
    printf("[SUMP DEBUG] ");
    va_list args;
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);
#endif
}

/********************************************************
* Function name:    get_ump_bdev_by_uuid
* Description:      根据传入的uuid，遍历全局变量尾队列ump_bdev_manage，查找相应的ump_bdev设备
* Parameter:
*   @uuid           spdk_uuid结构体指针uuid
* Return:           查找成功则返回相应的ump_bdev结构体指针，否则返回NULL      
**********************************************************/
struct ump_bdev *get_ump_bdev_by_uuid(struct spdk_uuid *uuid)
{
    struct ump_bdev *mbdev = NULL;
    TAILQ_FOREACH(mbdev, &ump_bdev_manage.ump_bdev_list, tailq)
    {
        if (!spdk_uuid_compare(&mbdev->bdev.uuid, uuid))
        {
            return mbdev;
        }
    }
    return NULL;
}

/********************************************************
* Function name:    ump_io_queue_init
* Description:      初始化存储 IO 操作信息的队列
* Parameter:
*   @t_queue        存储 IO 操作信息的队列
* Return:           NULL      
**********************************************************/
void ump_io_queue_init(struct io_queue *t_queue)
{
    TAILQ_INIT(&t_queue->time_list);
    t_queue->len = 0;
    t_queue->io_time_all = 0;
    t_queue->io_time_avg = 0;
    t_queue->io_rate_avg = 10e32;
    t_queue->io_size_all = 0;
}

/********************************************************
* Function name:    ump_iopath_init
* Description:      初始化路径状态
* Parameter:
*   @iopath         要初始化的路径（ump_bdev_iopath结构体指针）
* Return:           NULL      
**********************************************************/
void ump_iopath_init(struct ump_bdev_iopath *iopath)
{
    iopath->available = true;
    iopath->io_incomplete = 0;  // 未完成任务数初始化为0
    iopath->reconnecting = false;
    ump_io_queue_init(&iopath->io_read_queue);         // io时间初始化为最小，确保一开始每一条路径都会被加进去
    ump_io_queue_init(&iopath->io_write_queue);
    iopath->read_rate = 10e32;
    iopath->write_rate = 10e32;
}
/**************************************************************************/
/*******************************tools end********************************/
/**************************************************************************/