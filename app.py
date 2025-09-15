#!/usr/bin/env python3
# -*- coding: utf-8 -*-
from flask import Flask, render_template, request, jsonify, redirect, url_for, session
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

# 示例基金数据
SAMPLE_FUNDS = [
    {'code': '161725', 'name': '招商中证白酒指数', 'net_value': 1.680, 'daily_growth': 0.0205, 'daily_growth_percent': 1.23, 'fund_type': '指数型', 'risk_level': '中高风险', 'manager': '侯昊'},
    {'code': '001593', 'name': '天弘创业板ETF联接A', 'net_value': 1.456, 'daily_growth': -0.015, 'daily_growth_percent': -1.03, 'fund_type': '指数型', 'risk_level': '高风险', 'manager': '陈瑶'},
    {'code': '000001', 'name': '华夏成长混合', 'net_value': 2.345, 'daily_growth': 0.018, 'daily_growth_percent': 0.78, 'fund_type': '混合型', 'risk_level': '中风险', 'manager': '张帆'},
    {'code': '110011', 'name': '易方达中小盘混合', 'net_value': 3.678, 'daily_growth': 0.032, 'daily_growth_percent': 0.88, 'fund_type': '混合型', 'risk_level': '中高风险', 'manager': '张坤'},
    {'code': '001186', 'name': '易方达新丝路灵活配置', 'net_value': 1.890, 'daily_growth': -0.005, 'daily_growth_percent': -0.26, 'fund_type': '混合型', 'risk_level': '中风险', 'manager': '陈皓'},
    {'code': '000938', 'name': '国富焦点驱动灵活配置', 'net_value': 2.123, 'daily_growth': 0.021, 'daily_growth_percent': 1.00, 'fund_type': '混合型', 'risk_level': '中风险', 'manager': '赵晓东'},
    {'code': '001938', 'name': '中欧时代先锋股票A', 'net_value': 2.456, 'daily_growth': 0.015, 'daily_growth_percent': 0.62, 'fund_type': '股票型', 'risk_level': '高风险', 'manager': '周应波'},
    {'code': '005827', 'name': '易方达蓝筹精选混合', 'net_value': 2.789, 'daily_growth': 0.025, 'daily_growth_percent': 0.91, 'fund_type': '混合型', 'risk_level': '中高风险', 'manager': '张坤'}
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
    {'id': 1, 'title': '央行发布金融稳定报告，强调防范系统性风险', 'source': '中国基金报', 'time': '2023-09-15 09:30', 'url': '#', 'category': '宏观经济'},
    {'id': 2, 'title': '多只爆款基金一日售罄，市场情绪回暖', 'source': '上海证券报', 'time': '2023-09-15 08:45', 'url': '#', 'category': '基金政策'},
    {'id': 3, 'title': '公募基金规模突破27万亿，创历史新高', 'source': '证券时报', 'time': '2023-09-14 16:30', 'url': '#', 'category': '基金政策'},
    {'id': 4, 'title': '权益类基金业绩回暖，多只产品年内收益超20%', 'source': '第一财经', 'time': '2023-09-14 15:10', 'url': '#', 'category': '行业分析'},
    {'id': 5, 'title': '知名基金经理：长期看好科技创新和消费升级', 'source': '中国证券报', 'time': '2023-09-13 14:20', 'url': '#', 'category': '基金经理观点'},
    {'id': 6, 'title': '新能源行业迎来政策利好，相关基金表现抢眼', 'source': '财经网', 'time': '2023-09-13 11:30', 'url': '#', 'category': '行业分析'},
    {'id': 7, 'title': '多位基金经理展望四季度：关注低估值蓝筹股投资机会', 'source': '上海证券报', 'time': '2023-09-12 16:45', 'url': '#', 'category': '基金经理观点'},
    {'id': 8, 'title': '央行降准释放长期资金，债券基金收益有望提升', 'source': '金融时报', 'time': '2023-09-12 09:15', 'url': '#', 'category': '宏观经济'}
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
            self.logger.warning(f"股票代码{stock_code}未找到")
            return None
        except Exception as e:
            self.logger.error(f"获取股票行情出错: {e}")
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
            self.logger.error(f"获取K线数据出错: {e}")
            return pd.DataFrame()

# 初始化爬虫实例
crawler = Crawler()

# 生成模拟的基金历史净值数据
def generate_sample_fund_nav_data(days=30, base_nav=2.00):
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
                funds = json.load(f)
                for fund in funds:
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
        ax.set_title(f'{stock_code} K线图')
        ax.set_xlabel('日期')
        ax.set_ylabel('价格')
        ax.tick_params(axis='x', rotation=45)
        ax.grid(True)
        
        # 自动调整布局
        plt.tight_layout()
        
        # 保存图表到临时文件
        charts_dir = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'static', 'charts')
        create_directory_if_not_exists(charts_dir)
        chart_path = os.path.join(charts_dir, f'{stock_code}_line_{int(time.time())}.png')
        plt.savefig(chart_path)
        plt.close(fig)
        
        # 返回相对路径
        return f'/static/charts/{os.path.basename(chart_path)}'
    except Exception as e:
        logger.error(f"生成K线图时出错: {e}")
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
            if fund:
                return fund
                
            # 如果文件读取失败，返回模拟数据
            for fund in SAMPLE_FUNDS:
                if fund['code'] == fund_code:
                    return fund
            self.logger.warning(f"基金代码{fund_code}未找到，返回None")
            return None
        except Exception as e:
            self.logger.error(f"获取基金{fund_code}行情时发生错误: {e}")
            # 返回模拟数据作为备用
            for fund in SAMPLE_FUNDS:
                if fund['code'] == fund_code:
                    return fund
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
                self.logger.warning(f"Indexes file not found: {indexes_file}")
                return FUND_INDICES
        except Exception as e:
            self.logger.error(f"获取基金指数数据时出错: {e}")
            # 返回默认的模拟数据
            return FUND_INDICES
    
    def get_latest_news(self):
        try:
            # 获取最新基金相关新闻
            return FUND_NEWS
        except Exception as e:
            self.logger.error(f"获取最新基金新闻时出错: {e}")
            return FUND_NEWS
    
    def get_fund_history(self, fund_code, days=30):
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
            self.logger.info(f"使用模拟数据生成基金历史，基金代码: {fund_code}, 基础净值: {base_nav}")
            return generate_sample_fund_nav_data(days, base_nav)
        except Exception as e:
            self.logger.error(f"获取基金{fund_code}历史数据时出错: {e}")
            # 即使出错也尝试生成模拟数据
            base_nav = 2.00
            return generate_sample_fund_nav_data(days, base_nav)
    
    def get_hot_funds(self):
        try:
            # 获取热门基金
            # 按照日涨幅排序
            return sorted(SAMPLE_FUNDS, key=lambda x: x['daily_growth_percent'], reverse=True)[:5]
        except Exception as e:
            self.logger.error(f"获取热门基金时出错: {e}")
            # 返回默认的热门基金列表
            return sorted(SAMPLE_FUNDS, key=lambda x: x['daily_growth_percent'], reverse=True)[:5]
    
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
                    recommendation["reasons"].append(f"基金风险等级与您的风险偏好匹配度高")
                elif abs(user_risk - risk_level) == 2:
                    recommendation["risk_match"] = "风险匹配度一般"
                    recommendation["reasons"].append(f"基金风险等级与您的风险偏好有一定差异")
                else:
                    recommendation["risk_match"] = "风险匹配度低"
                    recommendation["reasons"].append(f"基金风险等级与您的风险偏好差异较大，建议谨慎投资")
            
            return recommendation
        except Exception as e:
            self.logger.error(f"生成投资推荐时出错: {e}")
            return {"status": "error", "message": "生成投资推荐失败"}

