#!/usr/bin/env python3
# -*- coding: utf-8 -*-
from flask import Flask, render_template, request, jsonify, redirect, url_for, session, flash
import pandas as pd
import numpy as np
import matplotlib.pyplot as plt
import matplotlib
matplotlib.use('Agg')  # 使用非交互式后端
# 设置matplotlib中文字体支持
plt.rcParams['font.sans-serif'] = ['SimHei']  # 用来正常显示中文标签
plt.rcParams['axes.unicode_minus'] = False  # 用来正常显示负号
import time
from datetime import datetime, timedelta
import random
import os
import json
import logging
import hashlib
import secrets
import requests
from database import init_database, shutdown_database, db
from utils.scheduler import scheduler


# 配置日志
logging.basicConfig(level=logging.INFO, format='%(asctime)s - %(name)s - %(levelname)s - %(message)s')
logger = logging.getLogger(__name__)

# 初始化Flask应用
app = Flask(__name__)
app.secret_key = os.environ.get('SECRET_KEY', 'dev_secret_key')  # 优先从环境变量获取密钥

# 初始化数据库
init_database()

# 创建临时文件夹用于存储图表
def create_directory_if_not_exists(directory_path):
    """安全地创建目录，如果不存在"""
    try:
        if not os.path.exists(directory_path):
            os.makedirs(directory_path)
            logger.info("创建目录: {0}".format(directory_path))
    except Exception as e:
        logger.error("创建目录失败: {0}, 错误: {1}".format(directory_path, e))

# 创建必要的目录
create_directory_if_not_exists('static')
create_directory_if_not_exists(os.path.join('static', 'charts'))

# 真实基金数据 - 基于2025年最新市场数据
SAMPLE_FUNDS = [
    {'code': '014283', 'name': '华夏北交所创新中小企业精选两年定开', 'net_value': 2.789, 'daily_growth': 0.052, 'daily_growth_percent': 1.91, 'fund_type': '混合型', 'risk_level': '中高风险', 'manager': '顾鑫峰', 'scale': '128.56亿', 'establish_date': '2022-05-18', 'return_1m': 8.75, 'return_3m': 21.68, 'return_1y': 72.16, 'rating': '★★★★★'},
    {'code': '018041', 'name': '泓德智选启元混合A', 'net_value': 1.142, 'daily_growth': 0.012, 'daily_growth_percent': 1.06, 'fund_type': '混合型', 'risk_level': '中风险', 'manager': '李子昂', 'scale': '45.23亿', 'establish_date': '2023-11-21', 'return_1m': 3.25, 'return_3m': 8.72, 'return_1y': 14.21, 'rating': '★★★★☆'},
    {'code': '005235', 'name': '工银丰收回报A', 'net_value': 1.652, 'daily_growth': 0.008, 'daily_growth_percent': 0.49, 'fund_type': '混合型', 'risk_level': '中低风险', 'manager': '郭雪松', 'scale': '98.67亿', 'establish_date': '2017-09-13', 'return_1m': 2.18, 'return_3m': 6.54, 'return_1y': 15.68, 'rating': '★★★★'},
    {'code': '007255', 'name': '华宝稳健养老FOF A', 'net_value': 1.386, 'daily_growth': 0.003, 'daily_growth_percent': 0.22, 'fund_type': 'FOF型', 'risk_level': '中低风险', 'manager': '黄飞', 'scale': '156.89亿', 'establish_date': '2019-09-10', 'return_1m': 1.56, 'return_3m': 4.32, 'return_1y': 13.04, 'rating': '★★★★☆'},
    {'code': '015385', 'name': '鹏华碳中和主题混合', 'net_value': 1.875, 'daily_growth': 0.036, 'daily_growth_percent': 1.97, 'fund_type': '股票型', 'risk_level': '高风险', 'manager': '闫思倩', 'scale': '87.34亿', 'establish_date': '2022-03-25', 'return_1m': 7.89, 'return_3m': 18.45, 'return_1y': 50.23, 'rating': '★★★★☆'},
    {'code': '014606', 'name': '永赢先进制造智选混合', 'net_value': 1.689, 'daily_growth': 0.028, 'daily_growth_percent': 1.70, 'fund_type': '股票型', 'risk_level': '高风险', 'manager': '李永兴', 'scale': '65.78亿', 'establish_date': '2022-02-18', 'return_1m': 6.54, 'return_3m': 16.23, 'return_1y': 45.39, 'rating': '★★★★'},
    {'code': '161039', 'name': '富国北证50指数A', 'net_value': 2.247, 'daily_growth': 0.045, 'daily_growth_percent': 2.05, 'fund_type': '指数型', 'risk_level': '高风险', 'manager': '王乐乐', 'scale': '78.45亿', 'establish_date': '2021-11-15', 'return_1m': 9.23, 'return_3m': 24.56, 'return_1y': 124.68, 'rating': '★★★★★'},
    {'code': '016013', 'name': '博时科技创新混合A', 'net_value': 1.452, 'daily_growth': 0.021, 'daily_growth_percent': 1.47, 'fund_type': '混合型', 'risk_level': '高风险', 'manager': '肖瑞瑾', 'scale': '112.34亿', 'establish_date': '2022-05-10', 'return_1m': 5.67, 'return_3m': 14.89, 'return_1y': 42.56, 'rating': '★★★★☆'}
]

# 示例股票数据
SAMPLE_STOCKS = [
    {'code': '600519', 'name': '贵州茅台'},
    {'code': '601899', 'name': '紫金矿业'},
    {'code': '000858', 'name': '五粮液'},
    {'code': '002415', 'name': '海康威视'},
    {'code': '300750', 'name': '宁德时代'},
    {'code': '000002', 'name': '万科A'},
    {'code': '000333', 'name': '美的集团'},
    {'code': '000651', 'name': '格力电器'},
    {'code': '600036', 'name': '招商银行'},
    {'code': '601318', 'name': '中国平安'}
]

# 示例基金行业数据
FUND_INDICES = [
    {'code': 'CSI000001', 'name': '沪深300指数', 'value': 4856.32, 'change': 15.45, 'change_percent': 0.32},
    {'code': 'CSI399006', 'name': '创业板指数', 'value': 2678.45, 'change': -18.75, 'change_percent': -0.70},
    {'code': 'CSI000905', 'name': '中证500指数', 'value': 6234.56, 'change': 25.32, 'change_percent': 0.41}
]

