/*===============================================
*   文件名称：server.c
*   创 建 者： 张峰源    
*   创建日期：2026年09月11日
*   描    述：
================================================*/
#include <stdio.h>
#include <sys/types.h>          /* See NOTES */
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <pthread.h>
#include <string.h>
#include <time.h>
#include <sqlite3.h>
#include <stdlib.h>

#define max_line 20
char chepai[max_line][10] = {0};//这个代表的是每个位置上的车牌号，前一个表示[]停车场的车位，后一个表示[]车牌号
int car_pos[max_line] = {0};
int pos = 0;
int num_pos = 0;

sqlite3 *db = NULL;//全局变量共享数据库


//接收客户端指令再作出反馈
void *myrecv(void *arg)
{
    int rws = *(int *)arg;
    char send_buf[128]={0};
    while(1)
    {
        if(fgets(send_buf,sizeof(send_buf),stdin) == NULL)
            break;
        write(rws,send_buf,strlen(send_buf));
    }
}

//匹配停车位，停车
int dtb_pos(char *temp)
{
    int cp = -1;
    for(int i = 0;i<max_line;i++)
    {   
        if(chepai[i][0] == '\0')//表示空车位
        {   
            //将temp（车牌号）输入给chepai[i]（车位），表示给这辆车找好了车位
            strncpy(chepai[i],temp,sizeof(chepai[i]) -1);
            chepai[i][sizeof(chepai[i]) - 1] = '\0';
            cp = i+1;//表示当前的车位,加一是因为数组从0开始记位;
            break;
        }
    }
    return cp;
}

//先匹配停车位，再开出停车场
int leave_pos(char *temp)
{
    int cp = -1;
    for(int i = 0;i<max_line;i++)
    {
        if(strcmp(chepai[i],temp)==0)
        {
            cp = i+1;
            break;
        }
    }
    return cp;
}

//第一步，停车的操作
int parking(int rws)
{   //先判断
    char *ch = "请选择你的操作：1.park 2.leave\n";
    write(rws,ch,strlen(ch));
    char choose[1024] = {0};
    int n =read (rws,choose,sizeof(choose)-1);//预留一个字符串给\0
    if(n>0)choose[n] = '\0';

    //先处理停车的信息
    if(strcmp(choose,"park")==0)
    {
        while(1)
        {      
            write(rws,"请输入车牌号:\n",strlen("请输入车牌号:\n"));
            char temp[64]={0};
            n = read(rws,temp,sizeof(temp)-1);
            if(n>0)temp[n] = '\0';//手动添加关键字符串结束
            
            int found = 0;
            for(int i=0;i<max_line;i++)
            {
                if(strcmp(temp,chepai[i])==0)
                {
                    found = 1;
                    break;
                }
            }
            if(found == 1)
            {   char *err = "车牌号错误，该车已在停车场内，请重新输入\n";
                write(rws,err,strlen(err));         
            }
            else
            {   char *wait_msg = "车牌正确，正在为您匹配车位，请稍后...\n";
                write(rws,wait_msg,strlen(wait_msg));
                int num = dtb_pos(temp);
                if(num != -1)
                {
                    char buf[1024] ={0};
                    strcpy(buf,"您的车位为：");
                    //num是int型变量，需要将其转换为char型，存入数组
                    char str[100] = {0};
                    sprintf(str,"%d",num);
                    
                    strcat(buf,str);
                    strcat(buf,"\n");
                    write(rws,buf,strlen(buf));

                    //将车牌号写入数据库
                    char sql[1024];
                    char *errmsg = NULL;
                    snprintf(sql, sizeof(sql),"INSERT INTO parking_records (plate, entry_time) VALUES ('%s', datetime('now', 'localtime'));",temp);
                    sqlite3_exec(db, sql, NULL, NULL, &errmsg);
                    if(errmsg) { fprintf(stderr, "DB Error: %s\n", errmsg); sqlite3_free(errmsg); }
                }
                else
                {   char *full_msg = "抱歉，当前车位已满\n";
                    write(rws,full_msg,strlen(full_msg));
                }
                break;
            }
        }
    }

    //再处理驶离的信息
    if(strcmp(choose,"leave")==0)
    {   
        while(1)
        {
         write(rws,"请输入车牌号:\n",strlen("请输入车牌号:\n"));
         char temp[64]={0};
         n = read(rws,temp,sizeof(temp)-1);
         if(n>0)temp[n] = '\0';
         
         int found = 0;
         for(int i=0;i<max_line;i++)
         {
            if(strcmp(temp,chepai[i])==0)
            {
                found = 1;
                break;
            }

         }
            if(found == 1)
            {   
                int num = leave_pos(temp);
                char buf[1024] = {0};
                strcpy(buf,"车牌号正确，您的车位为：");
                char str[100] = {0};
                sprintf(str,"%d",num);

                strcat(buf,str);
                strcat(buf,"，请缴费后离开");
                strcat(buf,"\n");
                write(rws,buf,strlen(buf));
                memset(chepai[num - 1], 0, sizeof(chepai[num - 1]));//车子离开，将车位清空
                break;
            }
            else
            {
                write(rws,"车牌号错误，停车场内未识别到该车辆，请重新输入正确的车牌号\n",strlen("车牌号错误，停车场内未识别到该车辆，请重新输入正确的车牌号\n"));
                break;
            }
         }
    }
}


