/*
 * mdhandler.cpp
 *
 *  Created on: 2018-2-4
 *      Author: tcz
 */
#include "mdhandler.h"
#include "unistd.h"

using namespace std;

static char SHOUR[2];
static int IHOUR;

static char SMINUTE[2];
static int IMINUTE;

MdHandler :: MdHandler(CThostFtdcMdApi *pUserApi, vector<string> code) : m_pUserApi(pUserApi),subcode(code) {}

MdHandler :: ~MdHandler() {}

void MdHandler :: OnFrontConnected()
{
	CThostFtdcReqUserLoginField reqUserLogin;
	strcpy(reqUserLogin.BrokerID, ACC_SETTING.brokerid.c_str());
	strcpy(reqUserLogin.UserID, ACC_SETTING.userid.c_str());
	strcpy(reqUserLogin.Password, ACC_SETTING.password.c_str());
    cout << "行情前置连接" << endl;
	int flag = m_pUserApi->ReqUserLogin(&reqUserLogin, 9);
	switch (flag){
	    case -1:
		    cout << "网络链接失败。" << endl;
		    break;
	    case -2:
		    cout << "未处理请求超过许可数" << endl;
		    break;
	    case -3:
		    cout << "每秒发送请求数超过许可数" << endl;
		    break;
	}
}

void MdHandler :: OnFrontDisconnected(int nReason)
{
	printf("MD OnFrontDisconnected.\n");
	//ISMDFIRSTLOGIN = false;
}

