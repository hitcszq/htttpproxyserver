#include "stafx.h"
#include <stdio.h>
#include <Windows.h>
#include <process.h>
#include <string.h>

#pragma comment(lib,"Ws2_3.lib")

#define MAXSIZE 65507
#define HTTP_PORT 80

struct HttpHeader{
	char method[4];
	char url[1024];
	char cookie[1024 * 10];
	HttpHeader(){
		ZeroMemory(this, sizeof(HttpHeader));
	}
};
BOOL InitSocket();
void ParseHttpHead(char* buffer, HttpHeader* httpHeader);
BOOL ConnectToServer(SOCKET* serverSocket, char *host);
unsigned int __stdcall ProxyThread(LPVOID lpParameter);

SOCKET proxyServer;
sockaddr_in ProxyServerAddr;
const int ProxyPort = 10240;

const int ProxyThreadMaxNum = 20;
HANDLE ProxyThreadHandle[ProxyThreadMaxNum] = { 0 };
DWORD ProxyThreadDW[ProxyThreadMaxNum] = { 0 };

struct ProxyParam{
	SOCKET clientSocket;
	SOCKET serverSocket;
};

int _tmain(int argc, _TCHAR* argv[])
{
	if (!InitSocket()){
		printf("innit socket failed\n");
		return -1;
	}
	printf("proxyserver is listening %d \n", ProxyPort);
	ProxyParam *lpProxyParam;//�������������Ҫ�Ĳ������͵�ָ�룬�����а�����������Ŀͻ��׽��֣��ͻ�������ķ��������׽���
	HANDLE hThread;//�߳̾��Ԥ���壻
	DWORD dwThreadID;
	while (true){//�������������
		acceptSocket = accept(proxyServer, NULL, NULL);
		lpProxyParam = new ProxyParam;
		if (NULL == lpProxyParam)
		{
			continue;
		}
		lpProxyParam->clientSocket = acceptSocket;
		hThread = (HANDLE)_beginthreadex(NULL, 0, &ProxyThread, (LPVOID)lpProxyParam, 0, 0);
		CloseHandle(hThread);
		sleep(200);
	}
	closesocket(ProxyServer);
	WSACleanup();
	return 0;
}
BOOL InitSocket()
{
	WORD wVersionRequested;
	WSADATA wsaData;
	int err;
	wVersionRequested = MAKEWORD(2, 2);
	err = WSAStartup(wVersionRequested, &wsaData);//�����׽��ֶ�̬���ӿ�
	if (err != 0)
	{
		printf("fail to load dll winsokcet,error num: %d",WSAGetLastError());
		return FALSE;
	}
	if (LOBYTE(wsaData.wVersion) != 2 || HIBYTE(wsaData.wVersion) != 2)
	{
		printf("can not find the right winsock version");
		WSACleanup();
		return FALSE;
	}
	ProxyServer = socket(AF_INET, SOCK_STREAM, 0);//����tcp�����׽���
	if (INVALID_SOCKET == proxyServer)
	{
		printf("create socket failed,error num:%d", WSAGetLastError());
		return FALSE;
	}
	ProxyServerAddr.sin_family = AF_INET;
	ProxyServerAddr.sin_port = htons(ProxyPort);
	ProxyServerAddr.sin_addr.S_un.S_addr=

}