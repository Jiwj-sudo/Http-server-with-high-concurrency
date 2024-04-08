#include "HttpRequest.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <assert.h>
#include <sys/stat.h>
#define HeaderSize 12

struct HttpRequest* httpRequestInit()
{
	struct HttpRequest* request = (struct HttpRequest*)malloc(sizeof(struct HttpRequest));
	httpRequestReset(request);
	request->reqHeaders = (struct RequestHeader*)malloc(sizeof(struct RequestHeader) * HeaderSize);
	return request;
}

void httpRequestReset(struct HttpRequest* req)
{
	req->curState = ParseReqLine;
	req->method = NULL;
	req->url = NULL;
	req->version = NULL;
	req->reqHeadersNum = 0;
}

void httpRequestResetEX(struct HttpRequest* req)
{
	free(req->method);
	free(req->url);
	free(req->version);
	if (req->reqHeaders != NULL)
	{
		for (int i = 0; i < req->reqHeadersNum; i++)
		{
			free(req->reqHeaders[i].key);
			free(req->reqHeaders[i].value);
		}
		free(req->reqHeaders);
	}
	httpRequestReset(req);
}

void httpRequestDestory(struct HttpRequest* req)
{
	if (req != NULL)
	{
		httpRequestResetEX(req);
		free(req);
	}
}

enum HttpRequestState httpRequestState(struct HttpRequest* req)
{
	return req->curState;
}

void httpRequestAddHeader(struct HttpRequest* req, const char* key, const char* value)
{
	/*if (req->reqHeaders[req->reqHeadersNum].key == NULL)
	{
		req->reqHeaders[req->reqHeadersNum].key = (char*)malloc(sizeof(strlen(key) + 1));
	}
	if (req->reqHeaders[req->reqHeadersNum].value == NULL)
	{
		req->reqHeaders[req->reqHeadersNum].value = (char*)malloc(sizeof(strlen(value) + 1));
	}
	strncpy(req->reqHeaders[req->reqHeadersNum].key, key, strlen(key));
	strncpy(req->reqHeaders[req->reqHeadersNum].value, value, strlen(value));*/
	req->reqHeaders[req->reqHeadersNum].key = key;
	req->reqHeaders[req->reqHeadersNum].value = value;
	req->reqHeadersNum++;
}

char* httpRequestGetHeader(struct HttpRequest* req, const char* key)
{
	if (req != NULL)
	{
		for (int i = 0; i < req->reqHeadersNum; i++)
		{
			if (strncasecmp(req->reqHeaders[i].key, key, strlen(key)) == 0)
			{
				return req->reqHeaders[i].value;
			}
		}
	}
	return NULL;
}

char* splitRequestLine(const char* start, const char* end, char* sub, char** ptr)
{
	char* space = end;
	if (sub != NULL)
	{
		space = memmem(start, end - start, sub, strlen(sub));
		assert(space != NULL);
	}
	int length = space - start;
	char* tmp = (char*)malloc(length + 1);
	strncpy(tmp, start, length);
	tmp[length] = '\0';
	*ptr = tmp;

	return space + 1;
}

bool parseHttpRequestLine(struct HttpRequest* req, struct Buffer* readBuf)
{
	// 读出请求行, 保存字符串结束地址
	char* end = bufferFindCRLF(readBuf);
	// 保存字符串起始地址
	char* start = readBuf->data + readBuf->readPos;
	// 请求行总长度
	int lineSize = end - start;

	if (lineSize)
	{
		start = splitRequestLine(start, end, " ", &req->method);
		start = splitRequestLine(start, end, " ", &req->url);
		splitRequestLine(start, end, NULL, &req->version);
#if 0
		// get  /xxx/xxx/xxx.txt http/1.1
		// 请求方式
		char* space = memmem(start, lineSize, " ", 1);
		assert(space != NULL);
		int methodSize = space - start;
		req->method = (char*)malloc(methodSize + 1);
		strncpy(req->method, start, methodSize);
		req->method[methodSize] = '\0';

		// 请求静态资源
		start = space + 1;
		space = memmem(start, end - start, " ", 1);
		assert(space != NULL);
		int urlSize = space - start;
		req->url = (char*)malloc(urlSize + 1);
		strncpy(req->url, start, urlSize);
		req->url[urlSize] = '\0';

		// 请求http版本
		start = space + 1;
		req->version = (char*)malloc(end - start + 1);
		strncpy(req->version, start, end - start);
		req->version[end - start] = '\0';
#endif

		// 为解析请求头做准备
		readBuf->readPos += lineSize;
		readBuf->readPos += 2;
		// 修改状态
		req->curState = ParseReqHeader;
		return true;
	}
	return false;
}

