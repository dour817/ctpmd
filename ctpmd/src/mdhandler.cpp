/*
 * mdhandler.cpp
 *
 *  Created on: 2018-2-4
 *      Author: tcz
 */
#include "mdhandler.h"
#include "unistd.h"

using namespace std;


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

	//初始化信号量 通知写k线数据线程，k线行情队列有可用数据。
	sem_init(&Md_Queue_K_LAST, 0, 0);


	pthread_t threadMdID;
	pthread_create(&threadMdID, NULL, calcu_k_func, (void *)this);

	pthread_t threadMdID1;
	pthread_create(&threadMdID1, NULL, write_k2mongo, (void *)this);

	pthread_t threadMdID2;
	pthread_create(&threadMdID2, NULL, update_k2mongo, (void *)this);

}

void MdHandler :: OnRtnDepthMarketData(CThostFtdcDepthMarketDataField *pDepthMarketData)
{

	int ihour;
	int iminute;
	char chour[3] ={'\0'};
	char cminute[3] ={'\0'};
	ihour = atoi(strncpy(chour, pDepthMarketData->UpdateTime, 2));
	iminute = atoi(strncpy(cminute, pDepthMarketData->UpdateTime+3, 2));

	// 存日行情数据
	//if (pDepthMarketData->SettlementPrice > 0 && pDepthMarketData->SettlementPrice <9999999){
	if ((ihour == 15 || ihour == 16)&& pDepthMarketData->SettlementPrice > 0 && pDepthMarketData->SettlementPrice <9999999){
		md_daily *p_this_data = new md_daily;
		p_this_data->instrument = pDepthMarketData->InstrumentID;
		p_this_data->date = pDepthMarketData->TradingDay;
		p_this_data->open = pDepthMarketData->OpenPrice;
		p_this_data->high = pDepthMarketData->HighestPrice;
		p_this_data->low = pDepthMarketData->LowestPrice;
		p_this_data->close = pDepthMarketData->ClosePrice;
		p_this_data->settlement = pDepthMarketData->SettlementPrice;
		p_this_data->vol = pDepthMarketData->Volume;
		p_this_data->oi = pDepthMarketData->OpenInterest;
		p_this_data->pre_settlement = pDepthMarketData->PreSettlementPrice;
	    p_this_data->pre_close = pDepthMarketData->PreClosePrice;
		p_this_data->pre_oi = pDepthMarketData->PreOpenInterest;

		//推入写日行情队列
		CLOSE_MARKET_QUEQUE.push(p_this_data);
		sem_post(&Md_Queue_Write_Daily);
	}


	//粗略过滤。
    //白天8点到19点之间登录，却收到 16点以后，或者 凌晨1点和2点的行情
	if (atoi(LOGINHOUR)>=8 && atoi(LOGINHOUR) < 20 && (ihour> 15||ihour<=2)){
		//cout << pDepthMarketData->UpdateTime <<  " 日盘收到夜盘时间行情，过滤" << endl;
		return;
	}
	//minute = atoi(strncpy(temp2char, this_data.UpdateTime+3, 2));

	//16点-20点，或 凌晨3点-早上8点 或 中午11点半到12点的数据
	// 3 4 5 6 7 12 16 17 18 19 11：30 的行情数据过滤
	if ( (ihour >15 && ihour < 20) || (ihour<8 && ihour>=3) || (ihour==11 && iminute>30) || ihour == 12 ){
		//cout << pDepthMarketData->UpdateTime <<  " 非交易时间行情，过滤" << endl;
		return;
	}



    //cout <<  pDepthMarketData->UpdateTime << " " << pDepthMarketData->InstrumentID << endl;
	market_data *p_this_data = new market_data;
	strcpy(p_this_data->TradingDay, pDepthMarketData->TradingDay);
	strcpy(p_this_data->InstrumentID, pDepthMarketData->InstrumentID);
	p_this_data->LastPrice = pDepthMarketData->LastPrice;
	//p_this_data->PreSettlementPrice = pDepthMarketData->PreSettlementPrice;
	//p_this_data->PreClosePrice = pDepthMarketData->PreClosePrice;
	//p_this_data->PreOpenInterest = pDepthMarketData->PreOpenInterest;
	//p_this_data->OpenPrice = pDepthMarketData->OpenPrice;
	//p_this_data->HighestPrice = pDepthMarketData->HighestPrice;
	//p_this_data->LowestPrice = pDepthMarketData->LowestPrice;
	p_this_data->Volume = pDepthMarketData->Volume;
	p_this_data->Turnover = pDepthMarketData->Turnover;
	p_this_data->OpenInterest = pDepthMarketData->OpenInterest;
	p_this_data->UpperLimitPrice = pDepthMarketData->UpperLimitPrice;
	p_this_data->LowerLimitPrice = pDepthMarketData->LowerLimitPrice;
	strcpy(p_this_data->UpdateTime, pDepthMarketData->UpdateTime);
	//p_this_data->UpdateMillisec = pDepthMarketData->UpdateMillisec;
	p_this_data->BidPrice1 = pDepthMarketData->BidPrice1;
	p_this_data->BidVolume1 = pDepthMarketData->BidVolume1;
	p_this_data->AskPrice1 = pDepthMarketData->AskPrice1;
	p_this_data->AskVolume1 = pDepthMarketData->AskVolume1;
	strcpy(p_this_data->ActionDay, pDepthMarketData->ActionDay);

	//推入写tick队列
	MARKET_QUEQUE.push(p_this_data);
	sem_post(&Md_Queue_Write);

    market_data_for_k *p_this_data_k = new market_data_for_k;
    memset(p_this_data_k, 0, sizeof(market_data_for_k));
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
        temp_bar.OpenPrice = -1;
        temp_bar.HighPrice = -1;
        temp_bar.LowPrice = 10000000.0;
        temp_bar.ClosePrice = -1;
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

    //上一根k线时间
    char lastktime[6] = {'\0'};

    //当前k线时间
    char nowktime[6] = {'\0'};
    strncpy(nowktime, DATETIME+11, 5);
    //cout << "nowktime:"<< nowktime << endl;

    //本次收到的tick行情时间(分钟)
    char revmdtime[6] = {'\0'};

    //int loginhour;

    time_t now ;
    struct tm *tm_now ;
	while(true){

		//等待行情数据推过来
		sem_wait(&(thisp->k_signal));

		//提取行情数据
		thisp->market_tick_queue.pop(p_this_data);
		market_data_for_k this_data = *p_this_data;
		delete (market_data_for_k *)p_this_data;
		p_this_data = NULL;

		//找到合约
		it=bars.find(this_data.InstrumentID);
		//处理盘中启动时 第一跟bar开盘价为0的bug
		//loginhour = atoi(LOGINHOUR);
		//if ((it->second).front().OpenPrice == -1 && ((loginhour >=9 && loginhour<15) || (loginhour >=21 || loginhour<3)) ){
		if ((it->second).front().OpenPrice == -1 ){
			//程序启动后本合约第一笔数据

			(it->second).back().OpenPrice = this_data.LastPrice;
			(it->second).back().HighPrice = this_data.LastPrice;
			(it->second).back().LowPrice = this_data.LastPrice;
			(it->second).back().ClosePrice = this_data.LastPrice;
			(it->second).back().TotalVolume = this_data.Volume;
			strcpy((it->second).back().OpenTime, this_data.UpdateTime);
			strcpy((it->second).back().CloseTime, this_data.UpdateTime);

			(it->second).front().OpenPrice = this_data.LastPrice;
			(it->second).front().HighPrice = this_data.LastPrice;
			(it->second).front().LowPrice = this_data.LastPrice;
			(it->second).front().ClosePrice = this_data.LastPrice;
			(it->second).front().TotalVolume = this_data.Volume;
			strcpy((it->second).back().OpenTime, this_data.UpdateTime);
			strcpy((it->second).back().CloseTime, this_data.UpdateTime);
		}


		strncpy(revmdtime, this_data.UpdateTime, 5);
		//cout <<"DATETIME:" << DATETIME << "  nowktime：" << nowktime << "  revmdtime:" << revmdtime << "*************************************"<< endl;
		if ( (strcmp(nowktime, revmdtime) <0 || (strcmp(nowktime, "23:58")>=0 && strcmp(revmdtime, "00:01")<=0)) \
				&& !(strcmp(nowktime,"03:00") < 0 && strcmp(revmdtime,"15:30") > 0)  ){
			//收到的tick行情的分钟大于当前bar的分钟，表明新的一分钟开始。
			//cout << "nowktime:"<<nowktime << " revmdtime:"<< revmdtime << endl;
			//新的一分钟开始
			strncpy(lastktime, nowktime, 5);
			strncpy(nowktime, revmdtime, 5);
			cout <<"-----------------------------------------------------------------------------------"<< endl;
			//将所有bars内所有合约的bar循环写入mongo。
			for (map< string,vector<bar> >::iterator ite = bars.begin(); ite!=bars.end();ite++){

				//成交量
				(ite->second).back().Volume = (ite->second).back().TotalVolume - (ite->second).front().TotalVolume;
				//推入待写bar队列
				(thisp->MARKET_K_QUEUE).push((ite->second).back());

				//当前bar前推赋值给上一个根bar
				(ite->second).front() = (ite->second).back();

				//重置当前bar
				strcpy(ite->second.back().OpenTime, ite->second.back().CloseTime);

				ite->second.back().OpenPrice = ite->second.back().ClosePrice;
				ite->second.back().HighPrice = ite->second.back().ClosePrice;
				ite->second.back().LowPrice = ite->second.back().ClosePrice;
				ite->second.back().Volume = 0;
				//ite->second.back().TotalVolume = 0;

				//最新(当前)一根bar的时间 自然日+分钟
				//bar时间 HH:SS
				//bar日期 yyyy-mm-dd

				// 郑州的夜盘 业务日期和交易日期相同 ，而其他三个交易所的业务日期是自然日，所以bar真实时间不能用ActionDay业务日期
				// 需要从
			    time(&now) ;
				tm_now = localtime(&now) ;
				sprintf(((ite->second).back().UpdateTime),"%04d-%02d-%02d %s", tm_now->tm_year+1900, tm_now->tm_mon+1, tm_now->tm_mday,\
						nowktime);\


				sem_post(&(thisp->Md_Queue_K));
			}

		}else if (strcmp(nowktime, revmdtime) == 0){
			//分钟内

		    //更新收盘价
			it->second.back().ClosePrice = this_data.LastPrice;

			//判断开盘价是否有误，开盘价的时间可能是上一分钟末的行情
			if (strncmp(this_data.UpdateTime, it->second.back().OpenTime, 5) !=0){
				it->second.back().OpenPrice = this_data.LastPrice;
				strcpy(it->second.back().OpenTime, this_data.UpdateTime);
				//it->second.back().TotalVolume = this_data.Volume;
				cout << ">>>>>>>>>>>>>>>>>>>>>>>>>>>>>更新开盘价<<<<<<<<<<<<<<<<<<<<<<<<<<<<<" << endl;
			}

			//更新最高价
			if (this_data.LastPrice > (it->second).back().HighPrice)
				(it->second).back().HighPrice = this_data.LastPrice;

			//更新最低价
			if (this_data.LastPrice < (it->second).back().LowPrice)
				(it->second).back().LowPrice = this_data.LastPrice;

			//更新总成交量
			(it->second).back().TotalVolume = this_data.Volume;

			//更新收盘价时间 HH:MM:SS
			strcpy(it->second.back().CloseTime, this_data.UpdateTime);

		}/*else if(strcmp(nowktime, revmdtime) > 0){

			//收到的行情时间小于当前时间，表明本笔行情延迟了,并且假设延迟不超过一分钟，即延迟收到的行情是上一分钟的行情。
			cout << this_data.UpdateTime << "  " << this_data.InstrumentID << " 行情延迟" << endl;

			//修正收盘价
			it->second.front().ClosePrice = this_data.LastPrice;

			//修正最高价
			if (this_data.LastPrice > (it->second).front().HighPrice)
				(it->second).front().HighPrice = this_data.LastPrice;

			//修正最低价
			if (this_data.LastPrice < (it->second).front().LowPrice)
				(it->second).front().LowPrice = this_data.LastPrice;

			//更新成交量
			(it->second).front().Volume = (it->second).front().Volume + ( this_data.Volume - (it->second).front().TotalVolume);

			//更新总成交量
			(it->second).front().TotalVolume = this_data.Volume;
			//时间 HH:SS
			//strncpy((it->second).front().UpdateTime+11, this_data.UpdateTime, 5);

			//修正(it->second).front()上一分钟的bar数据，推入修正bar线程
			//推入bar队列，将会覆盖update上一分钟的bar
			(thisp->MARKET_K_QUEUE_LAST).push((it->second).front());

			sem_post(&(thisp->Md_Queue_K_LAST));
		}
       */
	}
}