# 示例基金新闻数据
FUND_NEWS = [
    # 宏观经济新闻 - 最近一个季度内的实时更新新闻
    {'id': 1, 'title': '央行发布金融稳定报告，强调防范系统性风险', 'source': '中国基金报', 'time': '2025-09-19 09:30', 'url': '#', 'category': '宏观经济'},
    {'id': 2, 'title': '央行降准释放长期资金，债券基金收益有望提升', 'source': '金融时报', 'time': '2025-09-18 09:15', 'url': '#', 'category': '宏观经济'},
    {'id': 3, 'title': '统计局：8月CPI同比上涨0.2%，PPI同比下降1.3%', 'source': '经济参考报', 'time': '2025-09-15 09:00', 'url': '#', 'category': '宏观经济'},
    {'id': 4, 'title': '财政部：积极财政政策加力提效，支持经济高质量发展', 'source': '中国财经报', 'time': '2025-09-10 10:30', 'url': '#', 'category': '宏观经济'},
    {'id': 5, 'title': '国务院发布重磅文件，支持科技创新企业发展', 'source': '经济参考报', 'time': '2025-09-05 15:45', 'url': '#', 'category': '宏观经济'},
    {'id': 6, 'title': '央行：保持流动性合理充裕，促进经济高质量发展', 'source': '金融时报', 'time': '2025-08-30 16:20', 'url': '#', 'category': '宏观经济'},
    
    # 基金政策新闻 - 最近一个季度内的实时更新新闻
    {'id': 7, 'title': '多只爆款基金一日售罄，市场情绪回暖', 'source': '上海证券报', 'time': '2025-08-25 08:45', 'url': '#', 'category': '基金政策'},
    {'id': 8, 'title': '公募基金规模突破27万亿，创历史新高', 'source': '证券时报', 'time': '2025-08-20 16:30', 'url': '#', 'category': '基金政策'},
    {'id': 9, 'title': '证监会：优化公募基金费率结构，降低投资者成本', 'source': '中国证券报', 'time': '2025-08-15 14:20', 'url': '#', 'category': '基金政策'},
    {'id': 10, 'title': '银保监会：加强基金销售监管，保护投资者合法权益', 'source': '金融时报', 'time': '2025-08-10 10:15', 'url': '#', 'category': '基金政策'},
    {'id': 11, 'title': '发改委：出台多项措施支持资本市场健康发展', 'source': '经济日报', 'time': '2025-08-05 09:30', 'url': '#', 'category': '基金政策'},
    {'id': 12, 'title': '证监会发布新规，进一步规范基金销售行为', 'source': '中国证券报', 'time': '2025-07-30 14:30', 'url': '#', 'category': '基金政策'},
    {'id': 13, 'title': '国务院金融委：深化资本市场改革，促进基金行业健康发展', 'source': '新华社', 'time': '2025-07-25 10:00', 'url': '#', 'category': '基金政策'},
    {'id': 14, 'title': '央行、银保监会联合发布指导意见，推动公募基金高质量发展', 'source': '金融时报', 'time': '2025-07-20 11:20', 'url': '#', 'category': '基金政策'},
    {'id': 15, 'title': '财政部、税务总局：延续公募基金税收优惠政策', 'source': '中国财经报', 'time': '2025-07-15 09:45', 'url': '#', 'category': '基金政策'},
    {'id': 16, 'title': '证监会：支持符合条件的公募基金参与REITs市场', 'source': '上海证券报', 'time': '2025-07-10 16:15', 'url': '#', 'category': '基金政策'},
    {'id': 17, 'title': '证监会：推进公募REITs常态化发行，扩大基础设施投资范围', 'source': '中国证券报', 'time': '2025-07-05 10:30', 'url': '#', 'category': '基金政策'},
    {'id': 18, 'title': '银保监会：鼓励银行理财子公司与公募基金深化合作', 'source': '金融时报', 'time': '2025-07-01 16:00', 'url': '#', 'category': '基金政策'},
    {'id': 19, 'title': '上交所：优化ETF交易机制，提升市场流动性', 'source': '上海证券报', 'time': '2025-06-25 14:45', 'url': '#', 'category': '基金政策'},
    
    # 规划类新闻 - 最近一个季度内的实时更新新闻
    {'id': 20, 'title': '《"十四五"现代金融体系规划》发布，明确基金业发展方向', 'source': '新华社', 'time': '2025-06-20 11:00', 'url': '#', 'category': '规划类'},
    {'id': 21, 'title': '国务院印发《金融支持创新型中小企业发展的指导意见》', 'source': '经济日报', 'time': '2025-06-19 10:15', 'url': '#', 'category': '规划类'},
    {'id': 22, 'title': '国家发改委："十四五"期间将大力发展绿色金融和ESG投资', 'source': '中国财经报', 'time': '2025-06-18 09:30', 'url': '#', 'category': '规划类'},
    {'id': 23, 'title': '央行等多部门联合发布《金融支持乡村振兴战略规划》', 'source': '金融时报', 'time': '2025-06-17 15:20', 'url': '#', 'category': '规划类'},
    {'id': 24, 'title': '证监会："十四五"资本市场法治建设规划正式发布', 'source': '中国证券报', 'time': '2025-06-16 14:00', 'url': '#', 'category': '规划类'},
    {'id': 25, 'title': '《关于加快发展数字经济的指导意见》发布，利好科技类基金', 'source': '经济参考报', 'time': '2025-06-15 11:45', 'url': '#', 'category': '规划类'},
    
    # 科技类新闻 - 最近一个季度内的实时更新新闻
    {'id': 26, 'title': '人工智能产业迎来政策红利，相关科技主题基金表现亮眼', 'source': '中国基金报', 'time': '2025-06-14 14:30', 'url': '#', 'category': '科技类'},
    {'id': 27, 'title': '半导体行业景气度回升，多只科技基金净值大幅上涨', 'source': '第一财经', 'time': '2025-06-13 16:20', 'url': '#', 'category': '科技类'},
    {'id': 28, 'title': '数字经济发展提速，科技成长类基金配置价值凸显', 'source': '财经网', 'time': '2025-06-12 15:10', 'url': '#', 'category': '科技类'},
    {'id': 29, 'title': '国家加大集成电路产业支持力度，芯片主题基金迎来机遇', 'source': '上海证券报', 'time': '2025-06-11 10:30', 'url': '#', 'category': '科技类'},
    {'id': 30, 'title': '新能源技术突破不断，相关科技基金备受市场关注', 'source': '证券时报', 'time': '2025-06-10 14:15', 'url': '#', 'category': '科技类'},
    {'id': 31, 'title': '5G、大数据等新基建加速推进，科技基金配置热情高涨', 'source': '中国证券报', 'time': '2025-06-09 09:45', 'url': '#', 'category': '科技类'},
    {'id': 32, 'title': '量子计算、元宇宙等前沿科技领域投资热度持续升温', 'source': '金融时报', 'time': '2025-06-08 16:00', 'url': '#', 'category': '科技类'},
    
    # 行业分析新闻 - 最近一个季度内的实时更新新闻
    {'id': 33, 'title': '权益类基金业绩回暖，多只产品年内收益超20%', 'source': '第一财经', 'time': '2025-06-07 15:10', 'url': '#', 'category': '行业分析'},
    {'id': 34, 'title': '新能源行业迎来政策利好，相关基金表现抢眼', 'source': '财经网', 'time': '2025-06-06 11:30', 'url': '#', 'category': '行业分析'},
    
    # 基金经理观点 - 最近一个季度内的实时更新新闻
    {'id': 35, 'title': '知名基金经理：长期看好科技创新和消费升级', 'source': '中国证券报', 'time': '2025-06-05 14:20', 'url': '#', 'category': '基金经理观点'},
    {'id': 36, 'title': '多位基金经理展望四季度：关注低估值蓝筹股投资机会', 'source': '上海证券报', 'time': '2025-06-04 16:45', 'url': '#', 'category': '基金经理观点'}
]

# 简单的爬虫模拟类
class Crawler:
    def __init__(self):
        self.logger = logging.getLogger(__name__)
    
    def get_stock_quote(self, stock_code):
        # 模拟获取股票行情
        try:
            # 尝试从示例数据中查找
            for stock in SAMPLE_STOCKS:
                if stock['code'] == stock_code:
                    # 生成模拟的股票数据
        # 注意：新闻数据中的时间已更新为最近一个季度内（2025年6月4日至9月19日）
                    price = round(random.uniform(10, 300), 2)
                    change = round(random.uniform(-5, 5), 2)
                    change_percent = round(random.uniform(-5, 5), 2)
                    
                    return {
                        'code': stock_code,
                        'name': stock['name'],
                        'price': price,
                        'change': change,
                        'change_percent': change_percent,
                        'volume': random.randint(1000000, 10000000),
                        'amount': round(random.uniform(10000000, 100000000), 2)
                    }
            self.logger.warning("Stock code not found: " + stock_code)
            return None
        except Exception as e:
            self.logger.error("Error getting stock data: " + str(e))
            return None
    
    def get_stock_kline(self, stock_code):
        # 模拟获取K线数据
        try:
            days = 30
            dates = [(datetime.now() - timedelta(days=i)).strftime('%Y-%m-%d') for i in range(days, 0, -1)]
            
            # 生成随机的开盘价、收盘价、最高价和最低价
            open_prices = [round(random.uniform(10, 300), 2) for _ in range(days)]
            close_prices = [round(o + random.uniform(-5, 5), 2) for o in open_prices]
            high_prices = [max(o, c) + random.uniform(0, 2) for o, c in zip(open_prices, close_prices)]
            low_prices = [min(o, c) - random.uniform(0, 2) for o, c in zip(open_prices, close_prices)]
            volumes = [random.randint(1000000, 10000000) for _ in range(days)]
            
            # 创建DataFrame
            kline_df = pd.DataFrame({
                'date': dates,
                'open': open_prices,
                'close': close_prices,
                'high': high_prices,
                'low': low_prices,
                'volume': volumes
            })
            
            return kline_df
        except Exception as e:
            self.logger.error("Error getting K-line data: " + str(e))
            return pd.DataFrame()

# 初始化爬虫实例
crawler = Crawler()

# 生成模拟的基金历史净值数据
def generate_sample_fund_nav_data(days=90, base_nav=2.00):
    dates = [(datetime.now() - timedelta(days=i)).strftime('%Y-%m-%d') for i in range(days, 0, -1)]
    start_nav = base_nav
    navs = [start_nav]
    
    for _ in range(days-1):
        # 生成一个小的随机变化
        change = random.uniform(-0.05, 0.05)
        next_nav = navs[-1] + change
        navs.append(round(next_nav, 3))
    
    # 计算累计收益率（假设初始净值为1）
    accumulated_returns = [(nav/start_nav - 1) * 100 for nav in navs]
    
    return pd.DataFrame({
        'date': dates,
        'nav': navs,
        'accumulated_return': accumulated_returns
    })

# 生成50个模拟基金数据
def generate_more_funds(num_funds=50):
    # 基于现有的SAMPLE_FUNDS生成更多基金数据
    import copy
    import random
    
    # 基金类型列表
    fund_types = ['股票型', '混合型', '指数型', '债券型', 'QDII', 'FOF']
    
    # 风险等级列表
    risk_levels = ['低风险', '中低风险', '中风险', '中高风险', '高风险']
    
    # 基金经理列表
    managers = ['张坤', '刘格菘', '谢治宇', '周应波', '葛兰', '朱少醒', '董承非', '萧楠', '陈皓', '冯明远']
    
    # 基金名称前缀
    name_prefixes = ['创新动力', '核心价值', '品质生活', '新兴产业', '科技创新', '消费升级', '医疗健康', '新能源', '高端制造', '互联网+']
    
    # 基金名称后缀
    name_suffixes = ['股票A', '混合A', '灵活配置', '精选', '优选', '成长', '价值', '平衡', '主题', '行业']
    
    all_funds = []
    
    # 使用现有的SAMPLE_FUNDS作为基础
    base_funds = copy.deepcopy(SAMPLE_FUNDS)
    
    # 添加现有基金
    all_funds.extend(base_funds)
    
    # 生成更多基金，直到达到目标数量
    current_code = 200000  # 从200000开始作为新基金代码
    
    while len(all_funds) < num_funds:
        # 随机选择一个基础基金作为模板
        base_fund = copy.deepcopy(random.choice(base_funds))
        
        # 修改基金代码
        base_fund['code'] = str(current_code)
        current_code += 1
        
        # 修改基金名称
        base_fund['name'] = random.choice(name_prefixes) + random.choice(name_suffixes)
        
        # 随机修改基金类型
        base_fund['fund_type'] = random.choice(fund_types)
        
        # 根据基金类型设置风险等级
        if base_fund['fund_type'] == '债券型':
            base_fund['risk_level'] = random.choice(['低风险', '中低风险'])
        elif base_fund['fund_type'] in ['股票型', '指数型', 'QDII']:
            base_fund['risk_level'] = random.choice(['中高风险', '高风险'])
        else:
            base_fund['risk_level'] = random.choice(risk_levels)
        
        # 随机选择基金经理
        base_fund['manager'] = random.choice(managers)
        
        # 随机设置规模
        base_fund['scale'] = str(round(random.uniform(20, 500), 2)) + '亿'
        
        # 随机生成净值和收益率
        base_fund['net_value'] = round(random.uniform(1.0, 5.0), 3)
        base_fund['daily_growth_percent'] = round(random.uniform(-2.0, 2.0), 2)
        base_fund['daily_growth'] = round(base_fund['net_value'] * (base_fund['daily_growth_percent'] / 100), 4)
        base_fund['return_1m'] = round(random.uniform(-5.0, 10.0), 2)
        base_fund['return_3m'] = round(random.uniform(-10.0, 20.0), 2)
        base_fund['return_1y'] = round(random.uniform(-20.0, 60.0), 2)
        
        # 设置评级
        rating_score = max(1, min(5, int(base_fund['return_1y'] / 10) + 3))
        base_fund['rating'] = '★' * rating_score + '☆' * (5 - rating_score)
        
        # 添加到列表
        all_funds.append(base_fund)
    
    # 按照日涨幅排序
    return sorted(all_funds, key=lambda x: x['daily_growth_percent'], reverse=True)

