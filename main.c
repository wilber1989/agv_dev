#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/time.h>

#include <cjson/cJSON.h>
#include <mqtt.h>
#include "templates/posix_sockets.h"

const char * deviceID;//获取shell当中的环境变量设备ID
char rev_msg[512]={0};//获取订阅消息变量
char tmpTopic[50] = {0};//拼接各个主题
int sockfd = -1;//socket句柄

char* dpath = NULL;//项目文件路径
char* tmpath = NULL;//拼接各类路径
char* cut = NULL;//裁剪拼接字符串

struct mqtt_client client;
pthread_t client_daemon;

#define DPATH 

/*结束关闭socket*/
void exit_example(int status, int sockfd, pthread_t *client_daemon)
{
    memset(tmpTopic,0,sizeof(tmpTopic)); 
    snprintf(tmpTopic,sizeof(tmpTopic),"%s%s%s","/devices/",deviceID,"/onlinemsg"); 
    mqtt_publish(&client, tmpTopic, "offline", 
                 strlen("offline"), MQTT_PUBLISH_QOS_1| MQTT_PUBLISH_RETAIN); 
    mqtt_disconnect(&client);
    usleep(2000000U);
    printf("\033[1m\033[45;33m设备连接已断开！\033[0m\n\n");
    if (sockfd != -1) close(sockfd);
    if (client_daemon != NULL) pthread_cancel(*client_daemon);
    exit(status);
}

/*0.1秒同步一次客户端便于接受数据*/
void* client_refresher(void* client)
{
    while(1) 
    {
        mqtt_sync((struct mqtt_client*) client);
        usleep(100000U);
    }
    return NULL;
}
/*客户端回调函数，用于发布消息回调，订阅消息接收*/
void publish_callback(void** unused, struct mqtt_response_publish *published) 
{
    /* note that published->topic_name is NOT null-terminated (here we'll change it to a c-string) */
    char* topic_name = (char*) malloc(published->topic_name_size + 1);
    memcpy(topic_name, published->topic_name, published->topic_name_size);
    topic_name[published->topic_name_size] = '\0';
    //usleep(2000000U);
    //printf("\033[1m\033[45;32m主题('%s')最新消息:\n %s\033[0m\n", topic_name, (const char*) published->application_message);
    strcpy(rev_msg,(const char*) published->application_message);
    free(topic_name);
}



int GetDateTime(char * psDateTime) {
    time_t nSeconds;
    struct tm * pTM;
    
    time(&nSeconds);
    pTM = localtime(&nSeconds);

    /* 系统日期和时间,格式: yyyymmddHHMMSS */
    sprintf(psDateTime, "%04d-%02d-%02dT%02d:%02d:%02dZ",
            pTM->tm_year + 1900, pTM->tm_mon + 1, pTM->tm_mday,
            pTM->tm_hour, pTM->tm_min, pTM->tm_sec);
            
    return 0;
}
int count = 0;
int count2 = 100;

int collect(const char* topic)
{
    count++;
    printf("----------------------------------------\n");
    printf("\033[1m\033[45;33m              第%d次全量采集            \033[0m\n",count);
    printf("----------------------------------------\n");
     

    /*创建json并摘要*/
    cJSON *root;   
    root=cJSON_CreateObject();
    cJSON_AddStringToObject(root,"clientid","AGV-2020");
    cJSON_AddStringToObject(root,"task","[(0,0),(1,0),(1,1),(1,2),(1,3),(1,4),(2,4),(3,4),(4,4),(4,5),(5,5)]"); 
    cJSON_AddStringToObject(root,"position","(0,0)");
    
    printf("数据采集中............\n\n");
    usleep(1000000U);
    if(count<10 && count%10 == 5)  count2-=1;
    if(count>=10 && count <50 && (count%10 == 3 || count%10 == 8))  count2-=1;
    cJSON_AddNumberToObject(root,"power", count2);

    char DateTime[18];
    memset(DateTime, 0, sizeof(DateTime));
    int ret = GetDateTime(DateTime);
    if(ret != 0)
    {
        printf("GetDateTime error!");
        exit_example(EXIT_FAILURE, sockfd, &client_daemon);
    }
    cJSON_AddStringToObject(root,"time",DateTime);
    char p[10] = {0};
    sprintf(p, "%0.2f", 1.2-2.0*(rand()%10)*0.01-0.1);
    cJSON_AddStringToObject(root,"speed", p);
    cJSON_AddStringToObject(root,"ip","192.168.31.246");

    char* json1_1 = cJSON_Print(root);

    printf("\033[1m\033[45;33m采集数据上传:\033[0m\n\n");
    usleep(200000U);
    printf("%s\n\n",json1_1);
    cJSON_Delete(root);
    

    mqtt_publish(&client, topic, json1_1, strlen((const char *)json1_1), MQTT_PUBLISH_QOS_0);   
    if (client.error != MQTT_OK) 
    {
        fprintf(stderr, "error: %s\n", mqtt_error_str(client.error));
        exit_example(EXIT_FAILURE, sockfd, &client_daemon);
    }   
    
    root =NULL;
    json1_1 = NULL;
    return 0;
}

