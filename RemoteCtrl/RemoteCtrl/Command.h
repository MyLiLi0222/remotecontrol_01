#pragma once
#include"Resource.h"
#include<map>
#include<atlimage.h>
#include"ServerSocket.h"
#include<direct.h>
#include<stdio.h>
#include<io.h>
#include<list>
#include"Tool.h"
#include"LockInfoDialog.h"
#pragma waring(disable:4996) 
class CCommand
{
public:
    CCommand();
    ~CCommand();
    int ExcuteCommand(int cmd);
    static void RunCommand(void*arg,int status){
        CCommand* pthis = (CCommand*)arg;
        if(status>0){
            int ret = pthis->ExcuteCommand(status);
            if(ret != 0){
                TRACE("执行命令失败，%d ret = %d\r\n", status, ret);
            }
        }else{
            MessageBox(NULL,_T("无法正常接入用户，自动重试")
                ,_T("接入用户失败！"),MB_OK|MB_ICONERROR);
			
        }
    } 
protected:
    typedef int (CCommand::*CMDFUNC)();//成员函数指针
    std::map<int, CMDFUNC> m_mapCmdFunc;//命令映射表
    CLockInfoDialog dlg; //对话框对象
    unsigned int threadid; //线程ID
protected:
    int TestConnect() {
    CPacket pack(2002, NULL, 0);
    CServerSocket::getInstance()->Send(pack);
    TRACE("AAAAA\r\n");
    return 0;
    }

    int lockMachine() {
        if ((dlg.m_hWnd == NULL) || (dlg.m_hWnd == INVALID_HANDLE_VALUE)) {
            //_beginthread(threadLockDlg, 0, NULL);
            _beginthreadex(NULL, 0, &CCommand::threadLockDlg,this, 0, &threadid);//ASKAI: 使用_beginthreadex创建线程
            TRACE("threadid=%d\r\n", threadid);
        }
        CPacket pack(7, NULL, 0);
        CServerSocket::getInstance()->Send(pack);
        return 0;
    }

    int unlockMachine() {
        PostThreadMessage(threadid, WM_KEYDOWN, 0x41, 0);
        CPacket pack(8, NULL, 0);
        CServerSocket::getInstance()->Send(pack);
        return 0;
    }

    int DeleteLocalFile() {
        std::string strPath;
        CServerSocket::getInstance()->GetFilePath(strPath);
        TRACE("执行删除命令\r\n");
        DeleteFile(strPath.c_str());
        CPacket pack(9, NULL, 0);
        bool ret = CServerSocket::getInstance()->Send(pack);
        TRACE("AAAAA  -----   %d   -----\r\n",ret);
        return 0;
    }


    int DownloadFile(){
        std::string strPath;
        CServerSocket::getInstance()->GetFilePath(strPath);
        long long dataSize = 0;
        FILE* pFile  = NULL;
        errno_t err = fopen_s(&pFile, strPath.c_str(), "rb");
        if (err != 0) {
            OutputDebugStringA(_T("无法打开文件，可能文件不存在！"));
            CPacket pack(4, (BYTE*)&dataSize, 8); //发送下载失败的命令
            CServerSocket::getInstance()->Send(pack);
            return -1;
        }
        if(pFile != NULL) {
            //获取文件大小
            fseek(pFile, 0, SEEK_END);
            dataSize = _ftelli64(pFile);
            CPacket head(4, (BYTE*)&dataSize, 8); //发送文件大小
            CServerSocket::getInstance()->Send(head);
            //CServerSocket::getInstance()->Send(pack);
            fseek(pFile, 0, SEEK_SET); //重置文件指针到开头
            OutputDebugStringA(_T("开始下载文件..."));
            
            //分块读取文件并发送
            //每次读取1024字节  
            char buffer[1024];
            size_t rlen = 0;
            do{
                rlen = fread(buffer, 1, sizeof(buffer), pFile);
                CPacket pack(4, (BYTE*)buffer, rlen);
                CServerSocket::getInstance()->Send(pack);
                Sleep(100);
            }while (rlen >= 1024);
            fclose(pFile);
        }

        CPacket pack(4, NULL, 0); //发送下载完成的命令
        CServerSocket::getInstance()->Send(pack);
        OutputDebugStringA(_T("文件下载完成！"));//ASKAI OutputDebugStringA和 OutputDebugString的区别
        return 0;
    }
    