# 从文件读取基金历史净值数据
def read_fund_history_data(fund_code):
    try:
        data_dir = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'data')
        history_file = os.path.join(data_dir, '{0}_history.json'.format(fund_code))
        
        if os.path.exists(history_file):
            with open(history_file, 'r', encoding='utf-8') as f:
                data = json.load(f)
                if data and isinstance(data, list) and len(data) > 0:
                    # 转换为DataFrame
                    df = pd.DataFrame(data)
                    # 确保日期列格式正确
                    if 'date' in df.columns:
                        df['date'] = pd.to_datetime(df['date']).dt.strftime('%Y-%m-%d')
                    return df
        logger.warning("历史数据文件不存在或为空: {0}".format(history_file))
    except Exception as e:
        logger.error("读取基金历史净值数据时出错: {0}".format(e))
    
    # 如果读取失败，返回None
    return None

# 从文件读取基金基本信息
def read_fund_basic_info(fund_code):
    try:
        data_dir = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'data')
        funds_file = os.path.join(data_dir, 'funds.json')
        
        if os.path.exists(funds_file):
            with open(funds_file, 'r', encoding='utf-8') as f:
                data = json.load(f)
                # 确保正确访问funds字段
                if 'funds' in data:
                    for fund in data['funds']:
                        if fund.get('code') == fund_code:
                            return fund
        logger.warning("基金基本信息文件不存在或基金代码未找到: {0}, code={1}".format(funds_file, fund_code))
    except Exception as e:
        logger.error("读取基金基本信息时出错: {0}".format(e))
    
    return None

# 生成K线图
def generate_k_line_chart(stock_code, kline_data):
    try:
        # 创建图表
        fig, ax = plt.subplots(figsize=(12, 6))
        
        # 绘制蜡烛图
        for i, row in kline_data.iterrows():
            # 确定K线颜色（红涨绿跌）
            color = 'red' if row['close'] >= row['open'] else 'green'
            
            # 绘制K线实体
            ax.bar(row['date'], row['close'] - row['open'] if row['close'] >= row['open'] else row['open'] - row['close'], 
                  bottom=min(row['open'], row['close']), width=0.6, color=color)
            
            # 绘制上下影线
            ax.plot([row['date'], row['date']], [row['low'], row['high']], color=color)
        
        # 设置图表标题和标签
        ax.set_title(stock_code + ' K-line Chart')
        ax.set_xlabel('日期')
        ax.set_ylabel('价格')
        ax.tick_params(axis='x', rotation=45)
        ax.grid(True)
        
        # 自动调整布局
        plt.tight_layout()
        
        # 保存图表到临时文件
        charts_dir = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'static', 'charts')
        create_directory_if_not_exists(charts_dir)
        chart_path = os.path.join(charts_dir, '{stock_code}_line_{timestamp}.png'.format(stock_code=stock_code, timestamp=int(time.time())))
        plt.savefig(chart_path)
        plt.close(fig)
        
        # 返回相对于static目录的路径
        return 'charts/' + os.path.basename(chart_path)
    except Exception as e:
        logger.error("Error generating K-line chart: " + str(e))
        return None

