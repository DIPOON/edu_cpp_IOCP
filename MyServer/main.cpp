#include <stdio.h>
#include <thread>
#include <vector>
#include <WinSock2.h>
#include <Ws2tcpip.h>

#pragma comment(lib, "ws2_32.lib")

#define MAX_SOCKBUF 1024	//��Ŷ ũ��

const UINT16 SERVER_PORT = 11021; // ���� ��Ʈ
const UINT16 MAX_CLIENT = 100;    // �� �����Ҽ� �ִ� Ŭ���̾�Ʈ ��
const INT MAX_SOCKET_SIZE = 1024; // ���� ���� ũ��

enum class IOOperation
{
	RECV,
	SEND
};

// overlapped ����ü Ȯ��
struct stOverlappedEx
{
	WSAOVERLAPPED m_wsaOverlapped;          // Overlapped I/O ����ü
	WSABUF        m_wsaBuf;			        // �۾� ����
	char          m_szBuf[MAX_SOCKET_SIZE]; // ������ ����
	IOOperation   m_eOperation;	            // �۾� ���� ����
};

struct stClientInfo
{
	SOCKET         m_ClientSocket;          // Client accept�� ������� ����
	stOverlappedEx m_stReceiveOverlappedEx; // Receive overlapped I/O ��
	stOverlappedEx m_stSendOverlappedEx;    // Send overlapped I/O ��

	stClientInfo()
	{
		ZeroMemory(&m_stReceiveOverlappedEx, sizeof(m_stReceiveOverlappedEx));
		ZeroMemory(&m_stSendOverlappedEx, sizeof(m_stSendOverlappedEx));
		m_ClientSocket = INVALID_SOCKET;
	}
};

HANDLE IocpHandle;
std::vector<stClientInfo> mClientInfos;
SOCKET ListenSocket;
//���� �Ǿ��ִ� Ŭ���̾�Ʈ ��
int	mClientCnt = 0;

//������� �ʴ� Ŭ���̾�Ʈ ���� ����ü�� ��ȯ�Ѵ�.
stClientInfo* GetEmptyClientInfo()
{
	for (auto& client : mClientInfos)
	{
		if (INVALID_SOCKET == client.m_ClientSocket)
		{
			return &client;
		}
	}

	return nullptr;
}

//WSARecv Overlapped I/O �۾��� ��Ų��.
bool BindRecv(stClientInfo* pClientInfo)
{
	DWORD dwFlag = 0;
	DWORD dwRecvNumBytes = 0;

	//Overlapped I/O�� ���� �� ������ ������ �ش�.
	pClientInfo->m_stReceiveOverlappedEx.m_wsaBuf.len = MAX_SOCKBUF;
	pClientInfo->m_stReceiveOverlappedEx.m_wsaBuf.buf = pClientInfo->m_stReceiveOverlappedEx.m_szBuf;
	pClientInfo->m_stReceiveOverlappedEx.m_eOperation = IOOperation::RECV;

	int nRet = WSARecv(pClientInfo->m_ClientSocket,
		&(pClientInfo->m_stReceiveOverlappedEx.m_wsaBuf),
		1,
		&dwRecvNumBytes,
		&dwFlag,
		(LPWSAOVERLAPPED) & (pClientInfo->m_stReceiveOverlappedEx),
		NULL);

	//socket_error�̸� client socket�� �������ɷ� ó���Ѵ�.
	if (SOCKET_ERROR == nRet && (WSAGetLastError() != ERROR_IO_PENDING))
	{
		printf("[����] WSARecv()�Լ� ���� : %d\n", WSAGetLastError());
		return false;
	}

	return true;
}

//WSASend Overlapped I/O�۾��� ��Ų��.
bool SendMsg(stClientInfo* pClientInfo, char* pMsg, int nLen)
{
	DWORD dwRecvNumBytes = 0;

	//���۵� �޼����� ����
	CopyMemory(pClientInfo->m_stSendOverlappedEx.m_szBuf, pMsg, nLen);


	//Overlapped I/O�� ���� �� ������ ������ �ش�.
	pClientInfo->m_stSendOverlappedEx.m_wsaBuf.len = nLen;
	pClientInfo->m_stSendOverlappedEx.m_wsaBuf.buf = pClientInfo->m_stSendOverlappedEx.m_szBuf;
	pClientInfo->m_stSendOverlappedEx.m_eOperation = IOOperation::SEND;

	int nRet = WSASend(pClientInfo->m_ClientSocket,
		&(pClientInfo->m_stSendOverlappedEx.m_wsaBuf),
		1,
		&dwRecvNumBytes,
		0,
		(LPWSAOVERLAPPED) & (pClientInfo->m_stSendOverlappedEx),
		NULL);

	//socket_error�̸� client socket�� �������ɷ� ó���Ѵ�.
	if (nRet == SOCKET_ERROR && (WSAGetLastError() != ERROR_IO_PENDING))
	{
		printf("[����] WSASend()�Լ� ���� : %d\n", WSAGetLastError());
		return false;
	}
	return true;
}

