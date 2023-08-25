# 下载项目
git clone https://github.com/spdk/spdk --recursive
sudo scripts/pkgdep.sh --all
sudo ./configure --with-shared
sudo make
sudo scripts/setup.sh

cd module
git clone https://gitlab.eduxiji.net/202318123111333/project1466467-176092.git
mv project1466467-176092 sump

# 路径聚合
sudo module/sump/scripts/target.sh
sudo ./scripts/rpc.py -s /var/tmp/nvmf.sock nvmf_get_subsystems
cd module/sump; sudo make run
sudo ./scripts/rpc.py -s /var/tmp/vhost.sock bdev_nvme_attach_controller -b Nvme0 -t tcp -a 127.0.0.1 -f ipv4 -s 4420 -n "nqn.2016-06.io.spdk:cnode1"
sudo ./scripts/rpc.py -s /var/tmp/vhost.sock bdev_nvme_attach_controller -b Nvme1 -t tcp -a 127.0.0.1 -f ipv4 -s 4421 -n "nqn.2016-06.io.spdk:cnode1"
sudo ./scripts/rpc.py -s /var/tmp/vhost.sock bdev_nvme_attach_controller -b Nvme2 -t tcp -a 127.0.0.1 -f ipv4 -s 4422 -n "nqn.2016-06.io.spdk:cnode1"
sudo ./scripts/rpc.py -s /var/tmp/vhost.sock bdev_nvme_attach_controller -b Nvme3 -t tcp -a 127.0.0.1 -f ipv4 -s 4423 -n "nqn.2016-06.io.spdk:cnode1"
sudo ./scripts/rpc.py -s /var/tmp/vhost.sock bdev_get_bdevs

sudo ./scripts/rpc.py -s /var/tmp/nvmf.sock bdev_aio_create /dev/sda aio0 1024
sudo ./scripts/rpc.py -s /var/tmp/nvmf.sock nvmf_subsystem_add_ns nqn.2016-06.io.spdk:cnode1 aio0
sudo ./scripts/rpc.py -s /var/tmp/vhost.sock bdev_get_bdevs

sudo build/bin/vhost -m 0x2 -r /var/tmp/vhost.sock
sudo ./scripts/rpc.py -s /var/tmp/vhost.sock bdev_nvme_attach_controller -b Nvme0 -t tcp -a 127.0.0.1 -f ipv4 -s 4420 -n "nqn.2016-06.io.spdk:cnode1"
sudo ./scripts/rpc.py -s /var/tmp/vhost.sock bdev_nvme_attach_controller -b Nvme1 -t tcp -a 127.0.0.1 -f ipv4 -s 4421 -n "nqn.2016-06.io.spdk:cnode1"


# 单路径多路径速度对比
sudo module/sump/scripts/port_limit.sh 10 10 10 10
sudo module/sump/scripts/vhost.sh
sudo fio -ioengine=libaio -bs=4k -direct=1 -thread -rw=randrw -numjobs=1 -runtime=15 -time_based=1 -filename=/dev/nbd0 -name="fio test" -iodepth=128
sudo module/sump/scripts/stop.sh
sudo module/sump/scripts/target.sh
sudo build/bin/vhost -m 0x2 -r /var/tmp/vhost.sock
sudo ./scripts/rpc.py -s /var/tmp/vhost.sock bdev_nvme_attach_controller -b Nvme0 -t tcp -a 127.0.0.1 -f ipv4 -s 4420 -n "nqn.2016-06.io.spdk:cnode1"
sudo ./scripts/rpc.py -s /var/tmp/vhost.sock nbd_start_disk Nvme0n1 /dev/nbd0
sudo fio -ioengine=libaio -bs=4k -direct=1 -thread -rw=randrw -numjobs=1 -runtime=15 -time_based=1 -filename=/dev/nbd0 -name="fio test" -iodepth=128


