#! /bin/bash

# 0. 路径设置等
cd /home/bugmaker/Projects/spdk/module/sump/scripts
. kill.sh
kill_all "iscsi_tgt"
kill_all "nvmf_tgt"

cd ../../../

addr=127.0.0.1                                  # -a 地址,，如10.251.176.136
path_num=4                                      # -p 路径条数
storage_protocol="nvmf"                         # -s 储存协议


# 获取参数
while getopts "a:p:s:h" arg #选项后面的冒号表示该选项需要参数
do
        case $arg in
             a)
                addr=$OPTARG
                ;;
             h)
                echo "-a                The address of the storage target."
                echo "-h                Print this help message."
                echo "-p                The number of paths." 
                echo "-s                Storage Protocol. (nvmf or iscsi)"
                exit
                ;;
             p)
                path_num=$OPTARG
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

sudo scripts/setup.sh

# 一、iscsi
if [[ ${storage_protocol} = "iscsi" ]]
then
   ./build/bin/iscsi_tgt -m 0x1 -r /var/tmp/iscsi.sock &
   sleep 1
   ./scripts/rpc.py -s /var/tmp/iscsi.sock bdev_nvme_attach_controller -b NVMe0 -t PCIe -a 0000:02:00.0
   ./scripts/rpc.py -s /var/tmp/iscsi.sock iscsi_create_initiator_group 1 ANY 127.0.0.1/32
   ./scripts/rpc.py -s /var/tmp/iscsi.sock iscsi_create_portal_group 2 "127.0.0.1:4420 127.0.0.1:4421 127.0.0.1:4422 127.0.0.1:4423"
   ./scripts/rpc.py -s /var/tmp/iscsi.sock iscsi_create_target_node disk1 "Data Disk1" "NVMe0n1:0" 2:1 1024 -d
   exit
fi


# 二、nvmf
# 1. 启动存储端
./build/bin/nvmf_tgt -m 0x1 -r /var/tmp/nvmf.sock &
sleep 1

./scripts/rpc.py -s /var/tmp/nvmf.sock nvmf_create_transport -t tcp

# 2. 创建 NVMe-oF Subsystem 与监听端口
./scripts/rpc.py -s /var/tmp/nvmf.sock nvmf_create_subsystem -a nqn.2016-06.io.spdk:cnode1
for i in $(seq 1 ${path_num})
do
    ./scripts/rpc.py -s /var/tmp/nvmf.sock nvmf_subsystem_add_listener nqn.2016-06.io.spdk:cnode1 -t tcp -a ${addr} -f ipv4 -s 442`expr $i - 1`
done

# 3. 创建 块设备并加入 NVMe-oF Subsystem
./scripts/rpc.py -s /var/tmp/nvmf.sock bdev_nvme_attach_controller -b NVMe0 -t PCIe -a 0000:02:00.0

./scripts/rpc.py -s /var/tmp/nvmf.sock nvmf_subsystem_add_ns nqn.2016-06.io.spdk:cnode1 NVMe0n1

# ./scripts/rpc.py -s /var/tmp/nvmf.sock bdev_malloc_create 512 512 -b Malloc0

# ./scripts/rpc.py -s /var/tmp/nvmf.sock nvmf_subsystem_add_ns nqn.2016-06.io.spdk:cnode1 Malloc0