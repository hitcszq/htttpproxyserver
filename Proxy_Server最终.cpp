// Lab1_ProxyServer.cpp : Defines the entry point for the console application.
//
/*头文件---------------------------------------------*/
#include "stdafx.h"
#include <Windows.h>
#include <process.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#define IN 
#define OUT
#pragma comment(lib,"Ws2_32.lib")
const int max_f = 100;
const int max_fishing = 50;
static char* f_url[max_f];
static int fobidden_flag = 0;
static int fishing_flag = 0;
static long age = 604800;//一周
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
	char date[1024];
	HttpHeader(){
		ZeroMemory(this, sizeof(HttpHeader));
	}
};
struct ProxyParam{
	SOCKET clientSocket;
	SOCKET serverSocket;
};
struct fishing{
	char init_host[50];
	char fishing_host[50];
	char fishing_head[500];
};


/*函数声明----------------------------------------------*/
BOOL InitSocket();
void ParseHttpHead(char *buffer, HttpHeader * httpHeader);
BOOL ConnectToServer(SOCKET *serverSocket, char *host);
unsigned int __stdcall ProxyThread(LPVOID lpParameter);
int checkinf(char *host);
void read_void(char path[]);
void read_fishing(char path[]);
int checkfishing(char* host);
char* makechar(char* name);
int checknum(char* buffer);
int checkoutofdate(FILE* fp);
void addifsincemodified(char* buffer,HttpHeader* httpHeader);
time_t timeconvert(IN char *buf, OUT struct tm *p);
int monthcmp(IN char *p);
int weekcmp(IN char *p);

/*全局变量声明-------------------------------------------*/
//代理相关参数
SOCKET ProxyServer;
sockaddr_in ProxyServerAddr;
const int ProxyPort = 10240;
struct fishing fishing_list[max_fishing];

//由于新的连接都使用新线程进行处理，对线程的频繁的创建和销毁特别浪费资源
//可以使用线程池技术提高服务器效率
//const int ProxyThreadMaxNum = 20;
//HANDLE ProxyThreadHandle[ProxyThreadMaxNum] = {0};
//DWORD ProxyThreadDW[ProxyThreadMaxNum] = {0};

