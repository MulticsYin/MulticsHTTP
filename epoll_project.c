#include "epoll_http.h"
#include <sys/epoll.h>
#include <malloc.h>
#include <netdb.h>
#include <sys/utsname.h>
#include <net/if.h>
#include <sys/ioctl.h>
#define _MAXFD_ 10 
#define ERRORIP -1
 
int set_non_block(int fd)
{
    int old_flag=fcntl(fd,F_GETFL);
    if(old_flag<0){
        perror("fcntl");
    //  exit(-4);
        return -1;
    }
    if(fcntl(fd,F_SETFL,old_flag|O_NONBLOCK)<0){
        perror("fcntl");
        return -1;
    }
 
    return 0;
 
}
 
int startup(char* ip,int port)
{
    int sock=socket(AF_INET,SOCK_STREAM,0);
    struct sockaddr_in server;
    server.sin_family=AF_INET;
    server.sin_addr.s_addr=inet_addr(ip);
    server.sin_port=htons(port);
    int flag=0;
//  printf("port  %d  %d",port,(htons(port)));
    if(bind(sock,(struct sockaddr *)&server,sizeof(server))<0){
        perror("bind");
        exit(-2);
    }
    if( setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &flag, sizeof(flag)) == -1)  
    {  
        perror("setsockopt");  
        exit(1);                  
    }  
    if(listen(sock,50)<0){
        perror("listen");
        exit(-3);
    }
    return sock;
}
 
void usage(char* arg)
{
    printf("usage %s [ip] [port]\n",arg);
 
}
 
void epollup(int sock)
{  
    int epoll_fd=epoll_create(256);
    if(epoll_fd<0){
        perror("epoll");
        return;
    }
    int timeout_num=0;
    int done=0;
    int timeout=10000;
    int i=0;
    int ret_num=-1;
 
    struct epoll_event ev;
    struct epoll_event event[100];
    ev.data.fd=sock;
    ev.events=EPOLLIN|EPOLLET;
//  fd_num=1;
//  printf("listen sock%d\n",sock);
    if(epoll_ctl(epoll_fd,EPOLL_CTL_ADD,sock,&ev)<0){
        perror("epoll_ctl");
        return ;
    }
    while(!done){
        switch(ret_num=epoll_wait(epoll_fd,event,256,timeout)){
        case -1:{
            perror("epoll_wait");  
            break;
        }
        case 0 :{
            if( timeout_num++>20)
                done=1;
            printf("time out...\n");
            break;
        }
        default:{
                for(i=0;i<ret_num;++i){
                    if(event[i].data.fd==sock&&event[i].events&EPOLLIN){
                        int new_sock=-1;
                        struct sockaddr_in client;
                        socklen_t len=sizeof(client);
                        while((new_sock=accept(sock,(struct sockaddr*)&client,&len))){
                            if(new_sock<0){
                                //perror("accept");
                                break;
                            }
                            if(set_non_block(new_sock)<0){
                                echo_error(new_sock,500);
                                continue;
                            }
                            printf(" epoll :%d\n",new_sock);
                            ev.data.fd=new_sock;
                            ev.events=EPOLLIN|EPOLLET;
                            if(epoll_ctl(epoll_fd,EPOLL_CTL_ADD,new_sock,&ev)<0){
                                perror("epoll_ctl");
                                echo_error(new_sock,503);
                                continue;
                            }
                        }
                        break;
                    }
                    else {
                        if(event[i].events&EPOLLIN){
                            int fd=event[i].data.fd;
                            pev_buf pev=(pev_buf)malloc(sizeof(ev_buf));          
                            event[i].data.ptr=pev;
                            pev->fd=fd;
                            if(epoll_recv_http(&event[i])<0){
                                free(pev);
                                if(epoll_ctl(epoll_fd,EPOLL_CTL_DEL,fd,&ev)<0)  
                                    perror("read EPOLL_CTL_DEL");
                                close(fd);
                                continue;
                            }
                            else
                                event[i].events=EPOLLOUT;
                        }
                        if(event[i].events&EPOLLOUT){
                            int fd=((pev_buf)(event[i].data.ptr))->fd;
                            epoll_echo_http(event+i);
                            if(epoll_ctl(epoll_fd,EPOLL_CTL_DEL,fd,&ev)<0)  
                            perror("out_ctl_del");
                            free((pev_buf)event[i].data.ptr);
                            printf("epoll close :%d\n",fd);
                            close(fd);
 
                        }
                         
                    }
                }
            break;
        }
         
        }
    }
}
 
int main(int argc,char* argv[]){
    if(argc!=3){
        usage(argv[0]);
        exit(-1);
    }  
    int port=atoi(argv[2]);
    int listen_sock;
    char* ip=NULL;
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
 
         listen_sock=startup( argv[1],port);
 
//  printf("port %s %d",argv[2],port);
     
    set_non_block(listen_sock);
    epollup(listen_sock);
    close(listen_sock);
    return 0;
}
