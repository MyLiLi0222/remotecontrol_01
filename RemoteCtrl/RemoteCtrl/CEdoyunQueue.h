#pragma once
#include "pch.h"
#include <atomic>
#include <list>
#include "MThread.h"
template<class T>
class CEdoyunQueue
{//线程安全的队列（利用IOCP实现）
public:
	enum {
		EQNone,
		EQPush,
		EQPop,
		EQSize,
		EQClear
	};
	typedef struct IocpParam {
		size_t nOperator;//操作
		T Data;//数据
		HANDLE hEvent;//pop操作需要的 事件句柄，主要用于 Pop/Size 操作的同步
		IocpParam(int op, const T& data, HANDLE hEve = NULL) {
			nOperator = op;
			Data = data;
			hEvent = hEve;
		}
		IocpParam() {
			nOperator = EQNone;
		}
	}PPARAM;//post parameter 用于投递信息的结构体
public:
	// 初始化队列并创建 IOCP 和工作线程
	CEdoyunQueue() {
		// std::atomic<bool> 保证 m_lock 的读写是原子操作
		m_lock = false;

		// 创建 IOCP 完成端口
		// CreateIoCompletionPort 参数详解：
		// 1. INVALID_HANDLE_VALUE: 表示不与任何文件句柄关联（纯用于同步）
		// 2. NULL: 不指定现有完成端口（创建新端口）
		// 3. NULL: 完成键（此处不需要）
		// 4. 1: 并发线程数（1 表示同一时间只有一个线程处理该完成端口）
		m_hCompletionPort = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, NULL, 1);
		m_hThread = INVALID_HANDLE_VALUE;
		if (m_hCompletionPort != NULL) {
			m_hThread = (HANDLE)_beginthread(&CEdoyunQueue<T>::threadEntry,0, this);
		}
	}
	virtual ~CEdoyunQueue() {
		if (m_lock) return;
		m_lock = true;
		PostQueuedCompletionStatus(m_hCompletionPort, 0, NULL, NULL);
		WaitForSingleObject(m_hThread, INFINITE);
		if (m_hCompletionPort != NULL) {
			HANDLE hTemp = m_hCompletionPort;
			m_hCompletionPort = NULL;
			CloseHandle(hTemp);
		}
		
	}

	// PushBack: 将一个数据元素异步地添加到队列的末尾
	bool PushBack(const T& data) {//常量引用
		// 将它分配在堆上，可以确保它的生命周期足够长，直到工作线程处理完并手动'delete'它。
		IocpParam* pParam = new IocpParam(EQPush, data);
		if (m_lock) {
			delete pParam;
			return false;
		}
		bool ret = PostQueuedCompletionStatus(
			m_hCompletionPort, 
			sizeof(PPARAM), //非零标记值，纯粹用于消息通知
			(ULONG_PTR)pParam, 
			NULL//lpOverlapped。因为我们不是用IOCP来处理真正的异步文件I/O，所以这里是NULL。
		);
		if (ret == false)delete pParam;
		return ret;
	}
	virtual bool PopFront(T& data) {
    	// Event是Windows中一种基础的同步对象。可以处于“有信号”(Signaled)
    	// 或“无信号”(Non-Signaled)状态。线程可以等待一个Event变成有信号状态。
		HANDLE hEvent = CreateEvent(
			NULL, 
			TRUE, //手动重置SetEvent将其置为有信号，它会一直保持有信号状态，直到程序调用ResetEvent。
			FALSE,//bInitialState。FALSE表示初始状态是“无信号”。
			NULL);

		// 与PushBack不同，这里的Param对象是在栈上创建的局部变量。
    	// 因为PopFront是同步函数，它会一直等待，直到工作线程处理完这个Param。
    	// 在整个等待期间，PopFront函数不会退出，所以它的栈帧是有效的，Param对象也一直存活。
    	// 这样就避免了在堆上动态分配内存和释放的开销，更高效。
		IocpParam Param(EQPop, data, hEvent);
		if (m_lock) {
			if (hEvent)CloseHandle(Event);
			return false;
		}
		bool ret = PostQueuedCompletionStatus(m_hCompletionPort, sizeof(PPARAM), (ULONG_PTR)&Param, NULL);
		if (ret == false) {
			CloseHandle(hEvent);
			return false;
		}
		ret = WaitForSingleObject(hEvent, INFINITE) == WAIT_OBJECT_0;
		if (ret) {
			data = Param.Data;
		}
		return ret;
	}
	size_t Size() {
		// 同PopFront，创建一个Event用于同步等待。
		HANDLE hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
		
		// 这是T类型的“值初始化”或“零初始化”。对于内置类型（如int）它会是0，
    	// 对于类类型，会调用其默认构造函数。因为Size操作不需要传递数据，
    	// 但IocpParam构造函数需要一个T类型的参数，所以我们用T()创建一个临时的默认值。
		IocpParam Param(EQSize, T(), hEvent);
		if (m_lock) {
			if (hEvent)CloseHandle(hEvent);
			return -1;
		}
		bool ret = PostQueuedCompletionStatus(m_hCompletionPort, sizeof(PPARAM), (ULONG_PTR)&Param, NULL);
		if (ret == false) {
			CloseHandle(hEvent);
			return -1;
		}
		ret = WaitForSingleObject(hEvent, INFINITE) == WAIT_OBJECT_0;
		if (ret) {
			return Param.nOperator;
		}
		return -1;
	}
	bool Clear() {
		if (m_lock)return false;
		IocpParam* pParam = new IocpParam(EQClear, T());
		bool ret = PostQueuedCompletionStatus(m_hCompletionPort, sizeof(PPARAM), (ULONG_PTR)pParam, NULL);
		if (ret == false)delete pParam;
		return ret;
	}
