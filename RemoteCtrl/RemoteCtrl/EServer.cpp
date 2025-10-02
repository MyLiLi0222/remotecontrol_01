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
        (sockaddr**)&plocal,&lLength,//���ص�ַ
        (sockaddr**)&promote,lLength//Զ�̵�ַ
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
 * @brief �����������ĺ��ĺ�����
 * @details �������ִ����һϵ�г�ʼ���Ĺؼ����裬��������������Դ��
 * ���� IOCP���������̳߳�������δ���� I/O �¼���
 * @return bool - �ɹ����� true��ʧ�ܷ��� false��
 */
bool EServer::StartService(){
    // ---- Step 1: ���������ü����׽��� ----
    // ���ø�����������ʼ�� Winsock ������һ��֧���첽 I/O �� Socket��
    CreateSocket();
    // �󶨵�ַ
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

    // ---- Step 2: ���� I/O ��ɶ˿� (IOCP) ----
    // ������ Windows API��CreateIoCompletionPort - ��һ�ε��ã����� IOCP
    // - ��һ������Ϊ INVALID_HANDLE_VALUE: ��ʾ���ǲ��ǹ��������
    // ����Ҫ����һ��ȫ�µ� IOCP��
    // - ���ĸ����� '4': ����һ�����ܵ��Ų��� (NumberOfConcurrentThreads)��
    // ���������ϵͳ���ֻ���� 4 ���߳�ͬʱ�Ӵ� IOCP ������ȡ����ִ�С�
    // ����Ϊ 0 ͨ�������ѡ��,�Զ���Ϊϵͳ�� CPU ��������
    m_hIOCP = CreateIoCompletionPort(INVALID_HANDLE_VALUE,NULL,0,4);
    if(m_hIOCP == NULL){
        closesocket(m_sock);
        m_sock = INVALID_SOCKET;
        m_hIOCP = INVALID_HANDLE_VALUE;
        return false;
    }
    
   // ---- Step 3: �������׽����� IOCP ���� ----

   // ������ Windows API��CreateIoCompletionPort - �ڶ��ε��ã��������
   // ��ε��ý����ǵļ����׽��� m_sock ��ոմ����� IOCP (m_hIOCP) ����������
   // - ���������� (ULONG_PTR)this: ���ǡ���������ݡ�(Per-Handle Data)�����ǽ� EServer �����ָ��
   //   ��Ϊ CompletionKey ���롣����ζ�ţ��κ��� m_sock ��ص�����¼����� AcceptEx ��ɣ�
   //   �ڱ� GetQueuedCompletionStatus ȡ��ʱ���� CompletionKey ������ this ָ�롣
    CreateIoCompletionPort((HANDLE)m_sock,m_hIOCP,(ULONG_PTR)this,0);

    // ---- Step 4: ���������̲߳�Ͷ�ݵ�һ�� Accept ���� ----

    // �����̳߳صĹ����̡߳���Щ�߳������󣬺ܿ��ܾͻ������� threadIocp() �����е�
    // GetQueuedCompletionStatus �����ϣ��ȴ� I/O �¼��ķ�����
    m_pool.Invoke();
    m_pool.DispatchWorker(ThreadWorker(this,(FUNCTYPE)&EServer::threadIocp));
    if(!NewAccept()) return false;
    return true;
}
/**
 * @brief �������������ڴ����������׽��֡�
 */
void EServer::CreateSocket() {
    WSADATA WSAData;
    
    WSAStartup(MAKEWORD(2, 2), &WSAData);

    // ������ Winsock API��WSASocket - ����֧���첽 I/O ���׽���
    // ʹ�� WSASocket �����Ǳ�׼�� socket() ����������Ϊ������Ҫָ��һ���ؼ���־��
    // WSA_FLAG_OVERLAPPED�������־���߲���ϵͳ����������׽���ִ���ص� I/O�����첽 I/O����
    // ����Ҫ�� IOCP һ��ʹ�õ��׽��ֶ������ڴ���ʱ�����˱�־��
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

