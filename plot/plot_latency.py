import matplotlib.pyplot as plt

# 初始化列表，用于存储读取的数据
injection_rates = []
average_packet_latencies = []

# 读取文件
with open('latency_neighbor.txt', 'r') as file:
    for line in file:
        # 解析每一行，提取 injection_rate 和 average_packet_latency
        if 'injection_rate =' in line and 'average_packet_latency =' in line:
            parts = line.split()
            injection_rate = float(parts[2])  # 提取 injection_rate 的值
            latency = float(parts[5])         # 提取 average_packet_latency 的值
            
            # 将提取到的数据添加到列表中
            injection_rates.append(injection_rate)
            average_packet_latencies.append(latency)

# 绘制图形
plt.figure(figsize=(10, 6))
plt.plot(injection_rates, average_packet_latencies, marker='o', linestyle='-', color='b')

# 添加标题和标签
plt.title('Latency vs. Injection Rate')
plt.xlabel('Injection Rate')
plt.ylabel('Average Packet Latency')

# 显示网格
plt.grid(True)

# 显示图形
plt.savefig('neighbor.png')