protected:
	static void threadEntry(void* arg) {
		CEdoyunQueue<T>* thiz = (CEdoyunQueue<T>*)arg;
		thiz->threadMain();
		_endthread();
	}

	// - typename: 这是一个告诉编译器 `CEdoyunQueue<T>::PPARAM` 是一个类型(type)而不是一个
    //   静态成员变量的关键字。在模板编程中，当一个依赖于模板参数的名称是类型时，
    //   必须使用 typename 来明确指出。
	virtual void DealParam(typename CEdoyunQueue<T>:: PPARAM* pParam) {
		switch (pParam->nOperator) {
		case CEdoyunQueue<T>::EQPush:
			m_lstData.push_back(pParam->Data);
			
			// 重要：这个pParam是在PushBack函数中'new'出来的，处理完毕后必须在这里'delete'，
            // 以防止内存泄漏。这完成了堆内存的生命周期管理。
			delete pParam;
			break;
		case CEdoyunQueue<T>::EQPop:
			if (m_lstData.size() > 0) {
				pParam->Data = m_lstData.front();
				m_lstData.pop_front();
			}

			// Windows API: 将Event对象置为“有信号”状态，唤醒等待它的线程。
			if (pParam->hEvent != NULL)SetEvent(pParam->hEvent);
			break;
		case CEdoyunQueue<T>::EQSize:
			pParam->nOperator = m_lstData.size();
			if (pParam->hEvent != NULL)
				SetEvent(pParam->hEvent);
			break;
		case CEdoyunQueue<T>::EQClear:
			CEdoyunQueue<T>::m_lstData.clear();
			delete pParam;
			break;
		default:
			OutputDebugString("unknown operator!\r\n");
			break;

		}
	}
	void threadMain() {
		// 这些是 GetQueuedCompletionStatus 函数需要用到的输出参数。
        DWORD dwTransferred = 0;      // 接收已传输的字节数（在此处主要用于判断退出信号）。
        PPARAM* pParam = NULL;        // 用于接收指向我们任务参数包的指针。
        ULONG_PTR CompletionKey = 0;  // 接收完成键，我们把它用作了pParam指针。
        OVERLAPPED* pOverLapped = NULL; // 接收OVERLAPPED结构指针，我们没用到。

		// --- 主工作循环 ---
        // Windows API: GetQueuedCompletionStatus
        // 这个函数会尝试从IOCP队列中取出一个完成包（即我们post的任务）。
        // 关键点：当队列中没有任务时，INFINITE参数会使这个线程进入“睡眠”（阻塞）状态，
        // 完全不消耗CPU资源。直到有新任务被Post进来，操作系统才会唤醒它。这是IOCP高效的核心。
		while (GetQueuedCompletionStatus(
			m_hCompletionPort,
			&dwTransferred,
			&CompletionKey,
			&pOverLapped, INFINITE
		)) {
			if ((dwTransferred == 0) || (CompletionKey == NULL)) {
				printf("thread is prepare to exit!\r\n");
				break;
			}
			pParam = (PPARAM*)CompletionKey;
			DealParam(pParam);
		}

		// --- 清理循环（非常重要的细节）---
        // 设计思路：
        // 当上面的循环因为收到退出信号而跳出时，IOCP队列中可能仍然残留着一些
        // 在退出信号之前就已经被投递进来的正常任务。
        // 如果我们直接退出，这些任务就会丢失，如果是Push或Clear任务，还会导致内存泄漏。
        // 因此，需要这个“排干”队列的循环。
		while (GetQueuedCompletionStatus(
			&dwTransferred,
			&CompletionKey,
			&pOverLapped, 0
		)) {
			if ((dwTransferred == 0) || (CompletionKey == NULL)) {
				pritnf("thread is prepare to exit!\r\n");
				continue;
			}
			pParam = (PPARAM*)CompletionKey;
			DealParam(pParam);
		}
		HANDLE hTemp = m_hCompletionPort;
		m_hCompletionPort = NULL;
		CloseHandle(hTemp);
		
	}

