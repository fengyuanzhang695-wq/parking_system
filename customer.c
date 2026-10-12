/*===============================================
*   文件名称：customer.c
*   创 建 者： 张峰源    
*   创建日期：2026年09月14日
*   描    述：
================================================*/

#include <stdio.h>
#include <sys/types.h>          
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <pthread.h>
#include <string.h>
/* See NOTES */
void *myrecv(void *arg)
{
int sockfd = *(int*)arg;
char recv_buf[1024]={0};
int ret;


    while(1) {
        memset(recv_buf, 0, sizeof(recv_buf));
        ret = read(sockfd, recv_buf, sizeof(recv_buf) - 1);

        if (ret > 0) {
            recv_buf[ret] = '\0'; // 截断字符串
            printf("%s", recv_buf); // 直接打印即可，因为服务器已经加了 \n
            fflush(stdout); // 强制刷新屏幕显示
        } else if (ret == 0) {
            printf("服务器已断开连接\n");
            break;
        }
    }
    return NULL;
}
int main(int argc, char *argv[])
{ 
int sockfd = socket(AF_INET,SOCK_STREAM,0);
if(sockfd < 0)
{
perror("socket");
return -1;
}
//创建服务器的结构体
struct sockaddr_in myserver;
myserver.sin_family = AF_INET;
myserver.sin_port = htons(8888);//小端转换成大端，linux系统是小端存储，网络是大端存储
myserver.sin_addr.s_addr = inet_addr("192.168.2.166");
int s_size = sizeof(myserver);
connect(sockfd,(struct sockaddr*)&myserver,s_size);
puts("连接成功!");
pthread_t thread;
pthread_create(&thread,NULL,myrecv,&sockfd);
char send_buf[128]={0};
while(1)
{
    if (fgets(send_buf, sizeof(send_buf), stdin) != NULL) {
    // 【关键】去掉末尾的换行符，防止服务器读到空行
    send_buf[strcspn(send_buf, "\n")] = 0;

    // 只有输入不为空时才发送
    if (strlen(send_buf) > 0) {
        write(sockfd, send_buf, strlen(send_buf));
    }
}
}
return 0;
}  
