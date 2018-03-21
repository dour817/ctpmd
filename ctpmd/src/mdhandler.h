/*
 * mdhandler.h
 *
 *  Created on: 2018-2-4
 *      Author: tcz
 */

#ifndef MDHANDLER_H_
#define MDHANDLER_H_

#include "/home/tcz/ctpapi/ThostFtdcMdApi.h"
#include "SelfDataStruct.h"
using namespace std;
extern account_setting ACC_SETTING;
extern mongodb_setting MONGODB_SETTING;
extern boost::lockfree::queue< market_data*, boost::lockfree::fixed_sized<false> > MARKET_QUEQUE;
extern boost::lockfree::queue< bar, boost::lockfree::fixed_sized<false> > MARKET_K_QUEUE;
extern char DATETIME[30];
extern char LOGINHOUR[3];
extern char LOGINMINUTE[3];

extern sem_t Md_Queue_Write;
extern map<string, instrument_status> map_ins_status;

class MdHandler: public CThostFtdcMdSpi{

public:
	MdHandler(CThostFtdcMdApi *pUserApi, vector<string> code);
	~MdHandler();
	//static logininfo LoginInfo

	virtual void OnFrontConnected();

	virtual void OnFrontDisconnected(int nReason);
	virtual void OnRspUserLogin(CThostFtdcRspUserLoginField *pRspUserLogin,CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast);

	virtual void OnRtnDepthMarketData(CThostFtdcDepthMarketDataField *pDepthMarketData);

	virtual void OnRspError(CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast);
	virtual void OnRspUserLogout(CThostFtdcUserLogoutField *pUserLogout, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast);
	virtual void OnRspUnSubMarketData(CThostFtdcSpecificInstrumentField *pSpecificInstrument, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast);

	//计算k线线程
	static void *calcu_k_func(void *arg);

	//写k线到mongo线程
	static void *write_k2mongo(void *arg);


private:
	//请求接口指针
	CThostFtdcMdApi *m_pUserApi;

	//本Spi回调实例订阅的代码
	vector<string> subcode;

	//计算k线线程同步所需要的信号量
	sem_t k_signal;

	//写k线到mongo所需要的信号量
	sem_t Md_Queue_K;

    //计算k线所需行情队列
	boost::lockfree::queue< market_data_for_k*, boost::lockfree::capacity<12800> >  market_tick_queue;

	// 行情bar队列，队列bar依次写入mongo
	boost::lockfree::queue< bar, boost::lockfree::capacity<12800>  > MARKET_K_QUEUE;

	string ActionDay;

};




#endif /* MDHANDLER_H_ */
