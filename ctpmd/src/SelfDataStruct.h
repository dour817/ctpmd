/*
 * SelfDataStruct.h
 *
 *  Created on: 2018-2-4
 *      Author: tcz
 */

#ifndef SELFDATASTRUCT_H_
#define SELFDATASTRUCT_H_

#define INSTRUMENT_LENGTH 40
#define TIME_LENGTH 20
#include <iostream>
#include <string>
#include <vector>
#include <regex>

#include <bsoncxx/builder/stream/document.hpp>
#include <bsoncxx/json.hpp>
#include <bsoncxx/types.hpp>

#include <mongocxx/client.hpp>
#include <mongocxx/instance.hpp>
#include <mongocxx/uri.hpp>
#include <mongocxx/options/find.hpp>
#include <mongocxx/stdx.hpp>
#include <mongocxx/database.hpp>
#include <mongocxx/cursor.hpp>
#include <mongocxx/collection.hpp>

#include "semaphore.h"

#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/ini_parser.hpp>
#include <boost/algorithm/string/classification.hpp>
#include <boost/algorithm/string/split.hpp>

#include <boost/lockfree/queue.hpp>
//#include <boost/bind.hpp>
//#include <boost/thread.hpp>
//#include <boost/thread/mutex.hpp>


using namespace std;

//行情结构体，用于写入mongo
typedef struct market_data{

	//结算日
	char TradingDay[TIME_LENGTH];

	//合约代码
	char InstrumentID[INSTRUMENT_LENGTH];

	//最新价
	double LastPrice;

	//前结算价
	double PreSettlementPrice;

	//前收盘价
	double PreClosePrice;

	//前持仓量
	int    PreOpenInterest;

	//开盘价
	double OpenPrice;

	//最高价
	double HighestPrice;

	//最低价
	double LowestPrice;

	//成交量
	int    Volume;

	//成交额
	double Turnover;

	//持仓量
	int    OpenInterest;

	//涨停价
	double UpperLimitPrice;

	//跌停价
	double LowerLimitPrice;

	//时间戳 HH:MM:SS，精确到秒
	char UpdateTime[TIME_LENGTH];

	//时间戳 FFF 精确到毫秒
	int    UpdateMillisec;

	//买一价
	double BidPrice1;

	//买一量
	double BidVolume1;

	//卖一价
	double AskPrice1;

	//卖一量
	double AskVolume1;

	//自然日
	char ActionDay[TIME_LENGTH];

}market_data;


//行情结构，用于k线计算
typedef struct market_data_for_k{

	//结算日
    char TradingDay[TIME_LENGTH];

	//合约代码
	char InstrumentID[INSTRUMENT_LENGTH];

	//最新价
	double LastPrice;

	//成交量
	int    Volume;

	//时间戳 HH:MM:SS，精确到秒
	char UpdateTime[TIME_LENGTH];

	//时间戳 FFF 精确到毫秒
	int    UpdateMillisec;

	//自然日
	char ActionDay[TIME_LENGTH];

}market_data_for_k;


//bar结构体
typedef struct bar{

	//合约代码
	char InstrumentID[INSTRUMENT_LENGTH];

	//时间戳 HH:MM:SS，精确到秒
	char UpdateTime[TIME_LENGTH];

	//开盘价
	double OpenPrice;

	//最高价
	double HighPrice;

	//最低价
	double LowPrice;

	//收盘价
	double ClosePrice;

	//bar成交量
	int    Volume;

	//实时累计成交量
	int    TotalVolume;

	//上一根var的累计成交量
	int    LastTotalVolume;

}bar;

// 期货账户配置结构体
typedef struct account_setting{

	//期货账户
    string userid;

    //密码
    string password;

    //经纪商ID。
    string brokerid;

    //交易前置地址
    vector<string> traderaddress;

    //行情前置地址
    vector<string> mdaddress;

}account_setting;


// mongodb 配置
typedef struct mongodb_setting{

	//mongo地址
    string host;

    //mongo端口
    string port;

    //用户名
    string username;

    //密码
    string password;

    string auth;

    //数据库
    string db;

}mongodb_setting;


// 订阅合约信息
typedef struct instrument_setting{

	//商品
    vector<string> commodity;

    //合约
    vector<string> instrument;

    //行情接收实例个数
    int instance_num;

}instrument_setting;


//行情接收线程参数类型
typedef struct md_thread_arg{

	vector<string> code_list;

}md_thread_arg;

//写k线到mongo
void *write_k2mongo(void *arg);

//行情接收线程
void *mdstartfun(void* arg);

//启动行情接收
void start_rev_md(vector<string> code, int num, mongocxx::database db);

//计算所有需要订阅的合约
vector<string> Get_All_SubInstrument_Code(instrument_setting arg);


// 获取期货账户配置信息
account_setting Get_Account_Setting();


// 获取mongodb连接信息
mongodb_setting Get_Mongodb_Setting();


// 获取订阅合约配置信息
instrument_setting Get_Instrument_Setting();


#endif /* SELFDATASTRUCT_H_ */
