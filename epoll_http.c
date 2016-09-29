#include "epoll_http.h"
 
#define DEFAULT "/default.html"
#define IMG "src_html"
#define CGI "src_cgi"
 
int times=0;
 
void echo_error(int fd,int _errno)
{
    printf("join err\n");
    switch(_errno){
        case 400://Bad Request  //客户端请求有语法错误，不能被服务器所理解
            break;
        case 404:////请求资源不存在，eg：输入了错误的URL
            //printf("*******404\n");
            error_all(fd,404,"NOT_FIND!!");
            break;
        case 401://请求未经授权，这个状态代码必须和WWW-Authenticate报头域一起使用 
            break;
        case 403://服务器收到请求，但是拒绝提供服务 
            break;
        case 500:// Internal Server Error //服务器发生不可预期的错误
            break;
        case 503://Server Unavailable  //服务器当前不能处理客户端的请求，一段时间后可能恢复正常
            break;
        default:
            break;
    }
}
 
void error_all(int fd,int err,char* reason)
{
    char buf[_SIZE_]="";
    char error[_SIZE_]="";
    sprintf(buf,"HTTP/1.0 %d %s\r\n\r\n",err,reason);
    sprintf(error," %d %s",err,reason);
    printf("err buf:%s\n error:%s",buf,error);
    write(fd,buf,strlen(buf));
    write(fd,"<html>\n",strlen("<html>\n"));
    write(fd,"<head>",strlen("<head>"));
    write(fd,"<h1> HELLO PPH!!!</h1>\n",strlen("<h1> HELLO PPH!!!</h1>\n"));
    write(fd,"<h2>",strlen("<h2>"));
    write(fd,error,strlen(error));
    write(fd,"</h2>\n",strlen("</h2>\n"));
    write(fd,"</head>\n",strlen("</head>\n"));
    write(fd,"</html>",strlen("</html>"));
    //  echo_html(fd,"src_html/1.html",102400000);
}
void echo_html(int fd,const char* url,int fd_size ){
    char buf[_SIZE_]="HTTP/1.1 200 OK\r\n\r\n";
    int rfd=-1;
    off_t set=0;
    ssize_t size=1;
    printf("url:%s\n",url);
    if((rfd=open(url, O_RDONLY))<0){
        echo_error(fd,500);
        printf("e-html %s",strerror(errno));
    }
    printf("open success\n");
    if(write(fd,buf,strlen(buf))<0){
        perror(" echo_html_write");
        close(rfd);
        return;
    }
    printf("write head success\n");
    int i=0;
    while(set!=fd_size){
        size=sendfile(fd,rfd,&set,fd_size);
        if(errno!=EAGAIN&&size<0){
            printf("sendfile error %s %d\n",strerror(errno),errno);
            break;
        }
        //  printf("\nsend: size %d all size  %d time %d \n",set,fd_size,++i);
    }
    close(rfd);
    return;
}
 
int get_line(int sock_fd,char * line){
    int index=0;
    ssize_t size=1;
    char ch=0;
    printf("getline start\n");
    while(ch!='\n'){
        if((size=read(sock_fd,&ch,1))<0){
            perror("getline__");
            if(errno==EAGAIN)
                continue;
            else
                return -1;
        }
        if(ch=='\r'){
            char tmp=0;
            if(recv(sock_fd,&tmp,1,MSG_PEEK)>0){
                if(tmp=='\n'){
                line[index++]=tmp;
                read(sock_fd,&ch,1);
                continue;
                }
            }
        }
        //printf("index %d\n",index);
        if(index==1024){
            printf("httpd line full exit\n");
            line[1023]=0;
            return -2;
        }
        line[index++]=ch;
    }
    line[index]=0;
    if(strcmp(line,"\n")==0){
        return 0;
    }
    printf("getline  success\n");
    return 1;
}
 
 
//获取post正文 参数
 