    int MouseEvent(){//ASKAI
        MOUSEEVENT mouse;
        if(CServerSocket::getInstance()->GetMouseEvent(mouse)){
            //处理鼠标事件
            SetCursorPos(mouse.ptXY.x, mouse.ptXY.y);
            DWORD nFlags = 0;
            switch (mouse.nButton)
            {
            case 0:// 鼠标左键点击
                nFlags = 1;
                break;
            case 1:// 鼠标右键点击
                nFlags = 2;
                break;
            case 2:// 鼠标中键点击  
                nFlags = 4;
                break;
            case 4://没有按键只有移动
                nFlags = 8;
                break;
            }
            if(nFlags!=8){
                SetCursorPos(mouse.ptXY.x, mouse.ptXY.y); //设置鼠标位置
            }
            switch(mouse.nAction){
                case 0://单击
                    nFlags |=0x10;
                    break;
                case 1://双击
                    nFlags |= 0x20;
                    break;
                case 2://按下
                    nFlags |= 0x40;
                    break;
                case 3://释放
                    nFlags |= 0x80;
                    break;
                default:
                    break;
            }
            switch(nFlags)
            {
                case 0x21://左键双击
                    mouse_event(MOUSEEVENTF_LEFTDOWN, 0, 0, 0, GetMessageExtraInfo());
                    mouse_event(MOUSEEVENTF_LEFTUP, 0, 0, 0, GetMessageExtraInfo());
                case 0x11://左键单击
                    mouse_event(MOUSEEVENTF_LEFTDOWN, 0, 0, 0, GetMessageExtraInfo());
                    mouse_event(MOUSEEVENTF_LEFTUP, 0, 0, 0, GetMessageExtraInfo());
                    break;
                case 0x41://左键按下
                    mouse_event(MOUSEEVENTF_LEFTDOWN, 0, 0, 0, GetMessageExtraInfo());
                    break;
                case 0x81://左键放开
                    mouse_event(MOUSEEVENTF_LEFTUP, 0, 0, 0, GetMessageExtraInfo());
                    break;

                case 0x22://右键双击
                    mouse_event(MOUSEEVENTF_RIGHTDOWN, 0, 0, 0, GetMessageExtraInfo());
                    mouse_event(MOUSEEVENTF_RIGHTUP, 0, 0, 0, GetMessageExtraInfo());
                case 0x12://右键单击
                    mouse_event(MOUSEEVENTF_RIGHTDOWN, 0, 0, 0, GetMessageExtraInfo());
                    mouse_event(MOUSEEVENTF_RIGHTUP, 0, 0, 0, GetMessageExtraInfo());
                    break;
                
                case 0x42://右键按下
                    mouse_event(MOUSEEVENTF_RIGHTDOWN, 0, 0, 0, GetMessageExtraInfo());
                    break;
                case 0x82://右键放开
                    mouse_event(MOUSEEVENTF_RIGHTUP, 0, 0, 0, GetMessageExtraInfo());
                    break;

                case 0x24://中键双击
                    mouse_event(MOUSEEVENTF_MIDDLEDOWN, 0, 0, 0, GetMessageExtraInfo());
                    mouse_event(MOUSEEVENTF_MIDDLEUP, 0, 0, 0, GetMessageExtraInfo());
                case 0x14://中键单击
                    mouse_event(MOUSEEVENTF_MIDDLEDOWN, 0, 0, 0, GetMessageExtraInfo());
                    mouse_event(MOUSEEVENTF_MIDDLEUP, 0, 0, 0, GetMessageExtraInfo());
                    break;
                
                case 0x44://中键按下
                    mouse_event(MOUSEEVENTF_MIDDLEDOWN, 0, 0, 0, GetMessageExtraInfo());
                    break;
                case 0x84://中键放开
                    mouse_event(MOUSEEVENTF_MIDDLEUP, 0, 0, 0, GetMessageExtraInfo());
                    break;
                case 0x08://鼠标移动
                    mouse_event(MOUSEEVENTF_MOVE, mouse.ptXY.x, mouse.ptXY.y, 0, GetMessageExtraInfo());
                    break;
            }
            CPacket pack(4,NULL,0);
            CServerSocket::getInstance()->Send(pack);

        }else{
            OutputDebugString(_T("鼠标事件解析失败！"));
            return -1;
        }
        CServerSocket::getInstance();
        return 0;
    }

