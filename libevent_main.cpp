#include "libevent_main.h"
 
#define _MAXFD_ 10 
#define ERRORIP -1
 
int timeout_times=0;
 
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
 
void accept_cb(evutil_socket_t fd,short event, void * arg)
{
    pthread_t tid=0;
    if(event==EV_TIMEOUT){
        if((timeout_times++)==10){
            printf("time out too long to exit!\n");
            event_base_loopexit(((ev_base*)arg)->base, NULL);      
            return;
        }
 
        printf("time out!\n");
        return;
    }  
    evutil_socket_t new_sock=-1;
    struct sockaddr_in client;
    socklen_t len=sizeof(client);
    while((new_sock=accept(((ev_base*)arg)->fd,(struct sockaddr*)&client,&len))){  
        if(new_sock<0){
            //perror("accept");
            break;
    }
        if(set_non_block(new_sock)<0){
            echo_error(new_sock,500);
            continue;
        }
        printf(" accept success :%d\n",new_sock);
        if(pthread_create(&tid,NULL,accept_on_thread,(void*)new_sock)<0){
            perror("pthread_create");
            continue;
        }
        pthread_detach(tid);
    }
 
}
 
void* accept_on_thread(void* fd_arg)
{
    //线程内部创建base 来处理不同http请求
    read_buf read_buffer;
    read_buffer._buf.fd=(int)fd_arg;
    read_buffer._base=event_base_new();
    struct event * sock_event=NULL;
    struct timeval timeout;
    timeout.tv_sec=5;
    timeout.tv_usec=0;
    if(read_buffer._base==NULL){
        perror("base_new");
        return NULL;
    }
 
    sock_event=event_new(read_buffer._base,(evutil_socket_t)fd_arg,EV_READ|EV_PERSIST|EV_ET,read_cb,(void *)&read_buffer);
    if(sock_event==NULL){
        perror("event_new");
        return NULL;
    }
    event_add(sock_event,&timeout);
    event_base_dispatch(read_buffer._base);
//  struct bufferevent* ev_buf;
//  bev=bufferevent_socket_new(read_buffer._base,new_sock,BEV_OPT_CLOSE_ON_FREE );
//  if(ev_buf==NULL){
//      perror("ev_buf");
//      return;
//  }
//  bufferevent_setcb(bev, read_cb, NULL, error_bc, (void*)fd);  
//  bufferevent_enable(bev, EV_READ|EV_PERSIST);
    if(read_buffer._base!=NULL)
    {  
        printf("join_write_event!\n");
        event_free(sock_event);
        sock_event=event_new(read_buffer._base,(evutil_socket_t)fd_arg,EV_WRITE|EV_PERSIST|EV_ET,write_cb,(void *)&read_buffer);
        if(sock_event==NULL){
            perror("event_new");
            return NULL;
        }
        printf("fd:%d\n",read_buffer._buf.fd);
        event_add(sock_event,&timeout);
        printf("join_write_event_loop!\n");
        event_base_dispatch(read_buffer._base);
         
    }
    event_free(sock_event);
    event_base_free(read_buffer._base);
    close((evutil_socket_t)fd_arg);
    printf("client lib out! fd :%d \n",(int)fd_arg);
    return NULL;
}
 
 
void read_cb(evutil_socket_t fd,short event,void * arg)
{
    if(event==EV_TIMEOUT){
        printf("time out!\n");
    }
    int ret=event_recv_http((pread_buf)arg);
    if(ret<0){
        event_base_free(((pread_buf)arg)->_base);
        ((pread_buf)arg)->_base==NULL;
        return;
    }
    printf("");
    event_base_loopexit(((pread_buf)arg)->_base, NULL);
     
}
     
 
 
void write_cb(evutil_socket_t fd,short event,void * arg)
{
    printf("write_cb\n");
    event_echo_http((pread_buf)arg);
    printf("event_echo_http_success!\n");
    event_base_loopexit(((pread_buf)arg)->_base, NULL);
}
 
void error_cb(evutil_socket_t fd,short event,void * arg)
{
}
void libevent_up(int sock)
{
    ev_base base;
     
    base.base=event_base_new();
    base.fd=(evutil_socket_t)sock;
    struct event * listen_event=NULL;
    evutil_socket_t listener=sock;
    struct timeval timeout;
    timeout.tv_sec=5;
    timeout.tv_usec=0;
    if(base.base==NULL){
        perror("base_new");
        return;
    }
 
 
    listen_event=event_new(base.base,listener,EV_READ|EV_PERSIST|EV_ET,accept_cb,(void *)&base);
    if(listen_event==NULL){
        perror("event_new");
        return;
    }
    event_add(listen_event,&timeout);
    event_base_dispatch(base.base);
    event_free(listen_event);
    event_base_free(base.base);
 
    printf("server out!\n ");
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
    libevent_up(listen_sock);
    close(listen_sock);
    return 0;
}