protected:
	std::list<T> m_lstData;
	HANDLE m_hCompletionPort;
	HANDLE m_hThread;
	std::atomic<bool> m_lock;//队列正在析构
};

typedef int (ThreadFuncBase::* EDYCALLBACK)();
template<class T>
class ESendQueue :public CEdoyunQueue<T>, public ThreadFuncBase {
public:
	typedef int (ThreadFuncBase::* EDYCALLBACK)(T& data);
	ESendQueue(ThreadFuncBase* obj, EDYCALLBACK callback)
		:CEdoyunQueue<T>() ,m_base(obj), m_callback(callback)
	{
		m_thread.Start();
		m_thread.UpdateWorker(::ThreadWorker(this, (FUNCTYPE)&ESendQueue<T>::threadTick));
	}
	virtual ~ESendQueue() {
		m_base = NULL;
		m_callback = NULL;
		m_thread.Stop();
	}
protected:
	virtual bool PopFront(T& data) {
		return false;
	}
	bool PopFront() {
		typename CEdoyunQueue<T>::IocpParam* param = 
			new typename CEdoyunQueue<T>::IocpParam(CEdoyunQueue<T>::EQPop, T());
		if (CEdoyunQueue<T>::m_lock) {
			delete Param;
			return false;
		}
		bool ret = PostQueuedCompletionStatus(CEdoyunQueue<T>::m_hCompletionPort, sizeof(*Param), (ULONG_PRT)&Param, NULL);
		if (CEdoyunQueue<T>::m_lock) {
			delete Param;
			return false;
		}
		if (ret == false) {
			delete Param;
			return false;
		}
		return ret;
	}
	int threadTick() {
		if (WaitForSingleObject(CEdoyunQueue<T>::m_hThread, 0) != WAIT_TIMEOUT)
			return 0;
		if (CEdoyunQueue<T>::m_lstData.size() > 0) {
			PopFront();
		}
		Sleep(1);
		return 0;
	}
	virtual void DealParam(typename CEdoyunQueue<T>::PPARAM* pParam) {
		switch (pParam->nOperator) {
		case CEdoyunQueue<T>::EQPush:
			CEdoyunQueue<T>::m_lstData.push_back(pParam->Data);
			delete pParam;
			break;
		case CEdoyunQueue<T>::EQPop:
			if (CEdoyunQueue<T>::EQPop::m_lstData.size() > 0) {
				pParam->Data = CEdoyunQueue<T>::m_lstData.front();
				CEdoyunQueue<T>::m_lstData.pop_front();
			}
			if(pParam->hEvent != NULL)SetEvent(pParam->hEvent);
			break;
		case CEdoyunQueue<T>::EQSize:
			pParam->nOperator = CEdoyunQueue<T>::m_lstData.size();
			if (pParam->hHvent != NULL) {
				SetEvent(pParam->hEvent);
			}
			break;
		}
		case CEdoyunQueue<T>::EQClear:
			CEdoyunQueue<T>::m_lstData.clear();
			delete pParam;
			break;
		default:
			OutputDebugStringA("unknown operator!\r\n");
			break;
	}
private:
	ThreadFuncBase* m_base;
	EDYCALLBACK m_callback;
	MThread m_thread;
};
typedef ESendQueue<std::vector<char>>::EDYCALLBACK SENDCALLBACK;