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
	ProxyParam *lpProxyParam;//代理服务器所需要的参数类型的指针，类型中包括发出请求的客户套接字，客户所请求的服务器端套接字
	HANDLE hThread;//线程句柄预定义；

}