void WorkThreadRun()
{
	//CompletionKey�� ���� ������ ����
	stClientInfo* pClientInfo = NULL;

	//Overlapped I/O�۾����� ���۵� ������ ũ��
	DWORD dwIoSize = 0;

	//I/O �۾��� ���� ��û�� Overlapped ����ü�� ���� ������
	LPOVERLAPPED lpOverlapped = NULL;

	// ��Ŀ ������ ����
	while (1)
	{
		// �����尡 I/O ���� ���� �ҷ��� ó���ϰ� ����
		bool result1 = GetQueuedCompletionStatus(IocpHandle, &dwIoSize, (PULONG_PTR)&pClientInfo, &lpOverlapped, INFINITE);
		if (result1 != true)
		{
			wprintf(L"GetQueuedCompletionStatus return false : %d\n", WSAGetLastError());
		}
		if (dwIoSize == 0 && result1 == true)
		{
			wprintf(L"GetQueuedCompletionStatus 0 dwIoSize and return true : %d\n", WSAGetLastError());
		}

		// Accept �����忡�� IOCP ť�� ���� �� lpOverlapped �������ʿ� stOverlappedEx ����ü�� ����� �־��� ������ ���� �����ؾ���
		stOverlappedEx* pOverlappedEx = (stOverlappedEx*)lpOverlapped;
		if (pOverlappedEx->m_eOperation == IOOperation::RECV)
		{
			pOverlappedEx->m_szBuf[dwIoSize] = NULL;
			printf("[����] bytes : %d , msg : %s\n", dwIoSize, pOverlappedEx->m_szBuf);

			//Ŭ���̾�Ʈ�� �޼����� �����Ѵ�.
			SendMsg(pClientInfo, pOverlappedEx->m_szBuf, dwIoSize);
			BindRecv(pClientInfo);
		}
		else if (pOverlappedEx->m_eOperation == IOOperation::RECV)
		{
			printf("[�۽�] bytes : %d , msg : %s\n", dwIoSize, pOverlappedEx->m_szBuf);
		}
		else
		{
			printf("socket(%d)���� ���ܻ�Ȳ\n", (int)pClientInfo->m_ClientSocket);
		}
	}
}

void AcceptThreadRun()
{
	SOCKADDR_IN	stClientAddr;
	int nAddrLen = sizeof(SOCKADDR_IN);
	bool mIsAccepterRun = true;
	while (mIsAccepterRun == true)
	{
		// ������ ���� ����ü�� �ε����� ���´�.
		stClientInfo* pClientInfo = GetEmptyClientInfo();
		if (pClientInfo == NULL)
		{
			printf("[����] Client Full\n");
			return;
		}

		//Ŭ���̾�Ʈ ���� ��û�� ���� ������ ��ٸ���.
		pClientInfo->m_ClientSocket = accept(ListenSocket, (SOCKADDR*)&stClientAddr, &nAddrLen);
		if (INVALID_SOCKET == pClientInfo->m_ClientSocket)
		{
			continue;
		}

		//socket�� pClientInfo�� I/O CompletionPort��ü�� �����Ų��.
		auto hIOCP = CreateIoCompletionPort((HANDLE)pClientInfo->m_ClientSocket, IocpHandle, (ULONG_PTR)(pClientInfo), 0);
		if (NULL == hIOCP || IocpHandle != hIOCP)
		{
			printf("[����] CreateIoCompletionPort()�Լ� ����: %d\n", GetLastError());
			return;
		}

		//Recv Overlapped I/O�۾��� ��û�� ���´�.
		bool bRet = BindRecv(pClientInfo);
		if (false == bRet)
		{
			return;
		}

		char clientIP[32] = { 0, };
		inet_ntop(AF_INET, &(stClientAddr.sin_addr), clientIP, 32 - 1);
		printf("Ŭ���̾�Ʈ ���� : IP(%s) SOCKET(%d)\n", clientIP, (int)pClientInfo->m_ClientSocket);

		//Ŭ���̾�Ʈ ���� ����
		++mClientCnt;
	}
}

int main()
{
	// WSA �ʱ�ȭ
	WSADATA wsaData;
	int Result = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (Result != 0)
	{
		wprintf(L"WSAStartup failed : %d\n", Result);
		return -1;
	}

	// ���� ����
	ListenSocket = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, NULL, WSA_FLAG_OVERLAPPED);
	if (ListenSocket == INVALID_SOCKET)
	{
		wprintf(L"socket failed : %d\n", WSAGetLastError());
		return -1;
	}

	// Set up the sockaddr structure
	SOCKADDR_IN saServer;
	saServer.sin_family = AF_INET;
	saServer.sin_port = htons(SERVER_PORT);
	saServer.sin_addr.s_addr = htonl(INADDR_ANY);

	// bind
	Result = bind(ListenSocket, (SOCKADDR*)&saServer, sizeof(SOCKADDR_IN));
	if (Result != 0)
	{
		wprintf(L"bind failed : %d\n", WSAGetLastError());
		return -1;
	}

	// listen
	Result = listen(ListenSocket, 5);
	if (Result != 0)
	{
		wprintf(L"listen failed : %d\n", WSAGetLastError());
		return -1;
	}

	//Ŭ���̾�Ʈ ���� ���� ����ü �ʱ�ȭ
	for (UINT32 i = 0; i < MAX_CLIENT; ++i)
	{
		mClientInfos.emplace_back();
	}

	//// IOCP ����
	HANDLE IocpHandle = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);
	if (IocpHandle == NULL)
	{
		wprintf(L"CreateIoCompletionPort failed : %d\n", WSAGetLastError());
		return -1;
	}

	// ��Ŀ ������ ����
	std::vector<std::thread> WorkerThreads;
	for (int i = 0; i < 3; i++) // TODO ���μ��� * 2
	{
		WorkerThreads.emplace_back([]() { WorkThreadRun(); });
	}

	// Accept ������
	std::thread mAcceptThread = std::thread([]() { AcceptThreadRun(); });

	// IOCP �ڵ� ����
	CloseHandle(IocpHandle);

	// ��Ŀ ������ ����
	for (auto& th : WorkerThreads)
	{
		if (th.joinable())
		{
			th.join();
		}
	}

	// Accept ������ ����		
	if (mAcceptThread.joinable())
	{
		mAcceptThread.join();
	}


	// WSA ����
	WSACleanup();

	return 0;
}