import matplotlib.pyplot as plt

kernel_path = "./res/用户态内核态对比1-1-100-100MBps限速/内核iscsi/读-写-单位MiBps"
user_path = "./res/用户态内核态对比1-1-100-100MBps限速/用户iscsi/读-写-单位MiBps"

plt.figure(figsize=(10, 8))

with open(kernel_path, "r") as kernel_file:
    values = []
    lines = kernel_file.readlines()
    for line in lines:
        data = line.strip().split(",")
        value = (float(data[0]) + float(data[1])) / 2
        values.append(value)
    plt.plot(list(range(61)), values, label="kernel", color="orange")

with open(user_path, "r") as user_file:
    values = []
    lines = user_file.readlines()
    for line in lines:
        data = line.strip().split(",")
        value = (float(data[0]) + float(data[1])) / 2
        values.append(value)
    plt.plot(list(range(61)), values, label="user", color="red")

plt.xlabel('Time (s)')
plt.ylabel('Bandwidth (MBps)')
plt.legend()
plt.title('100MBps-user-kernel')
plt.savefig("./pic/用户态内核态对比1-1-100-100MBps限速.png")