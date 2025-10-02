#include "pch.h"
#include "ClientSocket.h"
CClientSocket* CClientSocket::m_instance = NULL;
CClientSocket::CHelper CClientSocket::m_helper;

CClientSocket* pclient = CClientSocket::getInstance();

std::string GetErrorMessage(int nErrorCode) {
	std::string strError;
	LPVOID lpMsgBuf = NULL;
	FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
		NULL,
		nErrorCode,
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		(LPSTR)&lpMsgBuf, 0, NULL);
	strError = (char*)lpMsgBuf;
	LocalFree(lpMsgBuf);
	return strError;
}
void Dumper(BYTE* pData, size_t nSize) {
	std::string strOut;
	for (size_t i = 0; i < nSize; i++) {
		char buf[8];
		if (i > 0 && (i % 16 == 0)) strOut += "\n";
		sprintf_s(buf, sizeof(buf), "%02X ", pData[i] & 0xFF);
		strOut += buf;
	}
	strOut += "\n";
	OutputDebugStringA(strOut.c_str());
}
CClientSocket::CClientSocket(const CClientSocket& ss) {
	m_hThread = INVALID_HANDLE_VALUE;
	m_bAutoClose = ss.m_bAutoClose;
	m_sock = ss.m_sock;
	m_nIp = ss.m_nIp;
	m_nPort = ss.m_nPort;
	std::map<UINT, CClientSocket::MSGFUNC>::const_iterator it = ss.m_mapFunc.begin();
	for (; it != ss.m_mapFunc.end(); it++) {
		m_mapFunc.insert(std::pair<UINT, MSGFUNC>(it->first, it->second));
	}
}
CClientSocket::CClientSocket() :
	m_nIp(INADDR_ANY), m_nPort(0), m_sock(INVALID_SOCKET), m_bAutoClose(true),
	m_hThread(INVALID_HANDLE_VALUE) 
{
	if (InitSockEnv() == FALSE) {
		MessageBox(NULL, _T("�޷���ʼ���׽��ֻ���,�����������ã�"), _T("��ʼ������"), MB_OK | MB_ICONERROR);//MB_OK | MB_ICONERROR ���������ʾ��Ϣ�����ʾһ��������ȷ������ť��һ������ͼ�����Ϣ��
		exit(0);
	}
	m_eventInvoke = CreateEvent(NULL, TRUE, FALSE, NULL);
	m_hThread = (HANDLE)_beginthreadex(NULL, 0, &CClientSocket::threadEntry, this, 0, &m_nThreadID);
	if (WaitForSingleObject(m_eventInvoke, 100) == WAIT_TIMEOUT) {
		TRACE("������Ϣ�����߳�����ʧ����!\r\n");
	}
	CloseHandle(m_eventInvoke);
	m_buffer.resize(BUFFER_SIZE);
	memset(m_buffer.data(), 0, BUFFER_SIZE);
	//_beginthread(&CClientSocket::threadEntry, 0, this);
	struct {
		UINT message;
		MSGFUNC func;
	}funcs[] = {
		{WM_SEND_PACK,&CClientSocket::SendPack},
		{0,NULL}
	};
	for (int i = 0; funcs[i].message != 0; i++) {
		if (m_mapFunc.insert(std::pair<UINT, MSGFUNC>(funcs[i].message, funcs[i].func)).second == false) {
			TRACE("����ʧ�ܣ���Ϣֵ��%d ����ֵ:%08X ���:%d\r\n", funcs[i].message, funcs[i].func, i);
		}
	}
}

bool CClientSocket::InitSocket()
{ //����ʼ����û�гɹ�
	if (m_sock != INVALID_SOCKET)CloseSocket();
	m_sock = socket(PF_INET, SOCK_STREAM, 0);
	if (m_sock == -1) return false;
	//TODO:У��
	sockaddr_in serv_adr;
	memset(&serv_adr, 0, sizeof(serv_adr));
	serv_adr.sin_family = AF_INET;
	TRACE("addr %08X nIP %08X\r\n", inet_addr("127.0.0.1"), m_nIp);
	serv_adr.sin_addr.s_addr = htonl(m_nIp); //�������� IP��ַ���ִ���
	serv_adr.sin_port = htons(m_nPort); //һ��ʼ˳��һ��
	if (serv_adr.sin_addr.s_addr == INADDR_NONE) {
		AfxMessageBox("ָ����IP��ַ�������ڣ�");
		return false;
	}
	int ret = connect(m_sock, (sockaddr*)&serv_adr, sizeof(serv_adr));
	if (ret == -1) {
		AfxMessageBox("����ʧ�ܣ�");
		TRACE("����ʧ�ܣ�%d %s\r\n", WSAGetLastError(), GetErrorInfo(WSAGetLastError()).c_str());
		return false;
	}
	TRACE("socket init done!\r\n");
	return true;
}

