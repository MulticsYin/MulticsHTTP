//包括后续非 exec 逻辑
 
#include "MultiThread_httpd.h"
 
#define DEFAULT "src_html/default.html"
#define IMG "src_html"
#define CGI "src_cgi"
 
int times=0;
void echo_error(int fd,int _errno)
{
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
    close(fd);
}
 
void error_all(int fd,int err,char* reason)
{  
    char buf[_SIZE_]="";
    char error[_SIZE_]="";
    sprintf(buf,"HTTP/1.1 %d %s\r\n\r\n",err,reason);
    sprintf(error," %d %s\n",err,reason);
//  printf("err buf:%s\n",buf);
    write(fd,buf,strlen(buf));
    write(fd,"<html>\n",strlen("<html>\n"));
    write(fd,"<head>\n",strlen("<head>\n"));
    write(fd,"<h1> HELLO PPH!!!</h1>\n",strlen("<h1> HELLO PPH!!!</h1>\n"));
    write(fd,"<h2>\n",strlen("<h2>\n"));
    write(fd,error,strlen(error));
    write(fd,"</h2>\n",strlen("</h2>\n"));
    write(fd,"</head>\n",strlen("</head>\n"));
    write(fd,"</html>\n",strlen("</html>\n"));
//  echo_html(fd,"src_html/1.html",102400000);
}
 
int get_line(int sock_fd,char * line){
    int index=0;
    ssize_t size=1;
    char ch=0;
    while(ch!='\n'){
        if((size=read(sock_fd,&ch,1))<0){
            perror("read_get_line");
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
        if(index==1024){
            line[1023]=0;
            return -1;
        }
             
            line[index++]=ch;
    }
    line[index]=0;
    printf("line :%d,%d,%d,%s",index,strlen(line),line[0],line);
    if(strcmp(line,"\n")==0){
//      printf("*****************");
        return 0;
    }
    return 1;
}
 
void echo_html(int fd,const char* url,int fd_size ){ 
    char buf[_SIZE_]="HTTP/1.1 200 OK\r\n\r\n";
    if(url==NULL){
        int rfd=-1;
        off_t set=0;
        ssize_t size=0;
        if((rfd=open(DEFAULT, O_RDONLY))<0){
            echo_error(fd,500);
            exit(0);
        }
//      printf("%d %d\n",fd,rfd);
         
        write(fd,buf,strlen(buf));
        size=sendfile(fd,rfd,&set,fd_size);
        if(size<0){
            close(rfd);
            printf("senfile error!\n");
            return;
        }
        printf("size: %d\n",size);
        close(rfd);
     
    }else{
        int rfd=-1;
        off_t set=0;
        ssize_t size=0;
        //printf("url:%s\n",url);
        if((rfd=open(url, O_RDONLY))<0){
            echo_error(fd,500);
        //  exit(0);
        }
        write(fd,buf,strlen(buf));
        size=sendfile(fd,rfd,NULL,fd_size);
        if(size<0){
            printf("senfile error!\n");
            return;
        }
    //  printf("size: %d\n",size);
        close(rfd);
        return;
    }
}
//清除client_sock
 
int  clear_buf(int fd)
{
    char buf[_SIZE_]="";
    ssize_t size=1;
    size=read(fd,buf,_SIZE_);
     
    if(size<0){
        perror("clear_buf_read");
        return -1;
    }
    return 0;
}
 
 
//获取post正文 参数
char* get_length(int fd,char* content_length)
{
    int size=1;
    int tag=0;
    int index=0;
    while(size!=0){//通过持续读取一行 直到读到空行结束
        size=get_line(fd,content_length);
        if(strncasecmp(content_length,"content-length: ",16)==0){
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
        printf("father start\n");
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
            printf("if end?\n");
         
        }
        waitpid(0,NULL,0);
        close(pwrite[0]);
    }  
}
 
 
void* http_action(void * client_sock){  
    int fd=(int)client_sock;
    struct stat stat_buf; 
    char line[_SIZE_];
    char* method=NULL;
    char* version=NULL;
    char* url=NULL;
    char* parameter=NULL;//get方法 参数
    char  text[_SIZE_]="";
    int cgi=0;
    if(get_line(fd,line)==-1){
        printf("this is a cache requset , so can't process!\n");
        return NULL;
    }
    int index=strlen(line)-1;
    //printf("%d  \n",index);
    //GET / HTTP/1.1
    printf("%d\n index ",index);
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
    char real_url[128]=CGI;
    //判断请求方法
    if(strcasecmp("POST",method)==0){
        printf("real_url :%s\n",real_url);
        strcat(real_url,url);
 
        printf("start:%s %s %s",method,url ,version);   
        get_length(fd,text);
        cgi_action(fd,method,real_url,text);
        printf("fork ...end!\n");
    }
    else if(strcasecmp("GET",method)==0){
        index=0;
        while(url[index]){
            if(url[index]=='?'){
                cgi=1;
                parameter=url+index+1;
                url[index]=0;
                printf(" get url:%s\n",url);
                break;
            }
            ++index;
        }
        if(cgi==1){
            //cgi;
            strcat(real_url,url);
 
            printf("start:%s %s %s",method,url ,version);   
            cgi_action(fd,method,real_url,parameter);
            printf("ret cgi\n");
        }
        else{
            if(strcmp("/",url)==0){
                 
                if(stat(DEFAULT, &stat_buf)<0){
                    echo_error(fd,500);
                    close(fd);
                    return ;
                }
                echo_html(fd, NULL,stat_buf.st_size);
            }
            else{
                char real_url[128]="src_html";
                strcat(real_url,url);
                printf("real_url :%s\n",real_url);
                if(stat(real_url, &stat_buf)<0){   
                    echo_error(fd,404);
                    //printf("times:%d end!\n",times++);
                    //close(fd);
                    //return;
                }
                else{
                    printf("fd size:%d",stat_buf.st_size);
                    echo_html(fd,real_url,stat_buf.st_size);
                    }
                }
             
        }
    }
    else{
        echo_error(fd,403);
    }
    if(strcasecmp(method,"POST")==0){
        printf("fork ...end!\n");
        close(fd);
        printf("times:%d end!\n",times++);
        return;
        }
    if(clear_buf(fd)<0){
        printf("clear error\n");
    }
    else
        printf("clear success!\n");
    printf("times:%d end!\n",times++);
    close(fd);
    return;
}