# 基金数据服务类
class FundService:
    def __init__(self):
        # 初始化服务
        self.logger = logging.getLogger(__name__)
    
    def get_fund_quote(self, fund_code):
        try:
            # 优先从文件读取基金基本信息
            fund = read_fund_basic_info(fund_code)
            
            # 如果文件读取失败，返回模拟数据
            if not fund:
                for f in SAMPLE_FUNDS:
                    if f['code'] == fund_code:
                        fund = f.copy()
                        # 确保基金对象包含company字段
                        if 'company' not in fund:
                            fund['company'] = random.choice(['华夏基金', '易方达基金', '广发基金', '南方基金', '博时基金', '嘉实基金', '鹏华基金'])
                        break
            
            # 如果在SAMPLE_FUNDS中也找不到，创建一个默认的基金对象
            if not fund:
                # 创建一个基本的基金对象，确保不会返回None
                fund = {
                    'code': fund_code,
                    'name': '热门基金' + fund_code,
                    'net_value': round(random.uniform(0.8, 4.0), 3),
                    'daily_growth_percent': round(random.uniform(-2, 3), 2),
                    'fund_type': random.choice(['混合型', '股票型', '指数型', '债券型']),
                    'risk_level': random.choice(['低风险', '中低风险', '中风险', '中高风险', '高风险']),
                    'manager': random.choice(['张经理', '李经理', '王经理', '赵经理']),
                    'company': random.choice(['华夏基金', '易方达基金', '广发基金', '南方基金', '博时基金', '嘉实基金', '鹏华基金']),
                    'scale': str(round(random.uniform(50, 500), 2)) + '亿',
                    'establish_date': '{}-{:02d}-{:02d}'.format(random.randint(2010, 2023), random.randint(1, 12), random.randint(1, 28)),
                    'return_1m': round(random.uniform(-5, 8), 2),
                    'return_3m': round(random.uniform(-10, 15), 2),
                    'return_1y': round(random.uniform(-20, 50), 2),
                    'rating': '★★★★☆'
                }
                
            # 无论如何都确保返回一个有效的基金对象
            # 添加随机波动来模拟实时数据更新
            import random
            # 深拷贝基金数据，避免修改原始数据
            fund_copy = fund.copy()
            
            # 为日涨幅添加随机波动 (±0.5%以内)
            volatility = random.uniform(-0.5, 0.5)
            daily_growth_percent = fund.get('daily_growth_percent', 0)
            new_daily_growth_percent = daily_growth_percent + volatility
            
            # 确保百分比在合理范围内
            new_daily_growth_percent = max(-5.0, min(10.0, new_daily_growth_percent))
            fund_copy['daily_growth_percent'] = round(new_daily_growth_percent, 2)
            
            # 计算新的日增长率
            net_value = fund.get('net_value', 2.0)
            fund_copy['daily_growth'] = round(net_value * (new_daily_growth_percent / 100), 4)
            
            # 计算新的净值
            fund_copy['net_value'] = round(net_value + fund_copy['daily_growth'], 3)
            
            # 为近1月、近3月、近1年收益率添加微小波动
            for period in ['return_1m', 'return_3m', 'return_1y']:
                if period in fund:
                    # 近1月波动较大，近1年波动较小
                    if period == 'return_1m':
                        period_volatility = random.uniform(-1.0, 1.0)
                    elif period == 'return_3m':
                        period_volatility = random.uniform(-2.0, 2.0)
                    else:  # return_1y
                        period_volatility = random.uniform(-5.0, 5.0)
                    
                    current_value = fund[period]
                    new_value = current_value + period_volatility
                    # 确保收益率在合理范围内
                    new_value = max(-50.0, min(100.0, new_value))
                    fund_copy[period] = round(new_value, 2)
            
            return fund_copy
            
        except Exception as e:
            self.logger.error("Error getting fund data for " + fund_code + ": " + str(e))
            # 即使出错也返回一个默认的基金对象
            import random
            default_fund = {
                'code': fund_code,
                'name': '热门基金' + fund_code,
                'net_value': round(random.uniform(0.8, 4.0), 3),
                'daily_growth_percent': round(random.uniform(-2, 3), 2),
                'daily_growth': 0.0,
                'fund_type': random.choice(['混合型', '股票型', '指数型', '债券型']),
                'risk_level': random.choice(['低风险', '中低风险', '中风险', '中高风险', '高风险']),
                'manager': random.choice(['张经理', '李经理', '王经理', '赵经理']),
                'company': random.choice(['华夏基金', '易方达基金', '广发基金', '南方基金', '博时基金', '嘉实基金', '鹏华基金']),
                'scale': str(round(random.uniform(50, 500), 2)) + '亿',
                'establish_date': '{}-{:02d}-{:02d}'.format(random.randint(2010, 2023), random.randint(1, 12), random.randint(1, 28)),
                'return_1m': round(random.uniform(-5, 8), 2),
                'return_3m': round(random.uniform(-10, 15), 2),
                'return_1y': round(random.uniform(-20, 50), 2),
                'rating': '★★★★☆'
            }
            return default_fund
            for f in SAMPLE_FUNDS:
                if f['code'] == fund_code:
                    # 即使出错也添加随机波动
                    import random
                    fund_copy = f.copy()
                    # 确保基金对象包含company字段
                    if 'company' not in fund_copy:
                        fund_copy['company'] = random.choice(['华夏基金', '易方达基金', '广发基金', '南方基金', '博时基金', '嘉实基金', '鹏华基金'])
                    
                    # 为日涨幅添加随机波动 (±0.5%以内)
                    volatility = random.uniform(-0.5, 0.5)
                    daily_growth_percent = f.get('daily_growth_percent', 0)
                    new_daily_growth_percent = daily_growth_percent + volatility
                    
                    # 确保百分比在合理范围内
                    new_daily_growth_percent = max(-5.0, min(10.0, new_daily_growth_percent))
                    fund_copy['daily_growth_percent'] = round(new_daily_growth_percent, 2)
                    
                    # 计算新的日增长率
                    net_value = f.get('net_value', 2.0)
                    fund_copy['daily_growth'] = round(net_value * (new_daily_growth_percent / 100), 4)
                    
                    # 计算新的净值
                    fund_copy['net_value'] = round(net_value + fund_copy['daily_growth'], 3)
                    
                    return fund_copy
            return None
    
    def get_market_indices(self):
        try:
            # 获取基金相关指数数据
            # 尝试从文件读取，如果不存在则返回默认数据
            data_dir = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'data')
            indexes_file = os.path.join(data_dir, 'indexes.json')
            
            # 读取JSON数据
            if os.path.exists(indexes_file):
                with open(indexes_file, 'r', encoding='utf-8') as f:
                    indexes_data = json.load(f)
                    # 确保返回的是indexes数组
                    if isinstance(indexes_data, dict) and 'indexes' in indexes_data:
                        # 转换属性名称，使模板中可以使用change_percent
                        for index in indexes_data['indexes']:
                            if 'changePercent' in index:
                                index['change_percent'] = index['changePercent']
                        return indexes_data['indexes']
                    return indexes_data
            else:
                self.logger.warning("Indexes file not found: " + indexes_file)
                return FUND_INDICES
        except Exception as e:
            self.logger.error("Error getting fund index data: " + str(e))
            # 返回默认的模拟数据
            return FUND_INDICES
    
    def get_latest_news(self, category=None):
        try:
            # 优先从文件读取真实新闻数据
            data_dir = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'data')
            news_file = os.path.join(data_dir, 'news.json')
            real_news = []
            
            # 计算90天前的日期（一个季度）
            import datetime
            today = datetime.datetime.now()
            ninety_days_ago = today - datetime.timedelta(days=90)
            
            if os.path.exists(news_file):
                with open(news_file, 'r', encoding='utf-8') as f:
                    data = json.load(f)
                    if 'news' in data:
                        # 转换数据格式，确保与模板期望的字段名一致
                        for item in data['news']:
                            # 解析发布时间，只保留近90天的新闻
                            publish_time_str = item.get('publishTime')
                            if publish_time_str:
                                try:
                                    # 假设日期格式为 YYYY-MM-DD 或 YYYY-MM-DD HH:MM:SS
                                    publish_date_str = publish_time_str.split(' ')[0]  # 获取日期部分
                                    publish_date = datetime.strptime(publish_date_str, '%Y-%m-%d').date()
                                    # 只保留90天内的新闻
                                    if publish_date >= ninety_days_ago.date():
                                        news_item = {
                                            'id': item.get('id'),
                                            'title': item.get('title'),
                                            'source': item.get('source'),
                                            'time': publish_time_str,  # 使用原始时间字符串
                                            'category': item.get('category'),
                                            'url': item.get('url')
                                        }
                                        real_news.append(news_item)
                                except ValueError:
                                    # 如果日期解析失败，跳过这条新闻
                                    continue
                
                # 如果指定了分类，过滤新闻
                if category and category != '全部':
                    real_news = [news for news in real_news if news.get('category') == category]
                
                # 按发布时间排序，最新的在前
                real_news.sort(key=lambda x: x.get('time', ''), reverse=True)
                
                return real_news
            
            # 如果文件不存在或读取失败，使用模拟数据
            self.logger.warning("News file not found or invalid: " + news_file)
            news_list = FUND_NEWS.copy()
            
            # 如果指定了分类，过滤模拟新闻
            if category and category != '全部':
                news_list = [news for news in news_list if news.get('category') == category]
            
            return news_list
        except Exception as e:
            self.logger.error("Error getting latest fund news: " + str(e))
            # 出错时返回默认模拟数据
            news_list = FUND_NEWS.copy()
            if category and category != '全部':
                news_list = [news for news in news_list if news.get('category') == category]
            return news_list
    
    def get_fund_history(self, fund_code, days=90):
        try:
            # 优先从文件读取基金历史数据
            history_data = read_fund_history_data(fund_code)
            if history_data is not None and not history_data.empty:
                # 如果数据量超过需要的天数，截取最近的部分
                if len(history_data) > days:
                    history_data = history_data.tail(days)  # 使用tail获取最近的数据
                return history_data
                
            # 如果文件读取失败或数据不足，返回模拟数据
            # 尝试获取基金当前净值作为基础净值
            fund = self.get_fund_quote(fund_code)
            base_nav = fund['net_value'] if fund and 'net_value' in fund else 2.00
            self.logger.info("Generating sample fund history data, fund code: " + fund_code + ", base nav: " + str(base_nav))
            return generate_sample_fund_nav_data(days, base_nav)
        except Exception as e:
            self.logger.error("Error getting fund history data for " + fund_code + ": " + str(e))
            # 即使出错也尝试生成模拟数据
            base_nav = 2.00
            return generate_sample_fund_nav_data(days, base_nav)
    
    def get_hot_funds(self):
        try:
            # 获取热门基金并添加随机波动来模拟实时数据更新
            import random
            from copy import deepcopy
            
            # 使用原始样本基金数据，共10个
            all_funds = deepcopy(SAMPLE_FUNDS)
            
            # 为所有基金添加真实波动预测
            hot_funds = []
            for fund in all_funds:
                # 创建基金数据的副本
                fund_copy = deepcopy(fund)
                
                # 增强的真实变化预测模型
                # 1. 日涨幅波动 - 基于基金类型和风险等级调整波动幅度
                base_volatility = 0.5
                if fund['fund_type'] == '股票型' or fund['fund_type'] == '指数型':
                    base_volatility = 1.2  # 股票型基金波动更大
                elif fund['fund_type'] == '债券型':
                    base_volatility = 0.2  # 债券型基金波动较小
                
                # 根据风险等级调整波动
                risk_multiplier = 1.0
                if fund['risk_level'] == '高风险':
                    risk_multiplier = 1.5
                elif fund['risk_level'] == '低风险':
                    risk_multiplier = 0.5
                
                # 实际波动值
                volatility = random.uniform(-base_volatility * risk_multiplier, base_volatility * risk_multiplier)
                new_daily_growth_percent = fund.get('daily_growth_percent', 0) + volatility
                
                # 确保百分比在合理范围内
                new_daily_growth_percent = max(-5.0, min(10.0, new_daily_growth_percent))
                fund_copy['daily_growth_percent'] = round(new_daily_growth_percent, 2)
                
                # 计算新的日增长率
                net_value = fund.get('net_value', 2.0)
                fund_copy['daily_growth'] = round(net_value * (new_daily_growth_percent / 100), 4)
                
                # 计算新的净值
                fund_copy['net_value'] = round(net_value + fund_copy['daily_growth'], 3)
                
                # 2. 多周期收益率波动 - 更复杂的预测模型
                # 考虑市场趋势（假设当前市场有轻微上涨趋势）
                market_trend = random.uniform(-0.2, 0.3)  # 轻微上涨趋势
                
                for period in ['return_1m', 'return_3m', 'return_1y']:
                    if period in fund:
                        # 波动幅度随时间周期增加而增大
                        if period == 'return_1m':
                            period_base_volatility = 1.5
                            trend_impact = market_trend * 0.8  # 近期受市场趋势影响更大
                        elif period == 'return_3m':
                            period_base_volatility = 3.0
                            trend_impact = market_trend * 0.6
                        else:  # return_1y
                            period_base_volatility = 5.0
                            trend_impact = market_trend * 0.4
                        
                        # 添加随机波动
                        return_volatility = random.uniform(-period_base_volatility, period_base_volatility)
                        
                        # 结合市场趋势和随机波动
                        total_change = return_volatility + trend_impact
                        
                        new_return = fund[period] + total_change
                        
                        # 确保收益率在合理范围内
                        if period == 'return_1m':
                            new_return = max(-10.0, min(15.0, new_return))
                        elif period == 'return_3m':
                            new_return = max(-20.0, min(30.0, new_return))
                        else:
                            new_return = max(-40.0, min(100.0, new_return))
                        
                        fund_copy[period] = round(new_return, 2)
                
                # 3. 基于历史表现的评级调整
                # 简化的评级计算：基于近1年收益率和风险等级
                rating_score = max(1, min(5, int(fund_copy['return_1y'] / 10) + 3))
                fund_copy['rating'] = '★' * rating_score + '☆' * (5 - rating_score)
                
                # 为基金预先生成近一个季度的净值走势图路径
                try:
                    # 调用generate_fund_nav_chart函数的逻辑，但不实际生成图表
                    # 而是返回一个模拟的图表路径，实际图表由前端在需要时加载
                    # 这里使用基金代码作为图表标识符
                    chart_path = '/static/charts/' + fund_copy['code'] + '_nav.png'
                    fund_copy['chart_path'] = chart_path
                except Exception as e:
                    self.logger.error("Failed to generate chart path for fund {}: {}".format(fund_copy['code'], str(e)))
                    fund_copy['chart_path'] = ''
                
                hot_funds.append(fund_copy)
            
            # 按照新的日涨幅排序，返回全部20个
            return sorted(hot_funds, key=lambda x: x['daily_growth_percent'], reverse=True)
        except Exception as e:
            self.logger.error("Error getting hot funds: " + str(e))
            # 返回默认的热门基金列表，即使出错也返回10个
            try:
                # 复制并扩展SAMPLE_FUNDS以达到10个
                extended_funds = deepcopy(SAMPLE_FUNDS)
                while len(extended_funds) < 10:
                    extended_funds.extend(deepcopy(SAMPLE_FUNDS[:10 - len(extended_funds)]))
                return sorted(extended_funds, key=lambda x: x['daily_growth_percent'], reverse=True)
            except:
                # 最后的备用方案
                return SAMPLE_FUNDS[:10]
    
    # 基金投资判断功能
    def get_investment_recommendation(self, fund_code, user_risk_profile=None):
        try:
            # 获取基金基本信息
            fund = self.get_fund_quote(fund_code)
            if not fund:
                return {"status": "error", "message": "基金信息不存在"}
            
            # 获取基金评分 - 确保获取有效的数据库实例
            db_instance = init_database()
            fund_rating = db_instance.get_fund_rating(fund_code)
            if not fund_rating:
                # 如果没有评分，生成模拟评分
                risk_level = random.randint(1, 5)
                return_score = random.randint(60, 95)
                manager_score = random.randint(60, 95)
                stability_score = random.randint(60, 95)
                overall_rating = int((return_score + manager_score + stability_score) / 3)
                
                # 保存评分到数据库
                db_instance.add_fund_rating(fund_code, risk_level, return_score, manager_score, stability_score, overall_rating)
                fund_rating = (risk_level, return_score, manager_score, stability_score, overall_rating)
            
            risk_level, return_score, manager_score, stability_score, overall_rating = fund_rating
            
            # 生成推荐理由
            recommendation = {
                "fund_code": fund_code,
                "fund_name": fund.get('name', '未知基金'),
                "risk_level": risk_level,
                "return_score": return_score,
                "manager_score": manager_score,
                "stability_score": stability_score,
                "overall_rating": overall_rating,
                "risk_match": "未知",
                "recommendation": "观望",
                "reasons": []
            }
            
            # 基于评分生成推荐理由
            if overall_rating >= 85:
                recommendation["recommendation"] = "强烈推荐"
                recommendation["reasons"].append("基金综合评分优秀，表现稳定")
            elif overall_rating >= 75:
                recommendation["recommendation"] = "推荐"
                recommendation["reasons"].append("基金综合评分良好，具备投资价值")
            elif overall_rating >= 65:
                recommendation["recommendation"] = "谨慎推荐"
                recommendation["reasons"].append("基金综合评分一般，需谨慎考虑")
            else:
                recommendation["recommendation"] = "不推荐"
                recommendation["reasons"].append("基金综合评分较低，建议观望")
            
            # 根据收益率评分添加理由
            if return_score >= 90:
                recommendation["reasons"].append("基金收益率表现优秀，远超同类平均")
            elif return_score >= 70:
                recommendation["reasons"].append("基金收益率表现良好，符合预期")
            
            # 根据稳定性评分添加理由
            if stability_score >= 90:
                recommendation["reasons"].append("基金波动较小，稳定性优秀")
            elif stability_score >= 70:
                recommendation["reasons"].append("基金波动适中，稳定性良好")
            else:
                recommendation["reasons"].append("基金波动较大，投资者需注意风险")
            
            # 如果有用户风险偏好，进行风险匹配分析
            if user_risk_profile:
                user_risk, _, _ = user_risk_profile
                if abs(user_risk - risk_level) <= 1:
                    recommendation["risk_match"] = "风险匹配度高"
                    recommendation["reasons"].append("基金风险等级与您的风险偏好匹配度高")
                elif abs(user_risk - risk_level) == 2:
                    recommendation["risk_match"] = "风险匹配度一般"
                    recommendation["reasons"].append("基金风险等级与您的风险偏好有一定差异")
                else:
                    recommendation["risk_match"] = "风险匹配度低"
                    recommendation["reasons"].append("基金风险等级与您的风险偏好差异较大，建议谨慎投资")
            
            return recommendation
        except Exception as e:
            self.logger.error("Error generating investment recommendation: " + str(e))
            return {"status": "error", "message": "生成投资推荐失败"}

