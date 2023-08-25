import matplotlib.pyplot as plt

hash_path = "./res/多路径负载均衡1-1-100-100MBps限速/哈希/读-写-单位KiBps"
min_latency_path = "./res/多路径负载均衡1-1-100-100MBps限速/最短平均时延/读-写-单位MiBps"
max_speed_path = "./res/多路径负载均衡1-1-100-100MBps限速/最大平均速度/读-写-单位MiBps"
weight_path = "./res/多路径负载均衡1-1-100-100MBps限速/权重/读-写-单位KiBps"
round_robin_path = "./res/多路径负载均衡1-1-100-100MBps限速/轮询/读-写-单位KiBps"
queue_length_path = "./res/多路径负载均衡1-1-100-100MBps限速/队列深度/读-写-单位MiBps"
random_path =  "./res/多路径负载均衡1-1-100-100MBps限速/随机/读-写-单位KiBps"

paths = [hash_path, min_latency_path, max_speed_path, weight_path, round_robin_path, queue_length_path, random_path]
is_KiBps = [1, 0, 0, 1, 1, 0, 1]
colors = ['b', 'g', 'r', 'c', 'm', 'y', 'k']
labels = ["hash", "service time", "average rate", "weight", "round robin", "queue length", "random"]

plt.figure(figsize=(10, 8))

for index, filename in enumerate(paths):
    values = []
    with open(filename, "r") as file:
        lines = file.readlines()
        for line in lines:
            data = line.strip().split(",")
            value = (float(data[0]) + float(data[1])) / 2
            if is_KiBps[index] == 1:
                values.append(value / 1024)
            else:
                values.append(value)
    plt.plot(list(range(len(values))), values, label=labels[index], color=colors[index])

plt.xlabel('Time (s)')
plt.ylabel('Bandwidth (MBps)')
plt.legend(bbox_to_anchor=(1, 0.75))
plt.title('1-1-100-100MBps-load-balancing')
plt.savefig("./pic/多路径负载均衡1-1-100-100MBps限速.png")