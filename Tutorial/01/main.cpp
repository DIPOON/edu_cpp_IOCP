#include "IOCompletionPort.h"

const INT16 SERVER_PORT = 32452;

int main()
{
	IOCompletionPort ioCompletionPort;

	//������ �ʱ�ȭ
	ioCompletionPort.InitSocket();

	//���ϰ� ���� �ּҸ� �����ϰ� ��� ��Ų��.
	ioCompletionPort.BindandListen(SERVER_PORT);

	ioCompletionPort.StartServer();

	printf("�ƹ� Ű�� ���� ������ ����մϴ�\n");
	getchar();
	ioCompletionPort.DestroyThread();
	return 0;
}

