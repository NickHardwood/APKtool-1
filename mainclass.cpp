#include "mainclass.h"
#include <QDir>
#include <QFile>
#include <QThread>
#include <QDebug>
#include <QSettings>
#include <QStringList>

#if defined(Q_OS_ANDROID)
#include <QtAndroid>
#include <QtAndroidExtras/QAndroidJniObject>
#endif

MainClass::MainClass(QObject *parent) : QObject(parent), watcher(NULL), searchProc(NULL)
{
    QSettings settings;
    _currentPath = settings.value("user/lastpath", "/").toString();
    _aapt = settings.value("user/aapt","aapt6.0").toString();
    if(QFileInfo("/system/bin/su").exists()||QFileInfo("/system/xbin/su").exists()||QFileInfo("/bin/su").exists())
        _shell = "su";
    else
        _shell = "sh";
    QProcessEnvironment env = listProc.processEnvironment();
    QString path = env.value("PATH");
    env.insert("PATH", "/data/data/per.pqy.apktool/apktool/openjdk/bin:"+path);
    listProc.setProcessEnvironment(env);
    refreshCurrentPath();
    connect(this, SIGNAL(copyFinished()), this, SLOT(refreshCurrentPath()));
    connect(this, SIGNAL(deleteFinished()), this, SLOT(refreshCurrentPath()));
    connect(this, SIGNAL(cutFinished()), this, SLOT(refreshCurrentPath()));
    connect(this, SIGNAL(signFinished()), this, SLOT(refreshCurrentPath()));
    connect(this, SIGNAL(taskFinished(QString,QString,QString)), this, SLOT(refreshCurrentPath()));
/*
    QProcess proc;
    proc.start("id");
    proc.waitForFinished();
    qWarning()<<proc.readAllStandardOutput();
*/

}

MainClass::~MainClass()
{
    QSettings settings;
    settings.setValue("user/lastpath", _currentPath);
    settings.setValue("user/aapt", _aapt);
}

void MainClass::refreshCurrentPath()
{
    if(listProc.state()==QProcess::Running)
        return;
    listProc.start(_shell, QStringList()<<"-c"<<"ls -Agoh --full-time  --color=never "+_currentPath);
    listProc.waitForFinished();
    if(listProc.exitCode()==2){
        _currentPath = "/";
        listProc.start(_shell, QStringList()<<"-c"<<"ls -Agoh --full-time  --color=never "+_currentPath);
        listProc.waitForFinished();
    }

    qDeleteAll(fList);
    fList.clear();
    QByteArray output = listProc.readAllStandardOutput();
    QTextStream out(&output);
    QString line, filename, fileinfo;
    QStringList splitStr;
    if(_currentPath!="/")
        fList.append(new FileModelItem("..", " "));
    line = out.readLine();
    while(!out.atEnd()){
        line = out.readLine();
        splitStr = line.split(QRegExp("\\s+"),  QString::SkipEmptyParts);
        if(splitStr.count()<1)
            continue;
        if(splitStr[0].startsWith('-')){
            filename = splitStr[6];
            fileinfo =splitStr[0] + "    " + splitStr[2] + "    "+splitStr[3]+ "  "+splitStr[4].left(8) ;
        }
        else if(splitStr[0].startsWith('d')){
            filename = splitStr[6];
            fileinfo =splitStr[0] + "            "+splitStr[3]+ "  "+splitStr[4].left(8) ;
        }
        else if(splitStr[0].startsWith('l')){
            filename = splitStr[6];
            fileinfo =splitStr[0] + "            "+splitStr[3]+ "  "+splitStr[4].left(8) +"  "+splitStr[7]+ splitStr[8];
        }
        else if(splitStr[0].startsWith('b')||splitStr[0].startsWith('c')){
            filename = splitStr[7];
            fileinfo = splitStr[0] + "    " + splitStr[2] +splitStr[3]+ "    "+splitStr[4]+ "  "+splitStr[5].left(8) ;
        }
        fList.append(new FileModelItem(filename, fileinfo));
    }

    emit fileModelChanged();
}

void MainClass::singlePress(int index)
{

    FileModelItem *item = qobject_cast<FileModelItem *>(fList[index]);
    if(index==0&&_currentPath!="/"){
        QDir dir(_currentPath);
        dir.cdUp();
        _currentPath = dir.absolutePath();
        if(!_currentPath.endsWith('/')){
            _currentPath+="/";
        }
        refreshCurrentPath();
    }
    else if(item->info().startsWith('d')){
        QFileInfo finfo(_currentPath, item->name());
        if(_shell=="su"||(finfo.isReadable()&&finfo.isExecutable())){
            _currentPath +=  item->name() + "/";
            refreshCurrentPath();
        }
    }
    else if(item->info().startsWith('-')){
        emit clickFile(QFileInfo( item->name()).suffix());
    }
    else if(item->info().startsWith('l')){
        if(listProc.state()==QProcess::Running)
            return;

        listProc.start(_shell, QStringList()<<"-c"<<"ls -hHd --full-time --color=never "+_currentPath+ item->name());
        listProc.waitForFinished();
        if(listProc.readAllStandardOutput().startsWith('d')){
            _currentPath +=  item->name() + "/";
            refreshCurrentPath();
        }
        else{
            emit clickFile(QFileInfo( item->name()).suffix());
        }
    }
}

