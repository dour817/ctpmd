/*
 * tdhandler.cpp
 *
 *  Created on: 2018-2-4
 *      Author: tcz
 */
#include "tdhandler.h"

TdHandler :: TdHandler(CThostFtdcTraderApi *pTraderApi) :m_pTraderApi(pTraderApi) {};

TdHandler :: ~TdHandler() {}

void TdHandler :: OnFrontConnected()
{
	CThostFtdcReqUserLoginField reqUserLogin;
	strcpy(reqUserLogin.BrokerID, ACC_SETTING.brokerid.c_str());
	strcpy(reqUserLogin.UserID, ACC_SETTING.userid.c_str());
	strcpy(reqUserLogin.Password, ACC_SETTING.password.c_str());
    m_pTraderApi->ReqUserLogin(&reqUserLogin, 0);
}

void TdHandler :: OnFrontDisconnected(int nReason)
{
     printf("Trader OnFrontDisconnected.\n");
}

void TdHandler :: OnRspUserLogin(CThostFtdcRspUserLoginField *pRspUserLogin, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
	if(pRspInfo->ErrorID != 0){
		/*cout << "**********Trader Failed to login!  ***********" << endl;
		cout << "ErrorCode = [" << pRspInfo->ErrorID << "], ErrorMsg = [" << pRspInfo->ErrorMsg << "]" << endl;
		cout << "RequestID = [" << nRequestID << "], Chain = [" << bIsLast << "]" << endl;*/
	}else{
		cout << endl;
		cout  << "**********  trade api logined successfull!  **********" << endl;
		cout << endl;

		//设置DATETIME
        strncpy(DATETIME, pRspUserLogin->TradingDay, 4);
        strncpy(DATETIME+5, pRspUserLogin->TradingDay+4, 2);
        strncpy(DATETIME+8, pRspUserLogin->TradingDay+6, 2);
        DATETIME[4] = '-';
        DATETIME[7] = '-';
        DATETIME[10] = ' ';
        strcat(DATETIME, pRspUserLogin->LoginTime);

        //cout << "DATETIME :" << DATETIME << endl;

        strncpy(LOGINHOUR, DATETIME+11 ,2);
        strncpy(LOGINMINUTE, DATETIME+14 ,2);
        LOGINHOUR[2] = '\0';
        LOGINMINUTE[2] = '\0';


		//投资者结算结果确认
		CThostFtdcSettlementInfoConfirmField temp1;
		memset(&temp1,0,sizeof(CThostFtdcSettlementInfoConfirmField));
		strcpy(temp1.BrokerID, ACC_SETTING.brokerid.c_str());
		strcpy(temp1.InvestorID, ACC_SETTING.userid.c_str());
		m_pTraderApi->ReqSettlementInfoConfirm(&temp1,1);
	}
}

void TdHandler :: OnRspUserLogout(CThostFtdcUserLogoutField *pUserLogout, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
	if (pRspInfo->ErrorID !=0){
		cout << "交易登出失败!" << endl;
		cout << "ErrorCode = [" << pRspInfo->ErrorID << "], ErrorMsg = [" << pRspInfo->ErrorMsg << "]" << endl;
	}else{
		cout << "交易登出成功！" << endl;
	}

}



void TdHandler :: OnRspError(CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
	printf(" ********** OnRspError:**********\n");
	printf("ErrorCode=[%d], ErrorMsg=[%s]\n",
	pRspInfo->ErrorID, pRspInfo->ErrorMsg);
	printf("RequestID=[%d], Chain=[%d]\n", nRequestID, bIsLast);
}

void TdHandler :: OnRspQrySettlementInfo (CThostFtdcSettlementInfoField *pSettlementInfo, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
	cout << "查询响应  " << endl;
	/*if (pRspInfo->ErrorID !=0){
		cout << "查询结算失败!" << endl;
		cout << "ErrorCode = [" << pRspInfo->ErrorID << "], ErrorMsg = [" << pRspInfo->ErrorMsg << "]" << endl;
	}else{
		cout << "查询结算成功！" << endl;
	}*/
}

void TdHandler :: OnRspSettlementInfoConfirm(CThostFtdcSettlementInfoConfirmField *pSettlementInfoConfirm, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
	if (pRspInfo->ErrorID !=0){
		cout << "结算确认失败!" << endl;
		cout << "ErrorCode = [" << pRspInfo->ErrorID << "], ErrorMsg = [" << pRspInfo->ErrorMsg << "]" << endl;
	}else{
		cout << endl;
		cout << "**********    结算确认成功!    **********" << endl;
		cout << endl;

		// 查询全部合约
		CThostFtdcQryInstrumentField req;
		memset(&req, 0, sizeof(req));
		m_pTraderApi->ReqQryInstrument(&req, 2);
	}
}


void TdHandler :: OnRspQryInstrument(CThostFtdcInstrumentField *pInstrument, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
	//cout << "准备订阅:： " << pInstrument->InstrumentID << endl;

	ALL_CODE.push_back(pInstrument->InstrumentID);

	if (bIsLast){
		// 全市场合约查询完成，通知行情线程开始订阅
		sem_post(&Md_Thread);
		sem_post(&Md_Thread);
	    //m_pTraderApi->RegisterSpi(NULL);
		//m_pTraderApi->Release();
		//m_pTraderApi = NULL;
		//pthread_exit((void *)("交易接口线程退出"));
	}
}

///合约交易状态通知
void TdHandler :: OnRtnInstrumentStatus(CThostFtdcInstrumentStatusField *pInstrumentStatus){
    //cout << pInstrumentStatus->InstrumentID<< "  "<< pInstrumentStatus->InstrumentStatus<< "   **********************************************************" << endl;
	pthread_mutex_lock( &STATUS_LOCK);
	map<string,char>::iterator it = map_ins_status.find(pInstrumentStatus->InstrumentID);
    if (it != map_ins_status.end()){
		cout << pInstrumentStatus->EnterTime << "  "<< pInstrumentStatus->InstrumentID << " 状态由" << (it->second) << " 变为 "\
			<<pInstrumentStatus->InstrumentStatus << endl;
		(it->second) = pInstrumentStatus->InstrumentStatus;;
    }else{
		map_ins_status.insert(pair<string, char>(pInstrumentStatus->InstrumentID, pInstrumentStatus->InstrumentStatus));
    }
    pthread_mutex_unlock( &STATUS_LOCK );
}



