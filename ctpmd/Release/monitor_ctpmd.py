import datetime
import time
import os
import sys
import psutil
import traceback
import threading

# cmd : python3 monitor_ctpmd.py /home/tcz/learngit/ctpmd/Release/
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
            else:
                print(datetime.datetime.now().strftime('%Y-%m-%d %H:%M:%S'), '交易时间，程序已经启动')
        else:
            if isrun:
                #杀掉进程
                print(datetime.datetime.now().strftime('%Y-%m-%d %H:%M:%S'), '交易时间，程序已经启动，准备关掉......')
                p = psutil.Process(isrun)
                p.kill()
            else:
                print(datetime.datetime.now().strftime('%Y-%m-%d %H:%M:%S'), '交易时间，程序未启动')
        time.sleep(2)

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
        if (hour == 8 and minute >= 50) or (9 <= hour < 15) or (hour == 15 and minute <= 15) \
                or (hour == 20 and minute >= 50) or (hour >= 21 or hour <= 1) or (hour == 2 and minute <= 45):
            # 星期2至星期5 交易时间
            return True
    elif weekday == 1:
        if (hour == 8 and minute >= 50) or (9 <= hour < 15) or (hour == 15 and minute <= 15) \
                or (hour == 20 and minute >= 50) or hour >=21:
            # 周1交易时间
            return True
    elif weekday == 6:
        if (hour == 1) or ((hour == 2 and minute >= 40)):
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
    return 0



if __name__ == '__main__':
    #isprocrun('ctpmd')
    main()