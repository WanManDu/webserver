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
void doit(int client_fd);
void read_requesthdrs(rio_t *rp);
void parse_uri(char *uri, char *path);
void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg);
void get_host_and_port_from_headers(rio_t *rp, char *hostname, char *port);

int main(int argc, char **argv) {
  int listenfd, client_fd;
  socklen_t clientlen;
  struct sockaddr_storage clientaddr;  // 클라이언트 주소 정보를 저장할 구조체
  char client_hostname[MAXLINE], client_port[MAXLINE];

  // 명령줄 인자 확인: 실행 시 포트 번호가 제공되었는지 확인
  if (argc != 2) {
    fprintf(stderr, "usage: %s <port>\n", argv[0]);
    exit(1);
  }

  // 서버 소켓 열기: 지정된 포트에서 클라이언트 연결 요청을 대기
  listenfd = Open_listenfd(argv[1]);

  // 무한 루프: 클라이언트의 연결을 계속해서 받아들이고 처리
  while (1) {
    clientlen = sizeof(clientaddr);
    // 클라이언트의 연결 요청을 받아들임
    client_fd = Accept(listenfd, (SA *)&clientaddr, &clientlen);

    // 클라이언트의 호스트 이름과 포트 번호를 가져와 출력
    Getnameinfo((SA *)&clientaddr, clientlen, client_hostname, MAXLINE, client_port, MAXLINE, 0);
    printf("Accepted connection from (%s, %s)\n", client_hostname, client_port);

    // 클라이언트 요청 처리: doit 함수 호출
    doit(client_fd);

    // 연결 종료
    Close(client_fd);
  }
}

void forward_request_headers(rio_t *client_rio, int server_fd) {
    char buf[MAXLINE];

    // 클라이언트의 헤더를 읽어서 그대로 서버로 전달
    while (Rio_readlineb(client_rio, buf, MAXLINE) > 0) {
        // 빈 줄을 만나면 헤더 끝
        if (!strcmp(buf, "\r\n")) {
            Rio_writen(server_fd, buf, strlen(buf));
            break;
        }
        // 서버로 헤더를 전송
        Rio_writen(server_fd, buf, strlen(buf));
    }
}

void doit(int client_fd) {
    char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
    char hostname[MAXLINE], port[MAXLINE], path[MAXLINE]; 
    rio_t client_rio, server_rio;
    int server_fd;
    size_t n;

    // 클라이언트 요청 읽기 및 분석
    Rio_readinitb(&client_rio, client_fd);
    Rio_readlineb(&client_rio, buf, MAXLINE);
    sscanf(buf, "%s %s %s", method, uri, version);

    printf("Method: %s, URI: %s, Version: %s\n", method, uri, version);

    // GET 메소드만 지원
    if (strcasecmp(method, "GET")) {
        clienterror(client_fd, method, "501", "Not Implemented", "Proxy does not implement this method");
        return;
    }

    // 요청 헤더 읽기
    while (Rio_readlineb(&client_rio, buf, MAXLINE) > 0) {
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
            }

            printf("Extracted Hostname: %s, Port: %s\n", hostname, port); // 확인용 출력
            break;
        }
    }

    // URI에서 경로만 추출
    parse_uri(uri, path); // Host 헤더에서 받은 hostname과 port는 그대로 사용

    printf("Parsed URI - Hostname: %s, Port: %s, Path: %s\n", hostname, port, path);

    // 대상 서버와의 연결 설정
    server_fd = Open_clientfd(hostname, "8000");
    if (server_fd < 0) {
        clienterror(client_fd, hostname, "404", "Not Found", "Proxy could not connect to server");
        return;
    }

    // 서버에 요청 보내기 위한 버퍼 초기화
    Rio_readinitb(&server_rio, server_fd);

    // 서버로 클라이언트의 요청을 보냄
    sprintf(buf, "GET %s HTTP/1.0\r\n", path);
    Rio_writen(server_fd, buf, strlen(buf));
    sprintf(buf, "Host: %s\r\n", hostname);
    Rio_writen(server_fd, buf, strlen(buf));
    sprintf(buf, "Connection: close\r\n");
    Rio_writen(server_fd, buf, strlen(buf));
    sprintf(buf, "Proxy-Connection: close\r\n\r\n");
    Rio_writen(server_fd, buf, strlen(buf));

    // 나머지 요청 헤더 전달
    forward_request_headers(&client_rio, server_fd);

    // 서버로부터 응답을 받아서 클라이언트로 전달
    while ((n = Rio_readlineb(&server_rio, buf, MAXLINE)) != 0) {
        printf("Proxy received %ld bytes, sending to client\n", n);
        Rio_writen(client_fd, buf, n);
    }

    // 연결 종료
    Close(server_fd);
}

// 수정된 parse_uri 함수: hostname과 port를 수정하지 않음
void parse_uri(char *uri, char *path) {
    // 기본적으로 URI가 '/'로 시작하면 그대로 path로 설정
    char *path_start = strchr(uri, '/');
    if (path_start != NULL) {
        strcpy(path, path_start);  // 경로 복사
    } else {
        strcpy(path, "/");  // 경로가 없을 때 기본 경로 설정
    }

    // 디버깅 메시지 추가
    printf("Debug: Parsed Path - Path: %s\n", path);
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

// Host 헤더에서 호스트와 포트를 추출하는 함수
void get_host_and_port_from_headers(rio_t *rp, char *hostname, char *port) {
  char buf[MAXLINE];
  strcpy(port, "80"); // 기본 포트 80 설정

  // 헤더를 읽어서 Host 필드를 찾음
  while (Rio_readlineb(rp, buf, MAXLINE) > 0) {
    if (!strcmp(buf, "\r\n")) break; // 빈 줄을 만나면 헤더 끝

    // Host 헤더를 찾으면 호스트와 포트를 추출
    if (strncasecmp(buf, "Host:", 5) == 0) {
      char *host_start = buf + 5; // "Host:" 다음부터 시작
      sscanf(host_start, "%s", hostname);

      // 포트가 있는 경우 추출
      char *port_start = strchr(hostname, ':');
      if (port_start != NULL) {
        *port_start = '\0'; // 호스트와 포트 분리
        strcpy(port, port_start + 1);
      }
      break;
    }
  }
}