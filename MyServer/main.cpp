#include <stdio.h>
#include <thread>
#include <vector>
#include <WinSock2.h>

#pragma comment(lib, "ws2_32.lib")

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

bool Init(int ServerPort, int MaxClient)
{
	

	return true;
}

void WorkRun()
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

		

		// lpOverlapped 는 overlapped 기본 객체인데, m_eOperation 등이 없지 않나요..? 값 세팅이 적절하게 안되어 있을 것 같은데 신기하게 되네요.
		stOverlappedEx* pOverlappedEx = (stOverlappedEx*)lpOverlapped;
		if (pOverlappedEx->m_eOperation == IOOperation::RECV)
		{
			int result2 = WSARecv(ConnSocket, &DataBuf, 1, &RecvBytes, &Flags, &RecvOverlapped, NULL);
		    if (result2 != 0 || result2 != WSA_IO_PENDING)
		    {
		        wprintf(L"WSARecv failed : %d\n", WSAGetLastError());
		    }
		}
	}

	// 접속을 받음
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
	SOCKET ListenSocket = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, NULL, WSA_FLAG_OVERLAPPED);
	if (ListenSocket == INVALID_SOCKET)
	{
		wprintf(L"socket failed : %d\n", WSAGetLastError());
		return -1;
	}

	// Set up the sockaddr structure
	SOCKADDR_IN saServer;
	saServer.sin_family = AF_INET;
	saServer.sin_addr.s_addr = htonl(INADDR_ANY);
	saServer.sin_port = htons(ServerPort);

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
		WorkerThreads.emplace_back([]() { WorkRun(); });
	}

	// Accept 스레드
	mAcceptThread = std::thread([this]() { AcceptThreadRun(); });

	// 워커 스레드 정리
	for (auto& th : WorkerThreads)
	{
		if (th.joinable())
		{
			th.join();
		}
	}

	// WSA 정리
	WSACleanup();

	return 0;
}