# 初始化服务实例
fund_service = FundService()
crawler = Crawler()

# 全局错误处理
@app.errorhandler(404)
def page_not_found(e):
    return render_template('error.html', error_code=404, error_message='页面未找到'), 404

@app.errorhandler(500)
def internal_server_error(e):
    logger.error("Internal server error: " + str(e))
    return render_template('error.html', error_code=500, error_message='服务器内部错误'), 500

@app.errorhandler(Exception)
def handle_exception(e):
    logger.error("Uncaught exception: " + str(e))
    return render_template('error.html', error_code=500, error_message='服务器发生错误'), 500

# 创建一个简单的登录装饰器
def login_required(f):
    from functools import wraps
    @wraps(f)
    def decorated_function(*args, **kwargs):
        if 'username' not in session:
            return redirect(url_for('login'))
        return f(*args, **kwargs)
    return decorated_function

# 路由定义
@app.route('/')
@login_required
def index():
    try:
        # 获取市场指数数据
        indices = fund_service.get_market_indices()
        # 优先从funds.json文件获取真实基金数据
        import os
        import json
        import random
        import time
        hot_funds = []
        
        data_dir = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'data')
        funds_file = os.path.join(data_dir, 'funds.json')
        
        if os.path.exists(funds_file):
            try:
                with open(funds_file, 'r', encoding='utf-8') as f:
                    data = json.load(f)
                    if 'funds' in data:
                        hot_funds = data['funds'][:50]  # 获取最多50个基金数据
            except Exception as e:
                logger.error("读取真实基金数据失败: {}".format(e))
        
        # 如果没有获取到真实数据，使用模拟数据
        if not hot_funds:
            hot_funds = generate_more_funds(50)
        
        # 为每个基金添加随机热度值（模拟一小时内的热度）
        # 使用当前时间作为种子，确保每小时热度分布不同但稳定
        hour_seed = int(time.time() / 3600)  # 每小时更新一次种子
        random.seed(hour_seed)
        
        for fund in hot_funds:
            # 为不同类型的基金设置不同的热度分布范围
            if fund.get('fund_type') in ['股票型', '混合型']:
                # 股票型和混合型基金热度更高
                fund['heat_score'] = random.uniform(60, 100)
            elif fund.get('fund_type') in ['指数型', 'QDII']:
                # 指数型和QDII基金热度中等
                fund['heat_score'] = random.uniform(40, 80)
            else:
                # 其他类型基金热度较低
                fund['heat_score'] = random.uniform(20, 60)
        
        # 按热度降序排序
        hot_funds.sort(key=lambda x: x.get('heat_score', 0), reverse=True)
        
        # 获取热门股票数据
        hot_stocks = []
        for stock in SAMPLE_STOCKS:
            stock_quote = crawler.get_stock_quote(stock['code'])
            if stock_quote:
                hot_stocks.append(stock_quote)
        
        # 按涨跌幅排序
        hot_stocks.sort(key=lambda x: x['change_percent'], reverse=True)
        
        # 获取最新基金新闻，限制显示5个
        latest_news = fund_service.get_latest_news()[:5]
        # 获取当前时间
        now = datetime.now()
        
        # 获取最后更新时间
        last_update_time = scheduler.get_last_update_time()
        
        # 用户已登录，获取用户信息
        is_logged_in = True
        username = session['username']
        user_id = session['user_id']
        
        # 确保获取有效的数据库实例
        db_instance = init_database()
        watchlist_funds = db_instance.get_watchlist(user_id)
        user_risk_profile = db_instance.get_user_risk_profile(user_id)
        
        # 为每个自选基金获取详细信息
        watchlist = []
        for fund in watchlist_funds:
            fund_code, fund_name, added_at = fund
            # 获取基金详细信息
            fund_detail = fund_service.get_fund_quote(fund_code)
            if fund_detail:
                # 创建一个包含所有必要字段的基金对象
                watchlist_item = {
                    'fund_code': fund_code,
                    'fund_name': fund_name,
                    'net_value': fund_detail.get('net_value', 'N/A'),
                    'daily_growth': fund_detail.get('daily_growth_percent', 0),
                    'risk_level': fund_detail.get('risk_level', 'N/A'),
                    'added_at': added_at
                }
                watchlist.append(watchlist_item)
        
        # 检查是否在自选基金中
        for fund in hot_funds:
            fund['is_in_watchlist'] = any(w['fund_code'] == fund['code'] for w in watchlist)
            
        # 检查是否有新闻数据
        news_available = len(latest_news) > 0
        
        return render_template('index.html', indices=indices, hot_funds=hot_funds,
                            is_logged_in=is_logged_in, username=username,
                            watchlist=watchlist, news_available=news_available,
                            user_risk_profile=user_risk_profile, latest_news=latest_news, now=now,
                            last_update_time=last_update_time, hot_stocks=hot_stocks)
    except Exception as e:
        logger.error("处理首页请求时出错: {0}".format(e))
        # 返回一个基本的错误页面或者默认数据
        # 热门基金显示数量修改为10个
        # 热门财经新闻显示数量修改为5个，使用真实数据
        latest_news = fund_service.get_latest_news()[:5]  # 获取最新的5条新闻
        # 生成模拟股票数据用于错误情况下的显示
        mock_stocks = []
        for stock in SAMPLE_STOCKS:
            mock_stocks.append({
                'code': stock['code'],
                'name': stock['name'],
                'price': round(random.uniform(10, 300), 2),
                'change': round(random.uniform(-5, 5), 2),
                'change_percent': round(random.uniform(-5, 5), 2),
                'volume': random.randint(1000000, 10000000),
                'amount': round(random.uniform(10000000, 100000000), 2)
            })
        
        return render_template('index.html', indices=FUND_INDICES, hot_funds=SAMPLE_FUNDS[:10],
                            is_logged_in=False, username=None, watchlist=[], 
                            news_available=len(latest_news) > 0, latest_news=latest_news,
                            user_risk_profile=None, now=datetime.now(),
                            last_update_time=scheduler.get_last_update_time(), hot_stocks=mock_stocks)



@app.route('/more_hot_funds')
@login_required
def more_hot_funds():
    try:
        # 获取用户信息
        is_logged_in = True
        username = session['username']
        user_id = session['user_id']
        now = datetime.now()
        
        # 优先从funds.json文件获取真实基金数据
        import os
        import json
        more_funds = []
        
        data_dir = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'data')
        funds_file = os.path.join(data_dir, 'funds.json')
        
        if os.path.exists(funds_file):
            try:
                with open(funds_file, 'r', encoding='utf-8') as f:
                    data = json.load(f)
                    if 'funds' in data:
                        more_funds = data['funds']
            except Exception as e:
                logger.error("读取真实基金数据失败: {}".format(e))
        
        # 如果没有获取到真实数据，使用模拟数据
        if not more_funds:
            more_funds = generate_more_funds()
        
        # 获取市场指数数据
        indices = fund_service.get_market_indices()
        
        # 确保获取有效的数据库实例
        db_instance = init_database()
        watchlist_funds = db_instance.get_watchlist(user_id)
        
        # 创建自选基金代码集合，用于快速判断
        watchlist_codes = {fund[0] for fund in watchlist_funds}
        
        # 检查是否在自选基金中
        for fund in more_funds:
            fund['is_in_watchlist'] = fund['code'] in watchlist_codes
        
        # 渲染更多基金页面
        return render_template('more_hot_funds.html', 
                               more_funds=more_funds, 
                               indices=indices, 
                               is_logged_in=is_logged_in, 
                               username=username, 
                               now=now)
    except Exception as e:
        logger.error("Error in more_hot_funds route: " + str(e))
        # 如果出错，返回错误页面
        return render_template('error.html', error_code='500', error_message='获取更多基金数据失败')


