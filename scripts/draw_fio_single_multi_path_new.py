import matplotlib.pyplot as plt

single_path = "./res/单多路径对比/单路径10MBps限速/读-写-单位KiBps"
multi_path = "./res/单多路径对比/多路径10MBps限速/读-写-单位MiBps"

plt.figure(figsize=(10, 8))

with open(single_path, "r") as single_file:
    values = []
    lines = single_file.readlines()
    for line in lines:
        data = line.strip().split(",")
        value = (float(data[0]) + float(data[1])) / 2 / 1024
        values.append(value)
    plt.plot(list(range(60)), values, label="single-path", color="orange")

with open(multi_path, "r") as multi_file:
    values = []
    lines = multi_file.readlines()
    for line in lines:
        data = line.strip().split(",")
        value = (float(data[0]) + float(data[1])) / 2
        values.append(value)
    plt.plot(list(range(60)), values, label="multi-path", color="red")

plt.xlabel('Time (s)')
plt.ylabel('Bandwidth (MBps)')
plt.ylim(0, 40)
plt.legend()
plt.title('10MBps-single-multi-path')
plt.savefig("./pic/单多路径10MBps限速对比.png")