char* get_length(int fd,char* content_length)
{
    int size=1;
    int tag=0;
    int index=0;
    while(size!=0){//通过持续读取一行 直到读到空行结束
        size=get_line(fd,content_length);
        if(size==-2)
            continue;
        if(strncasecmp(content_length,"content-length: ",16)==0){
            printf(" length success\n");
            break;
        }
        if(size==-1){
            printf("get line出错\n");
            return NULL;
        }
    }
    content_length[strlen(content_length)-1]=0;
    strcpy(content_length,content_length+16);
    printf("con end: %s\n",content_length);
    return content_length;
}
 
 
void cgi_action(int fd,char* method,char* url ,char* parameter)
{
    char env[20]="METHOD=";
    char par[_SIZE_]="PARAMETER=";
    int pwrite[2];
    if((pipe(pwrite)<0)){
        perror("pipe");
        return;
    }
    strcat(env,method);
    strcat(par,parameter);
    printf(" act url:%s\n",url);
    printf("parameter:%s\n",par);
    if(putenv(env)<0){
        perror("putenv");
        return;
    }
    if(putenv(par)<0){
        perror("putenv par");
        return;
    }
    //  printf("fork qian\n");
    pid_t id=fork();
    if(id<0){
        perror("fork");
        return;
    }
    else if(id==0){//子进程
        close(pwrite[0]);
        //printf("child\n");
        if(dup2(pwrite[1],1)<0){
            perror("dup2.1");
            return;
        }
        if(dup2(fd,0)<0){
            perror("dup2.2");
            return;
        }
        if(execl(url,NULL)<0){
            perror("execl");
            printf("exit url:\n",url);
            exit(-2);
        }
    }
    else{//父进程
        close(pwrite[1]);
        char buf[_SIZE_]="";
        int count=0;
        int i=0;
        ssize_t size=1;
        while(size>0){
            size=read(pwrite[0],buf,_SIZE_);
            if(size<0){
                echo_error(fd,500);
                break;
            }
            if(size==0)
                break;
            write(fd,buf,strlen(buf));
        }
    waitpid(-1,NULL,0);
    close(pwrite[0]);
    }
}
 
int epoll_echo_http(struct epoll_event * ev){ 
    char* method=((pev_buf)ev->data.ptr)->method;
    char* url=((pev_buf)ev->data.ptr)->url;
    char* parameter=((pev_buf)ev->data.ptr)->parameter;//get方法 参数
    int fd=((pev_buf)ev->data.ptr)->fd;
 
    int cgi=((pev_buf)ev->data.ptr)->cgi;
    struct stat stat_buf; 
    if(cgi==0)
    if(stat(url, &stat_buf)<0){
        printf("stat <0 \n");
        echo_error(fd,404);
        return 0;
    }
 
    if(strcasecmp("POST",method)==0){
 
        //printf("already cgi\n");
        cgi_action(fd,method,url,parameter);
    }
    else if(strcasecmp("GET",method)==0){
        if(cgi==1){
            //cgi
            //  printf("rev_http: parameter:%s\n",parameter);
            cgi_action(fd,method,url,parameter);
            printf("ret cgi\n");
        }
    else{
        echo_html(fd,url,stat_buf.st_size);
    }
    }
    if(strcasecmp(method,"POST")==0)
        clear_buf(fd);
        return 0;
}
 
int epoll_recv_http(struct epoll_event *ev){
    if(ev==NULL){
        printf("ev error\n");
        return -1;
    }
    int fd=((pev_buf)ev->data.ptr)->fd;
 
    char real_url[128]="src_html";
    char line[_SIZE_];
    char* method=NULL;
    char* version=NULL;
    char* url=NULL;
    char parameter[_SIZE_]="";//get方法 参数
    char  content_length[_SIZE_]="";
    if(get_line(fd,line)==-2){
        printf("it's a cache request! so can't process!\n");
        return 0;
    }
    int index=strlen(line)-1;
    //GET / HTTP/1.1
    while(index>0){//提取method url
        if(line[index]==' '&&version==NULL){
            version=((char*)line)+index+1;
            line[index]=0;
        }
        if(line[index]==' '&&url==NULL){
            url=line+index+1;
            line[index]=0;
        }
        --index;
    }
    method=line;
 
    ((pev_buf)ev->data.ptr)->cgi=0;
    if(strcasecmp("GET",method)==0){
        index=0;
        while(url[index]){
            if(url[index]=='?'){
                ((pev_buf)ev->data.ptr)->cgi=1;
                strcpy(parameter,url+index+1);
                url[index]=0;
                ((pev_buf)ev->data.ptr)->cgi=1;
                break;
            }
            ++index;
        }
    }
    else if(strcasecmp("POST",method)==0){
        ((pev_buf)ev->data.ptr)->cgi=1;
        if(get_length(fd,content_length)==NULL){
            echo_error(fd,503);
            printf("get len err\n");
            clear_buf(fd);
            return -1;
        }
        strcpy(parameter,content_length);
    }
    if(strcmp(url,"/")==0){
        strcat(real_url,DEFAULT);
    }
    else{
        if(((pev_buf)ev->data.ptr)->cgi==1){
            strcpy(real_url,CGI);
        }
        strcat(real_url,url);
    }
    printf("real_url :%s\n",real_url);
 
    strcpy(((pev_buf)ev->data.ptr)->method,method);
    strcpy(((pev_buf)ev->data.ptr)->url,real_url);
    strcpy(((pev_buf)ev->data.ptr)->parameter,parameter);
    printf(" get connect :%d times !\n",++times);
    if(strcasecmp(method,"get")==0)
        clear_buf(fd); 
        return 1;
}
 
int  clear_buf(int fd){
    char buf[_SIZE_]="";
    ssize_t size=0;
    size=read(fd,buf,_SIZE_);
    if(size<0){
        perror("clear_buf_read");
        return -1;
    }
    buf[size]=0;
    return 0;
}