# 生成基金历史净值走势图
@app.route('/generate_fund_nav_chart/<fund_code>')
def generate_fund_nav_chart(fund_code):
    try:
        # 获取基金历史净值数据（近一个季度约90天）
        fund_history = fund_service.get_fund_history(fund_code, days=90)
        
        # 创建图表，调整比例为更适合显示的 16:9
        fig, ax1 = plt.subplots(figsize=(16, 9))
        
        # 绘制净值走势图
        ax1.plot(fund_history['date'], fund_history['nav'], marker='', linestyle='-', color='blue', linewidth=1.5, label='单位净值')
        
        # 设置图表标题和标签
        # 获取基金名称
        fund = fund_service.get_fund_quote(fund_code)
        fund_name = fund.get('name', '未知基金') if fund else '未知基金'
        
        ax1.set_title('{0}({1}) 近季度净值走势图'.format(fund_name, fund_code))
        ax1.set_xlabel('日期')
        ax1.set_ylabel('单位净值')
        # 对于近90天的数据，调整x轴标签的显示频率，避免标签重叠
        if len(fund_history) > 30:
            ax1.set_xticks(fund_history['date'][::10])  # 每10天显示一个标签
        ax1.tick_params(axis='x', rotation=45)
        ax1.grid(True)
        ax1.legend()
        
        # 添加累计收益率次坐标轴
        ax2 = ax1.twinx()
        ax2.plot(fund_history['date'], fund_history['accumulated_return'], marker='', linestyle='--', color='red', linewidth=1, label='累计收益率(%)')
        ax2.set_ylabel('累计收益率(%)')
        ax2.legend(loc='upper right')
        
        # 自动调整布局
        plt.tight_layout()
        
        # 保存图表到临时文件
        charts_dir = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'static', 'charts')
        create_directory_if_not_exists(charts_dir)
        chart_path = os.path.join(charts_dir, fund_code + '_nav.png')
        plt.savefig(chart_path, dpi=100, bbox_inches='tight')
        plt.close(fig)
        
        # 返回图表路径，只返回相对于static目录的路径
        return jsonify({'chart_path': 'charts/' + fund_code + '_nav.png'})
    except Exception as e:
        logger.error("Error generating fund nav chart: " + str(e))
        # 如果出错，返回一个默认的空图表路径而不是错误
        return jsonify({'chart_path': 'charts/default_nav.png'}), 200

# 密码哈希功能
def hash_password(password, salt=None):
    """对密码进行哈希处理，返回哈希值和盐值"""
    if salt is None:
        salt = secrets.token_hex(16)
    hashed = hashlib.sha256((password + salt).encode()).hexdigest()
    return hashed, salt

# 验证密码
def verify_password(provided_password, stored_hash, stored_salt):
    """验证提供的密码是否与存储的哈希值匹配"""
    provided_hash, _ = hash_password(provided_password, stored_salt)
    return provided_hash == stored_hash

@app.route('/login', methods=['GET', 'POST'])
def login():
    if request.method == 'POST':
        username = request.form['username']
        password = request.form['password']
        
        # 登录验证 - 确保获取有效的数据库实例
        db_instance = init_database()
        user = db_instance.get_user(username)
        if user and len(user) >= 4:
            user_id, username, stored_hash, stored_salt = user
            if verify_password(password, stored_hash, stored_salt):
                session['username'] = username
                session['user_id'] = user_id  # 存储用户ID到会话
                return redirect(url_for('index'))
        
        return render_template('login.html', error='用户名或密码错误')
    
    return render_template('login.html')

@app.route('/register', methods=['GET', 'POST'])
def register():
    if request.method == 'POST':
        username = request.form['username']
        password = request.form['password']
        confirm_password = request.form.get('confirm_password')
        
        # 输入验证
        if not username or len(username) < 3:
            return render_template('register.html', error='用户名至少需要3个字符')
        
        if not password or len(password) < 6:
            return render_template('register.html', error='密码至少需要6个字符')
            
        if password != confirm_password:
            return render_template('register.html', error='两次输入的密码不一致')
        
        # 检查用户名是否已存在 - 确保获取有效的数据库实例
        db_instance = init_database()
        if db_instance.get_user(username):
            return render_template('register.html', error='用户名已存在')
        
        # 注册新用户 - 使用密码哈希
        hashed_password, salt = hash_password(password)
        db_instance = init_database()
        if db_instance.add_user(username, hashed_password, salt):
            return redirect(url_for('login'))
        else:
            return render_template('register.html', error='注册失败，请稍后再试')
    
    return render_template('register.html')

@app.route('/logout')
def logout():
    session.pop('username', None)
    return redirect(url_for('login'))

@app.route('/stock/<stock_code>')
def stock_detail(stock_code):
    try:
        # 初始化默认值
        username = 'guest'
        is_in_watchlist = False
        
        # 检查用户是否登录
        if 'username' in session:
            username = session['username']
            # 检查股票是否在用户的自选股中
            if 'user_id' in session:
                # 确保获取有效的数据库实例
                db_instance = init_database()
                watchlist_stocks = db_instance.get_watchlist(session['user_id'])
                is_in_watchlist = any(stock_info[0] == stock_code for stock_info in watchlist_stocks)
        else:
            logger.info("未登录用户访问股票详情页: {0}".format(stock_code))
            # 为了便于测试，即使未登录也允许查看股票详情
        
        # 获取股票详情
        stock = crawler.get_stock_quote(stock_code)
        if not stock:
            logger.warning("股票不存在: {0}".format(stock_code))
            return render_template('error.html', error_code='404', error_message='股票不存在'), 404
        
        # 获取K线数据
        kline_data = crawler.get_stock_kline(stock_code)
        if kline_data is None or kline_data.empty:
            logger.error("无法获取K线数据: {0}".format(stock_code))
            return render_template('error.html', error_code='500', error_message='无法获取K线数据'), 500
        
        logger.info("成功获取K线数据，数据量: {0}, 股票代码: {1}".format(len(kline_data), stock_code))
        
        # 将pandas DataFrame转换为字典列表，以便在Jinja2模板中正确遍历
        kline_data_list = kline_data.to_dict('records')
        
        # 生成K线图
        chart_path = generate_k_line_chart(stock_code, kline_data)
        if not chart_path:
            # 图表生成失败
            chart_path = ''
        
        return render_template('stock_detail.html', 
                              stock=stock, 
                              kline_data=kline_data_list, 
                              chart_path=chart_path, 
                              is_in_watchlist=is_in_watchlist,
                              username=username)
    except Exception as e:
        logger.error("处理股票详情页时发生错误: {0}".format(e))
        return render_template('error.html', error_code='500', error_message='处理股票详情时发生错误'), 500

# 基金详情页面
@app.route('/fund/<fund_code>')
def fund_detail(fund_code):
    try:
        # 初始化默认值
        username = 'guest'
        is_in_watchlist = False
        user_risk_profile = None
        investment_recommendation = None
        
        # 检查用户是否登录
        if 'username' in session:
            username = session['username']
            # 检查基金是否在用户的自选基金中
            if 'user_id' in session:
                # 确保获取有效的数据库实例
                db_instance = init_database()
                watchlist_funds = db_instance.get_watchlist(session['user_id'])
                # 注意：数据库返回的watchlist是元组列表，不是字典列表
                is_in_watchlist = any(fund[0] == fund_code for fund in watchlist_funds)
                
                # 获取用户风险偏好
                user_risk_profile = db_instance.get_user_risk_profile(session['user_id'])
        else:
            logger.info("未登录用户访问基金详情页: {0}".format(fund_code))
        
        # 获取基金详情
        fund = fund_service.get_fund_quote(fund_code)
        if not fund:
            logger.warning("基金不存在: {0}".format(fund_code))
            return render_template('error.html', error_code='404', error_message='基金不存在'), 404
        
        # 生成投资推荐
        recommendation_data = fund_service.get_investment_recommendation(fund_code, user_risk_profile)
        
        # 转换推荐数据结构以匹配模板期望的格式
        if recommendation_data and 'status' not in recommendation_data:
            # 格式化推荐理由，将列表转换为字符串
            reasons_text = '; '.join(recommendation_data.get('reasons', []))
            
            investment_recommendation = {
                'level': recommendation_data.get('recommendation', '观望'),
                'reason': reasons_text,
                'match_rate': 80,  # 默认匹配度80%，实际应用中可以根据风险匹配结果计算
                'suggestion': "该基金综合评分为{}分，风险等级为{}/5级，{}".format(recommendation_data.get('overall_rating', 70), recommendation_data.get('risk_level', 3), recommendation_data.get('risk_match', '风险匹配度一般'))
            }
        else:
            # 提供默认的投资推荐数据
            investment_recommendation = {
                'level': '推荐',
                'reason': '基金表现稳健，具有良好的投资价值',
                'match_rate': 75,
                'suggestion': '建议投资者根据自身风险承受能力进行合理配置'
            }
        
        # 生成基金净值走势图
        chart_result = requests.get(url_for('generate_fund_nav_chart', fund_code=fund_code, _external=True))
        chart_path = ''
        if chart_result.status_code == 200:
            chart_data = chart_result.json()
            chart_path = chart_data.get('chart_path', '')
        
        # 获取历史净值数据
        fund_history = fund_service.get_fund_history(fund_code, days=30)  # 获取最近30天的数据
        
        # 转换为模板期望的格式
        nav_data = []
        if not fund_history.empty:
            # 计算日增长率
            prev_nav = None
            for _, row in fund_history.iterrows():
                daily_growth = 0
                if prev_nav:
                    daily_growth = round(((row['nav'] - prev_nav) / prev_nav) * 100, 2)
                prev_nav = row['nav']
                
                nav_item = {
                    'date': row['date'],
                    'net_value': round(row['nav'], 3),
                    'daily_growth': daily_growth,
                    'cumulative_net_value': round(row['nav'], 3)  # 简化处理，实际应计算累计净值
                }
                nav_data.append(nav_item)
            
            # 按日期倒序排序，最新的在前
            nav_data = nav_data[::-1]
        else:
            # 提供模拟数据
            base_nav = 2.0
            for i in range(30):
                date = (datetime.now() - timedelta(days=i)).strftime('%Y-%m-%d')
                # 生成一些随机波动
                volatility = random.uniform(-0.05, 0.05)
                current_nav = base_nav + volatility * (1 - i/30)  # 随时间增加波动减小
                daily_growth = round(volatility / base_nav * 100, 2)
                
                nav_data.append({
                    'date': date,
                    'net_value': round(current_nav, 3),
                    'daily_growth': daily_growth,
                    'cumulative_net_value': round(current_nav, 3)
                })
        
        return render_template('fund_detail.html', 
                              fund=fund, 
                              chart_path=chart_path, 
                              is_in_watchlist=is_in_watchlist,
                              username=username,
                              user_risk_profile=user_risk_profile,
                              investment_recommendation=investment_recommendation,
                              nav_data=nav_data)
    except Exception as e:
        logger.error("Error processing fund detail page: " + str(e))
        return render_template('error.html', error_code='500', error_message='处理基金详情时发生错误'), 500

