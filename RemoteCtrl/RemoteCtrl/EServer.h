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
    // ��Ϊ GetQueuedCompletionStatus ���ص���һ�� LPOVERLAPPED ָ�룬
    // ��Ҫ������ȫ��ת��Ϊ EdoyunOverlapped* ָ�롣
    // ���� C++ �ڴ沼�ֹ���ָ��ṹ���ָ�����ָ�����һ����Ա��ָ��
    OVERLAPPED m_overlapped;
    DWORD m_operator;//�������� �μ�EOperator
    std::vector<char> m_buffer;//������
    // ָ��ò������ʱӦ�ñ����õĴ��������ص����Ƶ�ʵ�֡�
    // ��һ����Ա������һ������ʵ���󶨡�
    ThreadWorker m_worker;//������
    EServer* m_server;//�������
    EClient* m_client;//��Ӧ�Ŀͻ���

    // Winsock �Ļ������ṹ����һ��ָ��ͳ�����ɡ�
    // WSARecv �� WSASend ������Ҫ����ṹ��ָ�����ݻ�������
    // ͨ�������� .buf ��Ա��ָ�� m_buffer.data()��.len ��ָ�� m_buffer.size()��
    WSABUF m_wsabuf;//wsabuf �ṹ��
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
 * @brief ����һ������������ӵĿͻ��˻Ự��
 * @details ������װ���뵥���ͻ�����ص�������Դ������ SOCKET�����ݻ���������ַ��Ϣ�Լ�
 * ���ڽ��պͷ��͵� Overlapped �ṹ��
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
    // ���ø��� Overlapped �����ڲ�ָ��ǰ EClient ʵ����ָ�롣
    void SetOverlaped(EClient* ptr);
    // ��C++ ���������ء�����ת��������ʹ�� EdoyunClient 
    // ������Ա���ʽת��ΪSOCKET ���͡�
    // ��������������������Ҫ SOCKET �ĵط�ֱ�Ӵ��� EClient ���󣬼򻯴��롣
    // ���磺`bind(client, ...)` ������ `bind(client->GetSocket(), ...)`
    operator SOCKET() {
        return m_sock;
    }
    // ����ת��������ת��Ϊ void ָ�룬ָ�� m_buffer ����ʼ��ַ��
    // ���� AcceptEx �������ú�����Ҫһ�������������տͻ��˵ĵ�һ�����ݰ��͵�ַ��Ϣ��
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
    size_t m_used;//�Ѿ�ʹ�õĻ�������С
    sockaddr_in m_laddr;
    sockaddr_in m_raddr;
    bool m_isbusy;
    ESendQueue<std::vector<char>> m_vecSend;//�������ݶ���

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
    MThreadPool m_pool;//�̳߳�
    HANDLE m_hIOCP;
    SOCKET m_sock;
    sockaddr_in m_addr;
    // ���������ݽṹ���ͻ��˹�������
    // ���ڴ洢���������ӵĿͻ��ˡ�
    // - Key: SOCKET���ͻ��˵��׽��־����Ψһ��ʶһ�����ӡ�
    // - Value: std::shared_ptr<EClient>������ EClient �����������ڵ�����ָ�롣
    // ʹ�� map ����������ͨ�� SOCKET ������ٵ��ҵ���Ӧ�Ŀͻ��˻Ự����
    // ʹ�� shared_ptr ȷ���˼�ʹ�ڶ��̻߳����£�ֻҪ���еط�������һ�����ڽ��е� I/O ������
    // ���� EClient �������Ͳ��ᱻ���٣��Ӷ�����������ָ������⡣
    std::map<SOCKET,std::shared_ptr<EClient>> m_client;


};

