//============================================================================
// Name        : ctpmd.cpp
// Author      :
// Version     :
// Copyright   : Your copyright notice
// Description : Hello World in C++, Ansi-style
//============================================================================

#include "ctpmd.h"

using namespace std;

//全局变量 配置信息
account_setting ACC_SETTING;
mongodb_setting MONGODB_SETTING;
instrument_setting INSTRUMENT_SETTING;

//自然日期和时间 yyyy-mm-dd HH:MM:SS
char DATETIME[30];

char LOGINHOUR[3];

char LOGINMINUTE[3];

//每个合约的状态map
map<string, instrument_status> map_ins_status;

//全局变量，所有订阅合约列表
vector<string> ALL_CODE;

//全局变量 登录结构体变量
CThostFtdcReqUserLoginField REQ_USER_LOGIN;

//全市场合约查询完成后，通知行情线程开始订阅
sem_t Md_Thread;
sem_t Md_Queue_Write;


// 行情接口实例列表
vector<CThostFtdcMdApi*> PUSERAPI_LIST;
vector<MdHandler> MDHANDLER_LIST;

// 交易接口指针
CThostFtdcTraderApi * p_tdreq;

// 行情字段
vector<string> FIELDS{"TradingDay","InstrumentID","LastPrice","PreSettlementPrice","PreClosePrice","PreOpenInterest",\
	                 "OpenPrice","HighestPrice","LowestPrice","Volume","Turnover","OpenInterest","UpperLimitPrice","LowerLimitPrice",\
					 "UpdateTime","UpdateMillisec","BidPrice1","BidVolume1","AskPrice1","AskVolume1","ActionDay"};

// 行情队列，接收tick写入mongo勇
boost::lockfree::queue< market_data*, boost::lockfree::fixed_sized<false> > MARKET_QUEQUE(12800);

// 行情bar队列，队列bar依次写入mongo
boost::lockfree::queue< bar, boost::lockfree::fixed_sized<false> > MARKET_K_QUEUE(12800);


int main() {
	//读取mongo配置
	MONGODB_SETTING = Get_Mongodb_Setting();

	mongocxx::instance inst{};
	string uristring = "mongodb://";
	if (!MONGODB_SETTING.username.empty())
		uristring = uristring + MONGODB_SETTING.username + ":" + MONGODB_SETTING.password + "@";
	uristring = uristring + MONGODB_SETTING.host + ":" + MONGODB_SETTING.port;
	mongocxx::uri uri(uristring.c_str());
	//mongocxx::client client{mongocxx::uri{}};
	mongocxx::client client(uri);
	mongocxx::database db = client[MONGODB_SETTING.db.c_str()];

	//初始化信号量 通知行情Spi实例开始工作
	sem_init(&Md_Thread, 0, 0);

	//初始化信号量 通知写tick数据线程 tick行情队列有可用数据
	sem_init(&Md_Queue_Write, 0, 0);


	//读取期货账户配置信息
    ACC_SETTING = Get_Account_Setting();

    //读取
    INSTRUMENT_SETTING = Get_Instrument_Setting();

    // 获取需要订阅的合约，交易线程可能会再次启动，查询全市场合约
    ALL_CODE = Get_All_SubInstrument_Code(INSTRUMENT_SETTING);

    /*//临时打印
    for (vector<string>::iterator ite = ALL_CODE.begin(); ite < ALL_CODE.end(); ite++ )
    	cout << *ite << endl;*/

    // 创建行情接收实例，程序开始工作。
    start_rev_md(ALL_CODE, INSTRUMENT_SETTING.instance_num, db);

	return 0;
}


// 行情接收线程
void *mdstartfun(void *arg){

	//拿到本行情接收实例需要订阅的合约列表。
	md_thread_arg code_list_struct = *((md_thread_arg*)arg);
	vector<string> code_list = code_list_struct.code_list;

    delete (md_thread_arg*)arg;

    //创建一个行情请求接口指针
	CThostFtdcMdApi *pUserApi = CThostFtdcMdApi::CreateFtdcMdApi("./md/");

	//创建行情接收Spi回调类实例
	MdHandler shmd(pUserApi, code_list);

	//注册行情Spi回调类实例
	pUserApi->RegisterSpi(&shmd);

	//注册行情前置地址
	for ( vector<string>::size_type i = 0; i<ACC_SETTING.mdaddress.size(); i++ ){
		char *p = const_cast<char*>(ACC_SETTING.mdaddress[i].c_str());
		pUserApi->RegisterFront(p);
	}

	//行情接收线程开始工作
	pUserApi->Init();

	pUserApi->Join();
	return NULL ;
}