bool CClientSocket::SendPacket(HWND hWnd, const CPacket& pack, bool isAutoClosed, WPARAM wParam)
{
	UINT nMode = isAutoClosed ? CSM_AUTOCLOSE : 0;
	std::string strOut;
	pack.Data(strOut);
	PACKET_DATA* pData = new PACKET_DATA(strOut.c_str(), strOut.size(), nMode, wParam);
	bool ret = PostThreadMessage(m_nThreadID, WM_SEND_PACK, (WPARAM)pData, (LPARAM)hWnd);
	if (ret == false) {
		delete pData;
	}
	return ret;
}

unsigned CClientSocket::threadEntry(void* arg)
{
	CClientSocket* thiz = (CClientSocket*)arg;
	thiz->threadFunc();
	_endthreadex(0);
	return 0;
}

void CClientSocket::threadFunc()
{
	SetEvent(m_eventInvoke);
	MSG msg;
	while (::GetMessage(&msg, NULL, 0, 0)) {
		TranslateMessage(&msg);
		DispatchMessage(&msg);
		if (m_mapFunc.find(msg.message) != m_mapFunc.end()) {
			(this->*m_mapFunc[msg.message])(msg.message, msg.wParam, msg.lParam);
		}
	}
}
bool CClientSocket::Send(const CPacket& pack)
{
	TRACE("m_sock = %d\r\n", m_sock);
	if (m_sock == -1) return false;
	std::string strOut;
	pack.Data(strOut);
	return send(m_sock, strOut.c_str(), strOut.size(), 0) > 0;
}

bool CClientSocket::SendPack(UINT nMsg, WPARAM wParam, LPARAM lParam)
{
	//TODO:����һ����Ϣ�����ݽṹ(���ݺ����ݳ��ȣ�ģʽ)  �ص���Ϣ�����ݽṹ(HWND))
	PACKET_DATA data = *(PACKET_DATA*)wParam; //data��һ���ֲ����� ���ͷŵ㣬����ʹ�õ�ʱ��Ͳ������ڴ�й©��������
	delete (PACKET_DATA*)wParam;//
	HWND hWnd = (HWND)lParam;
	size_t nTemp = data.strData.size();
	CPacket current((BYTE*)data.strData.c_str(), nTemp);
	if (InitSocket() == true) {
		int ret = send(m_sock, (char*)data.strData.c_str(), (int)data.strData.size(), 0);
		if (ret > 0) {
			size_t index = 0;
			std::string strBuffer;
			strBuffer.resize(BUFFER_SIZE);
			char* pBuffer = (char*)strBuffer.c_str();
			while (m_sock != INVALID_SOCKET) {
				int length = recv(m_sock, pBuffer + index, BUFFER_SIZE - index, 0); //��������  lengthΪ����
				if (length > 0 || (index > 0)) {
					index += (size_t)length;
					size_t nLen = index;
					CPacket pack((BYTE*)pBuffer, nLen);
					if (nLen > 0) {
						TRACE("ack pack %d to hWnd %08X %d %d\r\n", pack.sCmd, hWnd, index, nLen);
						TRACE("%04X\r\n", *(WORD*)pBuffer + nLen);
						::SendMessage(hWnd, WM_SEND_PACK_ACK, (WPARAM)new CPacket(pack), data.wParam);
						if (data.nMode & CSM_AUTOCLOSE) {
							CloseSocket();
							return;
						}
						index -= nLen;
						memmove(pBuffer, pBuffer + nLen, index);
					}
				}
				else { //TODO:�Է��ر����׽��֣����������豸�쳣
					TRACE("recv failed length %d index %d cmd %d\r\n", length, index, current.sCmd);
					CloseSocket();
					::SendMessage(hWnd, WM_SEND_PACK_ACK, (WPARAM)new CPacket(current.sCmd, NULL, 0), 1);
				}
			}
		}
		else {
			CloseSocket();
			//������ֹ����
			::SendMessage(hWnd, WM_SEND_PACK_ACK, NULL, -1);
		}
	}
	else {
		//TODO:������
		::SendMessage(hWnd, WM_SEND_PACK_ACK, NULL, -2);
	}
	
}





