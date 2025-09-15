# -*- coding: utf-8 -*-
import sqlite3
import os
import logging
from datetime import datetime

# 设置日志
logger = logging.getLogger(__name__)

class Database:
    def __init__(self, db_path='data/fund_data.db'):
        """初始化数据库连接"""
        # 确保data目录存在
        data_dir = os.path.dirname(db_path)
        if not os.path.exists(data_dir):
            os.makedirs(data_dir)
            logger.info("创建数据目录: %s", data_dir)
        
        self.db_path = db_path
        # 初始化时不再创建连接，而是在每次操作时创建
        self.init_tables()  # 初始化表结构
    
    def _get_connection(self):
        """获取数据库连接，为每个操作创建独立连接"""
        try:
            conn = sqlite3.connect(self.db_path)
            # 启用外键约束
            conn.execute("PRAGMA foreign_keys = ON")
            return conn
        except sqlite3.Error as e:
            logger.error("创建数据库连接失败: %s", e)
            raise
    
    def connect(self):
        """建立数据库连接（仅用于初始化表结构）"""
        try:
            conn = self._get_connection()
            cursor = conn.cursor()
            return conn, cursor
        except sqlite3.Error as e:
            logger.error("连接数据库失败: %s", e)
            return None, None
    
    def init_tables(self):
        """初始化数据库表"""
        conn, cursor = self.connect()
        if not conn or not cursor:
            return
            
        try:
            # 创建用户表
            cursor.execute('''
                CREATE TABLE IF NOT EXISTS users (
                    id INTEGER PRIMARY KEY AUTOINCREMENT,
                    username TEXT NOT NULL UNIQUE,
                    password TEXT NOT NULL,
                    salt TEXT NOT NULL,
                    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
                )
            ''')
            
            # 创建自选基金表
            cursor.execute('''
                CREATE TABLE IF NOT EXISTS fund_watchlists (
                    id INTEGER PRIMARY KEY AUTOINCREMENT,
                    user_id INTEGER NOT NULL,
                    fund_code TEXT NOT NULL,
                    fund_name TEXT NOT NULL,
                    added_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
                    FOREIGN KEY (user_id) REFERENCES users (id) ON DELETE CASCADE,
                    UNIQUE (user_id, fund_code)
                )
            ''')
            
            # 创建基金评分表
            cursor.execute('''
                CREATE TABLE IF NOT EXISTS fund_ratings (
                    id INTEGER PRIMARY KEY AUTOINCREMENT,
                    fund_code TEXT NOT NULL UNIQUE,
                    risk_level INTEGER,
                    return_score INTEGER,
                    manager_score INTEGER,
                    stability_score INTEGER,
                    overall_rating INTEGER
                )
            ''')
            
            # 创建用户风险偏好表
            cursor.execute('''
                CREATE TABLE IF NOT EXISTS user_risk_profiles (
                    id INTEGER PRIMARY KEY AUTOINCREMENT,
                    user_id INTEGER NOT NULL UNIQUE,
                    risk_level INTEGER,
                    investment_horizon TEXT,
                    investment_experience TEXT,
                    investment_objective TEXT,
                    updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
                    FOREIGN KEY (user_id) REFERENCES users (id) ON DELETE CASCADE
                )
            ''')
            
            # 检查表结构是否需要升级
            self._check_and_upgrade_tables(cursor)
            
            conn.commit()
            logger.info("数据库表初始化成功")
        except sqlite3.Error as e:
            logger.error("初始化数据库表失败: %s", e)
            conn.rollback()
        finally:
            if conn:
                conn.close()
    
    def _check_and_upgrade_tables(self, cursor):
        """检查并升级表结构"""
        try:
            # 检查user_risk_profiles表结构是否需要升级
            cursor.execute("PRAGMA table_info(user_risk_profiles)")
            columns = [column[1] for column in cursor.fetchall()]
            
            # 如果表结构还是旧版本（包含risk_tolerance, investment_goal, time_horizon列）
            if 'risk_tolerance' in columns and 'investment_goal' in columns and 'time_horizon' in columns:
                logger.info("检测到旧版本的user_risk_profiles表结构，开始迁移数据")
                
                # 1. 创建临时表
                cursor.execute('''
                    CREATE TABLE IF NOT EXISTS user_risk_profiles_temp (
                        id INTEGER PRIMARY KEY AUTOINCREMENT,
                        user_id INTEGER NOT NULL UNIQUE,
                        risk_level INTEGER,
                        investment_horizon TEXT,
                        investment_experience TEXT,
                        investment_objective TEXT,
                        updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
                        FOREIGN KEY (user_id) REFERENCES users (id) ON DELETE CASCADE
                    )
                ''')
                
                # 2. 迁移数据
                cursor.execute('''
                    INSERT INTO user_risk_profiles_temp (user_id, risk_level, investment_horizon, investment_experience, investment_objective, updated_at)
                    SELECT user_id, risk_tolerance, time_horizon, '中等', investment_goal, updated_at
                    FROM user_risk_profiles
                ''')
                
                # 3. 删除旧表
                cursor.execute("DROP TABLE user_risk_profiles")
                
                # 4. 重命名临时表
                cursor.execute("ALTER TABLE user_risk_profiles_temp RENAME TO user_risk_profiles")
                
                logger.info("user_risk_profiles表结构迁移完成")
        except sqlite3.Error as e:
            logger.error("升级表结构失败: %s", e)
    
    # 以下是修改后的数据库操作方法，每个方法都创建和关闭独立的连接
    def add_user(self, username, password, salt):
        """添加用户"""
        try:
            conn = self._get_connection()
            cursor = conn.cursor()
            
            cursor.execute(
                "INSERT INTO users (username, password, salt) VALUES (?, ?, ?)",
                (username, password, salt)
            )
            conn.commit()
            logger.info("成功添加用户: %s", username)
            return True
        except sqlite3.IntegrityError:
            logger.error("用户已存在: %s", username)
            if conn:
                conn.rollback()
            return False
        except sqlite3.Error as e:
            logger.error("添加用户失败: %s", e)
            if conn:
                conn.rollback()
            return False
        finally:
            if conn:
                conn.close()
    
    def get_user(self, username):
        """获取用户信息"""
        try:
            conn = self._get_connection()
            cursor = conn.cursor()
            
            cursor.execute(
                "SELECT id, username, password, salt FROM users WHERE username = ?",
                (username,)
            )
            result = cursor.fetchone()
            return result
        except sqlite3.Error as e:
            logger.error("获取用户信息失败: %s", e)
            return None
        finally:
            if conn:
                conn.close()
    
    def add_to_watchlist(self, user_id, fund_code, fund_name):
        """添加基金到自选列表"""
        try:
            conn = self._get_connection()
            cursor = conn.cursor()
            
            # 检查是否已在自选列表中
            cursor.execute(
                "SELECT 1 FROM fund_watchlists WHERE user_id = ? AND fund_code = ?",
                (user_id, fund_code)
            )
            exists = cursor.fetchone() is not None
            
            if exists:
                logger.info("基金 %s 已在自选列表中", fund_code)
                return False
            
            cursor.execute(
                "INSERT INTO fund_watchlists (user_id, fund_code, fund_name) VALUES (?, ?, ?)",
                (user_id, fund_code, fund_name)
            )
            conn.commit()
            logger.info("成功添加基金 %s 到自选列表", fund_code)
            return True
        except sqlite3.Error as e:
            logger.error("添加自选基金失败: %s", e)
            if conn:
                conn.rollback()
            return False
        finally:
            if conn:
                conn.close()
    
    def remove_from_watchlist(self, user_id, fund_code):
        """从自选基金中移除基金"""
        try:
            conn = self._get_connection()
            cursor = conn.cursor()
            
            cursor.execute(
                "DELETE FROM fund_watchlists WHERE user_id = ? AND fund_code = ?",
                (user_id, fund_code)
            )
            conn.commit()
            success = cursor.rowcount > 0
            if success:
                logger.info("成功从自选基金中移除: %s", fund_code)
            else:
                logger.info("未找到要移除的自选基金: %s", fund_code)
            return success
        except sqlite3.Error as e:
            logger.error("移除自选基金失败: %s", e)
            if conn:
                conn.rollback()
            return False
        finally:
            if conn:
                conn.close()
    
    def get_watchlist(self, user_id):
        """获取用户的自选基金列表"""
        try:
            conn = self._get_connection()
            cursor = conn.cursor()
            
            cursor.execute(
                "SELECT fund_code, fund_name, added_at FROM fund_watchlists WHERE user_id = ? ORDER BY added_at DESC",
                (user_id,)
            )
            result = cursor.fetchall()
            logger.info("成功获取用户 %s 的自选基金列表，共 %s 条记录", user_id, len(result))
            return result
        except sqlite3.Error as e:
            logger.error("获取自选基金列表失败: %s", e)
            return []
        finally:
            if conn:
                conn.close()
    
    def close(self):
        """关闭数据库连接"""
        # 由于每个操作都创建和关闭了独立连接，这里无需执行任何操作
        logger.info("数据库连接管理已优化，无需全局关闭")
    
    def add_fund_rating(self, fund_code, risk_level, return_score, manager_score, stability_score, overall_rating):
        """添加或更新基金评分"""
        try:
            conn = self._get_connection()
            cursor = conn.cursor()
            
            cursor.execute(
                "INSERT OR REPLACE INTO fund_ratings (fund_code, risk_level, return_score, manager_score, stability_score, overall_rating) VALUES (?, ?, ?, ?, ?, ?)",
                (fund_code, risk_level, return_score, manager_score, stability_score, overall_rating)
            )
            conn.commit()
            logger.info("成功添加/更新基金 %s 的评分", fund_code)
            return True
        except sqlite3.Error as e:
            logger.error("添加/更新基金评分失败: %s", e)
            if conn:
                conn.rollback()
            return False
        finally:
            if conn:
                conn.close()
    
    def get_fund_rating(self, fund_code):
        """获取基金评分"""
        try:
            conn = self._get_connection()
            cursor = conn.cursor()
            
            cursor.execute(
                "SELECT risk_level, return_score, manager_score, stability_score, overall_rating FROM fund_ratings WHERE fund_code = ?",
                (fund_code,)
            )
            result = cursor.fetchone()
            return result
        except sqlite3.Error as e:
            logger.error("获取基金评分失败: %s", e)
            return None
        finally:
            if conn:
                conn.close()
    
    def update_user_risk_profile(self, user_id, risk_level, investment_horizon, investment_experience, investment_objective):
        """更新用户风险偏好"""
        try:
            conn = self._get_connection()
            cursor = conn.cursor()
            
            # 检查是否已存在记录
            cursor.execute("SELECT 1 FROM user_risk_profiles WHERE user_id = ?", (user_id,))
            exists = cursor.fetchone() is not None
            
            if exists:
                cursor.execute(
                    "UPDATE user_risk_profiles SET risk_level = ?, investment_horizon = ?, investment_experience = ?, investment_objective = ?, updated_at = CURRENT_TIMESTAMP WHERE user_id = ?",
                    (risk_level, investment_horizon, investment_experience, investment_objective, user_id)
                )
            else:
                cursor.execute(
                    "INSERT INTO user_risk_profiles (user_id, risk_level, investment_horizon, investment_experience, investment_objective) VALUES (?, ?, ?, ?, ?)",
                    (user_id, risk_level, investment_horizon, investment_experience, investment_objective)
                )
            
            conn.commit()
            logger.info("成功更新用户 %s 的风险偏好", user_id)
            return True
        except sqlite3.Error as e:
            logger.error("更新用户风险偏好失败: %s", e)
            if conn:
                conn.rollback()
            return False
        finally:
            if conn:
                conn.close()
    
    def get_user_risk_profile(self, user_id):
        """获取用户风险偏好"""
        try:
            conn = self._get_connection()
            cursor = conn.cursor()
            
            cursor.execute(
                "SELECT id, user_id, risk_level, investment_horizon, investment_experience, investment_objective FROM user_risk_profiles WHERE user_id = ?",
                (user_id,)
            )
            result = cursor.fetchone()
            logger.info("成功获取用户 %s 的风险偏好", user_id)
            return result
        except sqlite3.Error as e:
            logger.error("获取用户风险偏好失败: %s", e)
            return None
        finally:
            if conn:
                conn.close()

# 创建全局数据库实例
db = None

def init_database():
    """初始化全局数据库实例"""
    global db
    if db is None:
        db = Database()
    return db

# 关闭数据库连接
def shutdown_database():
    """关闭数据库连接"""
    global db
    if db:
        db.close()
        db = None