void MainClass::longPress(int index)
{
    FileModelItem *item = qobject_cast<FileModelItem *>(fList[index]);
    if(item->name()==".." || item->info().startsWith('d')){
         emit clickDir();
    }
    else if(item->info().startsWith('-')){
        emit clickFile(QFileInfo( item->name()).suffix());
    }
    else if(item->info().startsWith('l')){
        if(listProc.state()==QProcess::Running)
            return;
        listProc.start(_shell, QStringList()<<"-c"<<"ls -hHd  --color=never "+_currentPath+ item->name());
        listProc.waitForFinished();
        if(listProc.readAllStandardOutput().startsWith('d')){
            emit clickDir();
        }
        else{
            emit clickFile(QFileInfo( item->name()).suffix());
        }
    }

}

bool MainClass::hasRoot()
{
    return _shell=="su";
}

void MainClass::selectAll()
{
    for(int i=0;i<fList.count();i++){
        FileModelItem *item = qobject_cast<FileModelItem *>(fList[i]);
        item->setChecked(true);
    }

}

void MainClass::unselectAll()
{
    for(int i=0;i<fList.count();i++){
        FileModelItem *item = qobject_cast<FileModelItem *>(fList[i]);
        item->setChecked(false);
    }

}

void MainClass::reverseSelect()
{
    for(int i=0;i<fList.count();i++){
        FileModelItem *item = qobject_cast<FileModelItem *>(fList[i]);
        item->setChecked(!item->checked());
    }

}

void MainClass::combineApkDex()
{
    QString zip, dex;
    for(int i=0;i<fList.count();i++){
        FileModelItem *item = qobject_cast<FileModelItem *>(fList[i]);
        if(item->checked()){
            if(item->name().endsWith(".apk", Qt::CaseInsensitive)||item->name().endsWith(".jar", Qt::CaseInsensitive)){
                if(zip.isEmpty())
                    zip = item->name();
                else{
                    emit combineHelp();
                    return;
                }

            }
            else if(item->name().endsWith(".dex")){
                if(dex.isEmpty())
                    dex = item->name();
                else{
                    emit combineHelp();
                    return;
                }
            }
            else{
                emit combineHelp();
                return;
            }
        }
    }
    if(dex!="classes.dex"){
        if(QFileInfo(_currentPath+"/classes.dex").exists())
            if(!QFile::remove(_currentPath+"/classes.dex")){
                emit combineHelp();
                return;
            }
        if(!QFile::rename(_currentPath+"/"+dex, _currentPath+"/classes.dex")){
            emit combineHelp();
            return;
        }
    }

    TaskModelItem *task = new TaskModelItem("cd "+_currentPath+";aapt5.0 a "+zip+ " classes.dex", _shell);
    connect(task,SIGNAL(finished(QString,QString,QString)),this,SIGNAL(taskFinished(QString,QString,QString)));
    task->startTask();
    tList.append(task);
}

void MainClass::deleteSelected()
{
    QString filesList;
    for(int i=0;i<fList.count();i++){
        FileModelItem *item = qobject_cast<FileModelItem *>(fList[i]);
        if(item->checked()){
            filesList+=_currentPath +"/" + item->name()+ " ";
        }
    }
    TaskModelItem *task = new TaskModelItem(QString("busybox rm -r ")+filesList, _shell);
    connect(task,SIGNAL(finished(QString, QString ,QString)),this,SIGNAL(deleteFinished()));
    task->startTask();
    tList.append(task);
}

bool MainClass::noItemSelected()
{
    for(int i=0;i<fList.count();i++){
        FileModelItem *item = qobject_cast<FileModelItem *>(fList[i]);
        if(item->checked())
            return false;
    }
    return true;
}

int MainClass::taskNum()
{
    int runningTasks=0;
    for(int i=0;i<tList.count();i++){
        TaskModelItem *item = qobject_cast<TaskModelItem *>(tList[i]);
        if(item->state()=="task_running")
            runningTasks++;
    }
    return runningTasks;
}

void MainClass::createFSWatcher()
{
    delete watcher;
    watcher = new QFileSystemWatcher;
    QDir d(_currentPath);
    do{
        if(d.isRoot())
            break;
        watcher->addPath(d.absolutePath());
        break;
        //            d.cdUp();
    }while(1);
    connect(watcher, SIGNAL(directoryChanged(QString)), this, SLOT(refreshCurrentPath()));
}

