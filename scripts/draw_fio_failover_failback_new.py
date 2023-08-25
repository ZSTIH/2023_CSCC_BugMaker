import matplotlib.pyplot as plt

single_path = "./res/单多路径故障处理对比10Mbps限速/单路径/读-写-单位KiBps"
multi_path = "./res/单多路径故障处理对比10Mbps限速/多路径/读-写-单位MiBps"

plt.figure(figsize=(10, 8))

with open(single_path, "r") as file:
    values = []
    lines = file.readlines()
    for line in lines:
        data = line.strip().split(",")
        value = (float(data[0]) + float(data[1])) / 2 / 1024
        values.append(value)
    while len(values) < 120:
        values.append(0)
    plt.plot(list(range(120)), values, label="single-path", color="orange")

with open(multi_path, "r") as file:
    values = []
    lines = file.readlines()
    for line in lines:
        data = line.strip().split(",")
        value = (float(data[0]) + float(data[1])) / 2
        values.append(value)
    plt.plot(list(range(120)), values, label="multi-path", color="red")

plt.xlabel('Time (s)')
plt.ylabel('Bandwidth (MBps)')
plt.legend()
plt.title('10MBps-failover-failback-single-multi-path')
plt.savefig("./pic/单多路径故障处理对比10Mbps限速.png")