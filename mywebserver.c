#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <signal.h>
#include <unistd.h>
#include <pthread.h>

#define WEB_ROOT "./www"


void* send_thr(void* arg);


int main(int argc, char** argv)
{
	if(argc != 2)
	{
		fprintf(stderr, "Usage error!\n");
		exit(1);
	}

	signal(SIGPIPE, SIG_IGN);

	int sock_listen;
	sock_listen = socket(AF_INET, SOCK_STREAM, 0);

	//setsockopt函数：设置套接字属性
	//将套接字的SO_REUSEADDR属性设置为1，即允许地址复用
	int optval = 1;
	setsockopt(sock_listen, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));


	struct sockaddr_in myaddr;
	myaddr.sin_family = AF_INET; //指定地址家族为Internet地址家族
	myaddr.sin_addr.s_addr = INADDR_ANY; //指定IP地址为本机任意IP
	myaddr.sin_port = htons(atoi(argv[1])); //指定端口号

	if(bind(sock_listen, (struct sockaddr*)&myaddr, sizeof(myaddr)) == -1)
	{
		perror("bind error");
		exit(1);
	}

	listen(sock_listen, 5);

	struct sockaddr_in clnaddr;
	socklen_t len;

	while(1)
	{	
		int sock_conn; //连接套接字，用于和相应的客户端通信

		len = sizeof(clnaddr);
		sock_conn = accept(sock_listen, (struct sockaddr*)&clnaddr, &len);

		if(sock_conn == -1)
		{
			perror("accept error");
			continue;
		}

		pthread_t tid;
		if(pthread_create(&tid, NULL, send_thr, (void*)(long)sock_conn))
		{
			perror("create new thread fail");
			close(sock_conn);
		}
	}
	
	close(sock_listen);	
	
	return 0;
}


void* send_thr(void* arg)
{
	int sock_conn = (long)arg;

	pthread_detach(pthread_self());

	struct sockaddr_in clnaddr;
	socklen_t len = sizeof(clnaddr);
	getpeername(sock_conn, (struct sockaddr*)&clnaddr, &len);

	//接收客户端连接请求成功
	printf("\n客户端%s:%d上线！\n", inet_ntoa(clnaddr.sin_addr), ntohs(clnaddr.sin_port));

	//设置send和recv超时时间都为2S
	struct timeval timeout = {2, 0};
	setsockopt(sock_conn, SOL_SOCKET,SO_SNDTIMEO, (char *)&timeout, sizeof(struct timeval));
	setsockopt(sock_conn, SOL_SOCKET,SO_RCVTIMEO, (char *)&timeout, sizeof(struct timeval));


	char request[1025];
	char response[1025];
	int ret;

	ret = recv(sock_conn, request, sizeof(request) - 1, 0);

	if(ret > 0)
	{
		request[ret] = '\0';
		printf("\n浏览器说：\n%s\n----------------------------\n", request);
	}
	
	char method[21], url[301], file_path[301];
	char* p = NULL;

	sscanf(request, "%s %s", method, url);
	
	p = strchr(url, '?');
	
	if(p != NULL) *p = '\0';
	
	if(strcmp(url, "/") == 0)
		sprintf(file_path, "%s/index.html", WEB_ROOT);
	else
		sprintf(file_path, "%s%s", WEB_ROOT, url);

	sprintf(response, "HTTP/1.1 200 OK\r\n"
			"Connection: close\r\n"
			"Content-Type: text/html; charset=utf-8\r\n"
			"\r\n");
	send(sock_conn, response, strlen(response), 0);


	FILE* fp = fopen(file_path, "rb");	
	char buff[1024];

	if(fp == NULL)
	{
		sprintf(file_path, "%s/404.html", WEB_ROOT );
		fp = fopen(file_path, "rb");
	}

	while(!feof(fp))
	{
		ret = fread(buff, 1, sizeof(buff), fp);
		send(sock_conn, buff, ret, 0);
	}		

	fclose(fp);

	printf("\n客户端%s:%d下线！\n", inet_ntoa(clnaddr.sin_addr), ntohs(clnaddr.sin_port));
	close(sock_conn);	

	return NULL;
}