void MainClass::genKey()
{

    _key = new keyThread;
    connect(_key, SIGNAL(gotKey(QString)), this, SIGNAL(gotKey(QString)));
    connect(_key, SIGNAL(finished()), this, SLOT(deleteKey()));
    _key->start();

}

void MainClass::deleteKey()
{
    delete _key;
    _key=NULL;
}

void MainClass::saveSelected()
{
    _selectedFiles.clear();
    _oldPath = _currentPath;
    for(int i=0;i<fList.count();i++){
        FileModelItem *item = qobject_cast<FileModelItem *>(fList[i]);
        if(item->checked())
            _selectedFiles+=_currentPath+ item->name()+" ";
    }
}

bool MainClass::copySelected(bool cover)
{
    if(_currentPath==_oldPath){
        return false;
    }
    TaskModelItem *task = new TaskModelItem(QString("busybox cp -r ")+(cover?"-f ":"") +_selectedFiles +" "+ _currentPath, _shell);
    connect(task,SIGNAL(finished(QString,QString,QString)),this,SIGNAL(copyFinished()));
    task->startTask();
    tList.append(task);
    return true;
}

bool MainClass::cutSelected(bool cover)
{
    if(_currentPath==_oldPath){
        return false;
    }
    TaskModelItem *task = new TaskModelItem(QString("busybox mv ")+(cover?"-f ":"-n ") +_selectedFiles +" "+ _currentPath, _shell);
    connect(task,SIGNAL(finished(QString,QString,QString)),this,SIGNAL(cutFinished()));
    task->startTask();
    tList.append(task);
    return true;
}

void MainClass::rename(QString oldName, QString newName)
{
    if(QFile(_currentPath+newName).exists()){
        emit sameNameExist();
        return;
    }

    QFile(_currentPath+oldName).rename(_currentPath+newName);
    refreshCurrentPath();
}

void MainClass::createNewFile(QString name, bool type)
{
    if(type){
        QFile f(_currentPath+name);
        if(f.exists()||!f.open(QIODevice::WriteOnly)){
            return;
        }
        f.close();
    }
    else
        QDir(_currentPath).mkdir(name);
}

void MainClass::decApk(QString apkFile, QString options, bool rootPerm)
{
    QString cmd("/data/data/per.pqy.apktool/apktool/apktool.sh ");
    cmd += options;
    cmd += _currentPath + apkFile + " -o " +_currentPath + apkFile.left(apkFile.length()-4) + "_src";
    TaskModelItem *task = new TaskModelItem(cmd,rootPerm?_shell:"sh");
    connect(task,SIGNAL(finished(QString,QString,QString)),this,SIGNAL(taskFinished(QString,QString,QString)));
    task->startTask();
    tList.append(task);
}

void MainClass::recApk(QString sourceDir, QString options, QString aapt, bool rootPerm)
{
    _aapt = aapt;
    QString cmd("/data/data/per.pqy.apktool/apktool/apktool.sh ");
    cmd += options;
    cmd += _currentPath + sourceDir + " -o " +_currentPath  + sourceDir + ".apk -a /data/data/per.pqy.apktool/apktool/openjdk/bin/"+aapt;
    TaskModelItem *task = new TaskModelItem(cmd,rootPerm?_shell:"sh");
    connect(task,SIGNAL(finished(QString,QString,QString)),this,SIGNAL(taskFinished(QString,QString,QString)));
    task->startTask();
    tList.append(task);
}

void MainClass::signApk(QString apkFile)
{
    QString cmd("/data/data/per.pqy.apktool/apktool/signapk.sh ");
    cmd += _currentPath  +apkFile + " " + _currentPath  + QFileInfo(apkFile).baseName()+"_sign.apk";
    TaskModelItem *task = new TaskModelItem(cmd, _shell);
    connect(task,SIGNAL(finished(QString,QString,QString)),this,SIGNAL(signFinished()));
    task->startTask();
    tList.append(task);
}

void MainClass::openFile(QString file)
{
#if defined(Q_OS_ANDROID)
    QAndroidJniObject filePath = QAndroidJniObject::fromString(_currentPath+file);
    QAndroidJniObject activity = QtAndroid::androidActivity();
    if(file.endsWith(".apk", Qt::CaseInsensitive))
        QAndroidJniObject::callStaticMethod<void>("per/pqy/apktool/Extra", "installApk", "(Lorg/qtproject/qt5/android/bindings/QtActivity;Ljava/lang/String;)V",
                                                  activity.object<jobject>(), filePath.object<jstring>());
    else
        QAndroidJniObject::callStaticMethod<void>("per/pqy/apktool/Extra", "openFile", "(Lorg/qtproject/qt5/android/bindings/QtActivity;Ljava/lang/String;)V",
                                                  activity.object<jobject>(), filePath.object<jstring>());
#endif
}

