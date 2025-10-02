// RemoteCtrl.cpp : 此文件包含 "main" 函数。程序执行将在此处开始并结束。
//

#include "pch.h"
#include "framework.h"
#include "RemoteCtrl.h"
#include"ServerSocket.h"
#include"Command.h"
#include"conio.h"
#include "Tool.h"
#include"CEdoyunQueue.h"
#include<iostream>
#include <MSWSock.h>

#ifdef _DEBUG
#define new DEBUG_NEW
#endif



// 唯一的应用程序对象

CWinApp theApp;

using namespace std;
//ASKAI

HANDLE hIOCP = INVALID_HANDLE_VALUE;
void threadQueueEntry(void* arg) {
    CloseHandle(hIOCP);
    _endthread();
}
void test() {
    CEdoyunQueue<std::string> lstString;
    ULONGLONG tick0 = GetTickCount64(), tick = GetTickCount64(), total = GetTickCount64();
    while (GetTickCount64() - total <= 1000) {
        //if (GetTickCount64() - tick0 >= 5) 
        {
            lstString.PushBack("Hello World");
            tick0 = GetTickCount64();
        }
    }   
    printf("exit done! size %d \r\n", lstString.Size());
    total = GetTickCount64();
    while (GetTickCount64() - total <= 1000) {
        //if (GetTickCount64() - tick >= 5) 
        {
            std::string str;
            lstString.PopFront(str);
            tick = GetTickCount64();
        }
    }
    printf("exit done! size %d\r\n", lstString.Size());
    lstString.Clear();
    std::list<std::string>lstData;
    total = GetTickCount64();
    while (GetTickCount64() - total <= 1000) {
        lstData.push_back("hello world");
    }
    printf("lstData push done! size %d\r\n", lstData.size());

    total = GetTickCount64();
    while (GetTickCount64() - total <= 1000) {
        lstData.pop_front();
    }
    printf("lstData pop done! size %d\r\n", lstData.size());

}
class COverlapped {
public:
    OVERLAPPED m_overlapped;
    DWORD m_operator;
    char m_buffer[4096];
    COverlapped() {
        m_operator = 0;
        memset(&m_overlapped, 0, sizeof(m_overlapped));
        memset(m_buffer, 0, size(m_buffer));
    }
};
void iocp() {
    SOCKET sock = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);
    if (sock == INVALID_SOCKET) {
        CTool::ShowError();
        return;
    }
    HANDLE hIOCP = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, sock, 4);
    SOCKET client = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);
    CreateIoCompletionPort((HANDLE)sock, hIOCP, 0, 0);
    sockaddr_in addr;
    addr.sin_family = PF_INET;
    addr.sin_addr.s_addr = inet_addr("0.0.0.0");
    addr.sin_port = htons(8822);
    bind(sock, (sockaddr*)&addr, sizeof(addr));
    listen(sock, 5);
    COverlapped overlapped;
    overlapped.m_operator = 1;
    memset(&overlapped, 0, sizeof(OVERLAPPED));
    DWORD received = 0;

    if (AccepteEx(sock,client,overlapped.m_buffer,0,
        sizeof(sockeaddr_in)+16,
        sizeof(sockaddr_in)+16,
        &received,&overlapped.m_overlapped)
        ==FALSE)
    {
        CTool::ShowError();
    }
    overlapped.m_operator = 2;
    //WSASend();
    while (true) {
        LPOVERLAPPED pOverlapped = NULL;
        DWORD transferred = 0;
        DWORD key = 0;
        if (GetQueuedCompletionStatus(hIOCP, &transferred, &key, &pOverlapped, INFINITE)) {
            COverlapped* p0 = CONTAINING_RECORD(pOverlapped, COverlapped, m_overlapped);
            switch (p0->m_operator) {
            case 1:
                break;
            case 2:
                break;
            }
        }
        
    }
}
int main()
{
    if (!Tool::Init())return 1;
    for (int i = 0; i < 10; i++) {
        test();
    }
    hIOCP = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, NULL, 1);
    _beginthread(threadQueueEntry, 0, hIOCP);
    printf("preass any key to exit ...\r\n");
    /*
    int nRetCode = 0;

    HMODULE hModule = ::GetModuleHandle(nullptr);

    if (hModule != nullptr)
    {
        // 初始化 MFC 并在失败时显示错误
        if (!AfxWinInit(hModule, nullptr, ::GetCommandLine(), 0))
        {
            // TODO: 在此处为应用程序的行为编写代码。
            wprintf(L"错误: MFC 初始化失败\n");
            nRetCode = 1;
        }
        else
        {
            CCommand cmd;
            CServerSocket *pserver =CServerSocket::getInstance();
            int ret = pserver->Run(&CCommand::RunCommand,&cmd);
            switch(ret){
                case -1:
                    MessageBox(NULL,_T("网络初始化异常，未能成功初始化，请检查网络状态"),
                    _T("初始化网络失败"),MB_OK|MB_ICONERROR);
                    exit(0);
                    break;
                case -2:
                    MessageBox(NULL,_T("多次无法正常接入用户，结束程序！"),
                    _T("接入用户失败！"),MB_OK|MB_ICONERROR);
                    exit(0);
                    break;
                default:
                    break;
            }                       
        }
    }
    else
    {
        // TODO: 更改错误代码以符合需要
        wprintf(L"错误: GetModuleHandle 失败\n");
        nRetCode = 1;
    }

    return nRetCode;
    */
}






































