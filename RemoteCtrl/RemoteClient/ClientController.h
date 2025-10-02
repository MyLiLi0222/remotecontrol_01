#pragma once
#include"ClientSocket.h"
#include"CWatchDialog.h"
#include"RemoteClientDlg.h"
#include"StatusDlg.h"
#include "resource.h"
#include <map>

//ASKAI WM_USER是Windows消息的起始值 
#define WM_SEND_PACKET (WM_USER+1)//发送包数据
#define WM_SEND_DATA (WM_USER+2)//发送数据
#define WM_SHOW_STATUS (WM_USER+3)//展示状态
#define WM_SHOW_WATCH (WM_USER+4)//远程监控
#define WM_SEND_MESSAGE (WM_USER+0x1000)//发送消息处理消息



class CClientController
{
public: 
	//获取全局唯一对象
	static CClientController* getInstance();
	//初始化操作
	int InitController();
	//启动
	int Invoke(CWnd*& pMainWnd);
	LRESULT SendMessage(MSG msg);
	void UpdateAddress(int nIP, int nPort) {
		CClientSocket::getInstance()->UpdateAddress(nIP, nPort);
	}
	int DealCommand() {
		return CClientSocket::getInstance()->DealCommand();
	}
	void CloseSocket() {
		CClientSocket::getInstance()->CloseSocket();
	}
	//1 查看磁盘分区
	//2 查看指定目录下的文件
	//3 打开文件
	//4 下载文件
	//5 鼠标操作
	//6 发送屏幕内容==》发送屏幕截图
	//7 锁机
	//8 解锁
	//9 删除文件
	//1981 测试连接
	//返回值：是状态，true是成功，false是失败
	bool SendCommandPacket(
		int nCmd,
		bool bAutoClose = true,
		BYTE* pData = NULL,
		size_t nLength=0,
		std::initializer_list<CPacket>* plstpacket = NULL);
	int GetImage(CImage& image) {
		CClientSocket* pClient = CClientSocket::getInstance();
		return CTool::Bytes2Image(image, pClient->GetPacket().strData);

	}
	int DownFile(CString strPath);
	void StartWatchScreen();
protected:
	void threadWatchScreen();
	void threadDownloadFile();
	static void threadDownloadEntry(void* arg);
	static void CClientController::threadWatchScreenEntry(void* arg);

	CClientController() :
		m_statusDlg(&m_remoteDlg),
		m_watchDlg(&m_remoteDlg)
	{
		m_isClosed = true;
		m_hThreadWatch = INVALID_HANDLE_VALUE;
		m_hThreadDownload = INVALID_HANDLE_VALUE;
		m_hThread = INVALID_HANDLE_VALUE;
		m_nThreadID = -1;
	}

	~CClientController() {
		WaitForSingleObject(m_hThread, 100);
	}
	void threadFunc();
	static unsigned __stdcall threadEntry(void* arg);
	static void releaseInstance() {
		if(m_instance != NULL) {
			delete m_instance;
			m_instance = NULL;
		}
	}
	//LRESULT OnSendPack(UINT nMsg, WPARAM wParam, LPARAM lParam);
	LRESULT OnSendData(UINT nMsg, WPARAM wParam, LPARAM lParam);
	LRESULT OnShowStatus(UINT nMsg, WPARAM wParam, LPARAM lParam);
	LRESULT OnShowWatcher(UINT nMsg, WPARAM wParam, LPARAM lParam);
private:
	typedef struct MsgInfo{
		MSG msg;
		LRESULT result;
		MsgInfo(MSG m){
			result =0;
			memcpy(&msg,&m,sizeof(MSG));
		}
		MsgInfo(){
			result =0;
			memset(&msg,0,sizeof(MSG));
		}
		MsgInfo(const MsgInfo& mi){
			result = mi.result;
			memcpy(&msg,&mi.msg,sizeof(MSG));
		}
		MsgInfo& operator=(const MsgInfo& mi){
			if(this!=&mi) {
				result = mi.result;
				memcpy(&msg,&mi.msg,sizeof(MSG));
			}
			return *this;
		}
	}MSGINFO;

	typedef LRESULT(CClientController::* MSGFUNC)(UINT nMsg,
		WPARAM wParam,LPARAM lParam);
	//ASKAI 消息映射表 什么是消息映射表？使用的步骤是什么？
	static std::map<UINT, MSGFUNC>m_mapFunc;

	std::map<UINT,MSGINFO>m_mapMessage;

	CWatchDialog m_watchDlg;
	CRemoteClientDlg m_remoteDlg;
	CStatusDlg m_statusDlg;
	HANDLE m_hThread;
	HANDLE m_hThreadDownload;
	HANDLE m_hThreadWatch;
	//下载文件的远程路径
	CString m_strRemote;
	bool m_isClosed = true;
	//下载文件的本地保存路径
	CString m_strLocal;
	unsigned m_nThreadID;

	static CClientController* m_instance;
	class CHelper {
	public:
		CHelper() {
			CClientController::getInstance();
		}
		~CHelper() {
			CClientController::releaseInstance();
		}
	};
	static CHelper m_helper;

};