void MainClass::importFramework(QString apkFile)
{
    QString cmd("/data/data/per.pqy.apktool/apktool/apktool.sh if ");
    cmd += _currentPath + apkFile;
    TaskModelItem *task = new TaskModelItem(cmd, _shell);
    connect(task,SIGNAL(finished(QString,QString,QString)),this,SIGNAL(taskFinished(QString,QString,QString)));
    task->startTask();
    tList.append(task);
}

void MainClass::oat2dex(QString odexFile)
{
    if(!(QFileInfo(_currentPath+ "dex").exists() || QFileInfo(_currentPath+ "boot.oat").exists())){
        emit noBootClass();
        return;
    }
    QString cmd("/data/data/per.pqy.apktool/apktool/oat2dex.sh ");
    cmd += _currentPath+odexFile;
    TaskModelItem *task = new TaskModelItem(cmd, _shell);
    connect(task,SIGNAL(finished(QString,QString,QString)),this,SIGNAL(taskFinished(QString,QString,QString)));
    task->startTask();
    tList.append(task);
}

void MainClass::removeTask(int i)
{
    delete tList[i];
    tList.removeAt(i);
    emit taskModelChanged();
}


void MainClass::removeFinishedTasks()
{
    for(int i=0;i<tList.count();i++){
        TaskModelItem *item = qobject_cast<TaskModelItem *>(tList[i]);
        if(item->state()!="task_running"){
            delete tList[i];
            tList.removeAt(i);
            i--;
        }
    }
    emit taskModelChanged();
}

void MainClass::stopAllTasks()
{
    for(int i=0;i<tList.count();i++){
        TaskModelItem *item = qobject_cast<TaskModelItem *>(tList[i]);
        if(item->state()=="task_running"){
            item->stopTask();
        }
    }
}

void MainClass::searchFiles(QString cmd)
{
    if(!searchProc){
        searchProc = new QProcess;
        QProcessEnvironment env = searchProc->processEnvironment();
        QString path = env.value("PATH");
        env.insert("PATH", "/data/data/per.pqy.apktool/apktool/openjdk/bin:"+path);
        searchProc->setProcessEnvironment(env);
        connect(searchProc, SIGNAL(finished(int)), this, SLOT(searchResult()));
    }
    else if(searchProc->state()==QProcess::Running)
        searchProc->kill();

    searchProc->start(_shell,QStringList()<<"-c"<<"cd "+_currentPath+";"+cmd);
}

void MainClass::searchResult()
{
    qDeleteAll(sList);
    sList.clear();
    QString output = searchProc->readAllStandardOutput();
    QTextStream stream(&output);
    while(!stream.atEnd()){
        QString fname = stream.readLine();
        QFileInfo f(_currentPath+fname);
        QString finfo ;//= qtPerm2unix(f.permissions()) + "    " + (f.isDir()?"    ":qtFileSize(f.size())) + "    "+ qtDate(f.lastModified()) + (f.isSymLink()?("    -> "+f.symLinkTarget()):"");
        sList.append(new FileModelItem(fname, finfo));
    }
    emit searchModelChanged();

}

void MainClass::setTheme(QString type, QString fname)
{
    QSettings settings;
    if(!fname.isEmpty()){
        if(type=="bg"){
            QFile::remove(QDir::homePath()+"/bg");
            if(QFile::copy(fname, QDir::homePath()+"/bg"))
                settings.setValue("theme/background", "custom");
            else
                settings.setValue("theme/background", "");
        }

        else if(type=="itembg"){
            QFile::remove(QDir::homePath()+"/itembg");
            if(QFile::copy(fname, QDir::homePath()+"/itembg"))
                settings.setValue("theme/itembackground", "custom");
            else
                settings.setValue("theme/itembackground", "");
        }
        else if(type=="buttonbg"){
            QFile::remove(QDir::homePath()+"/buttonbg");
            if(QFile::copy(fname, QDir::homePath()+"/buttonbg"))
                settings.setValue("theme/buttonbackground", "custom");
            else
                settings.setValue("theme/buttonbackground", "");
        }
    }
    else{
        if(type=="bg"){
            QFile::remove(QDir::homePath()+"/bg");
            settings.setValue("theme/background", "");
        }

        else if(type=="itembg"){
            QFile::remove(QDir::homePath()+"/itembg");
            settings.setValue("theme/itembackground", "");
        }

        else if(type=="buttonbg"){
            QFile::remove(QDir::homePath()+"/buttonbg");
            settings.setValue("theme/buttonbackground", "");
        }
    }
    settings.sync();
    emit themeChanged();
}
