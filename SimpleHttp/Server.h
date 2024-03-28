﻿#pragma once

// 初始化用于监听的套接字
int initListenFd(unsigned short port);
// 启动epoll
int epollRun(int lfd);
// 和客户端建立连接
void* acceptClient(void* arg);
// 接收http请求
void* recvHttpRequest(void* arg);
// 解析请求行
int parseRequestLine(const char* line, int cfd);
// 发送文件
int sendFile(const char* fileName, int cfd);
// 发送响应头(状态行+响应头)
int sendHeadMsg(int cfd, int status, const char* descr, const char* type, int length);
const char* getFileType(const char* name);
// 发送目录
int sendDir(const char* dirName, int cfd);

int hexToDec(char c);
void decodeMsg(char* to, char* from);