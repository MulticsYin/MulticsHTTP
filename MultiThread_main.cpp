/*
 * http服务器的实现逻辑
 * 1.实现基于 tcp 协议的服务器端（创建监听套接字  bind； listen ）
 * 2.主线程循环 accept 建立连接，成功后创建子线程处理后续逻辑。主线程继续accept
 * 3.子线程 处理：
 *   1>获取请求行 分别 截取 method  url  version ，在此要通过判断是不是\r\n表示一行结尾。
 *     用到recv函数中的MSG_PEEK方法
 *   2>对正确的 请求行 进行处理
 *   CGI：绝大多数的CGI程序被用来解释处理来自表单的输入信息，并在服务器产生相应的处理，或将
 *     相应的信息反馈给浏览器。CGI程序使网页具有交互功能
 *     1）get方法无参数（非 CGI） 回复 请求资源。（在此用sendfile函数提高效率）
 *     2）get方法有参数（url中含‘？’之后为参数（从含表单的网页页面获取）），先将参数作为
 *        环境变量。fork子进程（子进程共享父进程的环境变量）。子进程 程序 替换 为 真实的
 *        可执行程序 （‘？’之前的url） 对参数做处理得到执行结果，通过管道 传给父进程，父
 *        进程对数据做差错检测后，发送给客户端。
 *     3）post方法 一定含有参数。参数的有效长度包含在消息报头中
 *   3> 清理sock的缓存。并清理子线程的资源，关闭sock。子线程退出。
 **/

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
#include <pthread.h>
#include "MultiThread_httpd.h"
#include <netdb.h>
#include <sys/utsname.h>
#include <net/if.h>
#include <sys/ioctl.h>
#define ERRORIP -1
 
int startup(char* ip,int port)
{
//  printf("%s %d\n",ip,port);
    int sock=socket(AF_INET,SOCK_STREAM,0);
    struct sockaddr_in server;
    server.sin_family=AF_INET;
    server.sin_addr.s_addr=inet_addr(ip);
    server.sin_port=htons(port);
    int flag=1;
    if( setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &flag, sizeof(flag)) == -1)  
    {  
        perror("setsockopt");  
        exit(1);                  
    }  
    if(bind(sock,(struct sockaddr *)&server,sizeof(server))<0){
        perror("bind");
        exit(-2);
    }
    if(listen(sock,5)<0){
        perror("listen");
        exit(-3);
    }
    //printf("%d\n",sock);
    return sock;
}
 
void usage(char* arg)
{
    printf("usage %s [ip] [port]\n",arg);
 
}
 
void http_start(int listen_sock){
 
    int done=0;
    int client_sock=-1;
//  printf("http_start\n");
    struct sockaddr_in client;
    socklen_t len=sizeof(client);
    while(!done){
//      printf("循环\n");
        if((client_sock=accept(listen_sock,(struct sockaddr *)&client,&len))<0){
            printf("connect error!\n");
            //做访问记录 存入数据库
            continue;
        }
     
        pthread_t tid;
//      printf("client:%d\n",client_sock);
        int ret=pthread_create(&tid,NULL,http_action,(void*)client_sock);
        if(ret!=0){
        //  printf("create thread error!\n");
            printf("%s\n",strerror(errno));
            continue;
        }
        else{
            pthread_detach(tid);
        }
     
     
    }
     
}
 
int main(int argc,char* argv[]){
    if(argc!=3){
        usage(argv[0]);
        exit(-1);
    }
    int listen_sock=0;
    char* ip=NULL;
    int port=atoi(argv[2]);   
    if(strcmp(argv[1],"any")==0){
            int sfd, intr;
            struct ifreq buf[16];
            struct ifconf ifc;
            sfd = socket (AF_INET, SOCK_DGRAM, 0); 
            if (sfd < 0)
                return ERRORIP;
            ifc.ifc_len = sizeof(buf);
            ifc.ifc_buf = (caddr_t)buf;
            if (ioctl(sfd, SIOCGIFCONF, (char *)&ifc))
                return ERRORIP;
            intr = ifc.ifc_len / sizeof(struct ifreq);
            while (intr-- > 0 && ioctl(sfd, SIOCGIFADDR, (char *)&buf[intr]));
                close(sfd);
            ip= inet_ntoa(((struct sockaddr_in*)(&buf[intr].ifr_addr))-> sin_addr);
         
        printf("%s\n",ip); 
        listen_sock=startup(ip,port);
    }
    else
        listen_sock=startup(argv[1],port);
//  printf("%d\n",listen_sock);
    http_start(listen_sock);
    close(listen_sock);
    return 0;
}