//CRITICAL_SECTION g_cs;
int _tmain(int argc, _TCHAR* argv[])
{
	//InitializeCriticalSection(&g_cs);   //
	if (fobidden_flag == 1)
	{
		read_void("fobidden.txt");
	}
	if (fishing_flag == 1)
	{
		read_fishing("fishing.txt");
	}
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
		//Sleep(20);
	}
	//DeleteCriticalSection(&g_cs);
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
	if (listen(ProxyServer, 3000) == SOCKET_ERROR){
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
	//EnterCriticalSection(&g_cs);
	char *Buffer;
	char *CacheBuffer;
	char *CacheBuffer2;
	Buffer = new char[MAXSIZE];
	ZeroMemory(Buffer, MAXSIZE);
	SOCKADDR_IN clientAddr;
	int length = sizeof(SOCKADDR_IN);
	int recvSize;
	int ret;
	char ch;
	int index;
	char *cache_dir;
	cache_dir = new char[100];
	memset(cache_dir, 0, 100);
	//从客户端接收http请求
	recvSize = recv(((ProxyParam *)lpParameter)->clientSocket, Buffer, MAXSIZE, 0);
	if (recvSize <= 0){
		printf("接受客户端请求出错！！！\n");
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
	1.2.2.2若返回304，返回客户端
	*/
	HttpHeader* httpHeader = new HttpHeader();
	HttpHeader* httpHeader_fishing = new HttpHeader();
	CacheBuffer = new char[recvSize + 1];
	ZeroMemory(CacheBuffer, recvSize + 1);
	memcpy(CacheBuffer, Buffer, recvSize);
	ParseHttpHead(CacheBuffer, httpHeader);
	if (strcmp(httpHeader->host, "masterconn.qq.com") == 0 || strcmp(httpHeader->host, "wup.browser.qq.com") == 0 || strcmp(httpHeader->host, "wup.browser.qq.com:443") == 0 || strcmp(httpHeader->host, "qbwup.imtt.qq.com") == 0 || strcmp(httpHeader->host, "plugin.browser.qq.com") == 0)
		goto liulanqi;
	
	if (fobidden_flag == 1 && checkinf(httpHeader->host) == 1)
	{
		return FALSE;
		goto success;
	}
	if (fishing_flag == 1 && (index = checkfishing(httpHeader->host)) != -1)
	{
		ZeroMemory(Buffer, MAXSIZE);
		memcpy(Buffer, fishing_list[index].fishing_head, strlen(fishing_list[index].fishing_head));
		CacheBuffer2 = new char[strlen(Buffer)];
		memset(CacheBuffer2, 0, strlen(Buffer));
		memcpy(CacheBuffer2, Buffer, strlen(Buffer));
		ParseHttpHead(CacheBuffer2, httpHeader_fishing);
	}
	printf("\n%s\n", Buffer);
	if (fishing_flag==1 && index!=-1)
		cache_dir = makechar(httpHeader_fishing->url);
	else
		cache_dir=makechar(httpHeader->url);
	FILE *fp = fopen(cache_dir, "rb");//cache中该缓存是否存在
	int bf = 0;
	if (fp != NULL && checkoutofdate(fp)==0)//缓存命中且没有过期
	{
		ZeroMemory(Buffer, MAXSIZE);
		/*while (!feof(fp))//读取本地缓存的报文
		{
			ch = fgetc(fp);
			if (ch == '\n')
			{
				Buffer[bf++] = '\r';
				Buffer[bf++] = '\n';
			}//
			else
				Buffer[bf++] = ch;
		}*/
		fread(Buffer, sizeof(char), MAXSIZE, fp);
		printf("缓存命中%s\n",cache_dir);
		printf("%d", strlen(Buffer));
		ret = send(((ProxyParam *)lpParameter)->clientSocket, Buffer,MAXSIZE, 0);//将缓存的报文回传
		fclose(fp);
		goto success;//结束该线程
	}
	else if (checkoutofdate(fp) == 1)//过期
	{
		addifsincemodified(Buffer,httpHeader);
	}
	//如果没有命中缓存
	
	if (!ConnectToServer(&((ProxyParam *)lpParameter)->serverSocket, httpHeader->host)) {
		printf("连接服务器出错！！！\n");
		goto error;
	}
	printf("代理连接主机%s成功\n", httpHeader->host);
	//将客户端发送的 HTTP 数据报文直接转发给目标服务器
	printf("\n%s\n", Buffer);
	ret = send(((ProxyParam *)lpParameter)->serverSocket, Buffer, strlen(Buffer), 0);



	//等待目标服务器返回数据
	ZeroMemory(Buffer, MAXSIZE);
	recvSize = recv(((ProxyParam *)lpParameter)->serverSocket, Buffer, MAXSIZE, 0);

	if (recvSize <= 0){
		printf("接受服务器返回错误！！！\n");
		goto error;
	}
	
	
	//将目标服务器返回的数据直接转发给客户端
	ret = send(((ProxyParam *)lpParameter)->clientSocket, Buffer, strlen(Buffer), 0);
	
	
	//将数据存入本地作为缓存
	if (checknum(Buffer) == 200)
	{
		FILE *fm = fopen(cache_dir, "wb");//缓存文件，以URL作为文件名
		if (fm != NULL)
		{
			//fprintf(fm, "%s", Buffer);
			//printf("%s", Buffer);
			/*int i = 0;
			while (Buffer[i] != 0)
			{
				fputc(Buffer[i], fm);
				i++;
			}*/
			fwrite(Buffer, sizeof(char), strlen(Buffer), fm);
			fclose(fm);
		}
	}
/*	if (strcmp(cache_dir, "1994") == 0)
	{
		FILE *fm = fopen("hehehe", "w");
		fprintf(fm, "%s", Buffer);
	}*/
	else{
		printf("缓存失败！！！！！\n");
	}
	goto success;
error:
	printf("代理失败关闭套接字\n");
	closesocket(((ProxyParam*)lpParameter)->clientSocket);
	closesocket(((ProxyParam*)lpParameter)->serverSocket);
	delete lpParameter;
	delete Buffer;
	_endthreadex(0);
	return 0;
success:
	printf("代理成功关闭套接字\n");
	closesocket(((ProxyParam*)lpParameter)->clientSocket);
	closesocket(((ProxyParam*)lpParameter)->serverSocket);
	delete lpParameter;
	delete Buffer;
	_endthreadex(0);
	return 0;
liulanqi:
	printf("浏览器套接字，直接关闭\n");
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
	const char * delim = "\r\n";
	p = strtok_s(buffer, delim, &ptr);//提取第一行
	printf("%s\n", p);
	if (p[0] == 'G'){//GET 方式
		memcpy(httpHeader->method, "GET", 3);
		memcpy(httpHeader->url, &p[4], strlen(p) - 13);
	}
	else if (p[0] == 'P'){//POST 方式
		memcpy(httpHeader->method, "POST", 4);
		memcpy(httpHeader->url, &p[5], strlen(p) - 14);
	}
	printf("%s\n", httpHeader->url);
	p = strtok_s(NULL, delim, &ptr);
	while (p){
		switch (p[0]){
		case 'H'://Host
			memcpy(httpHeader->host, &p[6], strlen(p) - 6);
			break;
		case 'C'://Cookie
			if (strlen(p) > 8){
				char header[8];
				ZeroMemory(header, sizeof(header));
				memcpy(header, p, 6);
				if (!strcmp(header, "Cookie")){
					memcpy(httpHeader->cookie, &p[8], strlen(p) - 8);
				}
			}
			break;
		default:
			break;
		}
		p = strtok_s(NULL, delim, &ptr);
	}
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
	printf("%s", host);
	if (fobidden_flag == 1 && checkinf(host) == 1)
	{
		return FALSE;
	}
	HOSTENT *hostent = gethostbyname(host);
	if (!hostent){
		printf("域名出错!!!");
		return FALSE;
	}
	in_addr Inaddr = *((in_addr*)*hostent->h_addr_list);
	serverAddr.sin_addr.s_addr = inet_addr(inet_ntoa(Inaddr));
	*serverSocket = socket(AF_INET, SOCK_STREAM, 0);
	if (*serverSocket == INVALID_SOCKET){
		printf("建立套接字出错!!!");
		return FALSE;
	}
	if (connect(*serverSocket, (SOCKADDR *)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR){
		printf("连接目标服务器出错!!!");
		closesocket(*serverSocket);
		return FALSE;
	}
	return TRUE;
}
int checkfishing(char* host)
{
	int equal = 1;
	for (int i = 0; i < max_fishing; i++)
	{
		if (strcmp(host, fishing_list[i].init_host) == 0)
		{
			memcpy(host, fishing_list[i].fishing_host, 50);//钓鱼
			return i;
		}
		/*int j = 0;
		equal = 1;
		while (host[j] != 0 || fishing_list[i].init_host[j] != 0)
		{
			if (host[j] != fishing_list[i].init_host[j])
			{
				equal = 0;
				break;
			}
		}
		if (equal == 1)
		{
			return i;
		}*/
	}
	return -1;
}
int checkinf(char* host)
{
	int i = 0;
	while (f_url[i] != NULL)
	{
		if (strcmp(f_url[i], host)==0)
			return 1;
		i++;
	}
	return 0;
}
void read_void(char path[])
{
	for (int i = 0; i < max_f; i++)
	{
		f_url[i] = NULL;
	}
	int i = 0;
	FILE *fp = fopen(path, "r");
	char Strurl[1024] = { 0 };
	if (fp == NULL)
	{
		printf("打开文件失败\n");
	}
	while (!feof(fp))
	{
		fgets(Strurl, 1024, fp);
		f_url[i] = new char[strlen(Strurl) + 1];
		ZeroMemory(f_url[i], strlen(Strurl) + 1);
		memcpy(f_url[i], Strurl,strlen(Strurl));
		printf("%s", Strurl);
		printf("%s", f_url[i]);
		i++;
	}
	fclose(fp);
}
void read_fishing(char path[])
{
	char ch;
	char fishing_host[1024] = { 0 };
	char fishing_head[1024] = { 0 }; 
	char init_host[1024] = { 0 };
	for (int i = 0; i < max_fishing; i++)
	{
		memset(fishing_list[i].fishing_head, 0, 500);
		memset(fishing_list[i].fishing_host, 0, 50);
		memset(fishing_list[i].init_host, 0, 50);
	}
	int j = 0;
	FILE *fp = fopen(path, "r");
	while (!feof(fp))
	{
		fgets(fishing_host, 1024, fp);
		printf("%s\n", fishing_host);
		memcpy(fishing_list[j].fishing_host, fishing_host, strlen(fishing_host)-1);
		printf("%s\n", init_host);
		fgets(init_host, 1024, fp);
		memcpy(fishing_list[j].init_host, init_host, strlen(init_host)-1);
		int k = 0;
		while ((ch = fgetc(fp)) != '!'){//请求报文以！结束
			if (ch == '\n')
			{
				fishing_head[k++] = '\r';
				fishing_head[k++] = '\n';
			}
			else
			fishing_head[k++] = ch;
		}
		memcpy(fishing_list[j].fishing_head, fishing_head, 500);
		if (ch == '!')
			break;

		j++;
	}
}
char* makechar(char* name)
{
	char* r_name = new char[100];
	memset(r_name, 0, 100);
	int i = 0;
	int i_name = 0;
	while (name[i] != NULL)
	{
		i_name = i_name + name[i];
		i++;
	}
	itoa(i_name, r_name, 10);
	return r_name;
}
/*int client_ret = 1, server_ret = 1;
char ch[2];
do{
	server_ret = recv(((ProxyParam *)lpParameter)->serverSocket, ch, 1, 0);
	client_ret = send(((ProxyParam *)lpParameter)->clientSocket, ch, server_ret, 0);
	//printf("recv: %d, send: %d\n", server_ret, client_ret);
} while (client_ret >= 1 && server_ret >= 1);
*/
int checknum(char* buffer)
{
	char *p;
	char *ptr;
	const char * delim = "\r\n";
	char num[10] = { 0 }; return 200;
	p = strtok_s(buffer, delim, &ptr);//提取第一行
	memcpy(num, &p[9], 3);
	if (strcmp(num, "304") == 0)
		return 304;
	return 200;
}
int checkoutofdate(FILE* fp){
	char date[40] = { 0 };
	long second=0;
	long nowt = 0;
	struct tm *nowtime = (struct tm*)malloc(sizeof(struct tm));
	struct tm *time1 = (struct tm*)malloc(sizeof(struct tm));
	if (fp == NULL || 1)
	{
		return 0;
	}
	else
	{
		char strline[200] = { 0 };
		while (fp != NULL)
		{
			fgets(strline, 1024, fp);
			if (strline[0] == 'D'&&strline[1] == 'a'&&strline[2] == 't'&&strline[3] == 'e')
			{
				memcpy(date, &strline[6], 25);
				timeconvert(date, time1);
				second = mktime(time1);
				break;
			}
		}
		if (nowt - second < age)
			return 0;
		else
			return 1;
	}
}
void addifsincemodified(char* buffer,HttpHeader* httpHeader){
	char add[1024] = { 0 };
	memcpy(add, "if modified since",20);
	strcat(add, httpHeader->date);
	strcat(buffer, add);
}

//比较周数，成功返回0-6的数，错误返回7
//p代表周数，取周数前3个字母，如Mon代表周1，以此类推
//改动周几不影响返回的时间值，可以通过改动日期的日数来达到修改时间
int weekcmp(IN char *p)
{
	char week[7][5] = { "Sun\0", "Mon\0", "Tue\0", "Wed\0", "Thu\0", "Fri\0", "Sat\0" };
	int i;


	for (i = 0; i<7; i++)
	if (strcmp(p, week[i]) == 0)
		break;

	if (i == 7)
	{
		printf("fail to find week.\n");
		return i;
	}
	return i;
}
//比较月份，成功返回0-11的数，错误返回12
//P 为月份的前三个字母，如Feb代表二月，以此类推
int monthcmp(IN char *p)
{
	char month[13][5] = { "Jan\0", "Feb\0", "Mar\0", "Apr\0", "May\0", "Jun\0", "Jul\0", "Aug\0", "Sep\0", "Oct\0", "Nov\0", "Dec\0" };

	int i;
	for (i = 0; i<12; i++)
	if (strcmp(p, month[i]) == 0)
		break;
	if (i == 12)
	{
		printf("fail to find month.\n");
		return i;
	}
	return i;
}
//将字串格式的时间转换为结构体,返回距离1970年1月1日0：0：0的秒数，当字符串格式错误或超值时返回0
//BUF 为类似Tue May 15 14:46:02 2007格式的，p为时间结构体
time_t timeconvert(IN char *buf, OUT struct tm *p)
{

	char cweek[4];
	char cmonth[4];
	time_t second;

	sscanf(buf, "%s %s %d %d:%d:%d %d", cweek, cmonth, &(p->tm_mday), &(p->tm_hour), &(p->tm_min), &(p->tm_sec), &(p->tm_year));
	p->tm_year -= 1900;
	printf("****%s,%s*****\n", cweek, cmonth);
	p->tm_mon = monthcmp(cmonth);
	//改动周几不影响返回的时间值，可以通过改动日期的日数来达到修改时间
	p->tm_wday = weekcmp(cweek);
	if (p->tm_mon == 12 && p->tm_wday == 7)
	{
		printf("monthcmp() or weekcmp() fail to use.\n");
		return 0;
	}
	return second = mktime(p);
}
