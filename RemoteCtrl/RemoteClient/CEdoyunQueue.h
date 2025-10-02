#pragma once
#include <string>
template<class T>
class CEdoyunQueue
{//线程安全队列
public:
	CEdoyunQueue();
	~CEdoyunQueue();
	bool PushBack(const T& data);
	bool PopFront(T& data);
	size_t Size();
	void Clear();
private:
	static void threadEntry(void* arg);
	void threadMain();
private:
	std::list<T>m_lstData;
	HANDLE m_hCompeletionPort;
	HAMDLE m_hThread;
public:
	typedef struct IocpParam {
		int nOperator;
		T strData;
		_beginthread_proc_type cbFunc;//回调
		HANDLE hEvent; 
		IocpParam(int op, const char* sData, _beginthread_proc_type cb = NUll) {
			nOperator = op;
			strData = sData;
			cbFunc = cb;
		}
		IocpParam() {
			nOperator = -1;
		}
	}PPARAM;//投递信息的结构体
	enum {
		EQPush,
		EQPop,
		EQSize,
		EQClear
	};

};