    int sendScreenShot() {
        CImage screen;//创建CImage对象 GDI
        HDC hdcScreen = ::GetDC(NULL); //获取屏幕设备上下文
        int nBitPerPixel = GetDeviceCaps(hdcScreen, BITSPIXEL); //获取屏幕位深
        int nWidth = GetDeviceCaps(hdcScreen, HORZRES); //获取屏幕宽度
        int nHeight = GetDeviceCaps(hdcScreen, VERTRES); //获取屏幕高度
        screen.Create(nWidth, nHeight, nBitPerPixel);//创建CImage对象
        BitBlt(screen.GetDC(), 0, 0, nWidth, nHeight, hdcScreen, 0, 0, SRCCOPY);//将屏幕内容复制到CImage对象
        ReleaseDC(NULL, hdcScreen); //释放屏幕设备上下文
        HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE,0);//创建全局内存对象
        if(hMem == NULL) return -1; //检查内存分配是否成功
        IStream* pStream = NULL;//创建IStream对象
        HRESULT ret = CreateStreamOnHGlobal(hMem, TRUE, &pStream);
        if(ret == S_OK) {
            screen.Save(pStream, Gdiplus::ImageFormatPNG); //将CImage对象保存到IStream对象
            //screen.Save(_T("screenshot.png"), Gdiplus::ImageFormatPNG); //保存屏幕截图到文件
            LARGE_INTEGER liSize = {0};
            pStream->Seek(liSize, STREAM_SEEK_SET, NULL); //重置IStream对象的指针到开头
            PBYTE pData = (PBYTE)GlobalLock(hMem); //锁定全局内存对象
            SIZE_T nSize = GlobalSize(hMem); //获取全局内存对象的大小
            CPacket pack(6, pData, nSize); //创建数据包
            CServerSocket::getInstance()->Send(pack); //发送数据包
            GlobalUnlock(hMem); //解锁全局内存对象
        }
        pStream->Release(); //释放IStream对象
        GlobalFree(hMem); //释放全局内存对象
        screen.ReleaseDC();; //释放CImage对象的设备上下文
        return 0;
    }

    int MakeDirctoryInfo(){
        std::string strPath;
        //std::list<FILEINFO> lstFileInfos;
        if (CServerSocket::getInstance()->GetFilePath(strPath) == false) {
            OutputDebugString(_T("当前的命令，不是获取文件列表，命令解析错误！！"));
            return -1;
        }
        if (_chdir(strPath.c_str()) != 0) {
            FILEINFO finfo;
            finfo.HasNext = FALSE;
            CPacket pack(2, (BYTE*)&finfo, sizeof(finfo));
            CServerSocket::getInstance()->Send(pack);
            OutputDebugString(_T("没有权限访问目录！！"));
            return -2;
        }
        _finddata_t fdata;
        intptr_t hfind = 0;
        if ((hfind = _findfirst("*", &fdata)) == -1) {
            OutputDebugString(_T("没有找到任何文件！！"));
            FILEINFO finfo;
            finfo.HasNext = FALSE;
            CPacket pack(2, (BYTE*)&finfo, sizeof(finfo));
            CServerSocket::getInstance()->Send(pack);
            return -3;
        }
        int count = 0;
        do {
            FILEINFO finfo;
            finfo.IsDirectory = (fdata.attrib & _A_SUBDIR) != 0; // 修复：正确的宏名

            // 关键修复：安全的字符串复制
            size_t nameLen = strlen(fdata.name);
            size_t copyLen = min(nameLen, sizeof(finfo.szFileName) - 1); // 预留空间给'\0'

            memcpy(finfo.szFileName, fdata.name, copyLen);
            finfo.szFileName[copyLen] = '\0'; // 确保字符串正确终止

            // 或者更安全的方法：
            // strncpy_s(finfo.szFileName, sizeof(finfo.szFileName), fdata.name, _TRUNCATE);

            TRACE("%s\r\n", finfo.szFileName);

            CPacket pack(2, (BYTE*)&finfo, sizeof(finfo));
            CServerSocket::getInstance()->Send(pack);
            count++;
        } while (_findnext(hfind, &fdata) == 0); // 修复：正确的条件判断

        _findclose(hfind); // 重要：关闭查找句柄，防止资源泄漏

        TRACE("server: count = %d\r\n", count);
        // 发送结束标记
        FILEINFO finfo;
        finfo.HasNext = FALSE;
        CPacket pack(2, (BYTE*)&finfo, sizeof(finfo));
        CServerSocket::getInstance()->Send(pack);
        return 0;
        
    }
    
    int RunFile(){
        std::string strPath;
        CServerSocket::getInstance()->GetFilePath(strPath);
        ShellExecuteA(NULL,NULL, strPath.c_str(), NULL, NULL, SW_SHOWNORMAL);
        CPacket pack(3,NULL, 0);//发送打开文件的命令
        CServerSocket::getInstance()->Send(pack);
        return 0;
    }

    int MackDirverInfo(){
        //_chdirve();
        //ASKAI 有优化的空间，这是一个比较古老的获取磁盘信息的方法
        std::string result;
        for(int i=1;i<=26;i++){
            if(_chdrive(i)==0){
                if(result.size()>0){
                    result += ";";
                }
                result+='A' + i - 1;
                if (result.back() == 'Z') {
                    result += ";";
                }
            }
        }
        
        CPacket pack(1,(BYTE*)result.c_str(),result.size());
        CTool::Dumper((BYTE*)pack.Data(),pack.Size());
        //Dumper((BYTE*)pack.Data(),pack.Size());
        CServerSocket::getInstance()->Send(pack);
        return 0;
    }

    static unsigned __stdcall threadLockDlg(void* arg)
    {
        CCommand* pThis = (CCommand*)arg;
        pThis->threadLockDlgMain();
        _endthreadex(0);
        return 0;
    }

    void threadLockDlgMain(){
        TRACE("%s(%d):%d\r\n", __FUNCTION__, __LINE__, GetCurrentThreadId());
        dlg.Create(IDD_DIALOG_INFO, NULL);
        dlg.ShowWindow(SW_SHOW);
        //遮蔽后台窗口
        CRect rect;
        rect.left = 0;
        rect.top = 0;
        rect.right = GetSystemMetrics(SM_CXFULLSCREEN);//w1
        rect.bottom = GetSystemMetrics(SM_CYFULLSCREEN);
        rect.bottom = LONG(rect.bottom * 1.10);
        TRACE("right = %d bottom = %d\r\n", rect.right, rect.bottom);
        dlg.MoveWindow(rect);
        CWnd* pText = dlg.GetDlgItem(IDC_STATIC);
        if (pText) {
            CRect rtText;
            pText->GetWindowRect(rtText);
            int nWidth = rtText.Width();//w0
            int x = (rect.right - nWidth) / 2;
            int nHeight = rtText.Height();
            int y = (rect.bottom - nHeight) / 2;
            pText->MoveWindow(x, y, rtText.Width(), rtText.Height());
        }

        //窗口置顶
        dlg.SetWindowPos(&dlg.wndTopMost, 0, 0, 0, 0, SWP_NOSIZE | SWP_NOMOVE);
        //限制鼠标功能
        ShowCursor(false);
        //隐藏任务栏
        ::ShowWindow(::FindWindow(_T("Shell_TrayWnd"), NULL), SW_HIDE);
        //限制鼠标活动范围
        dlg.GetWindowRect(rect);
        rect.left = 0;
        rect.top = 0;
        rect.right = 1;
        rect.bottom = 1;
        ClipCursor(rect);
        MSG msg;
        while (GetMessage(&msg, NULL, 0, 0)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
            if (msg.message == WM_KEYDOWN) {
                TRACE("msg:%08X wparam:%08x lparam:%08X\r\n", msg.message, msg.wParam, msg.lParam);
                if (msg.wParam == 0x41) {//按下a键 退出  ESC（1B)
                    break;
                }
            }
        }
        ClipCursor(NULL);
        //恢复鼠标
        ShowCursor(true);
        //恢复任务栏
        ::ShowWindow(::FindWindow(_T("Shell_TrayWnd"), NULL), SW_SHOW);
        dlg.DestroyWindow();
    }

};

