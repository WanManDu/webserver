#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "csapp.h"  // CSAPP 라이브러리 사용

#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400

/* Recommended max cache and object sizes */
static const char *user_agent_hdr =
    "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 "
    "Firefox/10.0.3\r\n";

/* 함수 프로토타입 선언 */
void doit(int connfd);
void *thread(void *vargp);
void read_requesthdrs(rio_t *rp);
void parse_uri(char *uri, char *hostname, char *port, char *path);
void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg);

int main(int argc, char **argv) {

  signal(SIGPIPE, SIG_IGN);

  // int listenfd, client_fd;
  int listenfd, *clientfd;
  socklen_t clientlen;
  struct sockaddr_storage clientaddr;  // 클라이언트 주소 정보를 저장할 구조체
  char client_hostname[MAXLINE], client_port[MAXLINE];
  pthread_t tid;

  // 명령줄 인자 확인: 실행 시 포트 번호가 제공되었는지 확인
  if (argc != 2) {
    fprintf(stderr, "usage: %s <port>\n", argv[0]);
    exit(0);
  }

  // 서버 소켓 열기: 지정된 포트에서 클라이언트 연결 요청을 대기
  listenfd = Open_listenfd(argv[1]);

  // 무한 루프: 클라이언트의 연결을 계속해서 받아들이고 처리
  while (1) {
    clientlen = sizeof(clientaddr);
    clientfd = Malloc(sizeof(int));  //클라이언트의 연결 소켓을 위한 메모리 할당
    *clientfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);  //클라이언트 연결 수락
    Getnameinfo((SA *)&clientaddr, clientlen, client_hostname, MAXLINE, client_port, MAXLINE, 0);
    printf("Accepted connection from (%s, %s)\n", client_hostname, client_port);

    //스레드를 생성하여 클라이언트 요청을 처리
    Pthread_create(&tid, NULL, thread, clientfd);
  }
  return 0;
}

/*Thread routine*/
void *thread(void *argptr) {
  int clientfd = *((int *)argptr); //전달된 연결 소켓의 값을 가져옴
  Pthread_detach(pthread_self()); //스레드를 분리하여 종료 시 자동으로 자원 회수
  Free(argptr);  //동적 할당된 메모리를 해제
  doit(clientfd); //클라이언트 요청 처리
  Close(clientfd);//연결 종료
  return NULL;
}

void doit(int client_fd) {
    char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
    char hostname[MAXLINE], port[MAXLINE], path[MAXLINE];
    rio_t rio, server_rio;
    int server_fd;
    size_t n;

    // 클라이언트 요청 읽기 및 분석
    Rio_readinitb(&rio, client_fd);
    Rio_readlineb(&rio, buf, MAXLINE);
    sscanf(buf, "%s %s %s", method, uri, version);

    printf("Method: %s, URI: %s, Version: %s\n", method, uri, version);

    // GET 메소드만 지원
    if (strcasecmp(method, "GET")) {
        clienterror(client_fd, method, "501", "Not Implemented", "Proxy does not implement this method");
        return;
    }

    // 요청 헤더 읽기
    while (Rio_readlineb(&rio, buf, MAXLINE) > 0) {
        printf("Header Line: %s", buf); // 디버깅용 출력
        if (!strcmp(buf, "\r\n")) break; // 빈 줄을 만나면 헤더 끝

        // Host 헤더를 찾으면 호스트와 포트를 추출
        if (strncasecmp(buf, "Host:", 5) == 0) {
            char *host_start = buf + 5;
            while (*host_start == ' ') host_start++; // 공백을 건너뛰기

            // 호스트와 포트를 추출하는 부분 수정
            if (sscanf(host_start, "%[^:]:%s", hostname, port) == 1) {
                // 포트가 명시되지 않았을 경우 기본값 설정
                strcpy(port, "80");
            } else {
              strcpy(port, "80");
            }

            printf("Extracted Hostname: %s, Port: %s\n", hostname, port); // 확인용 출력
            break;
        }
    }

    // URI에서 경로만 추출
    parse_uri(uri, hostname, port, path); 

    printf("Parsed URI - Hostname: %s, Port: %s, Path: %s\n", hostname, port, path);

    // 대상 서버와의 연결 설정
    server_fd = Open_clientfd(hostname, port); //서버의 주소를 유동적으로 받을 수 있게 만들어야된다. 포트번호를 딸 수 있는 parse_uri를 짜야한다.
    if (server_fd < 0) {
        clienterror(client_fd, hostname, "404", "Not Found", "Proxy could not connect to server");
        return;
    }

    // 서버에 요청 보내기 위한 버퍼 초기화
    Rio_readinitb(&server_rio, server_fd);

    // 서버로 요청을 보냄
    sprintf(buf, "GET %s HTTP/1.0\r\n", path);
    Rio_writen(server_fd, buf, strlen(buf));
    sprintf(buf, "Host: %s\r\n", hostname);
    Rio_writen(server_fd, buf, strlen(buf));
    sprintf(buf, "Connection: close\r\n");
    Rio_writen(server_fd, buf, strlen(buf));
    sprintf(buf, "Proxy-Connection: close\r\n\r\n");
    Rio_writen(server_fd, buf, strlen(buf));

    // 서버로부터 응답을 받아서 클라이언트로 전달
    while ((n = Rio_readlineb(&server_rio, buf, MAXLINE)) != 0) {
        printf("Proxy received %ld bytes, sending to client\n", n);
        Rio_writen(client_fd, buf, n);
    }

    // 연결 종료
    Close(server_fd);
}