// 启动行情接收,运行在主线程
void start_rev_md(vector<string> code_list, int instance_num, mongocxx::database db){

	//临时打印
	cout << "start_rev_md run...." << endl;

	//每个行情接收实例(实例)需要订阅的合约个数
	int each_code_num = code_list.size() / instance_num;

	vector<string>::iterator s_ite = code_list.begin();
	vector<string>::iterator e_ite = code_list.begin() + each_code_num;

	vector<pthread_t> thread_id_list;
	while(s_ite < code_list.end()){
		vector<string> temp_list(s_ite, e_ite);
		md_thread_arg *temp_arg = new md_thread_arg;
		temp_arg->code_list = temp_list;
		pthread_t threadMdID;

		//创建并启动每个行情接收实例(线程)，后面不需要join同步，因为主线程在死循环写mongo，主线程不会退出。
		pthread_create(&threadMdID, NULL, mdstartfun, (void*)temp_arg);
		thread_id_list.push_back(threadMdID);
		//mdid = &threadMdID;

		s_ite = e_ite;
		e_ite = s_ite + each_code_num;
		if (e_ite > code_list.end())
			e_ite = code_list.end();
	}

    mongocxx::collection coll;


    //循环将tick行情队列的tick数据写入mongo
    market_data *p_this_data;

	while(true){

		//等待tick行情队列有数据过来。
		sem_wait(&Md_Queue_Write);

		//cout << "****************************tick **********************************8" << endl;
	    MARKET_QUEQUE.pop(p_this_data);
	    market_data this_data = *p_this_data;
	    delete (market_data*)p_this_data;
		p_this_data = NULL;

        coll = db[this_data.InstrumentID];

        //写入mongodb
	    auto doc = bsoncxx::builder::basic::document{};

        doc.append(bsoncxx::builder::basic::kvp("InstrumentID", this_data.InstrumentID));
        doc.append(bsoncxx::builder::basic::kvp("TradingDay", this_data.TradingDay));
		doc.append(bsoncxx::builder::basic::kvp("ActionDay", this_data.ActionDay));
        doc.append(bsoncxx::builder::basic::kvp("UpdateTime", this_data.UpdateTime));
        doc.append(bsoncxx::builder::basic::kvp("UpdateMillisec", this_data.UpdateMillisec));

        doc.append(bsoncxx::builder::basic::kvp("LastPrice", this_data.LastPrice));
        doc.append(bsoncxx::builder::basic::kvp("BidPrice1", this_data.BidPrice1));
        doc.append(bsoncxx::builder::basic::kvp("BidVolume1", this_data.BidVolume1));
        doc.append(bsoncxx::builder::basic::kvp("AskPrice1", this_data.AskPrice1));
        doc.append(bsoncxx::builder::basic::kvp("AskVolume1", this_data.AskVolume1));
        doc.append(bsoncxx::builder::basic::kvp("Volume", this_data.Volume));
        doc.append(bsoncxx::builder::basic::kvp("OpenInterest", this_data.OpenInterest));
        doc.append(bsoncxx::builder::basic::kvp("Turnover", this_data.Turnover));

        doc.append(bsoncxx::builder::basic::kvp("OpenPrice", this_data.OpenPrice));
        doc.append(bsoncxx::builder::basic::kvp("HighestPrice", this_data.HighestPrice));
        doc.append(bsoncxx::builder::basic::kvp("LowestPrice", this_data.LowestPrice));
        doc.append(bsoncxx::builder::basic::kvp("UpperLimitPrice", this_data.UpperLimitPrice));
        doc.append(bsoncxx::builder::basic::kvp("LowerLimitPrice", this_data.LowerLimitPrice));

        doc.append(bsoncxx::builder::basic::kvp("PreOpenInterest", this_data.PreOpenInterest));
        doc.append(bsoncxx::builder::basic::kvp("PreSettlementPrice", this_data.PreSettlementPrice));
        doc.append(bsoncxx::builder::basic::kvp("PreClosePrice", this_data.PreClosePrice));

        coll.insert_one(doc.view());
	    //临时打印
        cout << this_data.UpdateTime << "   " << this_data.InstrumentID << "  "<< this_data.LastPrice << endl;
	}

}


