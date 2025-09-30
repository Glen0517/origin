#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""测试scheduler是否能正常工作"""
import os
import sys
import logging

# 添加项目根目录到Python路径
sys.path.append(os.path.dirname(os.path.abspath(__file__)))

# 配置日志
logging.basicConfig(level=logging.INFO, format='%(asctime)s - %(name)s - %(levelname)s - %(message)s')

try:
    # 导入Flask应用和scheduler
    from app import app, SAMPLE_FUNDS, SAMPLE_STOCKS, FUND_INDICES
    from utils.scheduler import scheduler

    # 打印修复前的数据快照
    print("修复前的数据快照：")
    print("基金样本 (第一个):", SAMPLE_FUNDS[0])
    print("股票样本 (第一个):", SAMPLE_STOCKS[0])
    print("指数样本 (第一个):", FUND_INDICES[0])

    # 运行scheduler的更新方法
    print("\n开始测试数据更新...")
    with app.app_context():
        scheduler.app = app
        # 先调用一次更新趋势因子
        scheduler._update_trend_factors()
        # 然后调用数据更新方法
        scheduler.update_data()
        print("数据更新成功！")
        
        # 打印更新后的数据
        print("\n更新后的数据：")
        print("基金样本 (第一个):", SAMPLE_FUNDS[0])
        print("股票样本 (第一个):", SAMPLE_STOCKS[0])
        print("指数样本 (第一个):", FUND_INDICES[0])
        
        # 验证是否有新的字段被添加
        print("\n验证新字段：")
        print("基金是否有previous_net_value字段:", 'previous_net_value' in SAMPLE_FUNDS[0])
        print("股票是否有price字段:", 'price' in SAMPLE_STOCKS[0])
        print("股票是否有previous_price字段:", 'previous_price' in SAMPLE_STOCKS[0])
        print("指数是否有previous_value字段:", 'previous_value' in FUND_INDICES[0])
        
        # 打印最后更新时间
        if scheduler.last_update_time:
            print("\n最后更新时间:", scheduler.last_update_time)
            
except Exception as e:
    print("测试失败:", str(e))
    import traceback
    traceback.print_exc()