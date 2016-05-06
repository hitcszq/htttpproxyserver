// Lab1_ProxyServer.cpp : Defines the entry point for the console application.
//
/*头文件---------------------------------------------*/
#include "stdafx.h"
#include <Windows.h>
#include <process.h>
#include <string.h>

#pragma comment(lib,"Ws2_32.lib")


/*宏定义---------------------------------------------*/
#define MAXSIZE 1024*1024*10//发送数据报文的最大长度,500KB
#define HTTP_PORT 80 //http 服务器端口


/*结构体定义-----------------------------------------*/
//Http 重要头部数据
struct HttpHeader{
	char method[4]; // POST 或者 GET，注意有些为 CONNECT，本实验暂不考虑
	char url[1024];  // 请求的 url
	char host[1024]; // 目标主机
	char cookie[1024 * 10]; //cookie
	HttpHeader(){
		ZeroMemory(this, sizeof(HttpHeader));
	}
};
struct ProxyParam{
	SOCKET clientSocket;
	SOCKET serverSocket;
};


/*函数声明----------------------------------------------*/
BOOL InitSocket();
void ParseHttpHead(char *buffer, HttpHeader * httpHeader);
BOOL ConnectToServer(SOCKET *serverSocket, char *host);
unsigned int __stdcall ProxyThread(LPVOID lpParameter);


/*全局变量声明-------------------------------------------*/
//代理相关参数
SOCKET ProxyServer;
sockaddr_in ProxyServerAddr;
const int ProxyPort = 10240;

//由于新的连接都使用新线程进行处理，对线程的频繁的创建和销毁特别浪费资源
//可以使用线程池技术提高服务器效率
//const int ProxyThreadMaxNum = 20;
//HANDLE ProxyThreadHandle[ProxyThreadMaxNum] = {0};
//DWORD ProxyThreadDW[ProxyThreadMaxNum] = {0};


int _tmain(int argc, _TCHAR* argv[])
{
	printf("代理服务器正在启动\n");
	printf("初始化...\n");
	if (!InitSocket()){
		printf("socket 初始化失败\n");
		return -1;
	}
	printf("代理服务器正在运行，监听端口 %d\n", ProxyPort);
	SOCKET acceptSocket = INVALID_SOCKET;
	ProxyParam *lpProxyParam;
	HANDLE hThread;
	DWORD dwThreadID;
	//代理服务器不断监听
	while (true){
		acceptSocket = accept(ProxyServer, NULL, NULL);
		lpProxyParam = new ProxyParam;
		if (lpProxyParam == NULL){
			continue;
		}
		lpProxyParam->clientSocket = acceptSocket;
		hThread = (HANDLE)_beginthreadex(NULL, 0, &ProxyThread, (LPVOID)lpProxyParam, 0, 0);
		CloseHandle(hThread);
		Sleep(200);
	}
	closesocket(ProxyServer);
	WSACleanup();
	return 0;
}

//************************************
// Method: InitSocket
// FullName: InitSocket
// Access: public
// Returns: BOOL
// Qualifier: 初始化套接字
//************************************
BOOL InitSocket(){
	//加载套接字库（必须）
	WORD wVersionRequested;
	WSADATA wsaData;
	//套接字加载时错误提示
	int err;
	//版本 2.2
	wVersionRequested = MAKEWORD(2, 2);
	//加载 dll 文件 Scoket 库
	err = WSAStartup(wVersionRequested, &wsaData);
	if (err != 0){
		//找不到 winsock.dll
		printf("加载 winsock 失败，错误代码为: %d\n", WSAGetLastError());
		return FALSE;
	}
	if (LOBYTE(wsaData.wVersion) != 2 || HIBYTE(wsaData.wVersion) != 2)
	{
		printf("不能找到正确的 winsock 版本\n");
		WSACleanup();
		return FALSE;
	}
	//创建TCP套接字
	ProxyServer = socket(AF_INET, SOCK_STREAM, 0);
	if (INVALID_SOCKET == ProxyServer){
		printf("创建套接字失败，错误代码为：%d\n", WSAGetLastError());
		return FALSE;
	}
	ProxyServerAddr.sin_family = AF_INET;
	ProxyServerAddr.sin_port = htons(ProxyPort);
	ProxyServerAddr.sin_addr.S_un.S_addr = INADDR_ANY;
	if (bind(ProxyServer, (SOCKADDR *)&ProxyServerAddr, sizeof(SOCKADDR)) == SOCKET_ERROR){
		printf("绑定套接字失败\n");
		return FALSE;
	}
	if (listen(ProxyServer, SOMAXCONN) == SOCKET_ERROR){
		printf("监听端口%d 失败", ProxyPort);
		return FALSE;
	}
	return TRUE;
}

