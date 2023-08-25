#! /bin/bash

# 0. 路径设置等
cd /home/bugmaker/Projects/spdk/module/sump/scripts
. kill.sh
kill_all "vhost"

method=queue                                    # -m 负载均衡算法
addr=127.0.0.1                                  # -a 地址,，如10.251.176.136
log_path=""                                     # -l 日志输出路径
fio=""                                          # -q nbd or qemu
subsystem_name="nqn.2016-06.io.spdk:cnode1"     # -n 子系统名字
path_num=4                                      # -p 路径条数
storage_protocol="nvmf"                         # -s 储存协议


# 1. 获取参数
while getopts "m:a:q:l:p:s:h" arg #选项后面的冒号表示该选项需要参数
do
        case $arg in
             a)
                addr=$OPTARG
                ;;
             h)
                echo "-a                The address of the storage target."
                echo "-h                Print this help message."
                echo "-l                The path(relative to sump) of log file.(include file name)"
                echo "-m                The algorithm of load balancing.(time,robin,queue,random,weight,hash,avg_rate)" 
                echo "-n                The name of subsystem." 
                echo "-p                The number of paths." 
                echo "-q                I/O method. (nbd or qemu)"
                echo "-s                Storage Protocol. (nvmf or iscsi)"
                exit
                ;;
             l)
                log_path=$OPTARG
                ;;
             m)
                method=$OPTARG
                ;;
             n)
                subsystem_name=$OPTARG
                ;;
             p)
                path_num=$OPTARG
                ;;
             q)
                fio=$OPTARG
                ;;
             s)
                storage_protocol=$OPTARG
                ;;
             ?)  #当有不认识的选项的时候arg为?
            echo "unkonw argument"
        exit 1
        ;;
        esac
done

# 2. 编译
cd ../

cd ./nvme
make
make install
cd ../

cd ./nvmf
make
make install
cd ../

cd ./iscsi
make
make install
cd ../

make ${method}

# 3. 运行主机端，启动vhost
if [[ ${log_path} = "" ]]
then
    echo "log path is null"
    make run &
else
    make run > ${log_path} &
fi

sleep 1

cd ../../

# 4. 启动 vhost 之后 连接存储端 
if [[ ${storage_protocol} = "iscsi" ]]          # iscsi
then
    for i in $(seq 1 ${path_num})
    do
        ./scripts/rpc.py -s /var/tmp/vhost.sock bdev_iscsi_create -b iSCSI`expr $i - 1` -i iqn.2016-06.io.spdk:init`expr $i - 1` --url iscsi://127.0.0.1:442`expr $i - 1`/iqn.2016-06.io.spdk:disk1/0
        # echo $?
    done
else                                            # 默认为nvmf
    for i in $(seq 1 ${path_num})
    do
        ./scripts/rpc.py -s /var/tmp/vhost.sock bdev_nvme_attach_controller -b ump1_`expr $i - 1` -t tcp -a ${addr} -f ipv4 -s 442`expr $i - 1` -n ${subsystem_name}
        # echo $?
    done
fi

# 5. 挂载
if [[ ${fio} = "qemu" ]]
then
    echo "qemu"
    bdev_output=`./scripts/rpc.py -s /var/tmp/vhost.sock bdev_get_bdevs | grep '"name"' | awk '{print $2}'`
    echo $bdev_output
else                        # 默认为nbd
    sudo modprobe nbd
    bdev_output=`./scripts/rpc.py -s /var/tmp/vhost.sock bdev_get_bdevs | grep '"name"' | awk '{print $2}'`
    i=0
    # echo $bdev_output
    for bdev in $bdev_output
    do
        echo ${bdev:1:-2}
        ./scripts/rpc.py -s /var/tmp/vhost.sock nbd_start_disk ${bdev:1:-2} /dev/nbd$i
        i=`expr $i + 1`
    done
fi



