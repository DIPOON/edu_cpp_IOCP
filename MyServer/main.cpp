#include <stdio.h>
#include <thread>
#include <vector>
#include <WinSock2.h>
#include <Ws2tcpip.h>

#pragma comment(lib, "ws2_32.lib")

#define MAX_SOCKBUF 1024	//패킷 크기

const UINT16 SERVER_PORT = 11021; // 서버 포트
const UINT16 MAX_CLIENT = 100;    // 총 접속할수 있는 클라이언트 수
const INT MAX_SOCKET_SIZE = 1024; // 소켓 버퍼 크기

enum class IOOperation
{
	RECV,
	SEND
};

// overlapped 구조체 확장
struct stOverlappedEx
{
	WSAOVERLAPPED m_wsaOverlapped;          // Overlapped I/O 구조체
	WSABUF        m_wsaBuf;			        // 작업 버퍼
	char          m_szBuf[MAX_SOCKET_SIZE]; // 데이터 버퍼
	IOOperation   m_eOperation;	            // 작업 동작 종류
};

struct stClientInfo
{
	SOCKET         m_ClientSocket;          // Client accept로 만들어진 소켓
	stOverlappedEx m_stReceiveOverlappedEx; // Receive overlapped I/O 용
	stOverlappedEx m_stSendOverlappedEx;    // Send overlapped I/O 용

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
//접속 되어있는 클라이언트 수
int	mClientCnt = 0;

//사용하지 않는 클라이언트 정보 구조체를 반환한다.
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

//WSARecv Overlapped I/O 작업을 시킨다.
bool BindRecv(stClientInfo* pClientInfo)
{
	DWORD dwFlag = 0;
	DWORD dwRecvNumBytes = 0;

	//Overlapped I/O을 위해 각 정보를 셋팅해 준다.
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

	//socket_error이면 client socket이 끊어진걸로 처리한다.
	if (SOCKET_ERROR == nRet && (WSAGetLastError() != ERROR_IO_PENDING))
	{
		printf("[에러] WSARecv()함수 실패 : %d\n", WSAGetLastError());
		return false;
	}

	return true;
}

//WSASend Overlapped I/O작업을 시킨다.
bool SendMsg(stClientInfo* pClientInfo, char* pMsg, int nLen)
{
	DWORD dwRecvNumBytes = 0;

	//전송될 메세지를 복사
	CopyMemory(pClientInfo->m_stSendOverlappedEx.m_szBuf, pMsg, nLen);


	//Overlapped I/O을 위해 각 정보를 셋팅해 준다.
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

	//socket_error이면 client socket이 끊어진걸로 처리한다.
	if (nRet == SOCKET_ERROR && (WSAGetLastError() != ERROR_IO_PENDING))
	{
		printf("[에러] WSASend()함수 실패 : %d\n", WSAGetLastError());
		return false;
	}
	return true;
}

void WorkThreadRun()
{
	//CompletionKey를 받을 포인터 변수
	stClientInfo* pClientInfo = NULL;

	//Overlapped I/O작업에서 전송된 데이터 크기
	DWORD dwIoSize = 0;

	//I/O 작업을 위해 요청한 Overlapped 구조체를 받을 포인터
	LPOVERLAPPED lpOverlapped = NULL;

	// 워커 쓰레드 실행
	while (1)
	{
		// 쓰레드가 I/O 끝난 것을 불러와 처리하게 만듬
		bool result1 = GetQueuedCompletionStatus(IocpHandle, &dwIoSize, (PULONG_PTR)&pClientInfo, &lpOverlapped, INFINITE);
		if (result1 != true)
		{
			wprintf(L"GetQueuedCompletionStatus return false : %d\n", WSAGetLastError());
		}
		if (dwIoSize == 0 && result1 == true)
		{
			wprintf(L"GetQueuedCompletionStatus 0 dwIoSize and return true : %d\n", WSAGetLastError());
		}

		// Accept 스레드에서 IOCP 큐에 넣을 때 lpOverlapped 포인터쪽에 stOverlappedEx 구조체로 만들어 넣었기 때문에 값이 존재해야함
		stOverlappedEx* pOverlappedEx = (stOverlappedEx*)lpOverlapped;
		if (pOverlappedEx->m_eOperation == IOOperation::RECV)
		{
			pOverlappedEx->m_szBuf[dwIoSize] = NULL;
			printf("[수신] bytes : %d , msg : %s\n", dwIoSize, pOverlappedEx->m_szBuf);

			//클라이언트에 메세지를 에코한다.
			SendMsg(pClientInfo, pOverlappedEx->m_szBuf, dwIoSize);
			BindRecv(pClientInfo);
		}
		else if (pOverlappedEx->m_eOperation == IOOperation::RECV)
		{
			printf("[송신] bytes : %d , msg : %s\n", dwIoSize, pOverlappedEx->m_szBuf);
		}
		else
		{
			printf("socket(%d)에서 예외상황\n", (int)pClientInfo->m_ClientSocket);
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
		// 접속을 받을 구조체의 인덱스를 얻어온다.
		stClientInfo* pClientInfo = GetEmptyClientInfo();
		if (pClientInfo == NULL)
		{
			printf("[에러] Client Full\n");
			return;
		}

		//클라이언트 접속 요청이 들어올 때까지 기다린다.
		pClientInfo->m_ClientSocket = accept(ListenSocket, (SOCKADDR*)&stClientAddr, &nAddrLen);
		if (INVALID_SOCKET == pClientInfo->m_ClientSocket)
		{
			continue;
		}

		//socket과 pClientInfo를 I/O CompletionPort객체와 연결시킨다.
		auto hIOCP = CreateIoCompletionPort((HANDLE)pClientInfo->m_ClientSocket, IocpHandle, (ULONG_PTR)(pClientInfo), 0);
		if (NULL == hIOCP || IocpHandle != hIOCP)
		{
			printf("[에러] CreateIoCompletionPort()함수 실패: %d\n", GetLastError());
			return;
		}

		//Recv Overlapped I/O작업을 요청해 놓는다.
		bool bRet = BindRecv(pClientInfo);
		if (false == bRet)
		{
			return;
		}

		char clientIP[32] = { 0, };
		inet_ntop(AF_INET, &(stClientAddr.sin_addr), clientIP, 32 - 1);
		printf("클라이언트 접속 : IP(%s) SOCKET(%d)\n", clientIP, (int)pClientInfo->m_ClientSocket);

		//클라이언트 갯수 증가
		++mClientCnt;
	}
}

int main()
{
	// WSA 초기화
	WSADATA wsaData;
	int Result = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (Result != 0)
	{
		wprintf(L"WSAStartup failed : %d\n", Result);
		return -1;
	}

	// 소켓 생성
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

	//클라이언트 정보 저장 구조체 초기화
	for (UINT32 i = 0; i < MAX_CLIENT; ++i)
	{
		mClientInfos.emplace_back();
	}

	//// IOCP 생성
	HANDLE IocpHandle = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);
	if (IocpHandle == NULL)
	{
		wprintf(L"CreateIoCompletionPort failed : %d\n", WSAGetLastError());
		return -1;
	}

	// 워커 스레드 생성
	std::vector<std::thread> WorkerThreads;
	for (int i = 0; i < 3; i++) // TODO 프로세서 * 2
	{
		WorkerThreads.emplace_back([]() { WorkThreadRun(); });
	}

	// Accept 스레드
	std::thread mAcceptThread = std::thread([]() { AcceptThreadRun(); });

	// IOCP 핸들 정리
	CloseHandle(IocpHandle);

	// 워커 스레드 정리
	for (auto& th : WorkerThreads)
	{
		if (th.joinable())
		{
			th.join();
		}
	}

	// Accept 스레드 정리		
	if (mAcceptThread.joinable())
	{
		mAcceptThread.join();
	}


	// WSA 정리
	WSACleanup();

	return 0;
}