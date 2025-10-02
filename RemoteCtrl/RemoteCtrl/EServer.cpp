#include "pch.h"
#include "EServer.h"
#include "Tool.h"
#pragma waring(disable:4407)
template<EOperator op>

AcceptOverlapped<op>::AcceptOverlapped(){
    m_worker = ThreadWorker(this,(FUNCTYPE)&AcceptOverlapped<op>::AcceptWorker);
    m_operator = EAccept;
    memset(&m_overlapped,0,sizeof(m_overlapped));
    m_buffer.resize(1024);
    m_server = NULL;

}
template<EOperator op>

int AcceptOverlapped<op>::AcceptWorker(){
    INT lLength = 0,rLength = 0;
    if(m_client->GetBufferSize()>0){
        sockaddr* plocal = NULL, *promote = NULL;
        GetAcceptExSockaddrs(*m_client,0
        sizeof(sockaddr_in)+16,sizeof(sockaddr_in)+16,
        (sockaddr**)&plocal,&lLength,//本地地址
        (sockaddr**)&promote,lLength//远程地址
        );
        memcpy(m_client->GetLocalAddr(),plocal,sizeif(sockaddr_in));
        memcpy(m_client->GetRemoteAddr(),promote,sizeof(sockaddr_in));
        m_server->BindNewSocket(*m_client);
        int ret = WSARecv((SOCK)*m_client,m_client->RecvWSABuffer(),1,*m_client,&m_client->flags(),m_client->RecvOverlapped(),NULL);
        if(ret == SOCKET_ERROR&&(WSAGetLasError() != WSA_IO_PENDING)){
            TRACE("ret = %d error = %d\r\n", ret, WSAGetLastError());
        }
        if(!m_server->NewAccept()){
            return -2;
        }
    }
    return -1;
}

template<EOperator op>
inline RecvOverlapped<op>::RecvOverlapped()
{
    m_operator = op;
    m_worker = ThreadWorker(this, (FUNCTYPE)&RecvOverlapped<op>::RecvWorker);
    memset(&m_overlapped, 0, sizeof(m_overlapped));
    m_buffer.resize(1024 * 256);
}

EClient::EClient():m_isbusy(false),m_flags(0),
                   m_overlapped(new ACCEPTOVERLAPPED()),
                   m_recv(new RECVOVERLAPPED()),
                   m_send(new SENDOVERLAPPED()),
                   m_vecSend(this,(SENDCALLBACK)&EClient::SendData)
{
    m_sock = WSASocket(PF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);
    m_buffer.resize(1024);
    memset(&m_laddr, 0, sizeof(m_laddr));
    memset(&m_laddr, 0, sizeof(m_laddr));
}

void EClient::SetOverlaped(EClient* ptr)
{

}

EClient::operator LPOVERLAPPED()
{
}

LPWSABUF EClient::RecvWSABuffer()
{
    return LPWSABUF();
}

LPWSAOVERLAPPED EClient::RecvOverlapped()
{
    return LPWSAOVERLAPPED();
}

LPWSABUF EClient::SendWSABuffer()
{
    return LPWSABUF();
}

LPWSAOVERLAPPED EClient::SendOverlapped()
{
    return LPWSAOVERLAPPED();
}

int EClient::Recv()
{
    return 0;
}

int EClient::Send(void* buffer, size_t nSize)
{
    return 0;
}

int EClient::SendData(std::vector<char>& data)
{
    return 0;
}

/**
 * @brief 启动服务器的核心函数。
 * @details 这个函数执行了一系列初始化的关键步骤，包括创建网络资源、
 * 设置 IOCP，并启动线程池来处理未来的 I/O 事件。
 * @return bool - 成功返回 true，失败返回 false。
 */