// 该函数处理请求头中的一行
bool parseHttpRequestHeader(struct HttpRequest* req, struct Buffer* readBuf)
{
	char* end = bufferFindCRLF(readBuf);
	if (end != NULL)
	{
		char* start = readBuf->data + readBuf->readPos;
		int lineSize = end - start;
		// 基于 ：搜索字符串
		char* middle = memmem(start, lineSize, ": ", 2);
		if (middle != NULL)
		{
			char* key = (char*)malloc(middle - start + 1);
			strncpy(key, start, middle - start);
			key[middle - start] = '\0';

			char* value = (char*)malloc(end - middle - 2 + 1);
			strncpy(value, middle + 2, end - middle - 2);
			key[end - middle - 2] = '\0';

			httpRequestAddHeader(req, key, value);

			readBuf->readPos += lineSize;
			readBuf->readPos += 2;
		}
		else
		{
			// 请求头被解析完了, 跳过空行
			readBuf->readPos += 2;
			// 修改解析状态
			// 忽略 post 请求
			req->curState = ParseReqDone;
		}
		return true;
	}
	return false;
}

bool parseHttpRequest(struct HttpRequest* req, struct Buffer* readBuf)
{
	bool flag = true;
	while (req->curState != ParseReqDone)
	{
		switch (req->curState)
		{
		case ParseReqLine:
			flag = parseHttpRequestLine(req, readBuf);
			break;
		case ParseReqHeader:
			flag = parseHttpRequestHeader(req, readBuf);
			break;
		case ParseReqBody:
			break;
		default:
			break;
		}
		if (flag == false)
		{
			return flag;
		}
		// 如果解析完毕了，需要准备回复数据
		if (req->curState == ParseReqDone)
		{
			// 1.根据解析出的原始数据，对客户端的请求做出处理
			// 2.组织响应数据并发送给客户端
		}
	}
	req->curState = ParseReqLine;   // 状态还原，保证还能继续处理第二条以及后续的请求
	return flag;
}

// 基于GET的http请求
bool processHttpRequest(struct HttpRequest* req)
{
	if (strcasecmp(req->method, "get") != 0)
	{
		return false;
	}

	decodeMsg(req->url, req->url);

	// 处理客户端请求的静态资源(目录或文件)
	char* file = NULL;
	if (strcmp(req->url, "/") == 0)
	{
		file = "./";
	}
	else
	{
		file = req->url + 1;
	}

	// 获取文件属性
	struct stat st;
	int ret = stat(file, &st);
	if (-1 == ret)
	{
		// 文件不存在 -- 回复404
		// sendHeadMsg(cfd, 404, "Not Found", getFileType(".html"), -1);
		// sendFile("404.html", cfd);
		return 0;
	}
	// 判断文件类型
	if (S_ISDIR(st.st_mode))
	{
		// 把这个目录返回给客户端
		// sendHeadMsg(cfd, 200, "OK", getFileType(".html"), -1);
		// sendDir(file, cfd);
	}
	else
	{
		// 把文件的内容发送给客户端
		// sendHeadMsg(cfd, 200, "OK", getFileType(file), st.st_size);
		// sendFile(file, cfd);
	}

	return false;
}

// 把字符转换为整形
int hexToDec(char c)
{
	if (c >= '0' && c <= '9')
		return c - '0';
	if (c >= 'a' && c <= 'f')
		return c - 'a' + 10;
	if (c >= 'A' && c <= 'F')
		return c - 'A' + 10;

	return 0;
}

// 解码
// to 存储解码之后的数据
void decodeMsg(char* to, char* from)
{
	for (; *from != '\0'; ++to, ++from)
	{
		if (from[0] == '%' && isxdigit(from[1]) && isxdigit(from[2]))
		{
			*to = hexToDec(from[1]) * 16 + hexToDec(from[2]);
			from += 2;
		}
		else
		{
			*to = *from;
		}

	}
	*to = '\0';
}