# 初始化基金服务实例
fund_service = FundService()

# 全局错误处理
@app.errorhandler(404)
def page_not_found(e):
    return render_template('error.html', error_code=404, error_message='页面未找到'), 404

@app.errorhandler(500)
def internal_server_error(e):
    logger.error(f"服务器内部错误: {e}")
    return render_template('error.html', error_code=500, error_message='服务器内部错误'), 500

@app.errorhandler(Exception)
def handle_exception(e):
    logger.error(f"未捕获的异常: {e}")
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
        # 获取热门基金
        hot_funds = fund_service.get_hot_funds()
        # 获取最新基金新闻
        latest_news = fund_service.get_latest_news()
        # 获取当前时间
        now = datetime.now()
        
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
                            user_risk_profile=user_risk_profile, latest_news=latest_news, now=now)
    except Exception as e:
        logger.error("处理首页请求时出错: {0}".format(e))
        # 返回一个基本的错误页面或者默认数据
        return render_template('index.html', indices=FUND_INDICES, hot_funds=SAMPLE_FUNDS[:5],
                            is_logged_in=False, username=None, watchlist=[], 
                            news_available=len(FUND_NEWS) > 0, latest_news=FUND_NEWS,
                            user_risk_profile=None, now=datetime.now())

# 生成基金历史净值走势图
@app.route('/generate_fund_nav_chart/<fund_code>')
def generate_fund_nav_chart(fund_code):
    try:
        # 获取基金历史净值数据
        fund_history = fund_service.get_fund_history(fund_code)
        
        # 创建图表
        fig, ax1 = plt.subplots(figsize=(12, 6))
        
        # 绘制净值走势图
        ax1.plot(fund_history['date'], fund_history['nav'], marker='o', linestyle='-', color='blue', label='单位净值')
        
        # 设置图表标题和标签
        # 获取基金名称
        fund = fund_service.get_fund_quote(fund_code)
        fund_name = fund.get('name', '未知基金') if fund else '未知基金'
        
        ax1.set_title(f'{fund_name}({fund_code}) 历史净值走势图')
        ax1.set_xlabel('日期')
        ax1.set_ylabel('单位净值')
        ax1.tick_params(axis='x', rotation=45)
        ax1.grid(True)
        ax1.legend()
        
        # 添加累计收益率次坐标轴
        ax2 = ax1.twinx()
        ax2.plot(fund_history['date'], fund_history['accumulated_return'], marker='s', linestyle='--', color='red', label='累计收益率(%)')
        ax2.set_ylabel('累计收益率(%)')
        ax2.legend(loc='upper right')
        
        # 自动调整布局
        plt.tight_layout()
        
        # 保存图表到临时文件
        charts_dir = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'static', 'charts')
        create_directory_if_not_exists(charts_dir)
        chart_path = os.path.join(charts_dir, f'{fund_code}_nav.png')
        plt.savefig(chart_path)
        plt.close(fig)
        
        # 返回图表路径
        return jsonify({'chart_path': f'/static/charts/{fund_code}_nav.png'})
    except Exception as e:
        logger.error(f"生成基金净值走势图时出错: {e}")
        return jsonify({'error': str(e)}), 500

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
        investment_recommendation = fund_service.get_investment_recommendation(fund_code, user_risk_profile)
        
        # 生成基金净值走势图
        chart_result = requests.get(url_for('generate_fund_nav_chart', fund_code=fund_code, _external=True))
        chart_path = ''
        if chart_result.status_code == 200:
            chart_data = chart_result.json()
            chart_path = chart_data.get('chart_path', '')
        
        return render_template('fund_detail.html', 
                              fund=fund, 
                              chart_path=chart_path, 
                              is_in_watchlist=is_in_watchlist,
                              username=username,
                              user_risk_profile=user_risk_profile,
                              investment_recommendation=investment_recommendation)
    except Exception as e:
        logger.error(f"处理基金详情页时发生错误: {e}")
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
    # 获取所有基金相关新闻
    all_news = fund_service.get_latest_news()
    
    return render_template('news.html', news=all_news)

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
        print(f"[{current_time}] 用户 {session.get('username')} 访问新闻详情 {news_id}")
        
        # 使用绝对路径读取新闻详情数据
        file_path = os.path.join(app.root_path, 'data', 'news_detail.json')
        print(f"尝试读取新闻数据文件: {file_path}")
        
        # 检查文件是否存在
        if not os.path.exists(file_path):
            print("错误: 新闻数据文件不存在")
            flash('新闻数据文件不存在', 'danger')
            return render_template('error.html', error_code='404', error_message='新闻数据文件不存在')
        
        try:
            # 读取新闻数据
            with open(file_path, 'r', encoding='utf-8') as f:
                raw_data = json.load(f)
            print(f"成功读取文件，数据类型: {type(raw_data).__name__}")
            
            # 检查并处理数据结构
            if isinstance(raw_data, dict) and 'news' in raw_data:
                news_data = raw_data['news']
                print(f"数据包含news数组，长度: {len(news_data)}")
            else:
                # 兼容旧的数组格式
                news_data = raw_data
                print("数据为直接数组格式")
            
        except json.JSONDecodeError as e:
            # JSON解析错误处理
            print(f"JSON解析错误: {str(e)}")
            flash('新闻数据格式错误', 'danger')
            return render_template('error.html', error_code='500', error_message=f'新闻数据格式错误: {str(e)}')
        except Exception as e:
            # 其他读取错误处理
            print(f"读取新闻数据时发生错误: {str(e)}")
            flash('读取新闻数据时发生错误', 'danger')
            return render_template('error.html', error_code='500', error_message=f'读取新闻数据时发生错误: {str(e)}')
        
        # 检查数据格式是否正确
        if not isinstance(news_data, list):
            print(f"数据格式错误: 期望列表，得到 {type(news_data).__name__}")
            flash('新闻数据格式错误', 'danger')
            return render_template('error.html', error_code='500', error_message='新闻数据格式错误')
        
        # 查找指定ID的新闻
        news_item = None
        for item in news_data:
            try:
                # 确保item是字典类型
                if not isinstance(item, dict):
                    print(f"跳过非字典项: {type(item).__name__}")
                    continue
                      
                # 处理ID匹配，考虑字符串和数字类型
                item_id = item.get('id')
                print(f"比较item_id={item_id}和news_id={news_id}")
                if str(item_id) == str(news_id):
                    news_item = item
                    print(f"找到匹配的新闻: {news_item['title']}")
                    break
            except Exception as e:
                print(f"处理新闻项时出错: {str(e)}")
                continue
        
        # 如果找不到新闻，返回404错误
        if not news_item:
            print(f"未找到ID为 {news_id} 的新闻")
            flash(f'未找到ID为 {news_id} 的新闻', 'danger')
            return render_template('error.html', error_code='404', error_message=f'未找到ID为 {news_id} 的新闻')
        
        # 添加浏览量字段（如果不存在）
        if 'views' not in news_item:
            news_item['views'] = 0
        news_item['views'] += 1
        print(f"更新新闻浏览量: {news_item['views']}")
        
        # 添加评论字段（如果不存在）
        if 'comments' not in news_item:
            news_item['comments'] = []
        
        # 准备相关新闻
        related_news = []
        for item in news_data:
            if isinstance(item, dict) and str(item.get('id')) != str(news_id):
                related_news.append(item)
                if len(related_news) >= 3:
                    break
        print(f"找到{len(related_news)}条相关新闻")
        
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
            print(f"更新浏览量时发生错误: {str(e)}")
        
        # 确保news_item中的content是有效的
        print(f"处理content前的类型: {type(news_item.get('content')).__name__}")
        if 'content' not in news_item or news_item['content'] is None or news_item['content'] == '':
            news_item['content'] = ["暂无内容"]
            print("content不存在，设置为['暂无内容']")
        elif isinstance(news_item['content'], str):
            # 如果content是字符串，将其转换为数组
            news_item['content'] = [news_item['content']]
            print(f"content是字符串，已转换为数组")
        elif not isinstance(news_item['content'], list):
            # 如果content既不是字符串也不是数组，转换为字符串后再转为数组
            news_item['content'] = [str(news_item['content'])]
            print(f"content是未知类型，已转换为字符串数组")
        print(f"处理content后的类型: {type(news_item['content']).__name__}")
        
        # 渲染新闻详情页面
        print(f"准备渲染新闻详情页面，标题: {news_item['title']}")
        return render_template('news_detail.html', news=news_item, related_news=related_news)
        
    except Exception as e:
        # 捕获所有其他异常并记录详细的错误信息
        import traceback
        error_traceback = traceback.format_exc()
        print(f"获取新闻详情时发生错误: {str(e)}")
        print(f"错误堆栈: {error_traceback}")
        flash(f'获取新闻详情时发生错误: {str(e)}', 'danger')
        return render_template('error.html', error_code='500', error_message=f'获取新闻详情时发生错误: {str(e)}')