//************************************
// Method: ProxyThread
// FullName: ProxyThread
// Access: public
// Returns: unsigned int __stdcall
// Qualifier: 线程执行函数
// Parameter: LPVOID lpParameter
//************************************
unsigned int __stdcall ProxyThread(LPVOID lpParameter){
	char *Buffer = new char[MAXSIZE];
	char *CacheBuffer;
	ZeroMemory(Buffer, MAXSIZE);
	SOCKADDR_IN clientAddr;
	int length = sizeof(SOCKADDR_IN);
	int recvSize;
	int ret;
	//从客户端接收http请求
	recvSize = recv(((ProxyParam *)lpParameter)->clientSocket, Buffer, MAXSIZE, 0);
	if (recvSize <= 0){
		printf("接收客户端请求数据出错！！！！\n");
		goto error;
	}
	/*
	1.先判断缓存是否命中
	1.1若没有命中则直接请求对象，获取对象后，查看cache-control字段
	1.1.1若可以缓存，则保存到本地（完整的报文），并发送回客户端
	1.1.2若不可以缓存，则直接发送回客户端
	1.2若缓存命中（是否过期：DATE+max-age与当前时间的对比）
	1.2.1若没有过期，直接从缓存中读取，并发回客户端
	1.2.2若过期，向服务器发送IF-MODIFIED-SINCE：LAST MODIFIED中的时间
	1.2.2.1若返回200，则更新缓存，并返回客户端
	1.2.2.2若返回304，则更新缓存，并返回客户端

	*/
	HttpHeader* httpHeader = new HttpHeader();
	CacheBuffer = new char[recvSize + 1];
	ZeroMemory(CacheBuffer, recvSize + 1);
	memcpy(CacheBuffer, Buffer, recvSize);
	ParseHttpHead(CacheBuffer, httpHeader);
	//delete CacheBuffer;
	if (!ConnectToServer(&((ProxyParam *)lpParameter)->serverSocket, httpHeader->host)) {
		printf("连接目标服务器%s出错!!!", httpHeader->host);
		goto error;
	}
	printf("代理连接主机 %s 成功\n", httpHeader->host);
	//将客户端发送的 HTTP 数据报文直接转发给目标服务器
	ret = send(((ProxyParam *)lpParameter)->serverSocket, CacheBuffer, strlen(Buffer), 0);
	//printf("向目标服务器发送：%s\nOVER...", Buffer);
	//等待目标服务器返回数据
	ZeroMemory(Buffer, MAXSIZE);
	recvSize = recv(((ProxyParam *)lpParameter)->serverSocket, Buffer, MAXSIZE, 0);
	if (recvSize <= 0){
		printf("接收数据出错！！！\n");
		goto error;
	}
	//printf("目标服务器返回：%s\nOVER...", Buffer);
	//将目标服务器返回的数据直接转发给客户端
	ret = send(((ProxyParam *)lpParameter)->clientSocket, Buffer, recvSize, 0);
	//错误处理
	goto success;
error:
	printf("代理失败并关闭套接字\n");
	//Sleep(2);
	closesocket(((ProxyParam*)lpParameter)->clientSocket);
	closesocket(((ProxyParam*)lpParameter)->serverSocket);
	delete lpParameter;
	delete Buffer;
	_endthreadex(0);
	return 0;
success:
	printf("代理成功并关闭套接字\n");
	//Sleep(2);
	closesocket(((ProxyParam*)lpParameter)->clientSocket);
	closesocket(((ProxyParam*)lpParameter)->serverSocket);
	delete lpParameter;
	delete Buffer;
	_endthreadex(0);
	return 0;
}