//计算所有需要订阅的合约
vector<string> Get_All_SubInstrument_Code(instrument_setting arg){
	vector<string> result;

	//创建交易接口类指针
	p_tdreq = CThostFtdcTraderApi::CreateFtdcTraderApi("./td/");

	//创建交易Spi回调类。
	TdHandler shtrader(p_tdreq);

	//注册Spi回调类实例
	p_tdreq->RegisterSpi(&shtrader);

	//订阅各种流
	p_tdreq->SubscribePrivateTopic(THOST_TERT_QUICK);
	p_tdreq->SubscribePublicTopic(THOST_TERT_QUICK);

	//注册交易前置地址
	for ( vector<string>::size_type i = 0; i<ACC_SETTING.traderaddress.size(); i++ ){
		char *p = const_cast<char*>(ACC_SETTING.traderaddress[i].c_str());
		p_tdreq->RegisterFront(p);
	}

	//启动交易接口线程
	p_tdreq->Init();

	// 等待全市场合约查询返回
	sem_wait(&Md_Thread);

	//交易接口工作完成，释放。
	p_tdreq->Release();
	p_tdreq = NULL;
	//p_tdreq->Join();

	cout << "全市场共  " << ALL_CODE.size() << " 个合约" << endl;

    if(arg.instrument[0]=="" && arg.instrument.size()==1){
        // 配置文件没有指定具体订阅哪些具体合约

		//如果配置文件指定了具体品种，则筛选出指定品种的全部合约
		if(arg.commodity[0]!=""){
            //配置文件指定了具体品种
			regex re("^[a-zA-Z]{0,3}");
			smatch sm;
			vector <string> temp_all_subcode;
			for(vector<string>::size_type i = 0; i<ALL_CODE.size(); i++){
				// 存放string结果的容器
				bool b = regex_search(ALL_CODE[i], sm, re);
				if(b){
					vector<string>::iterator ite = find( arg.commodity.begin(), arg.commodity.end(), sm[0] );
					if ( ite != arg.commodity.end() ){
					    temp_all_subcode.push_back(ALL_CODE[i]);
					}
				}
			}
			cout << "一共需订阅  " << temp_all_subcode.size() << " 个合约" << endl;
			return temp_all_subcode;
		}else{
			return ALL_CODE;
		}

    }else{
    	// 配置文件指定了具体订阅合约代码
        result = arg.instrument;
        return result;
    }

}


// 获取期货账户配置信息
account_setting Get_Account_Setting(void){

	account_setting result;
	boost::property_tree::ptree pt, tag_setting;
	boost::property_tree::ini_parser::read_ini("./ctpmd.ini", pt);
	tag_setting = pt.get_child("Account");

	//读取账户密码和brokerid
	result.userid = tag_setting.get<string>("userid","");
	result.password = tag_setting.get<string>("password","");
	result.brokerid = tag_setting.get<string>("brokerid","");

	//读取交易前置地址
	string traderaddress = tag_setting.get<string>("traderaddress","");
	boost::split(result.traderaddress, traderaddress, boost::is_any_of( "," ), boost::token_compress_off );

	//读取行情前置地址
	string mdaddress = tag_setting.get<string>("mdaddress","");
	boost::split(result.mdaddress, mdaddress, boost::is_any_of( "," ), boost::token_compress_off );

    return result;
}


// 获取mongodb连接信息
mongodb_setting Get_Mongodb_Setting(){
	mongodb_setting result;
	boost::property_tree::ptree pt, tag_setting;
	boost::property_tree::ini_parser::read_ini("./ctpmd.ini", pt);
	tag_setting = pt.get_child("mongodb");

	//mongo地址
	result.host = tag_setting.get<string>("host","");

	//端口
	result.port = tag_setting.get<string>("port","");

	//数据库名
	result.db = tag_setting.get<string>("db","");

	result.username = tag_setting.get<string>("username","");

	result.password = tag_setting.get<string>("password","");

	result.auth = tag_setting.get<string>("auth","");
	return result;
}


// 获取订阅合约配置信息
instrument_setting Get_Instrument_Setting(){
	instrument_setting result;
	boost::property_tree::ptree pt, tag_setting;
    boost::property_tree::ini_parser::read_ini("./ctpmd.ini", pt);
	tag_setting = pt.get_child("programe");

	//读取品种列表
	string commoditylist = tag_setting.get<string>("commodity","");
	boost::split(result.commodity, commoditylist, boost::is_any_of( " ," ), boost::token_compress_off );

	//读取合约列表
	string instrumentlist = tag_setting.get<string>("instrument","");
	boost::split(result.instrument, instrumentlist, boost::is_any_of( " ," ), boost::token_compress_off );

	//读取行情接收实例个数
	result.instance_num = tag_setting.get<int>("md_instance_num",1);

	return result;
}


