#include "Buffer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/uio.h>

struct Buffer* bufferInit(int size)
{
	struct Buffer* buffer = (struct Buffer*)malloc(sizeof(struct Buffer));
	if (buffer != NULL)
	{
		buffer->data = (char*)malloc(size);
		buffer->capacity = size;
		buffer->readPos = buffer->writePos = 0;
		memset(buffer->data, 0, size);
	}
	return buffer;
}

void bufferDestory(struct Buffer* buf)
{
	if (buf != NULL)
	{
		if (buf->data != NULL)
		{
			free(buf->data);
		}
	}
	free(buf);
}

void bufferExtendRoom(struct Buffer* buffer, int size)
{
	// 1.内存足够用 - 不需要扩容
	if (bufferWriteAbleSize(buffer))
	{
		return;
	}
	// 2.内存需要合并才够用 - 不需要扩容
	// 剩余的可写内存 + 已读的内存 > size
	else if (buffer->readPos + bufferWriteAbleSize(buffer) >= size)
	{
		// 得到未读的内存大小
		int readAble = bufferReadAbleSize(buffer);
		// 移动内存
		memcpy(buffer->data, buffer->data + buffer->readPos, readAble);
		// 更新位置
		buffer->readPos = 0;
		buffer->writePos = readAble;
	}
	// 3.内存不够用 - 需要扩容
	else
	{
		void* temp = realloc(buffer->data, buffer->capacity + size);
		if (temp == NULL)
		{
			return;
		}
		memset(temp + buffer->capacity, 0, size);
		// 更新数据
		buffer->data = (char*)temp;
		buffer->capacity += size;
	}
}

int bufferWriteAbleSize(struct Buffer* buffer)
{
	return buffer->capacity - buffer->writePos;
}

int bufferReadAbleSize(struct Buffer* buffer)
{
	return buffer->writePos - buffer->readPos;
}

int bufferAppendData(struct Buffer* buffer, const char* data, int size)
{
	if (buffer == NULL || data == NULL || size <= 0)
	{
		return -1;
	}
	// 扩容
	bufferExtendRoom(buffer, size);
	// 数据拷贝
	memcpy(buffer->data + buffer->writePos, data, size);
	buffer->writePos += size;
	return 0;
}

int bufferAppendString(struct Buffer* buffer, const char* data)
{
	int size = strlen(data);
	return bufferAppendData(buffer, data, size);
}

int bufferSocketRead(struct Buffer* buffer, int fd)
{
	struct iovec vec[2];
	// 初始化数组
	int writeAble = bufferWriteAbleSize(buffer);
	vec[0].iov_base = buffer->data + buffer->writePos;
	vec[0].iov_len = writeAble;
	char* tmpbuf = (char*)malloc(40960);
	vec[1].iov_base = buffer->data + buffer->writePos;
	//vec[1].iov_base = tmpbuf;
	vec[1].iov_len = 40960;
	int result = readv(fd, vec, 2);
	if (-1 == result)
	{
		return -1;
	}
	else if (result <= writeAble)
	{
		buffer->writePos += result;
	}
	else
	{
		buffer->writePos += buffer->capacity;
		bufferAppendData(buffer, tmpbuf, result - writeAble);
	}
	free(tmpbuf);
	return result;
}

char* bufferFindCRLF(struct Buffer* buffer)
{
	// strstr  memmem
	char* ptr = memmem(buffer->data + buffer->readPos, bufferReadAbleSize(buffer), "\r\n", 2);
	return ptr;
}