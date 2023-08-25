# kill_all: 根据命令杀死对应的进程
kill_all()
{
    ID=`ps -ef | grep "$1" | grep -v "$0" | grep -v "grep" | awk '{print $2}'`  # -v表示反过滤，awk表示按空格或tab键拆分，{print $2}表示打印第二个（这里对应进程号）
    for id in $ID 
    do  
    sudo kill -9 $id  
    echo "killed $id"  
    done
}

# cd /home/bugmaker/Projects/spdk
# ./scripts/rpc.py -s /var/tmp/vhost.sock nbd_stop_disk /dev/nbd0
# modprobe -r nbd

kill_all "vhost"

kill_all "nvmf_tgt"

kill_all "iscsi_tgt"

