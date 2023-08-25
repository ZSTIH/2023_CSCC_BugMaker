import numpy as np
import matplotlib.pyplot as plt
import pandas as pd
from scipy import stats
# plt.rcParams["font.sans-serif"] = ["SimHei"] #解决中文字符乱码的问题
plt.rcParams["axes.unicode_minus"] = False #正常显示负号

def draw_pic(res, title, ylabel, time_interval, type_="line"):
    len_ = res.shape[1]
    print(len_)
    # 生成时间序列
    t = np.linspace(0,len_*time_interval,len_)
    if type_ == "line":
        # 画图
        plt.plot(t, res[0,:], label='path 0')
        plt.plot(t, res[1,:], label='path 1')
        plt.plot(t, res[2,:], label='path 2')
        plt.plot(t, res[3,:], label='path 3')
    #     plt.plot(t,np.sum(res, axis=0))
        plt.legend(['path 0', 'path 1', 'path 2', 'path 3'], loc='upper left')
        plt.xlabel("time / s")
        plt.ylabel(ylabel)
        plt.title(title)
        plt.savefig("../pic/"+title+".png", dpi=300)
        plt.show()
    elif type_ == "scatter":
        plt.scatter(t, res[0,:], s=5, label='path 0')
        plt.scatter(t, res[1,:], s=5, label='path 1')
        plt.scatter(t, res[2,:], s=5, label='path 2')
        plt.scatter(t, res[3,:], s=5, label='path 3')
    #     plt.plot(t,np.sum(res, axis=0))
        plt.legend(['path 0', 'path 1', 'path 2', 'path 3'], loc='upper left')
        plt.xlabel("time / s")
        plt.ylabel(ylabel)
        plt.title(title)
        plt.savefig("../pic/"+title+".png", dpi=300)
        plt.show()
    
def draw_path_io(path, title, time_interval = 0.1,):
    # 读取文件
    f = open(path)
    lines = f.readlines()
    # lines = lines[100:]
    f.close()

    # 各路径结果
    res0,res1,res2,res3 = [],[],[],[]
    flag = False                     # 是否开始记录
    for idx, line in enumerate(lines[:-1]):
        line_ = line.split(" ")
        if not flag:
            if (line_[0] == "path" and line_[1] == "0"):
                flag = True
        if flag and line_[0] == "path":
            eval('res'+line_[1]).append(eval(line_[2]))
            
    len_ = min(len(res0), len(res1), len(res2), len(res3))-1
    res0 = np.array(res0[:len_])
    res1 = np.array(res1[:len_])
    res2 = np.array(res2[:len_])
    res3 = np.array(res3[:len_])
    
    # 拼接
    res = np.vstack((res0,res1,res2,res3))
    res = res / 1024
    # 做累计图
    draw_pic(res, title+ " IO", "IO / MB", time_interval, "line")
    
    
    # 差分
    res = pd.DataFrame(res)
    res = res.diff(axis=1)
    # res.to_csv("./res/diff.csv")

    res = np.array(res.values[:,2:])
    
    res = res / time_interval
    
    # 去除离群点
    z = np.abs(stats.zscore(res, axis=1))
    x, y = np.where(z>1.5)
    print(x,y)
    for i in range(len(x)):
        res[x[i],y[i]] = np.mean(res[x[i]])
    
    # 做速度图
    draw_pic(res, title+ " IO Rate", "IO / (MB/s)", time_interval, "scatter")

draw_path_io(path="../res/多路径故障处理100MBps限速/log.txt", title="多路径故障处理100MBps限速", time_interval=0.3)