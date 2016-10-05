#ifndef EPOLL_HTTP_H
 
#define EPOLL_HTTP_H
#include <stdio.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/epoll.h>
#include <sys/sendfile.h>
#define _SIZE_ 1024
 
typedef struct event_buf{//自己维护的结构体
    int fd;
    char method[10];//http请求方法
    char url[1024];//请求参数
    char parameter[1024];//get方法 参数
    int cgi;//是否需要用到CGI 方法
}ev_buf,*pev_buf;//重命名
 
char* get_text(int fd,char* buf);//获取消息正文
 
int epoll_recv_http(struct epoll_event *ev);//接受消息
int epoll_echo_http(struct epoll_event * ev);//发送xiaoxi
void cgi_action(int fd,char* method,char* url,char* parameter);//CGI接口
 
int get_line(int sock_fd,char * buf);//从socket fd 读取一行信息
 
void* http_action(void* client_sock);//
 
void echo_error(int fd,int _errno);//回显错误信息
void error_all(int fd,int err,char* reason);//所有错误信息
void echo_html(int fd,const char* url,int size );//回显请求网页
 
#endif
