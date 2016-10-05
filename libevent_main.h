/*
 * 1.注册 、绑定 监听套接字文件描述符、设为监听状态.
 * 2.注册 事件库，初始化事件，将 监听套接字 添加到事件库，开始循环.
 * 3.分别 编写 accept 、read 、write 、error回调函数.
 * 4.accept回调函数中创建线程，该线程的线程函数中 重新注册事件库 ，将客户
     端 socket 添加进事件库（此时关注读事件，回调函数为read），开始循环.
 * 5.当该线程读事件 就绪，用read回调函数 处理http请求，将相关信息保存到自己
     维护的结构体中。停止 事件库循环，将关注的事件改为 写事件，回调函数改为write.
 * 6.处理完一次http请求，清理缓冲区和事件库.
 * */

#ifndef LIBEVENT_MAIN_H
#define LIBEVENT_MAIN_H
 
#include "libevent_httpd.h"
#include <event2/event.h>
#include <event2/bufferevent.h>
#include <malloc.h>
#include <netdb.h>
#include <sys/utsname.h>
#include <net/if.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <pthread.h>
#include <stdlib.h>
 
#define _MAX_ACCEPT_ 10
 
typedef struct fd_event_base{
    evutil_socket_t fd;
    struct event_base * base;
}ev_base;
 
void libevent_up(int sock);//启动libevent 事件库 循环
void error_cb(evutil_socket_t fd,short event,void * arg);//放生错误的回调函数
void write_cb(evutil_socket_t fd,short event,void * arg);//写事件的回调函数
void read_cb(evutil_socket_t fd,short event,void * arg);//读事件。。。。。
void accept_cb(evutil_socket_t fd,short event, void * arg);//建立连接的回调函数（主线程中）
void* accept_on_thread(void* fd_arg);//子线程处理函数
#endif

