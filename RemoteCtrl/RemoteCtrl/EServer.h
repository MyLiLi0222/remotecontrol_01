#pragma once
#include <MSWSock.h>
#include "MThread.h"
#include "CEdoyunQueue.h"
#include <map>

enum EOperator{
    ENode,
    EAccept,
    ERecv,
    ESend,
    EError
};

class EServer;
class EClient;
typedef std::shared_ptr<EClient> PCLIENT;

class EOverlapped{
    public:
    // 因为 GetQueuedCompletionStatus 返回的是一个 LPOVERLAPPED 指针，
    // 需要将它安全地转换为 EdoyunOverlapped* 指针。
    // 根据 C++ 内存布局规则，指向结构体的指针就是指向其第一个成员的指针
    OVERLAPPED m_overlapped;
    DWORD m_operator;//操作类型 参加EOperator
    std::vector<char> m_buffer;//缓冲区
    // 指向该操作完成时应该被调用的处理函数。回调机制的实现。
    // 将一个成员函数与一个对象实例绑定。
    ThreadWorker m_worker;//处理函数
    EServer* m_server;//服务对象
    EClient* m_client;//对应的客户端

    // Winsock 的缓冲区结构，由一个指针和长度组成。
    // WSARecv 和 WSASend 函数需要这个结构来指定数据缓冲区。
    // 通常，它的 .buf 成员会指向 m_buffer.data()，.len 会指向 m_buffer.size()。
    WSABUF m_wsabuf;//wsabuf 结构体
    virtual ~EOverlapped(){
        m_buffer.clear();
    }
};

template<EOperator>class AcceptOverlapped;
typedef AcceptOverlapped<EAccept> ACCEPTOVERLAPPED;

template<EOperator>class RecvOverlapped;
typedef RecvOverlapped<ERecv> RECVOVERLAPPED;

template<EOperator>class SendOverlapped;
typedef SendOverlapped<ESend> SENDOVERLAPPED;
/**
 * @brief 代表一个与服务器连接的客户端会话。
 * @details 这个类封装了与单个客户端相关的所有资源，包括 SOCKET、数据缓冲区、地址信息以及
 * 用于接收和发送的 Overlapped 结构。
 */
class EClient :public ThreadFuncBase {
public:
    EClient();
    ~EClient() {
        closesocket(m_sock);
        m_recv.reset();
        m_send.reset();
        m_overlapped.reset();
        m_buffer.clear();
    }
    // 设置各类 Overlapped 对象内部指向当前 EClient 实例的指针。
    void SetOverlaped(EClient* ptr);
    // 【C++ 操作符重载】类型转换函数，使得 EdoyunClient 
    // 对象可以被隐式转换为SOCKET 类型。
    // 这样做可以让我们在需要 SOCKET 的地方直接传递 EClient 对象，简化代码。
    // 例如：`bind(client, ...)` 而不是 `bind(client->GetSocket(), ...)`
    operator SOCKET() {
        return m_sock;
    }
    // 类型转换函数，转换为 void 指针，指向 m_buffer 的起始地址。
    // 用于 AcceptEx 函数，该函数需要一个缓冲区来接收客户端的第一个数据包和地址信息。
    operator PVOID() {
        return &m_buffer[0];
    }
    operator LPOVERLAPPED();

    operator LPDWORD() {
        return &m_received;
    }

    LPWSABUF RecvWSABuffer();
    LPWSAOVERLAPPED RecvOverlapped();
    LPWSABUF SendWSABuffer();
    LPWSAOVERLAPPED SendOverlapped();
    DWORD& flags() { return m_flags; }
    sockaddr_in* GetLocalAddr() { return &m_laddr; }
    sockaddr_in* GetRemoteAddr() { return &m_laddr; }
    size_t GetBufferSize()const { return m_buffer.size(); }
    int Recv();
    int Send(void* buffer, size_t nSize);
    int SendData(std::vector<char>& data);
private:
    SOCKET m_sock;
    DWORD m_received;
    DWORD m_flags;
    std::shared_ptr<RECVOVERLAPPED> m_recv;
    std::shared_ptr<SENDOVERLAPPED> m_send;
    std::shared_ptr<ACCEPTOVERLAPPED> m_overlapped;
    std::vector<char> m_buffer;
    size_t m_used;//已经使用的缓冲区大小
    sockaddr_in m_laddr;
    sockaddr_in m_raddr;
    bool m_isbusy;
    ESendQueue<std::vector<char>> m_vecSend;//发送数据队列

};


template<EOperator>
class AcceptOverlapped:public EOverlapped,ThreadFuncBase{
    public:
    AcceptOverlapped();
    int AcceptWorker();  
};

template<EOperator>
class RecvOverlapped:public EOverlapped,ThreadFuncBase{
    public:
    RecvOverlapped();
    int RecvWorker(){
        int ret = m_client->Recv();
        return ret;
    } 
};

template<EOperator>
class SendOverlapped:public EOverlapped,ThreadFuncBase{
    public:
    SendOverlapped();
    int SendWorker(){
        return -1;
    } 
};
typedef SendOverlapped<ESend> SENDOVERLAPPED;

template<EOperator>
class ErrorOverlapped:public EOverlapped,ThreadFuncBase{
    public:
    ErrorOverlapped():m_operator(EError),m_worker(this,&ErrorOverlapped::ErrorWorker){
        memset(&m_overlapped,0,sizeof(m_overlapped));
        m_buffer.resize(1024);
    }
    int ErrorWorker(){
        return -1;
    } 
};
typedef ErrorOverlapped<EError> ERROROVERLAPPED;

class EServer :
    public ThreadFuncBase
{
public:
    EServer(const std::string& ip="0.0.0.0",short port = 8822):m_pool(10){
        m_hIOCP = INVALID_HANDLE_VALUE;
        m_sock = INVALID_SOCKET;
        m_addr.sin_family = AF_INET;
        m_addr.sin_port = htons(port);
        m_addr.sin_addr.s_addr = inet_addr(ip.c_str());
    }
    ~EServer(){}
    bool StartService();
    bool NewAccept();
    void BindNewSocket(SOCKET s);
private:
    void CreateSocket();
    int threadIocp();
private:
    MThreadPool m_pool;//线程池
    HANDLE m_hIOCP;
    SOCKET m_sock;
    sockaddr_in m_addr;
    // 【核心数据结构：客户端管理器】
    // 用于存储所有已连接的客户端。
    // - Key: SOCKET，客户端的套接字句柄，唯一标识一个连接。
    // - Value: std::shared_ptr<EClient>，管理 EClient 对象生命周期的智能指针。
    // 使用 map 可以让我们通过 SOCKET 句柄快速地找到对应的客户端会话对象。
    // 使用 shared_ptr 确保了即使在多线程环境下，只要还有地方（比如一个正在进行的 I/O 操作）
    // 引用 EClient 对象，它就不会被销毁，从而避免了悬挂指针等问题。
    std::map<SOCKET,std::shared_ptr<EClient>> m_client;


};