void MdHandler :: OnRspUserLogin(CThostFtdcRspUserLoginField *pRspUserLogin,CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{

	if (pRspInfo->ErrorID != 0)  {
		printf("MD Failed to login, errorcode=%d errormsg=%s requestid=%d chain = %d", pRspInfo->ErrorID, pRspInfo->ErrorMsg, nRequestID, bIsLast);
		printf("ErrorCode=[%d], ErrorMsg=[%s] \n", pRspInfo->ErrorID,pRspInfo->ErrorMsg);

		printf("RequestID=[%d], Chain=[%d] \n", nRequestID, bIsLast);
	}else{

		cout << endl;
		cout << "**********  行情接口登录成功!  **********" << endl;
		cout << endl;
	}

	/*ifstream readInstrument;
	readInstrument.open("./instrument.txt");
	string contract;
	getline(readInstrument,contract);
	//等待交易线程登录成功
	if(ISMDFIRSTLOGIN)
		sem_wait(&Trader_Thread);
	*/

	this->ActionDay = pRspUserLogin->TradingDay;
	char **p = new char *[subcode.size()];
	for(vector<string>::size_type beg=0; beg!=subcode.size(); beg++)
	{
		char *tempp = new char[40];
		strcpy(tempp,const_cast<char*>(subcode[beg].c_str()));
		p[(int)beg] = tempp;
	}

	m_pUserApi->SubscribeMarketData(p,(int)subcode.size());
	//释放char*数组
	for(vector<string>::size_type beg=0; beg!=subcode.size(); beg++)
		delete [] p[(int)beg];
	delete [] p;

	//初始化信号量 通知k线计算线程，有tick数据可用。
	sem_init(&k_signal, 0, 0);

	//初始化信号量 通知写k线数据线程，k线行情队列有可用数据。
	sem_init(&Md_Queue_K, 0, 0);


	pthread_t threadMdID;
	pthread_create(&threadMdID, NULL, calcu_k_func, (void *)this);

	pthread_t threadMdID1;
	pthread_create(&threadMdID1, NULL, write_k2mongo, (void *)this);

}

void MdHandler :: OnRtnDepthMarketData(CThostFtdcDepthMarketDataField *pDepthMarketData)
{

	//剔除出开盘前数据
	IHOUR = atoi(strncpy(SHOUR, pDepthMarketData->UpdateTime, 2));
	IMINUTE = atoi(strncpy(SMINUTE, pDepthMarketData->UpdateTime+3, 2));


	//粗略过滤。
    //白天8点到19点之间登录，却收到 16点以后，或者 凌晨1点和2点的行情
	if (atoi(LOGINHOUR)>=8 && atoi(LOGINHOUR) < 20 && (IHOUR> 15||IHOUR<=2)){
		cout << pDepthMarketData->UpdateTime <<  " 日盘收到夜盘时间行情，过滤" << endl;
		return;
	}
	//minute = atoi(strncpy(temp2char, this_data.UpdateTime+3, 2));

	//16点-20点，或 凌晨3点-早上8点 或 中午11点半到12点的数据
	if ( (IHOUR >15 && IHOUR < 20) || (IHOUR<8 && IHOUR>=3) || (IHOUR==11 && IMINUTE>30) ){
		cout << pDepthMarketData->UpdateTime <<  " 非交易时间行情，过滤" << endl;
		return;
	}


    //cout <<  pDepthMarketData->UpdateTime << " " << pDepthMarketData->InstrumentID << endl;
	market_data *p_this_data = new market_data;
	strcpy(p_this_data->TradingDay, pDepthMarketData->TradingDay);
	strcpy(p_this_data->InstrumentID, pDepthMarketData->InstrumentID);
	p_this_data->LastPrice = pDepthMarketData->LastPrice;
	p_this_data->PreSettlementPrice = pDepthMarketData->PreSettlementPrice;
	p_this_data->PreClosePrice = pDepthMarketData->PreClosePrice;
	p_this_data->PreOpenInterest = pDepthMarketData->PreOpenInterest;
	p_this_data->OpenPrice = pDepthMarketData->OpenPrice;
	p_this_data->HighestPrice = pDepthMarketData->HighestPrice;
	p_this_data->LowestPrice = pDepthMarketData->LowestPrice;
	p_this_data->Volume = pDepthMarketData->Volume;
	p_this_data->Turnover = pDepthMarketData->Turnover;
	p_this_data->OpenInterest = pDepthMarketData->OpenInterest;
	p_this_data->UpperLimitPrice = pDepthMarketData->UpperLimitPrice;
	p_this_data->LowerLimitPrice = pDepthMarketData->LowerLimitPrice;
	strcpy(p_this_data->UpdateTime, pDepthMarketData->UpdateTime);
	p_this_data->UpdateMillisec = pDepthMarketData->UpdateMillisec;
	p_this_data->BidPrice1 = pDepthMarketData->BidPrice1;
	p_this_data->BidVolume1 = pDepthMarketData->BidVolume1;
	p_this_data->AskPrice1 = pDepthMarketData->AskPrice1;
	p_this_data->AskVolume1 = pDepthMarketData->AskVolume1;
	strcpy(p_this_data->ActionDay, pDepthMarketData->ActionDay);

	//推入写tick队列
	MARKET_QUEQUE.push(p_this_data);
	sem_post(&Md_Queue_Write);

    market_data_for_k *p_this_data_k = new market_data_for_k;
	strcpy( p_this_data_k->TradingDay, pDepthMarketData->TradingDay );
	strcpy( p_this_data_k->InstrumentID, pDepthMarketData->InstrumentID );
	p_this_data_k->LastPrice = pDepthMarketData->LastPrice;
	p_this_data_k->Volume = pDepthMarketData->Volume;
	strcpy( p_this_data_k->UpdateTime, pDepthMarketData->UpdateTime);
	p_this_data_k->UpdateMillisec = pDepthMarketData->UpdateMillisec;
	strcpy( p_this_data_k->ActionDay, pDepthMarketData->ActionDay );

	//推入计算k线队列。k线计算线程开始计算
	market_tick_queue.push(p_this_data_k);
	sem_post(&k_signal);

	//pthread_t threadid = pthread_self();
	//cout << "线程：" << threadid << "   "<< pDepthMarketData->UpdateTime << " " << pDepthMarketData->InstrumentID << " " << pDepthMarketData->LastPrice << endl;
	return;
}


// 计算k线函数
void* MdHandler :: calcu_k_func(void *arg){
    MdHandler *thisp = (MdHandler *)arg;

    //初始化存储所有合约当前bar和上一根bar的map结构变量，
    map<string,vector<bar>> bars;
    for (vector<string>::size_type i=0; i < thisp->subcode.size(); i++){
        bar temp_bar;
        strcpy(temp_bar.InstrumentID, (thisp->subcode)[i].c_str());
        strncpy(temp_bar.UpdateTime, DATETIME, 16);
        temp_bar.OpenPrice = 0;
        temp_bar.HighPrice = 0;
        temp_bar.LowPrice = 10000000.0;
        temp_bar.ClosePrice = 0;
        temp_bar.Volume = 0;
        temp_bar.TotalVolume = 0;
        temp_bar.LastTotalVolume = 0;
        vector<bar> twobar;
        twobar.push_back(temp_bar);
        twobar.push_back(temp_bar);
        bars.insert(pair<string,vector<bar>>(temp_bar.InstrumentID, twobar));
    }

    map<string,vector<bar>>::iterator it;

    // 循环接收每个tick行情，计算k线
    market_data_for_k *p_this_data;
    //char temp2char[2];
    //int hour;
    char lasttime[6];
    lasttime[5] = '\0';
    char thistime[6];
    thistime[5] = '\0';
    strncpy(thistime, DATETIME+11, 5);

    char tempthistime[6];
	while(true){

		//等待行情数据推过来
		sem_wait(&(thisp->k_signal));

		//计算全局时间，所属分钟

		//提取行情数据
		thisp->market_tick_queue.pop(p_this_data);
		market_data_for_k this_data = *p_this_data;
		delete (market_data_for_k *)p_this_data;
		p_this_data = NULL;

		strncpy(tempthistime, this_data.UpdateTime, 5);

		if (strcmp(thistime, tempthistime) <0){

			//新的一分钟开始
			strncpy(lasttime, thistime, 5);
			strncpy(thistime, tempthistime, 5);

			//将所有bars内所有合约的bar循环写入mongo。
			for (map< string,vector<bar> >::iterator ite = bars.begin(); ite!=bars.end();ite++){

				//bar时间 HH:SS
				strncpy(((ite->second).back().UpdateTime)+11, lasttime, 5);
				//成交量
				(ite->second).back().Volume = (ite->second).back().TotalVolume - (ite->second).front().TotalVolume;
				//推入待写bar队列
				(thisp->MARKET_K_QUEUE).push((ite->second).back());

				//当前bar前推赋值给上一个根bar
				(ite->second).front() = (ite->second).back();

				//重置当前bar
				ite->second.back().OpenPrice = ite->second.back().ClosePrice;
				ite->second.back().HighPrice = ite->second.back().ClosePrice;
				ite->second.back().LowPrice = ite->second.back().ClosePrice;
				ite->second.back().Volume = 0;
				ite->second.back().TotalVolume = 0;

				sem_post(&(thisp->Md_Queue_K));
			}

		}else if (strcmp(thistime, tempthistime) == 0){
			//分钟内
			//找到合约
		    it=bars.find(this_data.InstrumentID);

		    //更新收盘价
			it->second.back().ClosePrice = this_data.LastPrice;

			//更新最高价
			if (this_data.LastPrice > (it->second).back().HighPrice)
				(it->second).back().HighPrice = this_data.LastPrice;

			//更新最低价
			if (this_data.LastPrice < (it->second).back().LowPrice)
				(it->second).back().LowPrice = this_data.LastPrice;

			//更新总成交量
			(it->second).back().TotalVolume = this_data.Volume;


		}else if(strcmp(thistime, tempthistime) == 0){

			//收到的行情时间小于当前时间，表明本笔行情延迟了,并且假设延迟不超过一分钟，即延迟收到的行情是上一分钟的行情。
			cout << this_data.UpdateTime << "  " << this_data.InstrumentID << " 行情延迟" << endl;
			//分钟内
			//找到合约
			it=bars.find(this_data.InstrumentID);

			//更新收盘价
			it->second.front().ClosePrice = this_data.LastPrice;

			//更新最高价
			if (this_data.LastPrice > (it->second).front().HighPrice)
				(it->second).front().HighPrice = this_data.LastPrice;

			//更新最低价
			if (this_data.LastPrice < (it->second).front().LowPrice)
				(it->second).front().LowPrice = this_data.LastPrice;

			//更新总成交量
			(it->second).front().Volume = (it->second).front().Volume + ( this_data.Volume-(it->second).front().TotalVolume);

			//时间 HH:SS
			//strncpy((it->second).front().UpdateTime+11, this_data.UpdateTime, 5);

			//推入bar队列，将会覆盖update上一分钟的bar
			(thisp->MARKET_K_QUEUE).push((it->second).front());

			sem_post(&(thisp->Md_Queue_K));
		}


/*
		//剔除出开盘前数据
		hour = atoi(strncpy(temp2char, this_data.UpdateTime, 2));
		minute = atoi(strncpy(temp2char, this_data.UpdateTime+3, 2));
        if ( (hour >=15 && hour <=21) || (hour<9 && hour>3) ){
        	//cout << "分钟过滤" << endl;
        	continue;
        }

		//找到合约
	    it=bars.find(this_data.InstrumentID);

		//判断本根bar是否走完。
		if ( (it->second).UpdateTime[14] == this_data.UpdateTime[3] && (it->second).UpdateTime[15] == this_data.UpdateTime[4] ){
			//bar中行情

			//更新收盘价
			(it->second).ClosePrice = this_data.LastPrice;

			//更新最高价
			if (this_data.LastPrice > (it->second).HighPrice)
				(it->second).HighPrice = this_data.LastPrice;

			//更新最低价
			if (this_data.LastPrice < (it->second).LowPrice)
				(it->second).LowPrice = this_data.LastPrice;

			//更新总成交量
			(it->second).TotalVolume = this_data.Volume;

			//时间 HH:SS
			strncpy((it->second).UpdateTime+11, this_data.UpdateTime, 5);
		}else{
			//本条行情是最新一个根bar的开盘价，上一个bar已经结束。

			//计算上一根bar的成交量
			(it->second).Volume = (it->second).TotalVolume - (it->second).LastTotalVolume;

			// 临时打印
			//cout << bars[i].UpdateTime << "  " << bars[i].InstrumentID << "  "<< bars[i].OpenPrice <<"  "<< bars[i].HighPrice <<"  "<< bars[i].LowPrice\
			//		<<"  "<< bars[i].ClosePrice <<"  "<< bars[i].Volume << endl;

			//上一根bar bars[i] 推入队列，准备写入mongo
			(thisp->MARKET_K_QUEUE).push((it->second));

			sem_post(&(thisp->Md_Queue_K));

			//初始化新的一根bar
			(it->second).OpenPrice = this_data.LastPrice;
			(it->second).HighPrice = this_data.LastPrice;
			(it->second).LowPrice = this_data.LastPrice;
			(it->second).ClosePrice = this_data.LastPrice;

			(it->second).LastTotalVolume = (it->second).TotalVolume;
			(it->second).TotalVolume = this_data.Volume;
			strncpy((it->second).UpdateTime+11, this_data.UpdateTime, 5);
		}
*/

        /*
		//开始计算，遍历所有合约
        for (vector<bar>::size_type i=0; i<bars.size(); i++){
        	//找到合约
        	if ( !strcmp(this_data.InstrumentID, bars[i].InstrumentID) ){
        		//判断本根bar是否走完。
                if ( bars[i].UpdateTime[14] == this_data.UpdateTime[3] && bars[i].UpdateTime[15] == this_data.UpdateTime[4] ){
                    //bar中行情

                	//更新收盘价
                	bars[i].ClosePrice = this_data.LastPrice;

                	//更新最高价
                	if (this_data.LastPrice > bars[i].HighPrice)
                		bars[i].HighPrice = this_data.LastPrice;

                	//更新最低价
                	if (this_data.LastPrice < bars[i].LowPrice)
                		bars[i].LowPrice = this_data.LastPrice;

                	//更新总成交量
                	bars[i].TotalVolume = this_data.Volume;

                	//时间 HH:SS
                	strncpy(bars[i].UpdateTime+11, this_data.UpdateTime, 5);
                }else{
                	//本条行情是最新一个根bar的开盘价，上一个bar已经结束。

                	//计算上一根bar的成交量
                	bars[i].Volume = bars[i].TotalVolume - bars[i].LastTotalVolume;

                	// 临时打印
                    //cout << bars[i].UpdateTime << "  " << bars[i].InstrumentID << "  "<< bars[i].OpenPrice <<"  "<< bars[i].HighPrice <<"  "<< bars[i].LowPrice\
                    //		<<"  "<< bars[i].ClosePrice <<"  "<< bars[i].Volume << endl;

                    //上一根bar bars[i] 推入队列，准备写入mongo
                	(thisp->MARKET_K_QUEUE).push(bars[i]);

                    sem_post(&(thisp->Md_Queue_K));

                    //初始化新的一根bar
                	bars[i].OpenPrice = this_data.LastPrice;
                	bars[i].HighPrice = this_data.LastPrice;
                	bars[i].LowPrice = this_data.LastPrice;
                	bars[i].ClosePrice = this_data.LastPrice;

                	bars[i].LastTotalVolume = bars[i].TotalVolume;
                	bars[i].TotalVolume = this_data.Volume;
                	strncpy(bars[i].UpdateTime+11, this_data.UpdateTime, 5);
                }
                break;
        	}
        }
        */
        //cout << "计算k线  " << this_data.UpdateTime << "   " << this_data.InstrumentID << "  "<< this_data.LastPrice << endl;

	}
}


// 写bar到mongo线程
void* MdHandler :: write_k2mongo(void *arg){
    MdHandler *thisp = (MdHandler *)arg;

    string uristring = "mongodb://";
	if (!MONGODB_SETTING.username.empty())
		uristring = uristring + MONGODB_SETTING.username + ":" + MONGODB_SETTING.password + "@";
	uristring = uristring + MONGODB_SETTING.host + ":" + MONGODB_SETTING.port;

	mongocxx::uri uri(uristring.c_str());
    mongocxx::client client(uri);
	//mongocxx::client client{mongocxx::uri{}};
   	mongocxx::database db = client[MONGODB_SETTING.db.c_str()];
   	mongocxx::collection coll;
   	char temptime[6] = {'\0'};

   	map<string,instrument_status>::iterator it;

   	char status;
   	tm tm_time;
   	time_t timep;
	while(true){

		bar this_data;

		sem_wait(&(thisp->Md_Queue_K));
		(thisp->MARKET_K_QUEUE).pop(this_data);
		strncpy(temptime, this_data.UpdateTime + 11, 5);

		it = map_ins_status.find(this_data.InstrumentID);

		pthread_mutex_lock( &((it->second).lock) );
		status = (it->second).status;
		pthread_mutex_unlock( &((it->second).lock) );
		if (status!='2'){
			//非交易
			if (! (strcmp(temptime,"10:14")==0 || strcmp(temptime,"11:29")==0 || strcmp(temptime,"14:59")==0 \
					|| strcmp(temptime,"22:59")==0 || strcmp(temptime,"23:29")==0 || strcmp(temptime,"00:59")==0\
					|| strcmp(temptime,"02:29")==0) ){
				continue;
			}
		}


		// 早上8点到9点之间的k线过滤
		if (strcmp(temptime,"09:00")<0 && strcmp(temptime,"08:00")>0)
			continue;
		// 晚上20点到21点之间的k线过滤
		if (strcmp(temptime,"21:00")<0 && strcmp(temptime,"20:00")>0)
			continue;
		// 下午15点到16点之间的k线过滤
		if (strcmp(temptime,"15:00")>=0 && strcmp(temptime,"16:00")<0)
			continue;
		// 中午11：30到13点之间的k线过滤
		if (strcmp(temptime,"11:30")>=0 && strcmp(temptime,"13:00")< 0 )
			continue;

		strptime(this_data.UpdateTime, "%Y-%m-%d %H:%M", &tm_time);
		timep = mktime(&tm_time);

		coll = db[this_data.InstrumentID + string("_1min")];

		//写入mongodb
		//bsoncxx::builder::basic::document document{};
		auto doc = bsoncxx::builder::basic::document{};

		doc.append(bsoncxx::builder::basic::kvp("timestamp", timep));
		doc.append(bsoncxx::builder::basic::kvp("time", this_data.UpdateTime));
		doc.append(bsoncxx::builder::basic::kvp("open", this_data.OpenPrice));
		doc.append(bsoncxx::builder::basic::kvp("high", this_data.HighPrice));
		doc.append(bsoncxx::builder::basic::kvp("low", this_data.LowPrice));
		doc.append(bsoncxx::builder::basic::kvp("close", this_data.ClosePrice));
		doc.append(bsoncxx::builder::basic::kvp("vol", this_data.Volume));

		coll.insert_one(doc.view());

		//临时打印
		cout << "1分钟K线 " <<  this_data.UpdateTime << "   " << this_data.InstrumentID << "  "<< this_data.OpenPrice << \
				"  "<< this_data.HighPrice << "  "<< this_data.LowPrice << "  "<< this_data.ClosePrice << endl;

	}
}



void MdHandler :: OnRspError(CThostFtdcRspInfoField *pRspInfo, int nRequestID,
	bool bIsLast) {
	printf("OnRspError: \n");
	printf("ErrorCode=[%d], ErrorMsg=[%s] \n", pRspInfo->ErrorID,
		pRspInfo->ErrorMsg);
	printf("RequestID=[%d], Chain=[%d] \n", nRequestID, bIsLast);
}

void MdHandler :: OnRspUserLogout(CThostFtdcUserLogoutField *pUserLogout, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
	if (pRspInfo->ErrorID != 0){
		cout << "MD Logout failed!";
		cout << "ErrorCode = [" << pRspInfo->ErrorID << "], ErrorMsg = [" << pRspInfo->ErrorMsg << "]"     << endl;
	}else
		cout << "MD Logout Successed!" << endl;

	cout << pRspInfo->ErrorID << "   " << pRspInfo->ErrorMsg << endl;
}
void MdHandler :: OnRspUnSubMarketData(CThostFtdcSpecificInstrumentField *pSpecificInstrument, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
	cout << pSpecificInstrument->InstrumentID << " 取消订阅行情" << endl;
	if (pRspInfo->ErrorID != 0)
		cout << pSpecificInstrument->InstrumentID << " 取消订阅行情失败" << endl;
	else
		cout << pSpecificInstrument->InstrumentID << " 取消订阅行情成功" << endl;
}



