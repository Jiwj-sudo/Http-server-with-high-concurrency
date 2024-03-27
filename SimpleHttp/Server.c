#include "Server.h"
#include <arpa/inet.h>
#include <sys/epoll.h>
#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <sys/sendfile.h>
#include <assert.h>

int initListenFd(unsigned short port)
{
	// 1.创建监听的fd
	int lfd = socket(AF_INET, SOCK_STREAM, 0);
	if (-1 == lfd)
	{
		perror("socket");
		return -1;
	}

	// 2.设置端口复用
	int opt = 1;
	int ret = setsockopt(lfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
	if (-1 == ret)
	{
		perror("setsockopt");
		return -1;
	}

	// 3.绑定
	struct sockaddr_in addr;
	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);
	addr.sin_addr.s_addr = INADDR_ANY;
	ret = bind(lfd, (struct scokaddr*)&addr, sizeof(addr));
	if (-1 == ret)
	{
		perror("bind");
		return -1;
	}

	// 4.设置监听
	ret = listen(lfd, 128);
	if (-1 == ret)
	{
		perror("listen");
		return -1;
	}
	// 5.返回fd
	return lfd;
}

// lfd为监听套接字
int epollRun(int lfd)
{
	// 1.创建epoll实例
	int epfd = epoll_create(1);
	if (-1 == epfd)
	{
		perror("epoll_create");
		return -1;
	}

	// 2.lfd 上树
	struct epoll_event ev;
	ev.data.fd = lfd;
	ev.events = EPOLLIN;
	int ret = epoll_ctl(epfd, EPOLL_CTL_ADD, lfd, &ev);
	if (-1 == ret)
	{
		perror("epoll_ctl");
		return -1;
	}

	// 3.检测
	struct epoll_event evs[1024];
	int size = sizeof(evs) / sizeof(struct epoll_event);
	while (1)
	{
		int num = epoll_wait(epfd, evs, size, -1);
		for (int i = 0; i < num; i++)
		{
			int fd = evs[i].data.fd;
			if (fd == lfd)
			{
				// 建立新连接 accept
				acceptClient(lfd, epfd);
			}
			else
			{
				// 主要是接收对端的数据
				recvHttpRequest(fd, epfd);
			}
		}
	}
	return 0;
}

int acceptClient(int lfd, int epfd)
{
	// 1.建立连接
	int cfd = accept(lfd, NULL, NULL);
	if (-1 == cfd)
	{
		perror("accept");
		return -1;
	}

	// 2.设置非阻塞
	int flag = fcntl(cfd, F_GETFL);
	flag |= O_NONBLOCK;
	fcntl(cfd, F_SETFL, flag);

	// 3.cfd添加到epoll模型里面
	struct epoll_event ev;
	ev.data.fd = cfd;
	ev.events = EPOLLIN | EPOLLET;
	int ret = epoll_ctl(epfd, EPOLL_CTL_ADD, cfd, &ev);
	if (-1 == ret)
	{
		perror("epoll_ctl");
		return -1;
	}

	return 0;
}

int recvHttpRequest(int cfd, int epfd)
{
	int len = 0, totle = 0;
	char tmp[1024] = { 0 };
	char buffer[4096] = { 0 };
	while (len = recv(cfd, tmp, sizeof(tmp), 0) > 0)
	{
		if (totle + len < sizeof(buffer))
		{
			memcpy(buffer + totle, tmp, len);
		}
		totle += len;
	}
	// 判断数据是否被完全接收
	if (-1 == len && errno == EAGAIN)
	{
		// 解析请求行

	}
	else if (0 == len)
	{
		// 客户端断开了连接
		epoll_ctl(epfd, EPOLL_CTL_DEL, cfd, NULL);
		close(cfd);
	}
	else
	{
		perror("recv");
	}
	return 0;
}

