import threading
import time
import logging
from datetime import datetime
import random
import numpy as np

# 配置日志
logging.basicConfig(level=logging.INFO, format='%(asctime)s - %(name)s - %(levelname)s - %(message)s')
logger = logging.getLogger('data_scheduler')

class DataScheduler:
    """定时数据更新调度器"""
    def __init__(self):
        self._running = False
        self._thread = None
        self.update_interval = 30 * 60  # 30分钟（秒）
        self.last_update_time = None
        self.app = None
        # 添加趋势因子，用于模拟中期价格趋势
        self.trend_factors = {}
        # 添加波动率历史记录，用于模拟真实市场波动性
        self.volatility_history = {}
        # 添加日期跟踪，用于模拟不同交易日的行为
        self.last_trading_day = None
    
    def start(self, app):
        """启动定时更新任务"""
        if self._running:
            logger.info("定时更新任务已经在运行中")
            return
        
        self.app = app
        self._running = True
        self._thread = threading.Thread(target=self._update_loop, daemon=True)
        self._thread.start()
        logger.info(f"定时更新任务已启动，每{self.update_interval//60}分钟更新一次数据")
        
        # 立即执行一次更新
        self.update_data()
    
    def stop(self):
        """停止定时更新任务"""
        if not self._running:
            logger.info("定时更新任务已经停止")
            return
        
        self._running = False
        if self._thread:
            self._thread.join(timeout=5.0)
            logger.info("定时更新任务已停止")
    
    def _update_loop(self):
        """更新循环"""
        while self._running:
            try:
                # 等待指定的时间间隔
                for _ in range(self.update_interval):
                    if not self._running:
                        break
                    time.sleep(1)
                
                if self._running:
                    self.update_data()
            except Exception as e:
                logger.error(f"定时更新过程中发生错误: {str(e)}")
    
    def update_data(self):
        """更新数据的方法"""
        try:
            with self.app.app_context():
                logger.info("开始更新数据...")
                
                # 检查是否是新的交易日
                current_day = datetime.now().date()
                if self.last_trading_day != current_day:
                    # 新的交易日，更新趋势因子
                    self._update_trend_factors()
                    self.last_trading_day = current_day
                
                # 更新基金数据
                self._update_fund_data()
                
                # 更新股票数据
                self._update_stock_data()
                
                # 更新指数数据
                self._update_index_data()
                
                # 记录最后更新时间
                self.last_update_time = datetime.now()
                logger.info(f"数据更新完成，最后更新时间: {self.last_update_time}")
        except Exception as e:
            logger.error(f"数据更新失败: {str(e)}")
    
    def _update_trend_factors(self):
        """每个交易日更新趋势因子"""
        # 为基金创建趋势因子
        from app import SAMPLE_FUNDS
        for fund in SAMPLE_FUNDS:
            code = fund['code']
            # 生成-0.1到0.1之间的趋势因子，决定中期价格走向
            self.trend_factors[code] = random.uniform(-0.1, 0.1)
            # 初始化波动率历史记录
            if code not in self.volatility_history:
                self.volatility_history[code] = []
        
        # 为股票创建趋势因子
        from app import SAMPLE_STOCKS
        for stock in SAMPLE_STOCKS:
            code = stock['code']
            self.trend_factors[code] = random.uniform(-0.15, 0.15)
            if code not in self.volatility_history:
                self.volatility_history[code] = []
        
        # 为指数创建趋势因子
        from app import FUND_INDICES
        for index in FUND_INDICES:
            code = index['name']
            self.trend_factors[code] = random.uniform(-0.08, 0.08)
            if code not in self.volatility_history:
                self.volatility_history[code] = []
    
    def _update_fund_data(self):
        """更新基金数据 - 使用更真实的波动逻辑"""
        import json
        import os
        from app import SAMPLE_FUNDS
        
        # 优先从funds.json文件读取真实基金数据
        funds = []
        try:
            # 构建funds.json文件的完整路径
            data_dir = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), 'data')
            fund_file_path = os.path.join(data_dir, 'funds.json')
            
            # 读取文件内容
            if os.path.exists(fund_file_path):
                with open(fund_file_path, 'r', encoding='utf-8') as f:
                    data = json.load(f)
                    # 从data对象中获取funds数组
                    funds = data.get('funds', [])
                
                logger.info(f"从funds.json文件成功读取{len(funds)}条基金数据")
        except Exception as e:
            # 文件读取失败，使用SAMPLE_FUNDS作为备选
            logger.warning(f"读取funds.json文件失败: {str(e)}，使用SAMPLE_FUNDS作为备选")
            funds = SAMPLE_FUNDS
        
        # 如果没有从任何来源获取到基金数据，使用SAMPLE_FUNDS
        if not funds:
            funds = SAMPLE_FUNDS
            logger.info(f"使用SAMPLE_FUNDS中的{len(funds)}条基金数据")
        
        # 遍历基金数据，应用波动逻辑
        for fund in funds:
            # 确保基金对象具有所有必要的字段
            code = fund.get('code', 'unknown')
            fund_type = fund.get('fund_type', '混合型')
            
            # 根据基金类型设置不同的波动率
            volatility_based_on_type = {
                '股票型': 0.015,  # 股票型基金波动率最高
                '混合型': 0.012,  # 混合型次之
                '指数型': 0.013,  # 指数型接近股票型
                'QDII': 0.014,    # QDII受汇率影响，波动率也较高
                '债券型': 0.003,  # 债券型波动率较低
                '货币型': 0.001   # 货币型几乎无波动
            }.get(fund_type, 0.012)
            
            # 获取该基金的趋势因子，如果没有则创建一个
            if code not in self.trend_factors:
                # 生成-0.1到0.1之间的趋势因子
                self.trend_factors[code] = random.uniform(-0.1, 0.1)
                # 初始化波动率历史记录
                self.volatility_history[code] = []
                
            trend_factor = self.trend_factors.get(code, 0)
            
            # 使用正态分布生成波动，更符合真实市场行为
            # 添加趋势因子影响，使价格有中期方向
            volatility = volatility_based_on_type * (1 + 0.3 * trend_factor)
            fluctuation = np.random.normal(0, volatility)  # 均值为0，标准差为波动率
            
            # 限制波动范围，防止极端异常值
            max_fluctuation = volatility_based_on_type * 3
            fluctuation = max(-max_fluctuation, min(max_fluctuation, fluctuation))
            
            # 确保基金有net_value字段
            if 'net_value' not in fund:
                fund['net_value'] = round(random.uniform(0.8, 4.0), 4)
            
            # 应用波动
            fund['net_value'] = round(fund['net_value'] * (1 + fluctuation), 4)
            
            # 计算日增长率
            previous_net_value = fund.get('previous_net_value', fund['net_value'])
            daily_growth_percent = (fund['net_value'] - previous_net_value) / previous_net_value * 100
            fund['daily_growth_percent'] = round(daily_growth_percent, 2)
            
            # 保存当前净值作为下次更新的基准
            fund['previous_net_value'] = fund['net_value']
            
            # 模拟更新单位净值日期
            fund['nav_date'] = datetime.now().strftime('%Y-%m-%d')
            
            # 记录波动率历史
            if code in self.volatility_history:
                self.volatility_history[code].append(abs(fluctuation))
                # 只保留最近30天的记录
                if len(self.volatility_history[code]) > 30:
                    self.volatility_history[code].pop(0)
    
    def _update_stock_data(self):
        """更新股票数据 - 使用更真实的波动逻辑"""
        from app import SAMPLE_STOCKS
        
        for stock in SAMPLE_STOCKS:
            code = stock['code']
            
            # 检查是否有price字段，如果没有则初始化一个随机价格
            if 'price' not in stock:
                # 根据股票名称和代码生成一个合理的初始价格
                # 贵州茅台等高端白酒股价格较高
                if '茅台' in stock['name'] or '五粮液' in stock['name']:
                    stock['price'] = round(random.uniform(1000, 2000), 2)
                # 银行股价格相对较低
                elif '银行' in stock['name']:
                    stock['price'] = round(random.uniform(5, 20), 2)
                # 其他股票取一个中等范围的价格
                else:
                    stock['price'] = round(random.uniform(20, 500), 2)
            
            # 假设股票波动率更高，使用正态分布
            base_volatility = 0.025
            
            # 获取该股票的趋势因子
            trend_factor = self.trend_factors.get(code, 0)
            
            # 根据趋势因子调整波动率
            volatility = base_volatility * (1 + 0.4 * trend_factor)
            fluctuation = np.random.normal(0, volatility)
            
            # 限制波动范围
            max_fluctuation = base_volatility * 3
            fluctuation = max(-max_fluctuation, min(max_fluctuation, fluctuation))
            
            # 应用波动
            stock['price'] = round(stock['price'] * (1 + fluctuation), 2)
            
            # 计算涨跌幅
            previous_price = stock.get('previous_price', stock['price'])
            change_percent = (stock['price'] - previous_price) / previous_price * 100
            stock['change_percent'] = round(change_percent, 2)
            
            # 保存当前价格作为下次更新的基准
            stock['previous_price'] = stock['price']
            
            # 记录波动率历史
            if code in self.volatility_history:
                self.volatility_history[code].append(abs(fluctuation))
                if len(self.volatility_history[code]) > 30:
                    self.volatility_history[code].pop(0)
    
    def _update_index_data(self):
        """更新指数数据 - 使用更真实的波动逻辑"""
        from app import FUND_INDICES
        
        for index in FUND_INDICES:
            code = index['name']
            
            # 指数波动率相对较低，使用正态分布
            base_volatility = 0.015
            
            # 获取该指数的趋势因子
            trend_factor = self.trend_factors.get(code, 0)
            
            # 根据趋势因子调整波动率
            volatility = base_volatility * (1 + 0.3 * trend_factor)
            fluctuation = np.random.normal(0, volatility)
            
            # 限制波动范围
            max_fluctuation = base_volatility * 3
            fluctuation = max(-max_fluctuation, min(max_fluctuation, fluctuation))
            
            # 应用波动
            index['value'] = round(index['value'] * (1 + fluctuation), 2)
            
            # 计算涨跌幅
            previous_value = index.get('previous_value', index['value'])
            change_percent = (index['value'] - previous_value) / previous_value * 100
            index['change_percent'] = round(change_percent, 2)
            
            # 保存当前值作为下次更新的基准
            index['previous_value'] = index['value']
            
            # 记录波动率历史
            if code in self.volatility_history:
                self.volatility_history[code].append(abs(fluctuation))
                if len(self.volatility_history[code]) > 30:
                    self.volatility_history[code].pop(0)
    
    def get_last_update_time(self):
        """获取最后更新时间"""
        return self.last_update_time

# 创建全局实例
scheduler = DataScheduler()