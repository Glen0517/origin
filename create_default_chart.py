import matplotlib.pyplot as plt
import numpy as np
import os

# 创建默认的图表
fig, ax = plt.subplots(figsize=(12, 6))

# 生成一些简单的示例数据
x = np.arange(0, 30)
y = np.random.randn(30).cumsum() + 10

# 绘制一条简单的线图
ax.plot(x, y, marker='', linestyle='-', color='blue', linewidth=1.5)

# 设置标题和标签
ax.set_title('基金净值走势（示例图）')
ax.set_xlabel('日期')
ax.set_ylabel('单位净值')
ax.grid(True)

# 自动调整布局
plt.tight_layout()

# 确保charts目录存在
charts_dir = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'static', 'charts')
if not os.path.exists(charts_dir):
    os.makedirs(charts_dir)

# 保存图表
default_chart_path = os.path.join(charts_dir, 'default_nav.png')
plt.savefig(default_chart_path, dpi=100, bbox_inches='tight')
plt.close(fig)

print(f'默认图表已创建：{default_chart_path}')