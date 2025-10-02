#include "pch.h"
#include "Command.h"
CCommand::CCommand():threadid(0)
{
    struct 
    {
        int cmd;
        CMDFUNC func;
    }data[]={
        {1,&CCommand::MackDirverInfo()},
        {2,&CCommand::MakeDirctoryInfo()},
        {3,&CCommand::RunFile()},
        {4,&CCommand::DownloadFile()},
        {5,&CCommand::MouseEvent()},
        {6,&CCommand::sendScreenShot()},
        {7,&CCommand::lockMachine()},
        {8,&CCommand::unlockMachine()},
        {9,&CCommand::DeleteLocalFile()},
        {2002,&CCommand::TestConnect()},
        {-1,NULL}

    };
    for(int i=0;data[i].nCmd!=-1;i++){
        m_mapCmdFunc.insert(std::pair<int,CMDFUNC>(data[i].cmd,data[i].func));
    }
}
int CCommand::ExcuteCommand(int cmd){
    std::map<int,CMDFUNC>::iterator it = m_mapCmdFunc.find(cmd);
    if(it==m_mapCmdFunc.end()){
        return -1;
    }
    return (thist->*it->second)();
}