bool EServer::StartService(){
    // ---- Step 1: 创建和配置监听套接字 ----
    // 调用辅助函数来初始化 Winsock 并创建一个支持异步 I/O 的 Socket。
    CreateSocket();
    // 绑定地址
    if(bind(m_sock,(sockaddr*)&m_addr,sizeof(m_addr)) == -1){
        closesocket(m_sock);
        m_sock = INVALID_SOCKET;
        return false;
    }
    if(listen(m_sock,3) == -1){
        closesocket(m_sock);
        m_sock = INVALID_SOCKET;
        return false;
    }

    // ---- Step 2: 创建 I/O 完成端口 (IOCP) ----
    // 【核心 Windows API】CreateIoCompletionPort - 第一次调用：创建 IOCP
    // - 第一个参数为 INVALID_HANDLE_VALUE: 表示我们不是关联句柄，
    // 而是要创建一个全新的 IOCP。
    // - 第四个参数 '4': 这是一个性能调优参数 (NumberOfConcurrentThreads)。
    // 它建议操作系统最多只允许 4 个线程同时从此 IOCP 队列中取任务并执行。
    // 设置为 0 通常是最佳选择,自动设为系统的 CPU 核心数。
    m_hIOCP = CreateIoCompletionPort(INVALID_HANDLE_VALUE,NULL,0,4);
    if(m_hIOCP == NULL){
        closesocket(m_sock);
        m_sock = INVALID_SOCKET;
        m_hIOCP = INVALID_HANDLE_VALUE;
        return false;
    }
    
   // ---- Step 3: 将监听套接字与 IOCP 关联 ----

   // 【核心 Windows API】CreateIoCompletionPort - 第二次调用：关联句柄
   // 这次调用将我们的监听套接字 m_sock 与刚刚创建的 IOCP (m_hIOCP) 关联起来。
   // - 第三个参数 (ULONG_PTR)this: 这是“单句柄数据”(Per-Handle Data)。我们将 EServer 对象的指针
   //   作为 CompletionKey 传入。这意味着，任何与 m_sock 相关的完成事件（即 AcceptEx 完成）
   //   在被 GetQueuedCompletionStatus 取出时，其 CompletionKey 都会是 this 指针。
    CreateIoCompletionPort((HANDLE)m_sock,m_hIOCP,(ULONG_PTR)this,0);

    // ---- Step 4: 启动工作线程并投递第一个 Accept 请求 ----

    // 启动线程池的工作线程。这些线程启动后，很可能就会阻塞在 threadIocp() 函数中的
    // GetQueuedCompletionStatus 调用上，等待 I/O 事件的发生。
    m_pool.Invoke();
    m_pool.DispatchWorker(ThreadWorker(this,(FUNCTYPE)&EServer::threadIocp));
    if(!NewAccept()) return false;
    return true;
}
/**
 * @brief 辅助函数，用于创建和配置套接字。
 */
void EServer::CreateSocket() {
    WSADATA WSAData;
    
    WSAStartup(MAKEWORD(2, 2), &WSAData);

    // 【核心 Winsock API】WSASocket - 创建支持异步 I/O 的套接字
    // 使用 WSASocket 而不是标准的 socket() 函数，是因为我们需要指定一个关键标志：
    // WSA_FLAG_OVERLAPPED。这个标志告诉操作系统，将对这个套接字执行重叠 I/O（即异步 I/O）。
    // 所有要与 IOCP 一起使用的套接字都必须在创建时包含此标志。
    m_sock = WSASocket(PF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);
    
    int opt = 1;
    setsockopt(m_sock, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt));

}

int EServer::threadIocp()
{
    DWORD transferred = 0;
    ULONG_PTR CompletionKey;
    OVERLAPPED* lpOverlapped = NULL;
    if (GetQueuedCompletionStatus(m_hIOCP, &transferred, &CompletionKey, &lpOverlapped, INFINITE)) {
        if (CompletionKey != 0) {
            EOverlapped* pOverlapped = CONTAINING_RECORD(lpOverlapped, EOverlapped, m_overlapped);
            TRACE("pOverlapped->m_operator % d \r\n",pOverlapped->m_operator);
            pOverlapped->m_server = this;
            switch (pOverlapped->m_operator) {
            case EAccept:
            {
                ACCEPTOVERLAPPED* pOver = (ACCEPTOVERLAPPED*)pOverlapped;
                m_pool.DispatchWorker(pOver->m_worker);
            }
            break;
            case ERecv:
            {
                RECVOVERLAPPED* pOver = (RECVOVERLAPPED*)pOverlapped;
                m_pool.DispatchWorker(pOver->m_worker);
            }
            break;
            case ESend:
            {
                SENDOVERLAPPED* pOver = (SENDOVERLAPPED*)pOverlapped;
                m_pool.DispatchWorker(pOver->m_worker);
            }
            break;
            case EError: 
            {
                ERROROVERLAPPED* pOver = (ERROROVERLAPPED*)pOverlapped;
                m_pool.DispatchWorker(pOver->m_worker);
            }
            break;

            }
        }
    }
    else {
        return -1;
    }
    return 0;
}

bool EServer::NewAccept()
{
    EClient* pClient = new EClient();
    pClient->SetOverlaped(pClient);
    if (!AcceptEx(m_sock, *pClient, *pClient, 0, sizeof(sockaddr_in) + 16, sizeof(sockaddr_in) + 16, *pClient, *pClient)) {
        TRACE("%d\r\n", WSAGetLastError());
        if (WSAGetLastError() != WSA_IO_PENDING) {
            closesocket(m_sock);
            m_sock = INVALID_SOCKET;
            m_hIOCP = INVALID_HANDLE_VALUE;
            return false;
        }

    }
    return true;
}
void EServer::BindNewSocket(SOCKET s)
{
    CreateIoCompletionPort((HANDLE)s, m_hIOCP, (ULONG_PTR)this, 0);
}