void math_money(int rws)
{
    write(rws,"请输入车牌号以计算费用:",strlen("请输入车牌号以计算费用:"));
    char temp[64] =  {0};
    int n = read(rws, temp, sizeof(temp) - 1);
    if (n > 0) temp[n] = '\0';

    char sql[256];
    sqlite3_stmt *stmt;
    double fee = 0.0;
    snprintf(sql, sizeof(sql), 
             "SELECT (julianday('now', 'localtime') - julianday(entry_time)) * 86400 / 5 as cost "
             "FROM parking_records WHERE plate='%s' AND exit_time IS NULL;",temp);

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, 0) == SQLITE_OK) {
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            fee = sqlite3_column_double(stmt, 0);
            if (fee < 1.0) fee = 1.0; // 测试用：最低消费改为 1 元
        }
        sqlite3_finalize(stmt);
    }

    char buf[128];
    snprintf(buf, sizeof(buf), "您的停车费用为：%.2f 元。请返回主界面进行支付。\n", fee);
    write(rws, buf, strlen(buf));
}

void pay_money(int rws)
{
    write(rws, "请输入车牌号进行支付:", strlen("请输入车牌号进行支付:"));
    char temp[64] = {0};
    int n = read(rws, temp, sizeof(temp) - 1);
    if (n > 0) temp[n] = '\0';
    
    // 【新增】查询该车当前费用
    char sql[256];
    sqlite3_stmt *stmt;
    double fee = 0.0;
    snprintf(sql, sizeof(sql),"SELECT (julianday('now', 'localtime') - julianday(entry_time)) * 86400 / 5 as cost ""FROM parking_records WHERE plate='%s' AND exit_time IS NULL;", temp);

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, 0) == SQLITE_OK) {
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            fee = sqlite3_column_double(stmt, 0);
            if (fee < 1.0) fee = 1.0;
        }
        sqlite3_finalize(stmt);
    }

    // 【新增】把费用告诉用户
    char buf[128];
    snprintf(buf, sizeof(buf), "您的停车费用为：%.2f 元。请输入支付金额：", fee);
    write(rws, buf, strlen(buf));

    // 【新增】等待用户输入金额
    char money_buf[64] = {0};
    read(rws, money_buf, sizeof(money_buf) - 1);
    double pay_amount = atof(money_buf);

    // 【修改】金额核对
if (pay_amount >= fee - 0.01 && pay_amount <= fee + 0.01) {
    // 金额对上了，才执行更新
    char update_sql[256];
    snprintf(update_sql, sizeof(update_sql),
             "UPDATE parking_records SET exit_time = datetime('now', 'localtime'), "
             "fee = %.2f "
             "WHERE plate='%s' AND exit_time IS NULL;",
             pay_amount, temp);

    char *errmsg = NULL;
    int rc = sqlite3_exec(db, update_sql, NULL, NULL, &errmsg);
    if (rc == SQLITE_OK && sqlite3_changes(db) > 0) {
        write(rws, "支付成功！感谢您的使用，一路顺风！\n", strlen("支付成功！感谢您的使用，一路顺风！\n"));
    } else {
        write(rws, "支付失败，未找到该车入场记录\n", strlen("支付失败，未找到该车入场记录\n"));
        if (errmsg) sqlite3_free(errmsg);
    }
} else {
    // 金额不对
    char err[128];
    snprintf(err, sizeof(err), "支付失败！应付 %.2f 元，实付 %.2f 元，请重新支付。\n", fee, pay_amount);
    write(rws, err, strlen(err));
}
}