//************************************
// Method: ParseHttpHead
// FullName: ParseHttpHead
// Access: public
// Returns: void
// Qualifier: 解析 TCP 报文中的 HTTP 头部
// Parameter: char * buffer
// Parameter: HttpHeader * httpHeader
//************************************
void ParseHttpHead(char *buffer, HttpHeader * httpHeader){
	char *p;
	char *ptr;
	char *content;
	char *get_line;
	char *proxy_line;
	char *rbuf;//重新修正的请求报文
	//char *head[10];
	//int head_i = 0;
	//memcpy(head, 0, 10 * sizeof(char*));
	const char * delim = "\r\n";
	const char * delim_c = "\r\n\r\n";
	
	p = strtok_s(buffer, delim_c, &content);//分割首部和头部
	
	printf("\n%s\n", buffer);
	
	p = strtok_s(buffer, delim, &ptr);//提取第一行
	
	printf("%s\n", p);

	rbuf = new char[MAXSIZE];
	ZeroMemory(rbuf, MAXSIZE);


	if (p[0] == 'G'){//GET 方式
		memcpy(httpHeader->method, "GET", 3);
		memcpy(httpHeader->url, &p[4], strlen(p) - 13);
		//get_line = new char[15];//GET / HTTP/1.1
		//memcpy(get_line, "GET / HTTP/1.1", 14);
		memcpy(rbuf, "GET / HTTP/1.1", 14);
	}
	else if (p[0] == 'P'){//POST 方式
		memcpy(httpHeader->method, "POST", 4);
		memcpy(httpHeader->url, &p[5], strlen(p) - 14);
	}
	printf("%s\n", httpHeader->url);
	p = strtok_s(NULL, delim, &ptr);
	//strcat(rbuf, p);
	while (p){
		switch (p[0]){
		case 'H'://Host
			memcpy(httpHeader->host, &p[6], strlen(p) - 6);
			strcat(rbuf, p);
			break;
		case 'C'://Cookie
			strcat(rbuf, p);
			if (strlen(p) > 8){
				char header[8];
				ZeroMemory(header, sizeof(header));
				memcpy(header, p, 6);
				if (!strcmp(header, "Cookie")){
					memcpy(httpHeader->cookie, &p[8], strlen(p) - 8);
				}
			}
			break;
		case 'P':
			proxy_line = new char[23];//Connection: Keep-Alive
			memcpy(proxy_line, "Connection: Keep-Alive", 22);
			strcat(rbuf, proxy_line);
		default:
			strcat(rbuf, p);
			break;
		}
		p = strtok_s(NULL, delim, &ptr);
	}
	strcat(rbuf, content);//报文重构完成
	buffer = rbuf;
}
//************************************
// Method: ConnectToServer
// FullName: ConnectToServer
// Access: public
// Returns: BOOL
// Qualifier: 根据主机创建目标服务器套接字，并连接
// Parameter: SOCKET * serverSocket
// Parameter: char * host
//************************************
BOOL ConnectToServer(SOCKET *serverSocket, char *host){
	sockaddr_in serverAddr;
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_port = htons(HTTP_PORT);
	HOSTENT *hostent = gethostbyname(host);
	if (!hostent){
		printf("host_name_error	!!!");
		return FALSE;
	}
	in_addr Inaddr = *((in_addr*)*hostent->h_addr_list);
	serverAddr.sin_addr.s_addr = inet_addr(inet_ntoa(Inaddr));
	*serverSocket = socket(AF_INET, SOCK_STREAM, 0);
	if (*serverSocket == INVALID_SOCKET){
		printf("创建套接字失败！！！！");
		return FALSE;
	}
	if (connect(*serverSocket, (SOCKADDR *)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR){
		printf("connect error!!!!!");
		closesocket(*serverSocket);
		return FALSE;
	}
	return TRUE;
}