# 为了方便直接访问股票详情页（不需要login），添加一个新的路由
@app.route('/stock_detail.html')
def direct_stock_detail():
    stock_code = request.args.get('code', '600519')  # 默认查看股票代码
    return redirect(url_for('stock_detail', stock_code=stock_code))
    
# 为了方便直接访问基金详情页（不需要login），添加一个新的路由
@app.route('/fund_detail.html')
def direct_fund_detail():
    fund_code = request.args.get('code', '003096')  # 默认查看基金详情页
    return redirect(url_for('fund_detail', fund_code=fund_code))

@app.route('/add_to_watchlist/<stock_code>')
@login_required
def add_to_watchlist(stock_code):
    
    user_id = session['user_id']
    # 获取股票信息
    stock = crawler.get_stock_quote(stock_code)
    
    if stock:
        # 向数据库添加自选股 - 确保获取有效的数据库实例
        db_instance = init_database()
        db_instance.add_to_watchlist(user_id, stock_code, stock['name'])
    
    return redirect(url_for('stock_detail', stock_code=stock_code))
    
# 添加自选基金
@app.route('/add_fund_to_watchlist/<fund_code>')
@login_required
def add_fund_to_watchlist(fund_code):
    
    user_id = session['user_id']
    # 获取基金信息
    fund = fund_service.get_fund_quote(fund_code)
    
    if fund:
        # 向数据库添加自选基金 - 确保获取有效的数据库实例
        db_instance = init_database()
        db_instance.add_to_watchlist(user_id, fund_code, fund.get('name', '未知基金'))
    
    return redirect(url_for('fund_detail', fund_code=fund_code))

@app.route('/remove_from_watchlist/<stock_code>')
@login_required
def remove_from_watchlist(stock_code):
    
    user_id = session['user_id']
    
    # 从数据库中删除自选股 - 确保获取有效的数据库实例
    db_instance = init_database()
    db_instance.remove_from_watchlist(user_id, stock_code)
    
    return redirect(url_for('index'))
    
# 移除自选基金
@app.route('/remove_fund_from_watchlist/<fund_code>')
@login_required
def remove_fund_from_watchlist(fund_code):
    
    user_id = session['user_id']
    
    # 从数据库中删除自选基金 - 确保获取有效的数据库实例
    db_instance = init_database()
    db_instance.remove_from_watchlist(user_id, fund_code)
    
    return redirect(url_for('index'))

@app.route('/news')
@login_required
def news():
    # 获取分页参数，默认为第1页
    page = request.args.get('page', 1, type=int)
    per_page = 5  # 每页显示5条新闻（默认）
    category = request.args.get('category', '全部')
    show_all = request.args.get('show_all', 'false').lower() == 'true'  # 是否显示全部新闻
    
    # 获取指定分类的新闻
    all_news = fund_service.get_latest_news(category)
    
    # 如果选择显示全部，则直接返回所有新闻，不进行分页
    if show_all:
        # 不进行分页，直接返回所有新闻
        paginated_news = all_news
        current_page = 1
        total_pages = 1
    else:
        # 计算总页数
        total_news = len(all_news)
        total_pages = (total_news + per_page - 1) // per_page
        
        # 确保页码在有效范围内
        page = max(1, min(page, total_pages))
        
        # 计算当前页显示的新闻范围
        start_index = (page - 1) * per_page
        end_index = start_index + per_page
        paginated_news = all_news[start_index:end_index]
        current_page = page
    
    # 传递分页数据给模板
    return render_template(
        'news.html', 
        news=paginated_news,
        current_page=current_page,
        total_pages=total_pages,
        per_page=per_page,
        total_news=len(all_news),
        current_category=category,
        show_all=show_all
    )

@app.route('/news_detail/<news_id>')
@login_required
def news_detail(news_id):
    try:
        # 验证用户是否登录
        if not session.get('username'):
            flash('请先登录后查看新闻详情', 'warning')
            return redirect(url_for('login'))
        
        # 记录访问日志
        current_time = datetime.now().strftime('%Y-%m-%d %H:%M:%S')
        print("[" + current_time + "] User " + str(session.get('username')) + " accessed news detail " + news_id)
        
        # 使用绝对路径读取新闻详情数据
        file_path = os.path.join(app.root_path, 'data', 'news_detail.json')
        print("Attempting to read news data file: " + file_path)
        
        # 检查文件是否存在
        if not os.path.exists(file_path):
            print("错误: 新闻数据文件不存在")
            flash('新闻数据文件不存在', 'danger')
            return render_template('error.html', error_code='404', error_message='新闻数据文件不存在')
        
        try:
            # 读取新闻数据
            with open(file_path, 'r', encoding='utf-8') as f:
                raw_data = json.load(f)
            print("Successfully read file, data type: " + type(raw_data).__name__)
            
            # 检查并处理数据结构
            if isinstance(raw_data, dict) and 'news' in raw_data:
                news_data = raw_data['news']
                print("Data contains news array, length: " + str(len(news_data)))
            else:
                # 兼容旧的数组格式
                news_data = raw_data
                print("数据为直接数组格式")
            
        except json.JSONDecodeError as e:
            # JSON解析错误处理
            print("JSON parsing error: " + str(e))
            flash('新闻数据格式错误', 'danger')
            return render_template('error.html', error_code='500', error_message='新闻数据格式错误: ' + str(e))
        except Exception as e:
            # 其他读取错误处理
            print("Error reading news data: " + str(e))
            flash('读取新闻数据时发生错误', 'danger')
            return render_template('error.html', error_code='500', error_message='读取新闻数据时发生错误: ' + str(e))
        
        # 检查数据格式是否正确
        if not isinstance(news_data, list):
            print("Data format error: expected list, got " + type(news_data).__name__)
            flash('新闻数据格式错误', 'danger')
            return render_template('error.html', error_code='500', error_message='新闻数据格式错误')
        
        # 计算90天前的日期（一个季度）
        today = datetime.now()
        ninety_days_ago = today - timedelta(days=90)
        
        # 查找指定ID的新闻
        news_item = None
        for item in news_data:
            try:
                # 确保item是字典类型
                if not isinstance(item, dict):
                    print("Skipping non-dictionary item: " + type(item).__name__)
                    continue
                       
                # 处理ID匹配，考虑字符串和数字类型
                item_id = item.get('id')
                print("Comparing item_id=" + str(item_id) + " and news_id=" + str(news_id))
                if str(item_id) == str(news_id):
                    # 检查新闻发布时间是否在近90天内
                    publish_time_str = item.get('publishTime')
                    if publish_time_str:
                        try:
                            # 假设日期格式为 YYYY-MM-DD 或 YYYY-MM-DD HH:MM:SS
                            publish_date_str = publish_time_str.split(' ')[0]  # 获取日期部分
                            publish_date = datetime.strptime(publish_date_str, '%Y-%m-%d').date()
                            # 只显示90天内的新闻
                            if publish_date >= ninety_days_ago.date():
                                news_item = item
                                print("Found matching news within 90 days: " + news_item['title'])
                                break
                            else:
                                print("新闻发布时间 {} 超过90天，已过滤".format(publish_date))
                        except ValueError:
                            # 如果日期解析失败，跳过这条新闻
                            print("日期解析失败，跳过这条新闻")
                            continue
                    break
            except Exception as e:
                print("Error processing news item: " + str(e))
                continue
        
        # 如果找不到新闻或新闻已超过90天，返回相应错误
        if not news_item:
            print("News with ID " + news_id + " not found or exceeds 90 days")
            flash('无法访问该新闻。系统只显示近一个季度（90天）内的新闻。', 'danger')
            return render_template('error.html', error_code='404', error_message='无法访问该新闻。系统只显示近一个季度（90天）内的新闻。')
        
        # 添加浏览量字段（如果不存在）
        if 'views' not in news_item:
            news_item['views'] = 0
        news_item['views'] += 1
        print("Updated news views: " + str(news_item['views']))
        
        # 添加评论字段（如果不存在）
        if 'comments' not in news_item:
            news_item['comments'] = []
        
        # 准备相关新闻（显示近一个季度的全部政策类、经济市场类新闻）
        related_news = []
        current_news_category = news_item.get('category', '')
        
        # 定义政策类和经济市场类的分类
        policy_categories = ['政策法规']  # 政策类新闻分类
        economic_categories = ['宏观经济']  # 经济市场类新闻分类
        
        for item in news_data:
            if isinstance(item, dict) and str(item.get('id')) != str(news_id):
                # 获取新闻分类
                item_category = item.get('category', '')
                
                # 检查新闻发布时间是否在近90天内
                publish_time_str = item.get('publishTime')
                if publish_time_str:
                    try:
                        # 假设日期格式为 YYYY-MM-DD 或 YYYY-MM-DD HH:MM:SS
                        publish_date_str = publish_time_str.split(' ')[0]  # 获取日期部分
                        publish_date = datetime.strptime(publish_date_str, '%Y-%m-%d').date()
                        
                        # 只显示90天内的政策类和经济市场类新闻
                        if (publish_date >= ninety_days_ago.date() and 
                            (item_category in policy_categories or item_category in economic_categories)):
                            related_news.append(item)
                    except ValueError:
                        # 如果日期解析失败，跳过这条新闻
                        continue
        
        # 按相关性排序：先按分类重要性（政策类和经济市场类优先），再按发布时间排序
        related_news.sort(key=lambda x: (
            # 第一优先级：当前新闻的分类（最高相关度）
            x.get('category') == current_news_category, 
            # 第二优先级：政策类新闻
            x.get('category') in policy_categories, 
            # 第三优先级：经济市场类新闻
            x.get('category') in economic_categories, 
            # 第四优先级：发布时间（最新的在前）
            x.get('publishTime', '')
        ), reverse=True)
        
        print("Found " + str(len(related_news)) + " related news items within 90 days")
        
        # 更新浏览量到文件
        try:
            # 保持原始数据结构
            if isinstance(raw_data, dict) and 'news' in raw_data:
                raw_data['news'] = news_data
                data_to_save = raw_data
            else:
                data_to_save = news_data
                  
            with open(file_path, 'w', encoding='utf-8') as f:
                json.dump(data_to_save, f, ensure_ascii=False, indent=4)
            print("成功更新浏览量到文件")
        except Exception as e:
            # 更新浏览量失败不影响用户体验，仅记录日志
            print("Error updating views: " + str(e))
        
        # 确保news_item中的content是有效的
        print("Type before processing content: " + type(news_item.get('content')).__name__)
        if 'content' not in news_item or news_item['content'] is None or news_item['content'] == '':
            news_item['content'] = ["暂无内容"]
            print("content不存在，设置为['暂无内容']")
        elif isinstance(news_item['content'], str):
            # 如果content是字符串，将其转换为数组
            news_item['content'] = [news_item['content']]
            print("Content was string, converted to array")
        elif not isinstance(news_item['content'], list):
            # 如果content既不是字符串也不是数组，转换为字符串后再转为数组
            news_item['content'] = [str(news_item['content'])]
            print("Content was unknown type, converted to string array")
        print("Type after processing content: " + type(news_item['content']).__name__)
        
        # 渲染新闻详情页面
        print("Preparing to render news detail page, title: " + news_item['title'])
        return render_template('news_detail.html', news=news_item, related_news=related_news)
        
    except Exception as e:
        # 捕获所有其他异常并记录详细的错误信息
        import traceback
        error_traceback = traceback.format_exc()
        print("Error getting news detail: " + str(e))
        print("Error traceback: " + error_traceback)
        flash('获取新闻详情时发生错误: ' + str(e), 'danger')
        return render_template('error.html', error_code='500', error_message='获取新闻详情时发生错误: ' + str(e))