// 更新上一分钟bar update到mongodb 的线程
void * MdHandler :: update_k2mongo(void *arg){
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

	tm tm_time;
	time_t timep;

	while (true){
		bar this_data;

		sem_wait(&(thisp->Md_Queue_K_LAST));
		(thisp->MARKET_K_QUEUE_LAST).pop(this_data);

		strptime(this_data.UpdateTime, "%Y-%m-%d %H:%M", &tm_time);
	    timep = mktime(&tm_time);

		coll = db[this_data.InstrumentID + string("_1min")];

		auto doc = bsoncxx::builder::basic::document{};
		doc.append(bsoncxx::builder::basic::kvp("timestamp", timep));
		doc.append(bsoncxx::builder::basic::kvp("time", this_data.UpdateTime));
		doc.append(bsoncxx::builder::basic::kvp("open", this_data.OpenPrice));
		doc.append(bsoncxx::builder::basic::kvp("high", this_data.HighPrice));
		doc.append(bsoncxx::builder::basic::kvp("low", this_data.LowPrice));
		doc.append(bsoncxx::builder::basic::kvp("close", this_data.ClosePrice));
		doc.append(bsoncxx::builder::basic::kvp("vol", this_data.Volume));
		doc.append(bsoncxx::builder::basic::kvp("update", 1));

		auto olddoc = bsoncxx::builder::basic::document{};
		olddoc.append(bsoncxx::builder::basic::kvp("time", this_data.UpdateTime));

		auto tempdoc = bsoncxx::builder::basic::document{};
		tempdoc.append(bsoncxx::builder::basic::kvp("$set", doc.view()));

		auto para = mongocxx::v_noabi::options::update{};
		para.upsert(false);

		coll.update_one(olddoc.view(), tempdoc.view(), para);

		cout << "********************** 更新  " <<this_data.UpdateTime << " " << this_data.InstrumentID \
				<< "***************************" << endl;

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
   	char thismdtime[6] = {'\0'};

   	//合约状态map 指针
   	map<string,char>::iterator it;

	//记录本合约本次k线时间的map结构的 指针
   	map<string,string>::iterator itit;

   	char status;
   	tm tm_time;
   	time_t timep;

   	// 记录每一个合约最近本次写入的k线的时间
   	map<string,string> maplastktime;
    for (vector<string>::size_type i=0; i < thisp->subcode.size(); i++){
    	maplastktime.insert( pair< string,string >((thisp->subcode)[i], "22:59") );
    }

   	//等待品种交易状态推送完毕。
    //sleep(2);

	while(true){

		bar this_data;

		sem_wait(&(thisp->Md_Queue_K));

		(thisp->MARKET_K_QUEUE).pop(this_data);
		strncpy(thismdtime, this_data.UpdateTime + 11, 5);

		if (this_data.OpenPrice == -1 || this_data.HighPrice == -1 || this_data.LowPrice == -1\
				|| this_data.ClosePrice == -1){
			continue;
		}

		//品种代码
		//去掉数字
		char pinzhong[50]={'\0'};
		strcpy(pinzhong, this_data.InstrumentID);

		for (unsigned int i = 0; i < strlen(pinzhong); i++){
			if ((*(this_data.InstrumentID + i) >='0' && *(this_data.InstrumentID + i) <='9') || *(this_data.InstrumentID + i) =='-'){
				pinzhong[i] = '\0';
				break;
			}
		}

		//去掉空格

		unsigned int spaceloc=strlen(pinzhong);
		for (unsigned int  i= 0; i < strlen(pinzhong); i++){
			char c = *(pinzhong + i);
			if (!((c>='a'&&c<='z') || (c>='A'&&c<='Z') )){
				spaceloc = i;
			}
		}
		char temp[50];
		if (spaceloc < strlen(pinzhong)){
			//有空格去掉空格前的字符
			strcpy(temp, pinzhong+spaceloc+1);
			strcpy(pinzhong, temp);
			//cout << " 本品种去空格：" << pinzhong <<  "  "<<this_data.InstrumentID<< endl;
		}

		//cout << " ****************本品种 ：" <<this_data.InstrumentID<< endl;
		pthread_mutex_lock( &STATUS_LOCK);
		it = map_ins_status.find(pinzhong);
        if (it != map_ins_status.end()){
			status = it->second;
			//cout << this_data.InstrumentID <<  "  status :"<< status << endl;
			pthread_mutex_unlock( &STATUS_LOCK );
			if (status!='2'){
				//cout << this_data.InstrumentID << "在状态map中 找到 " << status << endl;
				//非交易
				if (! (strcmp(thismdtime,"10:14")==0 || strcmp(thismdtime,"11:29")==0 || strcmp(thismdtime,"14:59")==0 \
						|| strcmp(thismdtime,"22:59")==0 || strcmp(thismdtime,"23:29")==0 || strcmp(thismdtime,"00:59")==0\
						|| strcmp(thismdtime,"02:29")==0) ){
					// 上午修盘，或者夜盘收盘的最后一分钟 ，有可能在收到合约状态为“收盘”的信号后，最后一分钟的k线还没写入。
					// 所以最后一分钟的关键节点的k线需要写入，其他分钟的k线则过滤
					// 但次数有个问题。比如rb 23:00收盘后，随后23:29 00:59 02:29 的关键节点k线都会写入。
					// 所以还要在本个if else 结构后面 再加一层判断。
					continue;
				}
			}
        }else{
        	pthread_mutex_unlock( &STATUS_LOCK );
        	//cout << this_data.InstrumentID << "在状态map中未找到。。。 "  << endl;

        	//如果在状态表中 map_ins_status未找到，默认未开盘。过滤
        	continue;
        }

        if (status!='2'){
        	// 判断在已经收盘的前提下，最后一个关键节点的k线数据是否已经写入，如果已经写入，则continue过滤
        	// 通过maplastktime 找到本品种上一次写入的分钟数
        	map<string,string>::iterator tempit  = maplastktime.find(this_data.InstrumentID);
            string lastktime;
        	if (tempit != maplastktime.end()){
        		lastktime = tempit->second;
        	}
        	if (strcmp(lastktime.c_str(),"22:59")==0 || strcmp(lastktime.c_str(),"23:29")==0 || strcmp(lastktime.c_str(),"00:59")==0\
        							|| strcmp(lastktime.c_str(),"02:29")==0 )
        		continue;
        }



		// 早上8点到9点之间的k线过滤
		if (strcmp(thismdtime,"09:00")<0 && strcmp(thismdtime,"02:30")>0)
			continue;
		// 晚上20点到21点之间的k线过滤
		if (strcmp(thismdtime,"21:00")<0 && strcmp(thismdtime,"20:00")>0)
			continue;
		// 下午15点到16点之间的k线过滤
		if (strcmp(thismdtime,"15:15")>=0 && strcmp(thismdtime,"16:00")<0)
			continue;
		// 中午11：30到13点之间的k线过滤
		if (strcmp(thismdtime,"11:30")>=0 && strcmp(thismdtime,"13:00")< 0 )
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

		//记录本合约本次写入的k线的时间分钟
		itit = maplastktime.find(this_data.InstrumentID);
		if (itit != maplastktime.end()){
			itit->second = thismdtime;
		}

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



