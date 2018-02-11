/*
 * tdhandler.h
 *
 *  Created on: 2018-2-4
 *      Author: tcz
 */

#ifndef TDHANDLER_H_
#define TDHANDLER_H_


#include "/home/tcz/ctpapi/ThostFtdcTraderApi.h"
#include "SelfDataStruct.h"

extern vector<string> ALL_CODE;
extern sem_t Md_Thread;
extern account_setting ACC_SETTING;

using namespace std;

class TdHandler : public CThostFtdcTraderSpi
{
public:
	TdHandler(CThostFtdcTraderApi *pTraderApi);
	~TdHandler();
	virtual void OnFrontConnected();
	virtual void OnFrontDisconnected(int nReason);
	virtual void OnRspUserLogin(CThostFtdcRspUserLoginField *pRspUserLogin, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast);
	virtual void OnRspError(CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast);
	virtual void OnRspQrySettlementInfo(CThostFtdcSettlementInfoField *pSettlementInfo, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast);
	virtual void OnRspSettlementInfoConfirm(CThostFtdcSettlementInfoConfirmField *pSettlementInfoConfirm, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast);
	virtual void OnRspUserLogout(CThostFtdcUserLogoutField *pUserLogout, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast);
 	virtual void OnRspQryInstrument(CThostFtdcInstrumentField *pInstrument, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast);

private:
	CThostFtdcTraderApi *m_pTraderApi;
};



#endif /* TDHANDLER_H_ */
