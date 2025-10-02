#pragma once
#include "pch.h"
#include <atomic>
#include <list>
#include "MThread.h"
template<class T>
class CEdoyunQueue
{//�̰߳�ȫ�Ķ��У�����IOCPʵ�֣�
public:
	enum {
		EQNone,
		EQPush,
		EQPop,
		EQSize,
		EQClear
	};
	typedef struct IocpParam {
		size_t nOperator;//����
		T Data;//����
		HANDLE hEvent;//pop������Ҫ�� �¼��������Ҫ���� Pop/Size ������ͬ��
		IocpParam(int op, const T& data, HANDLE hEve = NULL) {
			nOperator = op;
			Data = data;
			hEvent = hEve;
		}
		IocpParam() {
			nOperator = EQNone;
		}
	}PPARAM;//post parameter ����Ͷ����Ϣ�Ľṹ��
public:
	// ��ʼ�����в����� IOCP �͹����߳�
	CEdoyunQueue() {
		// std::atomic<bool> ��֤ m_lock �Ķ�д��ԭ�Ӳ���
		m_lock = false;

		// ���� IOCP ��ɶ˿�
		// CreateIoCompletionPort ������⣺
		// 1. INVALID_HANDLE_VALUE: ��ʾ�����κ��ļ����������������ͬ����
		// 2. NULL: ��ָ��������ɶ˿ڣ������¶˿ڣ�
		// 3. NULL: ��ɼ����˴�����Ҫ��
		// 4. 1: �����߳�����1 ��ʾͬһʱ��ֻ��һ���̴߳������ɶ˿ڣ�
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

	// PushBack: ��һ������Ԫ���첽����ӵ����е�ĩβ
	bool PushBack(const T& data) {//��������
		// ���������ڶ��ϣ�����ȷ���������������㹻����ֱ�������̴߳����겢�ֶ�'delete'����
		IocpParam* pParam = new IocpParam(EQPush, data);
		if (m_lock) {
			delete pParam;
			return false;
		}
		bool ret = PostQueuedCompletionStatus(
			m_hCompletionPort, 
			sizeof(PPARAM), //������ֵ������������Ϣ֪ͨ
			(ULONG_PTR)pParam, 
			NULL//lpOverlapped����Ϊ���ǲ�����IOCP�������������첽�ļ�I/O������������NULL��
		);
		if (ret == false)delete pParam;
		return ret;
	}
	virtual bool PopFront(T& data) {
    	// Event��Windows��һ�ֻ�����ͬ�����󡣿��Դ��ڡ����źš�(Signaled)
    	// �����źš�(Non-Signaled)״̬���߳̿��Եȴ�һ��Event������ź�״̬��
		HANDLE hEvent = CreateEvent(
			NULL, 
			TRUE, //�ֶ�����SetEvent������Ϊ���źţ�����һֱ�������ź�״̬��ֱ���������ResetEvent��
			FALSE,//bInitialState��FALSE��ʾ��ʼ״̬�ǡ����źš���
			NULL);

		// ��PushBack��ͬ�������Param��������ջ�ϴ����ľֲ�������
    	// ��ΪPopFront��ͬ������������һֱ�ȴ���ֱ�������̴߳��������Param��
    	// �������ȴ��ڼ䣬PopFront���������˳�����������ջ֡����Ч�ģ�Param����Ҳһֱ��
    	// �����ͱ������ڶ��϶�̬�����ڴ���ͷŵĿ���������Ч��
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
		// ͬPopFront������һ��Event����ͬ���ȴ���
		HANDLE hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
		
		// ����T���͵ġ�ֵ��ʼ���������ʼ�����������������ͣ���int��������0��
    	// ���������ͣ��������Ĭ�Ϲ��캯������ΪSize��������Ҫ�������ݣ�
    	// ��IocpParam���캯����Ҫһ��T���͵Ĳ���������������T()����һ����ʱ��Ĭ��ֵ��
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

	// - typename: ����һ�����߱����� `CEdoyunQueue<T>::PPARAM` ��һ������(type)������һ��
    //   ��̬��Ա�����Ĺؼ��֡���ģ�����У���һ��������ģ�����������������ʱ��
    //   ����ʹ�� typename ����ȷָ����
	virtual void DealParam(typename CEdoyunQueue<T>:: PPARAM* pParam) {
		switch (pParam->nOperator) {
		case CEdoyunQueue<T>::EQPush:
			m_lstData.push_back(pParam->Data);
			
			// ��Ҫ�����pParam����PushBack������'new'�����ģ�������Ϻ����������'delete'��
            // �Է�ֹ�ڴ�й©��������˶��ڴ���������ڹ���
			delete pParam;
			break;
		case CEdoyunQueue<T>::EQPop:
			if (m_lstData.size() > 0) {
				pParam->Data = m_lstData.front();
				m_lstData.pop_front();
			}

			// Windows API: ��Event������Ϊ�����źš�״̬�����ѵȴ������̡߳�
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
		// ��Щ�� GetQueuedCompletionStatus ������Ҫ�õ������������
        DWORD dwTransferred = 0;      // �����Ѵ�����ֽ������ڴ˴���Ҫ�����ж��˳��źţ���
        PPARAM* pParam = NULL;        // ���ڽ���ָ�����������������ָ�롣
        ULONG_PTR CompletionKey = 0;  // ������ɼ������ǰ���������pParamָ�롣
        OVERLAPPED* pOverLapped = NULL; // ����OVERLAPPED�ṹָ�룬����û�õ���

		// --- ������ѭ�� ---
        // Windows API: GetQueuedCompletionStatus
        // ��������᳢�Դ�IOCP������ȡ��һ����ɰ���������post�����񣩡�
        // �ؼ��㣺��������û������ʱ��INFINITE������ʹ����߳̽��롰˯�ߡ���������״̬��
        // ��ȫ������CPU��Դ��ֱ����������Post����������ϵͳ�Żỽ����������IOCP��Ч�ĺ��ġ�
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

		// --- ����ѭ�����ǳ���Ҫ��ϸ�ڣ�---
        // ���˼·��
        // �������ѭ����Ϊ�յ��˳��źŶ�����ʱ��IOCP�����п�����Ȼ������һЩ
        // ���˳��ź�֮ǰ���Ѿ���Ͷ�ݽ�������������
        // �������ֱ���˳�����Щ����ͻᶪʧ�������Push��Clear���񣬻��ᵼ���ڴ�й©��
        // ��ˣ���Ҫ������Ÿɡ����е�ѭ����
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
	std::atomic<bool> m_lock;//������������
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