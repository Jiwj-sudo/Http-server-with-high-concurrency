#pragma once
#include "Buffer.h"
#include "HttpResponse.h"
#include <stdbool.h>

// 请求头键值对
struct RequestHeader
{
	char* key;
	char* value;
};

// 当前的解析状态
enum HttpRequestState
{
	ParseReqLine,
	ParseReqHeader,
	ParseReqBody,
	ParseReqDone
};

// 定义http请求结构体
struct HttpRequest
{
	char* method;
	char* url;
	char* version;

	struct RequestHeader* reqHeaders;
	int reqHeadersNum;

	enum HttpRequestState curState;
};

// 初始化
struct HttpRequest* httpRequestInit();
// 重置
void httpRequestReset(struct HttpRequest* req);
void httpRequestResetEX(struct HttpRequest* req);
void httpRequestDestory(struct HttpRequest* req);
// 获取处理状态
enum HttpRequestState httpRequestState(struct HttpRequest* req);
// 添加请求头
void httpRequestAddHeader(struct HttpRequest* req, const char* key, const char* value);
// 根据key得到请求头的value
char* httpRequestGetHeader(struct HttpRequest* req, const char* key);
// 解析请求行
char* splitRequestLine(const char* start, const char* end, char* sub, char** ptr);
bool parseHttpRequestLine(struct HttpRequest* req, struct Buffer* readBuf);
// 解析请求头
bool parseHttpRequestHeader(struct HttpRequest* req, struct Buffer* readBuf);
// 解析http请求
bool parseHttpRequest(struct HttpRequest* req, struct Buffer* readBuf,
	struct HttpResponse* response, struct Buffer* sendBuf, int socket);
// 处理http请求协议
bool processHttpRequest(struct HttpRequest* req, struct HttpResponse* response);

// 解码
int hexToDec(char c);
void decodeMsg(char* to, char* from);
const char* getFileType(const char* name);
int sendFile(const char* fileName, struct Buffer* sendBuf, int cfd);
void sendDir(const char* dirName, struct Buffer* sendBuf, int cfd);