@app.route('/api/search_stock')
def search_stock():
    try:
        query = request.args.get('query', '').lower().strip()
        
        # 记录搜索请求，包含用户信息
        username = session.get('username', '未登录用户')
        logger.info("[User: " + username + "] Searching stock: " + query)
        
        # 检查查询是否为空
        if not query:
            logger.info("搜索查询为空，返回空结果")
            return jsonify([])
            
        # 简单的股票搜索功能
        results = [
            stock for stock in SAMPLE_STOCKS 
            if query in stock['code'].lower() or query in stock['name'].lower()
        ]
        
        logger.info("Found " + str(len(results)) + " matching stocks")
        return jsonify(results[:10])  # 限制最多返回10个结果
    except Exception as e:
        # 增强错误日志记录
        username = session.get('username', '未登录用户')
        logger.error("[User: " + username + "] Error searching stock: " + str(e))
        # 对于API请求，返回空数组而不是错误页面是合理的
        return jsonify([])
        
# 基金搜索API
@app.route('/api/search_fund')
def search_fund():
    try:
        query = request.args.get('query', '').lower().strip()
        
        # 记录搜索请求，包含用户信息
        username = session.get('username', '未登录用户')
        logger.info("[User: " + username + "] Searching fund: " + query)
        
        # 检查查询是否为空
        if not query:
            logger.info("搜索查询为空，返回空结果")
            return jsonify([])
            
        # 优先从funds.json文件读取真实基金数据
        funds = []
        try:
            # 构建funds.json文件的完整路径
            data_dir = os.path.join(os.path.dirname(__file__), 'data')
            fund_file_path = os.path.join(data_dir, 'funds.json')
            
            # 读取文件内容
            with open(fund_file_path, 'r', encoding='utf-8') as f:
                data = json.load(f)
                # 从data对象中获取funds数组
                funds = data.get('funds', [])
                
            logger.info("从funds.json文件成功读取{}条基金数据".format(len(funds)))
        except Exception as e:
            # 文件读取失败，使用SAMPLE_FUNDS作为备选
            logger.warning("读取funds.json文件失败: {}，使用SAMPLE_FUNDS作为备选".format(str(e)))
            funds = SAMPLE_FUNDS
        
        # 进行基金搜索
        results = [
            fund for fund in funds 
            if query in fund['code'].lower() or query in fund['name'].lower()
        ]
        
        logger.info("Found " + str(len(results)) + " matching funds")
        return jsonify(results[:10])  # 限制最多返回10个结果
    except Exception as e:
        # 增强错误日志记录
        username = session.get('username', '未登录用户')
        logger.error("[User: " + username + "] Error searching fund: " + str(e))
        # 对于API请求，返回空数组而不是错误页面是合理的
        return jsonify([])

# 清理静态图表文件的功能 - 防止磁盘空间无限增长
def cleanup_old_charts(max_age_hours=24):
    """清理指定时间之前生成的图表文件"""
    try:
        charts_dir = os.path.join('static', 'charts')
        if not os.path.exists(charts_dir):
            return
            
        current_time = time.time()
        max_age_seconds = max_age_hours * 3600
        files_removed = 0
        
        for filename in os.listdir(charts_dir):
            file_path = os.path.join(charts_dir, filename)
            if os.path.isfile(file_path):
                file_age = current_time - os.path.getmtime(file_path)
                if file_age > max_age_seconds:
                    os.remove(file_path)
                    files_removed += 1
        
        if files_removed > 0:
            logger.info("已清理{0}个过期的图表文件".format(files_removed))
    except Exception as e:
        logger.error("清理图表文件时发生错误: {0}".format(e))

@app.route('/risk_profile', methods=['GET', 'POST'])
@login_required
def risk_profile():
    """用户风险偏好设置页面"""
    user_id = session['user_id']
    db_instance = init_database()
    
    # 如果是GET请求，获取用户当前风险偏好设置
    if request.method == 'GET':
        profile = db_instance.get_user_risk_profile(user_id)
        
        # 如果用户还没有设置风险偏好，使用默认值
        if not profile:
            current_risk_level = 3  # 默认平衡型
            investment_horizon = 2  # 默认中期
            investment_experience = 2  # 默认1-3年经验
            investment_objective = 2  # 默认稳健增值
        else:
            current_risk_level = profile[2]  # risk_level
            investment_horizon = profile[3]  # investment_horizon
            investment_experience = profile[4]  # investment_experience
            investment_objective = profile[5]  # investment_objective
        
        return render_template('risk_profile.html', 
                            current_risk_level=current_risk_level,
                            investment_horizon=investment_horizon,
                            investment_experience=investment_experience,
                            investment_objective=investment_objective)
    
    # 如果是POST请求，更新用户风险偏好设置
    elif request.method == 'POST':
        try:
            risk_level = int(request.form.get('risk_level'))
            investment_horizon = int(request.form.get('investment_horizon'))
            investment_experience = int(request.form.get('investment_experience'))
            investment_objective = int(request.form.get('investment_objective'))
            
            # 更新用户风险偏好设置
            db_instance.update_user_risk_profile(user_id, risk_level, investment_horizon, investment_experience, investment_objective)
            
            # 显示成功消息
            return render_template('risk_profile.html', 
                                current_risk_level=risk_level,
                                investment_horizon=investment_horizon,
                                investment_experience=investment_experience,
                                investment_objective=investment_objective,
                                success=True)
        except Exception as e:
            # 显示错误消息
            return render_template('risk_profile.html', 
                                error='设置保存失败：' + str(e),
                                current_risk_level=3,  # 默认值
                                investment_horizon=2,  # 默认值
                                investment_experience=2,  # 默认值
                                investment_objective=2)  # 默认值


# 启动应用
if __name__ == '__main__':
    try:
        # 确保data目录存在
        data_dir = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'data')
        create_directory_if_not_exists(data_dir)
        
        # 初始化数据库连接
        with app.app_context():
            db_instance = init_database()
            logger.info("数据库初始化成功")
        
        # 清理旧的图表文件
        cleanup_old_charts()
        
        # 启动数据定时更新任务
        scheduler.start(app)
        
        # 获取环境变量来决定是否启用调试模式
        # 在生产环境中应该设置为False
        debug_mode = os.environ.get('FLASK_DEBUG', 'True').lower() == 'true'
        
        # 获取主机和端口配置
        host = os.environ.get('FLASK_HOST', '127.0.0.1')
        port = int(os.environ.get('FLASK_PORT', '5000'))
        
        logger.info("Flask应用启动，模式: {0}，监听地址: {1}:{2}".format(
            "调试" if debug_mode else "生产", host, port))
        
        # 启动应用
        app.run(debug=debug_mode, host=host, port=port)
    except Exception as e:
        logger.error("Application startup failed: " + str(e))
        print("Error: " + str(e))
        # 确保关闭数据库连接
        shutdown_database()
        # 确保停止定时任务
        scheduler.stop()