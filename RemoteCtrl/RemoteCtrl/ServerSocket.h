#pragma once
#include"pch.h"
#include"framework.h"
#include<list>
//中文测试
//阮疆南
//sub_func 分支测试
#pragma pack(push)//ASKAI
#pragma pack(1)
class CPacket {
public:
	CPacket() :sHead(0),nLength(0),sCmd(0),sSum(0){}
	//重构构造函数，打包数据
	CPacket(WORD nCmd,const BYTE *pData,size_t nSize) {
		sHead = 0xFEFF;
		nLength = nSize + 4; // 包长度包括命令和和校验
		sCmd = nCmd;
		if(nSize>0){
			strData.resize(nSize);
			memcpy((void*)strData.c_str(), pData, nSize);
		}else{
			strData.clear();
		}
		sSum = 0;
		for(size_t i = 0; i < nSize; i++) {
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
		size_t i = 0;
		for (; i < nSize; i++) {
			if (*(WORD*)(pData + i) == 0xFEFF) {
				sHead = *(WORD*)(pData + i);
				i += 2;
				break;
			}
		}
		if(i + 4 + 2 + 2 > nSize) {
			//数据包不完整
			nSize = 0;
			return;
		}
		nLength = *(DWORD*)(pData + i); i += 4;//跳过包长度
		
		if (nLength + i > nSize) {
			nSize = 0;
			return;
		}
		sCmd = *(WORD*)(pData + i); i += 2;//跳过命令
		if (nLength > 4) {
			strData.resize(nLength - 2 - 2);
			memcpy((void*)strData.c_str(), pData + i, nLength - 4);
			i += nLength - 4;
		}

		sSum = *(WORD*)(pData + i); i += 2;
		WORD sum = 0;
		for (size_t j = 0; j < strData.size(); j++)
		{
			sum += BYTE(strData[j]) & 0xFF;
		}
		if (sum == sSum) {
			nSize = i;//+2是和校验的长度
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
	int Size(){
		return nLength+6;
	}
	const char *Data(){
		strOut.resize(nLength+6);
		BYTE* pData = (BYTE*)strOut.c_str();
		*(WORD*)pData = sHead; pData+=2;
		*(DWORD*)(pData)=nLength;pData+=4;
		*(WORD*)pData = sCmd;pData+=2;
		memcpy(pData,strData.c_str(),strData.size());pData+=strData.size();
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
typedef struct MouseEvent{
	MouseEvent(){
		nAction = 0;
		nButton = -1;
		ptXY.x = 0;
		ptXY.y = 0;
	}
	WORD nAction; //鼠标操作类型,点击、移动、双击
	WORD nButton; //鼠标按键
	POINT ptXY; //鼠标位置
}MOUSEEVENT,*PMOUSEEVENT;

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
//单例模式
//CServerSocket类的设计
typedef void (*SOCKET_CALLBACK)(void *,int,std::list<CPacket>&);
class CServerSocket
{
public:
	static CServerSocket *getInstance() {
		if (m_instance == NULL) {//静态函数没有this指针，所以无法直接访问成员变量
			m_instance = new CServerSocket();
		}
		return m_instance;
		
	}
	bool InitSocket(short port) {

		if (m_sock == -1) return false;
		sockaddr_in serv_adr,client_adr;
		memset(&serv_adr, 0, sizeof(serv_adr));
		serv_adr.sin_family = AF_INET;
		serv_adr.sin_addr.s_addr = INADDR_ANY;
		serv_adr.sin_port = htons(port);
		if (bind(m_sock, (sockaddr*)&serv_adr, sizeof(serv_adr))==-1) {
			return false;
		}
		if (listen(m_sock, 1) == -1) {
			return false;
		}
		return true;
	}

	int Run(SOCKET_CALLBACK callback, void* arg,short port=8823) {
		bool ret = InitSocket(port);
		if (ret == false)  return -1;
		std::list<CPacket> lstPackets;
		m_callback = callback;
		m_arg = arg;
		int count =0;
		while(true){
			if(AcceptClient()==false){
				if(count>=3){
					return -2;
				}
				count++;
			}
			int ret = DealCommand();
			if(ret>0){
				m_callback(m_arg,ret);
			}
			CloseClient();
		}
		
		return 0;
	}

	bool AcceptClient() {
		sockaddr_in client_adr;
		int cli_sz = sizeof(client_adr);
		m_client = accept(m_sock, (sockaddr*)&client_adr, &cli_sz);
		TRACE("m_client = %d\r\n",m_client);
		if (m_client == -1) {
			return false;
		}
		return true;
	}
#define BUFFER_SIZE 4096
	int DealCommand(){
		if(m_client == -1)return -1;
		char *buffer = new char [BUFFER_SIZE];
		if (buffer == NULL) {
			TRACE("内存不足！\r\n");
			return -2;
		}
		memset(buffer, 0, BUFFER_SIZE);
		size_t index = 0;
		//接收数据
		while (true)
		{
			size_t len =recv(m_client,buffer+index,BUFFER_SIZE-index,0);
			if(len<=0){
				return -1;
				delete[]buffer;
			}
			index += len;
			len = index;
			//TODO:处理命理
			m_packet = CPacket((BYTE*)buffer, len);
			if (len > 0) {
				//len 的值已经改变 是&len的引用
				//memmove 如何使用？ASKAI
				//4096-len: 已经使用了len的长度，每次接收的buffer长度是4096，用了len减去len；
				memmove(buffer,buffer+len,BUFFER_SIZE-len);//清除已处理的数据
				index -= len;//更新index
				delete[] buffer;
				return	m_packet.sCmd;
			}
		}
		delete[]buffer;
		return -1;
		
	}
	bool Send(const char* pData,size_t nSize ){
		if(m_client == -1)return false;
		return send(m_client,pData,nSize,0) > 0;//这句的逻辑是什么？
	}
	//重构发送数据包
	bool Send(CPacket& packet) {
		if (m_client == -1)return false;
		return send(m_client,packet.Data(),packet.Size(), 0) > 0;
	}
	bool GetFilePath(std::string& strPath) {
		if((m_packet.sCmd >=2)&&(m_packet.sCmd <= 4) || (m_packet.sCmd == 9)) {
			strPath = m_packet.strData;
			return true;
		}
		return false;
	}
	bool GetMouseEvent(MOUSEEVENT& mouse) {
		if (m_packet.sCmd == 5) {
			memcpy(&mouse, m_packet.strData.c_str(), sizeof(MOUSEEVENT));
			return true;
		}
		return false;
	}
	CPacket& GetPacket() {
		return m_packet;
	}
	void CloseClient() {
		if(m_client!=INVALID_SOCKET){
			closesocket(m_client);
			m_client = INVALID_SOCKET;
		}
		
	}
private:
	SOCKET_CALLBACK m_callback;//ASKAI
	void* m_arg;
	SOCKET m_client;
	SOCKET m_sock;
	CPacket m_packet;
	CServerSocket& operator= (const CServerSocket &ss){}
	CServerSocket(const CServerSocket &ss){
		m_sock = ss.m_sock;
		m_client = ss.m_client;
	}
	CServerSocket() {
		m_client = INVALID_SOCKET;
		if (InitSocketEnv() == FALSE) {
			MessageBox(NULL, _T("无法初始化套接字环境，请检查网络设置！"), _T("初始化错误！"), MB_OK | MB_ICONERROR);
			exit(0);
		}
		m_sock = socket(PF_INET, SOCK_STREAM, 0);

	}
	~CServerSocket(){
		closesocket(m_sock);
		WSACleanup();
	}
	BOOL InitSocketEnv() {
		WSADATA data;
		if (WSAStartup(MAKEWORD(1, 1), &data)!=0) {
			return FALSE;
		}
		return TRUE;
	}
	static CServerSocket* m_instance;
	static void releaseInsetance() {
		if (m_instance != NULL) {
			CServerSocket* tmp = m_instance;
			m_instance = NULL;
			delete tmp;
		}
	}
	class CHelper {
	public:
		CHelper() {
			CServerSocket::getInstance();
		}
		~CHelper() {
			CServerSocket::releaseInsetance();
		}
	};
	static CHelper m_helper;//kkkkk中文显示测试
};

extern CServerSocket server;

