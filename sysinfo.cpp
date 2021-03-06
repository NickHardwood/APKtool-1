#include "sysinfo.h"

SysInfo::SysInfo(QObject *parent) : QObject(parent)
{
    connect(&mi, SIGNAL(meminfo(QString,QString,qreal)), this, SIGNAL(meminfo(QString,QString,qreal)));
    connect(&ci, SIGNAL(cpuinfo(QString)), this, SIGNAL(cpuinfo(QString)));
    connect(&timer, SIGNAL(timeout()), this, SLOT(getInfo()));
    timer.setSingleShot(false);
    timer.start(3000);
}

void SysInfo::getInfo()
{
    if(st.value("user/memInfo").toBool());
        mi.start();
    if(st.value("user/cpuInfo").toBool());
        ci.start();
}

SysInfo::~SysInfo()
{
    mi.terminate();
    mi.wait();
    ci.terminate();
    ci.wait();
}

void MemInfo::run()
{
    std::string tmp;
    std::ifstream file("/proc/meminfo");
    qint64 tmem=0, umem=0;

    while (file >> tmp){
          if (!(tmp.compare (0, tmp.length () - 1, "MemTotal")&&tmp.compare (0, tmp.length () - 1, "SwapTotal"))){
          file >> tmp;
          tmem += atol (tmp.c_str ());
         }
         else if (!
            (tmp.compare (0, tmp.length () - 1, "MemFree")
             && tmp.compare (0, tmp.length () - 1, "Buffers")
             && tmp.compare (0, tmp.length () - 1, "Cached")
             && tmp.compare (0, tmp.length () - 1, "SwapFree")))
        {
            file >> tmp;
            umem -= atol (tmp.c_str ());
        }

   }
    file.close();
   umem+=tmem;
   emit meminfo(QString("%1M").arg(tmem/1024), QString("%1M").arg(umem/1024), (qreal)umem/tmem);
}

void CpuInfo::run()
{
    long cpu_total = 0, cpu_idle = 0;
    QString info, prev, current;
   QFile file("/proc/stat");
   if(file.open(QFile::ReadOnly|QFile::Text)){
       prev = file.readAll();
       file.close();
   }
   QThread::msleep(500);
   if(file.open(QFile::ReadOnly|QFile::Text)){
       current = file.readAll();
       file.close();
   }
   if(prev.isEmpty()||current.isEmpty())
       return;
   else{
       QTextStream in1(&prev), in2(&current);
       in1.readLine();
       in2.readLine();
       QString tmp1, tmp2;
       while(!in1.atEnd()){
           tmp1 = in1.readLine();
           tmp2 = in2.readLine();
           if(!tmp1.startsWith("cpu"))
               break;
           QStringList splitStr1 = tmp1.split(QRegExp("\\s+"));
           QStringList splitStr2 = tmp2.split(QRegExp("\\s+"));
           if(splitStr1.count()<10||splitStr2.count()<10)
               continue;
           file.setFileName("/sys/bus/cpu/devices/"+splitStr1[0]+"/online");
           if(file.open(QFile::ReadOnly|QFile::Text)){
               if(file.readAll().startsWith('0')){
                   file.close();
                   continue;
               }
               file.close();
           }
           for(int i =1; i<8; i++){
                cpu_total += splitStr2[i].toLong() - splitStr1[i].toLong();
                if(i == 4 || i == 5)
                    cpu_idle += splitStr2[i].toLong() - splitStr1[i].toLong();
           }
           if(cpu_total==0)
               return;
           info += splitStr1[0] + ": " + QString::number((cpu_total - cpu_idle) * 100 / cpu_total) + "%\n";
       }

       emit cpuinfo(info);
   }
}
