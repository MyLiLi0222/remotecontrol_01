#pragma once

#include"pch.h"
#include"framework.h"
#include<string>
#include <vector> 
#include<vector>
#include<list>
#include<map>
#include<mutex>
#define WM_SEND_PACK (WM_USER+1)//发送报数据
#define WM_SEND_PACK (WM_USER+2)//发送报数据应答

#pragma pack(push)//ASKAI
#pragma pack(1)
class CPacket {
public:
	CPacket() :sHead(0), nLength(0), sCmd(0), sSum(0) {}
	CPacket(WORD nCmd, const BYTE* pData, size_t nSize) {
		sHead = 0xFEFF;
		nLength = nSize + 4; // 包长度包括命令和和校验
		sCmd = nCmd;
		if (nSize > 0) {
			strData.resize(nSize);
			memcpy((void*)strData.c_str(), pData, nSize);
		}
		else {
			strData.clear();
		}
		sSum = 0;
		for (size_t i = 0; i < nSize; i++) {
			sSum += BYTE(strData[i]) & 0xFF; // 计算和校验
		}
	}
	CPacket(const CPacket& pack) {
		sHead = pack.sHead;
		nLength = pack.nLength;
		sCmd = pack.sCmd;
		strData = pack.strData;
		sSum = pack.sSum;
	}
	CPacket(const BYTE* pData, size_t& nSize) {
		size_t offset = 0;
		for (; offset < nSize; offset++) {
			if (*(WORD*)(pData + offset) == 0xFEFF) {//ASKAI 这里应该是只取一个WORD长度的数据然后和0xFEFF对比
				sHead = *(WORD*)(pData + offset);
				offset = +2;//跳过头部
				break;
			}
		}
		if (offset + 4 + 2 + 2 > nSize) {
			//数据包不完整
			nSize = 0;
			return;
		}
		nLength = *(DWORD*)(pData + offset);
		offset += 4;//跳过包长度

		if (nLength + offset > nSize) {
			nSize = 0;
			return;
		}
		sCmd = *(WORD*)(pData + offset);
		offset += 2;//跳过命令
		if (nLength > 4) {
			strData.resize(nLength - 4);
			memcpy((void*)strData.c_str(), pData + offset, nLength - 4);
			offset += nLength - 4;//跳过数据
		}

		sSum = *(WORD*)(pData + offset); offset += 2;
		WORD sum = 0;
		for (size_t i = 0; i < strData.size(); i++) {
			sum += BYTE(strData[i]) & 0xFF;//TODO::ASKAI
		}
		if (sum == sSum) {
			nSize = offset ;//+2是和校验的长度
			return;
		}
		nSize = 0;
	}
	~CPacket() {

	}
	CPacket& operator=(const CPacket& pack) {
		if (this != &pack) {
			sHead = pack.sHead;
			nLength = pack.nLength;
			sCmd = pack.sCmd;
			strData = pack.strData;
			sSum = pack.sSum;
		}
		return *this;

	}
	int Size() {
		return nLength + 6;
	}
	const char* Data() {
		strOut.resize(nLength + 6);
		BYTE* pData = (BYTE*)strOut.c_str();
		*(WORD*)pData = sHead; pData += 2;
		*(DWORD*)(pData) = nLength; pData += 4;
		*(WORD*)pData = sCmd; pData += 2;
		memcpy(pData, strData.c_str(), strData.size()); pData += strData.size();
		*(WORD*)pData = sSum;

		return strOut.c_str();
	}
public:
	WORD sHead;//固定位 0XFEFF
	DWORD nLength;//包长度（从控制命令开始，到和校验结束）
	WORD sCmd;//控制命令
	std::string strData;//包数据
	WORD sSum;//和校验
	std::string strOut;//整个包的数据


};
#pragma pack(pop)
typedef struct MouseEvent {
	MouseEvent() {
		nAction = 0;
		nButton = -1;
		ptXY.x = 0;
		ptXY.y = 0;
	}
	WORD nAction; //鼠标操作类型,点击、移动、双击
	WORD nButton; //鼠标按键
	POINT ptXY; //鼠标位置
}MOUSEEVENT, * PMOUSEEVENT;

typedef struct file_info {
	file_info() {
		IsInvalid = FALSE;
		IsDirectory = -1;
		HasNext = TRUE;
		memset(szFileName, 0, sizeof(szFileName));
	}
	BOOL IsInvalid; //是否无效
	BOOL IsDirectory;//是否目录 0:否 1:是
	BOOL HasNext; //是否有下一个 0:否 1:是
	char szFileName[256]; //文件名
}FILEINFO, * PFILEINFO;
void Dumper(BYTE* pData, size_t nSize);

enum {
	CSM_AUTOCLOSE = 1,//自动关闭模式
};

