#include <winsock2.h>
#include <windows.h>
#include <iostream>
#include <thread>
#include <vector>

#pragma comment(lib, "Ws2_32.lib")

#define BUFFER_SIZE 1024

struct PER_IO_DATA {
    OVERLAPPED overlapped;
    char buffer[BUFFER_SIZE];
    WSABUF wsabuf;
    SOCKET socket;
};

HANDLE iocp;

void WorkerThread() {
    DWORD bytesTransferred;
    ULONG_PTR completionKey;
    LPOVERLAPPED overlapped;

    while (true) {
        // 대기 중인 완료된 I/O 작업 가져오기
        BOOL result = GetQueuedCompletionStatus(iocp, &bytesTransferred, &completionKey, &overlapped, INFINITE);

        if (!result) {
            std::cerr << "GetQueuedCompletionStatus failed with error: " << GetLastError() << std::endl;
            continue;
        }

        // 작업 처리 (수신한 데이터를 출력)
        PER_IO_DATA* ioData = reinterpret_cast<PER_IO_DATA*>(overlapped);
        std::cout << "Received data: " << ioData->buffer << std::endl;

        // 다시 비동기 I/O 요청 (재수신)
        DWORD flags = 0;
        ioData->wsabuf.len = BUFFER_SIZE;
        ioData->wsabuf.buf = ioData->buffer;
        WSARecv(ioData->socket, &ioData->wsabuf, 1, nullptr, &flags, &(ioData->overlapped), nullptr);
    }
}

int main() {
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);

    // 1. IOCP 생성
    iocp = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);

    // 2. 리슨 소켓 생성
    SOCKET listenSocket = socket(AF_INET, SOCK_STREAM, 0);
    sockaddr_in serverAddr = {};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(12345);
    serverAddr.sin_addr.s_addr = htonl(INADDR_ANY);
    bind(listenSocket, (sockaddr*)&serverAddr, sizeof(serverAddr));
    listen(listenSocket, SOMAXCONN);

    // 3. Worker Threads 생성 및 IOCP에 등록
    std::vector<std::thread> threads;
    for (int i = 0; i < 4; ++i) {
        threads.emplace_back(WorkerThread);
    }

    // 4. 클라이언트 접속 처리
    while (true) {
        SOCKET clientSocket = accept(listenSocket, NULL, NULL);
        if (clientSocket == INVALID_SOCKET) {
            std::cerr << "Accept failed with error: " << WSAGetLastError() << std::endl;
            continue;
        }

        // 클라이언트 소켓을 IOCP에 연결
        CreateIoCompletionPort((HANDLE)clientSocket, iocp, (ULONG_PTR)clientSocket, 0);

        // 5. 비동기 I/O 초기 요청 (데이터 수신)
        PER_IO_DATA* ioData = new PER_IO_DATA;
        ioData->socket = clientSocket;
        ioData->wsabuf.len = BUFFER_SIZE;
        ioData->wsabuf.buf = ioData->buffer;
        DWORD flags = 0;
        WSARecv(clientSocket, &ioData->wsabuf, 1, NULL, &flags, &(ioData->overlapped), NULL);
    }

    for (auto& thread : threads) {
        thread.join();
    }

    WSACleanup();
    return 0;
}