int main(int argc, const char *argv[]) 
{
    const char* addr;
    const char* port;
    /* get address (argv[1] if present) */
    if (argc > 1) {
        addr = argv[1];
    } else {
        //addr = "218.89.239.8";
        //addr = "127.0.0.1";
        //addr = "192.168.31.246";
        //addr = "192.168.31.183";
        addr = "47.112.10.111";
    }
    /* get port number (argv[2] if present) */
    if (argc > 2) {
        port = argv[2];
    } else {
        port = "1883";
    }

    printf("----------------------------------------\n");
    printf("\033[1m\033[45;33m              终端采集程序              \033[0m\n");
    printf("----------------------------------------\n");
    printf("\033[1m\033[45;33mPress ENTER to start.\033[0m\n");
    printf("----------------------------------------\n");
    printf("\033[1m\033[45;33mPress CTRL+D to exit.\033[0m\n");
    printf("----------------------------------------\n");
    
    while(fgetc(stdin)!= '\n');
    dpath = getenv("DPATH");
    //printf("dpath:%s\n",dpath ); 
    tmpath= dpath;
    deviceID = getenv("DEVICEID");
    //createKey();//创建密钥时用
    sockfd = open_nb_socket(addr, port);
    if (sockfd == -1) {
        perror("Failed to open socket: ");
        exit_example(EXIT_FAILURE, sockfd, NULL);
    }
    fcntl(sockfd, F_SETFL, fcntl(sockfd, F_GETFL) | O_NONBLOCK);

    /*建立mqtt客户端*/
    uint8_t sendbuf[2048]; 
    uint8_t recvbuf[1024]; 
    mqtt_init(&client, sockfd, sendbuf, sizeof(sendbuf), recvbuf, sizeof(recvbuf), publish_callback); 
    
    snprintf(tmpTopic,sizeof(tmpTopic),"%s%s%s","/devices/",deviceID,"/onlinemsg");
    mqtt_connect(&client, deviceID, tmpTopic, "exception", 
                strlen("exception"), NULL, NULL, MQTT_PUBLISH_QOS_1| MQTT_CONNECT_WILL_RETAIN, 400);
    if (client.error != MQTT_OK) 
    {
        fprintf(stderr, "error: %s\n", mqtt_error_str(client.error));
        exit_example(EXIT_FAILURE, sockfd, NULL);
    }

    if(pthread_create(&client_daemon, NULL, client_refresher, &client))
    {
        fprintf(stderr, "Failed to start client daemon.\n");
        exit_example(EXIT_FAILURE, sockfd, NULL);

    }
    memset(tmpTopic,0,sizeof(tmpTopic));
    snprintf(tmpTopic,sizeof(tmpTopic),"%s%s%s","/devices/",deviceID,"/onlinemsg"); 
    mqtt_publish(&client, tmpTopic, "online", 
                 strlen("online"), MQTT_PUBLISH_QOS_1| MQTT_PUBLISH_RETAIN);
    
    /*设备采集流程*/
    while(1)
    {
    memset(tmpTopic,0,sizeof(tmpTopic));
    snprintf(tmpTopic,sizeof(tmpTopic),"%s%s%s","/devices/",deviceID,"/realtime/status"); 
    collect(tmpTopic); 
    }
    
     
    while(fgetc(stdin)!=EOF);
    exit_example(EXIT_SUCCESS, sockfd, NULL);
    return 0;
}