typedef struct PacketData {
	std::string strData;
	UINT nMode;
	WPARAM wParam;
	PacketData(const char* pData, size_t nLen, UINT mode, WPARAM nParam = 0) {
		strData.resize(nLen);
		memcpy((char*)strData.c_str(), pData, nLen);
		nMode = mode;
		wParam = nParam;
	}
	PacketData(const PacketData& data) {
		strData = data.strData;
		nMode = data.nMode;
		wParam = data.wParam;
	}
	PacketData& operator = (const PacketData& data) {
		if (this != &data) {
			strData = data.strData;
			nMode = data.nMode;
			wParam = data.wParam;
		}
		return *this;
	}
}PACKET_DATA;

std::string GetErrorInfo(int wsaErrcode);
void Dump(BYTE* pData, size_t nSize);

class CClientSocket {
public:
	static CClientSocket* getInstance() {
		if (m_instance == NULL) {//静态函数没有this指针，所以无法直接访问成员变量
			m_instance = new CClientSocket();
			TRACE("CClientSocket size is %d \r\n", sizeof(*m_instance));
		}
		return m_instance;
	}
	bool InitSocket();
#define BUFFER_SIZE 4096000
	int DealCommand() {
		if (m_sock == -1)return -1;
		char* buffer = m_buffer.data();
		static size_t index = 0;
		while (true) {
			size_t len = recv(m_sock, buffer + index, BUFFER_SIZE - index, 0);
			if ((len <= 0) && (index <= 0)) {
				return -1;
			}
			TRACE("rece len = %d(0x%08X) index = %d(0x%08X)\r\n", len, len, index, index);
			index += len;
			len = index;
			TRACE("rece len = % d(0x % 08X) index = % d(0x % 08X)\r\n", len, len, index, index);
			m_packet = CPacket((BYTE*)buffer, len);
			TRACE("command %d\r\n", m_packet.sCmd);
			if (len > 0) {
				memmove(buffer, buffer + len, index - len);
				index -= len;
				return m_packet.sCmd;
			}
		}
		return -1;
	}
	bool SendPacket(HWND hWnd, const CPacket& pack, bool isAutoClosed = true, WPARAM wParam = 0);
	bool GetFilePath(std::string& strPath) {
		if ((m_packet.sCmd >= 2) && (m_packet.sCmd <= 4)) {
			strPath = m_packet.strData;
			return true;
		}
		return false;
	}
	bool GetMouseEvent(MOUSEEVENT& mouse) {
		if (m_packet.sCmd == 5) {
			memcpy(&mouse, m_packet.strData.c_str(), sizeof(MOUSEEVENT));
			return false;
		}
	}
	CPacket& GetPacket() {
		return m_packet;
	}
	void CloseSocket() {
		closesocket(m_sock);
		m_sock = INVALID_SOCKET;
	}
	void UpdateAddress(int nIP, int nPort) {
		if ((m_nIp != nIP) || (m_nPort != nPort)) {
			m_nIp = nIP;
			m_nPort = nPort;
		}
	}
private:
	HANDLE m_eventInvoke;
	UINT m_nThreadID;
	typedef void(CClientSocket::* MSGFUNC)(UINT nMsg, WPARAM wParam, LPARAM lParam);
	std::map<UINT, MSGFUNC>m_mapFunc;
	HANDLE m_hThread;
	bool m_bAutoClose;
	std::mutex m_lock;
	std::list<CPacket>m_lstSend;
	std::map<HANDLE, std::list<CPacket>&>m_mapACK;
	std::map<HANDLE, bool>m_mapAutoClosed;
	int m_nIp;
	int m_nPort;
	std::vector<char>m_buffer;
	SOCKET m_sock;
	CPacket m_packet;
	CClientSocket& operator = (const CClientSocket& ss){}
	CClientSocket(const CClientSocket& ss);
	CClientSocket();
	~CClientSocket() {
		closesocket(m_sock);
		m_sock = INVALID_SOCKET;
		WSACleanup;
	}
	static unsigned __stdcall threadEntry(void* arg);
	void threadFunc();
	BOOL InitSockEnv() {
		WSADATA data;
		if (WSAStartup(MAKEWORD(1, 1), &data) != 0) {
			return FALSE;
		}
		return TRUE;
	}
	bool Send(const CPacket& pack);
	bool Send(const char* pData, int nSize) {
		if (m_sock == -1)return false;
		return send(m_sock, pData, nSize, 0) > 0;//ASKAI 这里的>0是什么意思？

	}
	bool SendPack(UINT nMsg, WPARAM wParam/*缓冲区的值*/, LPARAM lParam/*缓冲区的长度*/);
	static void releasInstance() {
		TRACE("CClientSocket has been called! \r\n");
		if (m_instance != NULL) {
			CClientSocket* tmp = m_instance;
			m_instance = NULL;
			delete tmp;
			TRACE("CClientSocket has released!\r\n");
		}
	}
	static CClientSocket* m_instance;
	class CHelper {
	public:
		CHelper() {
			CClientSocket::getInstance();
		}
		~CHelper() {
			CClientSocket::releasInstance();
		}
	};
	static CHelper m_helper;
};
extern CClientSocket serer;