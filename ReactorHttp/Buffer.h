#pragma once

struct Buffer
{
	// 指向内存的指针
	char* data;
	int capacity;
	int readPos;
	int writePos;
};

// 初始化
struct Buffer* bufferInit(int size);
// 销毁内存
void bufferDestory(struct Buffer* buf);
// 扩容
void bufferExtendRoom(struct Buffer* buffer, int size);
// 得到剩余可写容量的大小
int bufferWriteAbleSize(struct Buffer* buffer);
// 得到剩余可读容量的大小
int bufferReadAbleSize(struct Buffer* buffer);
// 写内存
int bufferAppendData(struct Buffer* buffer, const char* data, int size);
int bufferAppendString(struct Buffer* buffer, const char* data);
int bufferSocketRead(struct Buffer* buffer, int fd);
// 根据\r\n取出一行,找到其在数据块的位置,返回该位置
char* bufferFindCRLF(struct Buffer* buffer);