int parseRequestLine(const char* line, int cfd)
{
	// 解析请求行  get  /xxx/xxx/1.jpg  http/1.1
	char method[12];
	char path[1024];
	sscanf(line, "%[^ ] %[^ ]", method, path);

	if (strcasecmp(method, "get") != 0)
	{
		return -1;
	}
	// 处理客户端请求的静态资源(目录或文件)
	char* file = NULL;
	if (strcmp(path, "/") == 0)
	{
		file = "./";
	}
	else
	{
		file = path + 1;
	}
	// 获取文件属性
	struct stat st;
	int ret = stat(file, &st);
	if (-1 == ret)
	{
		// 文件不存在 -- 回复404
		sendHeadMsg(cfd, 404, "Not Found", getFileType(".html"), -1);
		sendFile("404.html", cfd);
		return 0;
	}
	// 判断文件类型
	if (S_ISDIR(st.st_mode))
	{
		// 把这个目录返回给客户端
	}
	else
	{
		// 把文件的内容发送给客户端
		sendHeadMsg(cfd, 200, "OK", getFileType(file), st.st_size);
		sendFile(file, cfd);
	}

	return 0;
}

const char* getFileType(const char* name)
{
	// 自右向左找 '.'字符，如不存在返回NULL
	const char* dot = strrchr(name, '.');
	if (dot == NULL)
		return "text/plain; charset=utf-8";  //纯文本
	if (strcmp(dot, ".html") == 0 || strcmp(dot, ".htm") == 0)
		return "text/html; charset=utf-8";
	if (strcmp(dot, ".jpg") == 0 || strcmp(dot, ".jpeg") == 0)
		return "image/jepg";
	if (strcmp(dot, ".gif") == 0)
		return "image/gif";
	if (strcmp(dot, ".png") == 0)
		return "image/png";
	if (strcmp(dot, ".css") == 0)
		return "text/css";
	if (strcmp(dot, ".au") == 0)
		return "audio/basic";
	if (strcmp(dot, ".wav") == 0)
		return "audio/wav";
	if (strcmp(dot, ".avi") == 0)
		return "video/x-msvideo";
	if (strcmp(dot, ".mov") == 0 || strcmp(dot, ".qt") == 0)
		return "video/quicktime";
	if (strcmp(dot, ".mpeg") == 0 || strcmp(dot, ".mpe") == 0)
		return "video/mpeg";
	if (strcmp(dot, ".vrml") == 0 || strcmp(dot, ".wrl") == 0)
		return "model/vrml";
	if (strcmp(dot, ".midi") == 0 || strcmp(dot, ".mid") == 0)
		return "audio/midi";
	if (strcmp(dot, ".mp3") == 0)
		return "audio/mpeg";
	if (strcmp(dot, ".ogg") == 0)
		return "application/ogg";
	if (strcmp(dot, ".pac") == 0)
		return "application/x-ns-proxy-autoconfig";

	return "text/plain; charset=utf-8";
}

int sendFile(const char* fileName, int cfd)
{
	// 1.打开文件
	int fd = open(fileName, O_RDONLY);
	assert(fd > 0);
#if 0
	while (1)
	{
		char buffer[1024] = { 0 };
		int len = read(fd, buffer, sizeof(buffer));
		if (len > 0)
		{
			send(cfd, buffer, len, 0);
			usleep(10);  // 这非常重要,让接收端喘口气
		}
		else if (0 == len)
		{
			break;
		}
		else
		{
			perror("read");
		}
	}
#else
	int size = lseek(fd, 0, SEEK_END);
	sendfile(cfd, fd, NULL, size);
#endif
	return 0;
}

int sendHeadMsg(int cfd, int status, const char* descr, const char* type, int length)
{
	// 状态行
	char buffer[4096] = { 0 };
	sprintf(buffer, "http/1.1 %d %s\r\n", status, descr);

	// 响应头
	sprintf(buffer + strlen(buffer), "content-type: %s\r\n", type);
	sprintf(buffer + strlen(buffer), "content-length: %d\r\n\r\n", length);

	send(cfd, buffer, strlen(buffer), 0);
	return 0;
}
