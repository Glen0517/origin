# -*- coding: utf-8 -*-
import sqlite3
import logging
from database import init_database

# Configure logging
logging.basicConfig(level=logging.INFO)
logger = logging.getLogger(__name__)

if __name__ == "__main__":
    try:
        # Initialize database
        db = init_database()
        logger.info("Database initialized successfully")
        
        # Connect to fund database to check table structure
        conn = sqlite3.connect('data/fund_data.db')
        cursor = conn.cursor()
        
        # Check all table names
        cursor.execute("SELECT name FROM sqlite_master WHERE type='table';")
        tables = cursor.fetchall()
        logger.info("Tables in fund database: %s", tables)
        
        # Check users table structure
        cursor.execute("PRAGMA table_info(users)")
        user_columns = cursor.fetchall()
        logger.info("Users table structure:")
        for col in user_columns:
            logger.info("  %s", col)
        
        # Check fund_watchlists table structure
        cursor.execute("PRAGMA table_info(fund_watchlists)")
        watchlist_columns = cursor.fetchall()
        logger.info("Fund_watchlists table structure:")
        for col in watchlist_columns:
            logger.info("  %s", col)
        
        # Check user_risk_profiles table structure
        cursor.execute("PRAGMA table_info(user_risk_profiles)")
        risk_profile_columns = cursor.fetchall()
        logger.info("User_risk_profiles table structure:")
        for col in risk_profile_columns:
            logger.info("  %s", col)
        
        # Check user count
        cursor.execute("SELECT COUNT(*) FROM users")
        user_count = cursor.fetchone()[0]
        logger.info("Number of users: %s", user_count)
        
        conn.close()
        logger.info("Database connection test completed")
    except Exception as e:
        logger.error("Database test failed: %s", str(e))