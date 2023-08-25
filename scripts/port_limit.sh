# 先删除原有的配置
sudo tc qdisc del dev lo root handle 1:0

# 创建根序列
sudo tc qdisc add dev lo root handle 1:0 htb default 1

# 创建一个主分类绑定所有带宽资源
sum=0
for rate in $*
do
    sum=`expr ${sum} + ${rate}`
done
echo "total bandwidth: "${sum}MBps
sudo tc class add dev lo parent 1:0 classid 1:1 htb rate ${sum}MBps


i=1
for rate in $*
do
    # 创建子分类(所有子分类的带宽之和要小于主分类)
    sudo tc class add dev lo parent 1:1 classid 1:${i}00 htb rate ${rate}MBps

    # 避免一个ip霸占带宽资源
    sudo tc qdisc add dev lo parent 1:${i}00 handle 1$i: sfq perturb 10

    # 创建过滤器
    sudo tc filter add dev lo parent 1:0 protocol ip prio 1 handle 1$i fw classid 1:${i}00
    sudo iptables -A OUTPUT -t mangle -p tcp --sport 442`expr $i - 1` -j MARK --set-mark 1$i
    i=`expr $i + 1`
done




# sudo tc class add dev lo parent 1:0 classid 1:1 htb rate 10Mbps

# # 创建子分类(所有子分类的带宽之和要小于主分类)
# sudo tc class add dev lo parent 1:1 classid 1:100 htb rate 10Mbps

# # 避免一个ip霸占带宽资源
# sudo tc qdisc add dev lo parent 1:100 handle 10: sfq perturb 10

# # 创建过滤器
# sudo tc filter add dev lo parent 1:0 protocol ip prio 1 handle 10 fw classid 1:100
# sudo iptables -A OUTPUT -t mangle -p tcp --sport 4420 -j MARK --set-mark 10

# # 删除
# sudo tc qdisc del dev lo root handle 1:0