# 故障处理
### 故障处理展示
sudo module/sump/scripts/vhost.sh
sudo fio -ioengine=libaio -bs=4k -direct=1 -thread -rw=randrw -numjobs=1 -runtime=15 -time_based=1 -filename=/dev/nbd0 -name="fio test" -iodepth=64`
sudo ./scripts/rpc.py -s /var/tmp/nvmf.sock nvmf_subsystem_remove_listener nqn.2016-06.io.spdk:cnode1 -t tcp -a 127.0.0.1 -f ipv4 -s 4421
sudo ./scripts/rpc.py -s /var/tmp/nvmf.sock nvmf_subsystem_add_listener nqn.2016-06.io.spdk:cnode1 -t tcp -a 127.0.0.1 -f ipv4 -s 4421

sudo ./scripts/rpc.py -s /var/tmp/vhost.sock bdev_nvme_attach_controller -b Nvme0 -t tcp -a 127.0.0.1 -f ipv4 -s 4420 -n "nqn.2016-06.io.spdk:cnode1"
sudo ./scripts/rpc.py -s /var/tmp/vhost.sock nbd_start_disk Nvme0n1 /dev/nbd0
sudo build/bin/vhost -m 0x2 -r /var/tmp/vhost.sock
sudo fio -ioengine=libaio -bs=4k -direct=1 -thread -rw=randrw -numjobs=1 -runtime=15 -time_based=1 -filename=/dev/nbd0 -name="fio test" -iodepth=64
sudo ./scripts/rpc.py -s /var/tmp/nvmf.sock nvmf_subsystem_remove_listener nqn.2016-06.io.spdk:cnode1 -t tcp -a 127.0.0.1 -f ipv4 -s 4420
sudo ./scripts/rpc.py -s /var/tmp/nvmf.sock nvmf_subsystem_add_listener nqn.2016-06.io.spdk:cnode1 -t tcp -a 127.0.0.1 -f ipv4 -s 4420

# 负载均衡
### 负载均衡展示
sudo module/sump/scripts/port_limit.sh 1 1 100 100
sudo module/sump/scripts/vhost.sh -m robin
sudo fio -ioengine=libaio -bs=4k -direct=1 -thread -rw=randrw -numjobs=1 -runtime=15 -time_based=1 -filename=/dev/nbd0 -name="fio test" -iodepth=128
sudo module/sump/scripts/vhost.sh -m queue
sudo fio -ioengine=libaio -bs=4k -direct=1 -thread -rw=randrw -numjobs=1 -runtime=15 -time_based=1 -filename=/dev/nbd0 -name="fio test" -iodepth=128

sudo module/sump/scripts/vhost.sh -m random
sudo fio -ioengine=libaio -bs=4k -direct=1 -thread -rw=randrw -numjobs=1 -runtime=15 -time_based=1 -filename=/dev/nbd0 -name="fio test" -iodepth=128
sudo module/sump/scripts/vhost.sh -m hash
sudo fio -ioengine=libaio -bs=4k -direct=1 -thread -rw=randrw -numjobs=1 -runtime=15 -time_based=1 -filename=/dev/nbd0 -name="fio test" -iodepth=128
sudo module/sump/scripts/vhost.sh -m weight
sudo fio -ioengine=libaio -bs=4k -direct=1 -thread -rw=randrw -numjobs=1 -runtime=15 -time_based=1 -filename=/dev/nbd0 -name="fio test" -iodepth=128
sudo module/sump/scripts/vhost.sh -m time
sudo fio -ioengine=libaio -bs=4k -direct=1 -thread -rw=randrw -numjobs=1 -runtime=15 -time_based=1 -filename=/dev/nbd0 -name="fio test" -iodepth=128
sudo module/sump/scripts/vhost.sh -m avg_rate
sudo fio -ioengine=libaio -bs=4k -direct=1 -thread -rw=randrw -numjobs=1 -runtime=15 -time_based=1 -filename=/dev/nbd0 -name="fio test" -iodepth=128


# 内核用户对比
sudo module/sump/scripts/stop.sh
sudo module/sump/scripts/target.sh -s iscsi
sudo module/sump/scripts/vhost.sh -s iscsi
sudo fio -ioengine=libaio -bs=4k -direct=1 -thread -rw=randrw -numjobs=1 -runtime=15 -time_based=1 -filename=/dev/nbd0 -name="fio test" -iodepth=128
sudo module/sump/scripts/stop.sh
sudo scripts/setup.sh reset
sudo tgtadm --lld iscsi --mode target --op new --tid 1 --targetname tgt1
sudo tgtadm --lld iscsi --mode target --op bind --tid 1 -I ALL
sudo tgtadm --lld iscsi --op new --mode portal --param portal=127.0.0.1:4420 
sudo tgtadm --lld iscsi --op new --mode portal --param portal=127.0.0.1:4421 
sudo tgtadm --lld iscsi --op new --mode portal --param portal=127.0.0.1:4422 
sudo tgtadm --lld iscsi --op new --mode portal --param portal=127.0.0.1:4423
sudo tgtadm --lld iscsi --mode logicalunit --op new --tid 1 --lun 1 -b /dev/nvme0n1
sudo iscsiadm -m discovery -t sendtargets -p 127.0.0.1:4420
sudo iscsiadm -m discovery -t sendtargets -p 127.0.0.1:4421
sudo iscsiadm -m discovery -t sendtargets -p 127.0.0.1:4422
sudo iscsiadm -m discovery -t sendtargets -p 127.0.0.1:4423
sudo iscsiadm -m node --login
lsblk
sudo fio -ioengine=libaio -bs=4k -direct=1 -thread -rw=randrw -numjobs=1 -runtime=15 -time_based=1 -filename=/dev/mapper/mpathb -name="fio test" -iodepth=128
sudo iscsiadm -m node --logout
sudo iscsiadm -m node -o delete
sudo tgtadm --lld iscsi --op delete --mode target --tid 1
sudo tgtadm --lld iscsi --op delete --mode portal --param portal=127.0.0.1:4420 
sudo tgtadm --lld iscsi --op delete --mode portal --param portal=127.0.0.1:4421 
sudo tgtadm --lld iscsi --op delete --mode portal --param portal=127.0.0.1:4422 
sudo tgtadm --lld iscsi --op delete --mode portal --param portal=127.0.0.1:4423

# QEMU
wget http://cloud.centos.org/centos/7/images/CentOS-7-x86_64-GenericCloud-1608.qcow2
mv CentOS-7-x86_64-GenericCloud-1608.qcow2 guest_os_image.qcow2
sudo virt-customize -a /var/tmp/guest_os_image.qcow2 --root-password password:123456
sudo module/sump/scripts/target.sh 
sudo module/sump/scripts/vhost.sh -q qemu
sudo ./scripts/rpc.py -s /var/tmp/vhost.sock vhost_create_scsi_controller --cpumask 0x2 vhost.0
sudo ./scripts/rpc.py -s /var/tmp/vhost.sock vhost_scsi_controller_add_target vhost.0 0 ump-bdev-300c2275-a15f-47fa-b448-310087e7a6e3
sudo qemu-system-x86_64 \
-nographic \
--enable-kvm \
-cpu host -smp 2 \
-m 1024M -object memory-backend-file,id=mem0,size=1024M,mem-path=/dev/hugepages,share=on -numa node,memdev=mem0 \
-drive file=/var/tmp/guest_os_image.qcow2,if=none,id=disk \
-drive file=/var/tmp/share.img,if=virtio \
-device ide-hd,drive=disk,bootindex=0 \
-chardev socket,id=spdk_vhost_scsi0,path=module/sump/vhost.0 \
-device vhost-user-scsi-pci,id=scsi0,chardev=spdk_vhost_scsi0,num_queues=2
systemctl stop cloud-init
lsblk
mount -t ext4 /dev/vda /mnt/
cd /mnt
rpm -ivh *.rpm --nodeps --force
sudo fio -ioengine=libaio -bs=4k -direct=1 -thread -rw=randrw -numjobs=1 -runtime=15 -time_based=1 -filename=/dev/sda -name="fio test" -iodepth=128