void history_search(int rws)
{
    write(rws, "请输入车牌号查询历史记录:", strlen("请输入车牌号查询历史记录:"));
    char temp[64] = {0};
    int n = read(rws, temp, sizeof(temp) - 1);
    if (n > 0) temp[n] = '\0';

    char sql[256];
    sqlite3_stmt *stmt;
    char result[512] = {0}; // 用来拼接返回给客户端的字符串

    // 核心：查询该车牌的所有记录（包括还在停的和已经离开的）
    snprintf(sql, sizeof(sql), 
             "SELECT entry_time, exit_time, fee FROM parking_records WHERE plate='%s' ORDER BY id DESC LIMIT 5;", temp);

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, 0) == SQLITE_OK) {
        int found = 0;
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            found = 1;
            char entry[50], exit_t[50], fee_str[20];
            snprintf(entry, sizeof(entry), "%s", sqlite3_column_text(stmt, 0));
            
            // 如果 exit_time 为空，说明还在场内
            if (sqlite3_column_text(stmt, 1) == NULL) {
                snprintf(exit_t, sizeof(exit_t), "还在场内");
                snprintf(fee_str, sizeof(fee_str), "未结算");
            } else {
                snprintf(exit_t, sizeof(exit_t), "%s", sqlite3_column_text(stmt, 1));
                snprintf(fee_str, sizeof(fee_str), "%.2f", sqlite3_column_double(stmt, 2));
            }
            
            char line[1024];
            snprintf(line, sizeof(line), "入场:%s | 出场:%s | 费用:%s元\n", entry, exit_t, fee_str);
            strcat(result, line);
        }
        sqlite3_finalize(stmt);

        if (!found) {
            write(rws, "未找到该车辆的任何记录", strlen("未找到该车辆的任何记录"));
        } else {
            write(rws, result, strlen(result));
        }
    }
}

int main(int argc, char *argv[])
{   
    int rc = sqlite3_open("parking.db",&db);//打开一个停车的数据库，将停车信息放在里面
    if(rc != SQLITE_OK)
    {
        fprintf(stderr,"无法打开数据库：%s\n",sqlite3_errmsg(db));
        return -1;
    }
    //创建数据库表
    char *sql = "CREATE TABLE IF NOT EXISTS parking_records ("
            "id INTEGER PRIMARY KEY AUTOINCREMENT, "//序号
            "plate TEXT NOT NULL, "//车牌号
            "entry_time TEXT NOT NULL, "//进入时间
            "exit_time TEXT, "//离开时间
            "fee REAL DEFAULT 0);";//费用
    char *errmsg = NULL;
    sqlite3_exec(db, sql, NULL, NULL, &errmsg);
    if (errmsg) { sqlite3_free(errmsg); };//启动阶段释放错误信息,防止泄漏内存


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
    int ret = bind(sockfd,(struct sockaddr*)&myserver,s_size);
    if(ret < 0)
    {
        perror("bind");
        return -1;
    }
    listen(sockfd,10);
    pthread_t thread;
p:
    puts("等待客户端连接....");
    struct sockaddr_in con_client;//客户端的结构体
    int c_size = sizeof(con_client);
    int rws = accept(sockfd,(struct sockaddr*)&con_client,&c_size);
    puts("客户端连接成功!");
    pthread_create(&thread,NULL,myrecv,&rws);
    pthread_detach(thread);  //回收线程资源
m:    
{
    char *msg = "欢迎进入无人停车管理系统：1.停车/离开 2.费用计算 3.缴费 4.数据查询\n";
    write(rws,msg,strlen(msg));
    char recv_buf[128]={0};
    while(1)
    {
        ret = read(rws,recv_buf,128);
        if(ret > 0) recv_buf[ret] = '\0';
        recv_buf[strcspn(recv_buf, "\r\n")] = '\0';
        if(ret == 0)
        {
            printf("有一个客户端断开...\n");
            close(rws);
            goto p;
        }
        if(strcmp(recv_buf,"1") == 0)
        {
            parking(rws);//执行第一步操作,停车/驶离
            goto m;
        }
        if(strcmp(recv_buf,"2") == 0)//计算费用
        {
            math_money(rws);
            goto m;
        }
        if(strcmp(recv_buf,"3") == 0)//支付
        {
            pay_money(rws);
            goto m;
        }
        if(strcmp(recv_buf,"4") == 0)//输入车牌号，查询停车记录
        {
            history_search(rws);
            goto m;
        }
        printf("接受客户端的消息为:%s\n",recv_buf);
    }
    return 0;
}
}