// 수정된 parse_uri 함수: hostname과 port를 수정하지 않음
void parse_uri(char *uri, char *hostname, char *port, char *path) {

  strcpy(port, "80");

  char *host_start = strchr(uri, '://');
  if (host_start != NULL) {
    uri = host_start + 2;
  }

  // URI에서 경로를 찾음
  char *path_start = strchr(uri, '/');
  if (path_start != NULL) {
    strcpy(path, path_start);  // 경로 복사
  } else {
    strcpy(path, "/");  // 경로가 없을 때 기본 경로 설정
  }

  // ':'를 찾아서 포트를 추출
  char *port_start = strchr(uri, ':');          //uri == 3.36.117.66:8000/ path_start
  if (port_start != NULL && (path_start == NULL || port_start < path_start)) {
    *port_start = '\0';  // ':' 이전의 문자열을 호스트로 취급
    port_start++;
    size_t port_length = path_start - port_start;
    strncpy(port, port_start, port_length);  // ':' 뒤에 포트 번호 복사
    port[port_length] = '\0'; //NULL로 문자열 끝을 지정
  }

  // '/'를 기준으로 호스트명과 경로를 분리
  char *host_end = (port_start != NULL) ? port_start : path_start;
  if (host_end != NULL) {
    size_t hostname_length = host_end - uri;
    strncpy(hostname, uri, hostname_length);  // 호스트명 복사
    hostname[hostname_length] = '\0';  // NULL로 문자열 끝을 지정
  } else {
      strcpy(hostname, uri);  // 경로나 포트가 없을 경우 전체 URI를 호스트로 간주
  }

  // 예외 처리: 호스트가 빈 경우 기본 호스트 설정
  if (strlen(hostname) == 0) {
    fprintf(stderr, "Error: Invalid hostname in URI: %s\n", uri);
    strcpy(hostname, "localhost");  // 기본 호스트 설정
  }

  // 디버깅 메시지 추가
  printf("Debug: Parsed URI - Hostname: %s, Port: %s, Path: %s\n", hostname, port, path);
}

/* 클라이언트 오류 메시지를 출력하는 함수 */
void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg) {
    char buf[MAXLINE], body[MAXBUF];

    // HTML 응답 본문 작성
    sprintf(body, "<html><title>Tiny Error</title>");
    sprintf(body, "%s<body bgcolor=\"ffffff\">\r\n", body);
    sprintf(body, "%s%s: %s\r\n", body, errnum, shortmsg);
    sprintf(body, "%s<p>%s: %s\r\n", body, longmsg, cause);
    sprintf(body, "%s<hr><em>The Proxy Web server</em>\r\n", body);

    // 클라이언트에게 응답 전송
    sprintf(buf, "HTTP/1.0 %s %s\r\n", errnum, shortmsg);
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Content-type: text/html\r\n");
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Content-length: %ld\r\n\r\n", strlen(body));
    Rio_writen(fd, buf, strlen(buf));
    Rio_writen(fd, body, strlen(body));
}


/* 요청 헤더를 읽어들이는 함수 */
void read_requesthdrs(rio_t *rp) {
  char buf[MAXLINE];

  // 요청 헤더 한 줄씩 읽기
  Rio_readlineb(rp, buf, MAXLINE);
  while (strcmp(buf, "\r\n")) {  // 빈 줄이 나올 때까지 반복
    Rio_readlineb(rp, buf, MAXLINE);
    printf("%s", buf);  // 요청 헤더 출력 (디버깅 용도)
  }
}
