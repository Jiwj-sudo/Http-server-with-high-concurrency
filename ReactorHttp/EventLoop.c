#include "EventLoop.h"
#include <assert.h>
#include <sys/socket.h>

// 写数据
void taskWakeUp(struct EventLoop* evLoop)
{
	const char* msg = "我要成为海贼王一样的男人";
	write(evLoop->socketPair[0], msg, strlen(msg));
}

// 读数据
int readLocalMessage(void* arg)
{
	struct EventLoop* evLoop = (struct EventLoop*)arg;
	char buffer[256];
	read(evLoop->socketPair[1], buffer, sizeof(buffer));
	return 0;
}

struct EventLoop* eventLoopInit(void* name)
{
	struct EventLoop* evLoop = (struct EventLoop*)malloc(sizeof(struct EventLoop));
	evLoop->isQuit = false;
	evLoop->threadID = pthread_self();
	pthread_mutex_init(&evLoop->mutex, NULL);
	strcpy(evLoop->threadName, name == NULL ? "MainThread" : (char*)name);
	evLoop->dispatcher = &EpollDispatcher;
	evLoop->dispatcherData = evLoop->dispatcher->init();

	// 链表
	evLoop->head = evLoop->tail = NULL;
	// map
	evLoop->channelMap = ChannelMapInit(128);
	int ret = socketpair(AF_UNIX, SOCK_STREAM, 0, evLoop->socketPair);
	if (-1 == ret)
	{
		perror("socketpair");
		exit(0);
	}
	// 指定规则：evLoop->socketPair[0] 发送数据,evLoop->socketPair[1] 接收数据
	struct Channel* channel = channelInit(evLoop->socketPair[1], ReadEvent, readLocalMessage, NULL, evLoop);

	// channel 添加到任务队列
	eventLoopAddTask(evLoop, channel, ADD);

	return evLoop;
}

int eventLoopRun(struct EventLoop* evLoop)
{
	assert(evLoop != NULL);
	// 取出事件分发和检测模型
	struct Dispatcher* dispatcher = evLoop->dispatcher;
	// 比较线程ID是否正常
	if (evLoop->threadID != pthread_self())
	{
		return -1;
	}
	// 循环进行事件处理
	while (!evLoop->isQuit)
	{
		dispatcher->dispatch(evLoop, 2);  // 超时时长 2s
	}
	return 0;
}

int eventActivate(struct EventLoop* evLoop, int fd, int event)
{
	if (fd < 0 || evLoop == NULL)
	{
		return -1;
	}

	// 取出Channel
	struct Channel* channel = evLoop->channelMap->list[fd];
	assert(channel->fd == fd);
	if (event & ReadEvent && channel->readCallback)
	{
		channel->readCallback(channel->arg);
	}
	if (event & WriteEvent && channel->writeCallback)
	{
		channel->writeCallback(channel->arg);
	}
	return 0;
}

int eventLoopAddTask(struct EventLoop* evLoop, struct Channel* channel, int type)
{
	// 加锁,保护共享资源
	pthread_mutex_lock(&evLoop->mutex);
	// 创建新节点
	struct ChannelElement* node = (struct ChannelElement*)malloc(sizeof(struct ChannelElement));
	node->channel = channel;
	node->type = type;
	node->next = NULL;
	if (evLoop->head == NULL)
	{
		evLoop->head = evLoop->tail = node;
	}
	else
	{
		evLoop->tail->next = node;
		evLoop->tail = node;
	}
	pthread_mutex_unlock(&evLoop->mutex);
	// 处理节点
	if (evLoop->threadID == pthread_self())
	{
		// 当前子线程
	}
	else
	{
		// 主线程  --  告诉子线程处理任务队列中的任务
		taskWakeUp(evLoop);
	}
	return 0;
}