@app.route('/api/search_stock')
def search_stock():
    try:
        query = request.args.get('query', '').lower().strip()
        
        # 记录搜索请求，包含用户信息
        username = session.get('username', '未登录用户')
        logger.info(f"[用户: {username}] 搜索股票: {query}")
        
        # 检查查询是否为空
        if not query:
            logger.info("搜索查询为空，返回空结果")
            return jsonify([])
            
        # 简单的股票搜索功能
        results = [
            stock for stock in SAMPLE_STOCKS 
            if query in stock['code'].lower() or query in stock['name'].lower()
        ]
        
        logger.info(f"找到{len(results)}个匹配的股票")
        return jsonify(results[:10])  # 限制最多返回10个结果
    except Exception as e:
        # 增强错误日志记录
        username = session.get('username', '未登录用户')
        logger.error(f"[用户: {username}] 搜索股票时发生错误: {str(e)}")
        # 对于API请求，返回空数组而不是错误页面是合理的
        return jsonify([])
        
# 基金搜索API
@app.route('/api/search_fund')
def search_fund():
    try:
        query = request.args.get('query', '').lower().strip()
        
        # 记录搜索请求，包含用户信息
        username = session.get('username', '未登录用户')
        logger.info(f"[用户: {username}] 搜索基金: {query}")
        
        # 检查查询是否为空
        if not query:
            logger.info("搜索查询为空，返回空结果")
            return jsonify([])
            
        # 简单的基金搜索功能
        results = [
            fund for fund in SAMPLE_FUNDS 
            if query in fund['code'].lower() or query in fund['name'].lower()
        ]
        
        logger.info(f"找到{len(results)}个匹配的基金")
        return jsonify(results[:10])  # 限制最多返回10个结果
    except Exception as e:
        # 增强错误日志记录
        username = session.get('username', '未登录用户')
        logger.error(f"[用户: {username}] 搜索基金时发生错误: {str(e)}")
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
                                error=f'设置保存失败：{str(e)}',
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
        logger.error(f"应用启动失败: {str(e)}")
        print(f"错误: {str(e)}")
        # 确保关闭数据库连接
        shutdown_database()