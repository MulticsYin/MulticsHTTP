#ifndef __MYHTTP__
 
#define __MYHTTP__
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
 
typedef struct event_buf{//http请求信息的缓冲区
    int fd;
    char method[10];
    char url[1024];
    char parameter[1024];//get方法 参数
    int cgi;
}ev_buf,*pev_buf;
 
typedef struct ev_read_buf{//libevent 的缓冲区
    struct event_base * _base;
    ev_buf _buf;  
}read_buf,*pread_buf;
 
char* get_text(int fd,char* buf);//获取正文
 
int event_recv_http(pread_buf);//读取http请求
int event_echo_http(pread_buf);//回复客户端
void cgi_action(int fd,char* method,char* url,char* parameter);//cgi处理逻辑
 
int get_line(int sock_fd,char * buf);//获取socket_fd一行
 
void echo_error(int fd,int _errno);//错误回显
void error_all(int fd,int err,char* reason);//所有错误的处理
void echo_html(int fd,const char* url,int size );//回显网页
 
#endif
