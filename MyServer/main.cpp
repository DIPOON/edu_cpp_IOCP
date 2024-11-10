#include <stdio.h>
#include <thread>
#include <vector>
#include <WinSock2.h>

#pragma comment(lib, "ws2_32.lib")

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

bool Init(int ServerPort, int MaxClient)
{
	

	return true;
}

void WorkRun()
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

		

		// lpOverlapped �� overlapped �⺻ ��ü�ε�, m_eOperation ���� ���� �ʳ���..? �� ������ �����ϰ� �ȵǾ� ���� �� ������ �ű��ϰ� �ǳ׿�.
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

	// ������ ����
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
		WorkerThreads.emplace_back([]() { WorkRun(); });
	}

	// Accept ������
	mAcceptThread = std::thread([this]() { AcceptThreadRun(); });

	// ��Ŀ ������ ����
	for (auto& th : WorkerThreads)
	{
		if (th.joinable())
		{
			th.join();
		}
	}

	// WSA ����
	WSACleanup();

	return 0;
}