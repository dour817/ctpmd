import datetime
import time
import os
import sys
import psutil
import traceback
import threading
import logging
'''
行情接收程序部署说明：
可执行文件 ctpmd 为行情接收程序，本py文件是定时启动和关闭，并且监控ctpmd进程运行的脚本程序。
1,首先将Release文件夹下的所有.so文件复制到/usr/local/lib/文件夹下，然后运行ldconfig。若不想复制到/usr/local/lib/，也可以运行export LD_LIBRARY_PATH=.so文件所有在路径
将动态库查找的路径加入到LD_LIBRARY_PATH环境变量。
2,运行cmd : python3 本监控py脚本  ctpmd可执行文件所在路径 例如如下
          python3 /home/tcz/monitor_ctpmd.py /home/tcz/learngit/ctpmd/Release/
  另外需要安装psutil包。
'''
#
import logging
logging.basicConfig(level = logging.WARNING,format = '%(asctime)s - %(name)s - %(levelname)s - %(message)s', \
                    filename = './exception.txt')
logger = logging.getLogger('monitor_ctpmd')

def main():
    '''
    根据交易时间，启动和杀掉行情接收程序
    '''
    if len(sys.argv) == 1:
        path = '/home/tcz/learngit/ctpmd/Release/'
    else:
        path = sys.argv[1]
    procname = 'ctpmd'
    while True:

        isOnTime = istradetime()
        isrun = isprocrun(procname)
        if isOnTime:
            if not isrun:
                #启动进程
                print(datetime.datetime.now().strftime('%Y-%m-%d %H:%M:%S'), '交易时间，程序未启动，准备启动......')
                t = threading.Thread(target=threadrun, args = (path, procname))
                t.start()
                print('*********  ', datetime.datetime.now().strftime('%Y-%m-%d %H:%M:%S'), procname,' start   *********')
                try:
                    logger.warning('ctpmd 启动')
                except:
                    traceback.print_exc(file=open('./exception.txt', 'w+'))
            elif isrun >0:
                print(datetime.datetime.now().strftime('%Y-%m-%d %H:%M:%S'), '交易时间，程序已经启动')
            else:
                print(datetime.datetime.now().strftime('%Y-%m-%d %H:%M:%S'), '交易时间，程序运行情况未知，稍等......')
        else:
            if isrun > 0 :
                #杀掉进程
                print(datetime.datetime.now().strftime('%Y-%m-%d %H:%M:%S'), '非交易时间，程序已经启动，准备关掉......')
                try:
                    p = psutil.Process(isrun)
                    p.kill()
                except:
                    traceback.print_exc(file=open('./exception.txt', 'w+'))
            elif not isrun:
                print(datetime.datetime.now().strftime('%Y-%m-%d %H:%M:%S'), '非交易时间，程序未启动')
            else:
                print(datetime.datetime.now().strftime('%Y-%m-%d %H:%M:%S'), '非交易时间，程序运行情况未知，稍等......')
        if isOnTime:
            time.sleep(2)
        else:
            time.sleep(60)

def threadrun(path, procname):
    os.chdir(path)
    f = os.system("./" + procname)


def istradetime():
    '''
    判断是否交易时间
    :return: True：交易时间； False:非交易时间
    '''
    weekday = datetime.datetime.now().date().weekday() + 1
    hour = datetime.datetime.now().hour
    minute = datetime.datetime.now().minute
    second = datetime.datetime.now().second
    if weekday in (2, 3, 4, 5):
        if (hour == 8 and minute >= 40) or (9 <= hour < 15) or (hour == 15 and minute <= 30) \
                or (hour == 20 and minute >= 40) or (hour >= 21 or hour <= 1) or (hour == 2 and minute <= 45):
            # 星期2至星期5 交易时间
            return True
    elif weekday == 1:
        if (hour == 8 and minute >= 40) or (9 <= hour < 15) or (hour == 15 and minute <= 30) \
                or (hour == 20 and minute >= 40) or hour >=21:
            # 周1交易时间
            return True
    elif weekday == 6:
        if (hour <= 1) or ((hour == 2 and minute <= 45)):
            # 周6交易时间
            return True
    return False


def isprocrun(procname):
    '''
    判断进程是否存在，
    :param procname: 行情接收程序名称
    :return: 返回进程ID:进程已经存在。 0:进程不存在
    '''
    try:
        allproc = psutil.process_iter()
        for prociter in allproc:
            #print(prociter.name())
            if procname == prociter.name():
                return prociter.pid
    except:
        traceback.print_exc(file=open('./exception.txt','w+'))
        return -1
    return 0

if __name__ == '__main__':
    #isprocrun('ctpmd')
    main()
