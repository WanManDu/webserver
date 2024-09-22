/* $begin tinymain */
/*
 * tiny.c - A simple, iterative HTTP/1.0 Web server that uses the
 *     GET method to serve static and dynamic content.
 *
 * Updated 11/2019 droh
 *   - Fixed sprintf() aliasing issue in serve_static(), and clienterror().
 */
#include "csapp.h" //CSAPP에서 제공하는 함수들을 사용하기 위한 헤더


/*함수 프로토타입 선언*/
void doit(int fd);
void read_requesthdrs(rio_t *rp);
int parse_uri(char *uri, char *filename, char *cgiargs);
void serve_static(int fd, char *filename, int filesize);
void get_filetype(char *filename, char *filetype);
void serve_dynamic(int fd, char *filename, char *cgiargs);
void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg);

int main(int argc, char **argv) { //argc : 프로그램이 실행될 때 전달된 인자의 개수를 나타내는 정수
                                  //예를 들어, 커맨드 라인에서 ./tiny 8000과 같이 실행하면 argc는 2가 된다.(./tiny, 8000)
                                  //argv : 프로그램 실행 시 전달된 인자들을 문자열 포인터 배열로 가리킨다. argv[0]은 프로그램 이름(./tiny), argv[1]는 첫번째 인자(8000)이다.
  // SIGPIPE 신호를 무시하도록 설정
  signal(SIGPIPE, SIG_IGN);
  //SIGPIEP는 서버가 클라이언트로 데이터를 전송할 때 클라이언트가 연결을 끊으면 발생하는 시그널
  //기본적으로 이 시그널을 받으면 프로그램이 종료된다.
  //signal(SIGPIPE, SIG_IGN)은 이 시그널을 무시하도록 설정하는 코드이다.
  //클라이언트가 갑자기 연결을 끊더라도 서버 프로그램이 종료되지 않고 계속 실행되도록 한다.
  int listenfd, connfd;
  //listenfd(listening file descriptor) : 서버 소켓을
  // 생성하고 클라이언트의 연결을 대기하는 데 사용된다
  //서버는 이 파일 디스크립터를 사용해 클라이언트 요청을 듣는다.
  //connf(connected file descriptor): 클라이언트의 연결 요청이 들어오면 
  //accept() 함수를 통해 connfd에 새로운 연결이 할당된다.
  //서버는 이 파일 디스크립터를 통해 클라이언트와 데이터를 주고받는다.

  char hostname[MAXLINE], port[MAXLINE];
  //hostname과 port는 클라이언트의 호스트명과 포트 번호를 저장하기 위한 버퍼이다.
  //클라이언트의 소켓 정보를 getnameinfo() 등을 통해 읽어 올 때 사용된다.

  socklen_t clientlen;
  //clientlen은 클라이언트 주소 구조체(cllientaddr)의 크기를 저장하는 변수이다.
  //accept()함수 호출 시 클라이언트 주소의 크기를 지정하는 데 사용되며, 보통 socklen_t타입으로 선언된다.

  struct sockaddr_storage clientaddr;
  //struct sockaddr_storage는 모든 주소 구조체(sockaddr_in, sockaddr_in6 등)를 담을 수 있는 일반적인 구조체이다.
  //클라이언트의 IP주소와 포트 정보를 저장하기 위해 사용된다.

  /* 명렬줄 인자가 올바른지 확인 */
  if (argc != 2) {
    fprintf(stderr, "usage: %s <port>\n", argv[0]); //명령어 사용법 안내
    exit(1);
  }

  listenfd = Open_listenfd(argv[1]);  //서버 소켓을 열고 대기 (CSAPP의 Open_listenfd 함수 사용)
  while (1) { //키보드 interrupt 또는 에러일때 종료가 된다.
    clientlen = sizeof(clientaddr);
    connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);  // 클라이언트 연결 수락(csapp의 Accept 함수 사용)
    //Accept함수는 서버가 대기 중인 연결 요청을 받아들이고, 클라이언트와의 연결을 설정하는 역할을 한다.
    //Accept함수는 클라이언트가 연결을 요청할 때, 해당 연결을 받아들이고, 클라이언트의 주소를  clientaddr에 저장.
    //clientlen은 이 주소 정보를 저장할 버퍼의 크기를 알려주는 매개변수
    //Accept 함수가 성공하면 새로 연결된 소켓 파일 디스크립터(connfd)를 반환
    //이는 서버 클라이언트 간의 통신 채널을 나타낸다.
    Getnameinfo((SA *)&clientaddr, clientlen, hostname, MAXLINE, port, MAXLINE, 0); //클라이언트의 주소 정보 가져오기(csapp의 Getnameinfo 함수 사용)
    //Getnameinfo를 사용하여 로그 기록과 디버깅에 사용한다.
    //서버가 어떤 클라이언트와 연결되었는지 추적할 수 있다.
    //서버가 연결된 클라이언트에 대한 정보를 출력하거나 기록함으로써, 시스템 관리자는 서버의 동작을 실시간으로 모니터링 할 수 있다.
    printf("Accepted connection from (%s, %s)\n", hostname, port);  //연결된 클라이언트 정보 출력
    doit(connfd);   // line:netp:tiny:doit  | 클라이언트 요청 처리 (doit 함수로 전달)
    Close(connfd);  // line:netp:tiny:close | 소켓 닫기(csapp의 Close 함수 사용)
  }
  return 0;
}

/*클라이언트의 요청을 처리하는 함수*/
void doit(int fd) {
  // signal(SIGPIPE, SIG_IGN);

  int is_static;
  //is_static은 요청된 콘텐츠가 정적 콘텐츠인지 또는 동적 콘텐츠인지 판별하기 위해 사용
  //이 변수를 통해 서버가 클라이언트 요청을 어떻게 처리할지를 결정

  struct stat sbuf;
  //sbuf는 파일의 상태를 나타내는 stat구조체이다
  //stat 구조체는 파일의 크기, 수정 시간, 파일 유형(디렉토리, 일반 파일 등)과 같은 파일 메타데이터를 포함
  //이 구조체는 파일이 존재하는지, 접근 가능한지 등을 확인할 때 사용

  char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
  char filename[MAXLINE], cgiargs[MAXLINE];

  //buf는 클라이언트로부터 읽어온 요청 라인을 저장하는데 사용. 이 버퍼를 통해 요청의 전체적인 내용을 파악할 수 있음
  //method는 HTTP요청 메서드를 저장. 서버는 이 값을 통해 어떤 요청을 처리해야 할 지를 결정
  //uri는 요청된 리소스의 URI를 저장. URI는 클라이언트가 어떤 파일이나 서비스를 요청하고 있는지를 식별하는 문자열
  //version은 HTTP 버전 정보를 저장. 요청 처리에 필요한 프로토콜 버전을 파악할 수 있음
  //filename은 요청된 URI를 바탕으로 실제 서버 파일 시스템에서의 파일 이름을 저장. 파일 결로를 설정하여 서버가 접근 할 수 있도록 한다.
  //cgiargs는 동적 콘텐츠를 처리할 때 CGI 프로그램에 전달할 인자들을 저장. URI의 쿼리 문자열에서 파싱된 인자들이 저장됨
  
  rio_t rio;
  //rio_t는 CSAPP에서 제공하는 robust I/O구조체로, robust I/O는 안전하고 효율적인 입출력을 위한 패키지
  //이 구조체는 입출력의 버퍼링을 관리하고, 읽기/쓰기 작업에서 발생할 수 있는 다양한 오류를 방지하는 역할

  Rio_readinitb(&rio, fd);            //버러링된 I/O 초기화 (csapp의 Rio_readinitb 함수 사용)
  Rio_readlineb(&rio, buf, MAXLINE);  //클라이언트의 요청 라인을 읽어옴(csapp의 Rio_readline 함수 사용)
  printf("Request headers:/\n");
  printf("%s", buf);
  sscanf(buf, "%s %s %s", method, uri, version);  //클라이언트가 보낸 요청에서 HTTP 메소드, 요청한 주소(URI), 그리고 HTTP 버전을 뽑아냄
  if (strcasecmp(method, "GET")) {                //GET 메소드 이외는 지원하지 않음
    clienterror(fd, method, "501", "Not implemented", "Tiny does not implement this method"); //에러 처리
    return;
  }
  read_requesthdrs(&rio);   //추가 요청 헤더를 읽음

  is_static = parse_uri(uri, filename, cgiargs);  //URI를 파싱하여 파일 이름과 CGI인자를 구분
  if (stat(filename, &sbuf) < 0) {      //파일이 존재하는지 확인(stat 함수 사용)
    clienterror(fd, filename, "404", "Not found", "Tiny couldn't find this file"); //파일이 없으면 에러처리
    return;
  }

  if (is_static) {    //정적 콘텐츠 제공
    if (!(S_ISREG(sbuf.st_mode)) || !(S_IRUSR & sbuf.st_mode)) {  //파일이 읽기 가능한지 확인
      clienterror(fd, filename, "403", "Forbidden", "Tiny couldn't read the file");
      return;
    }
    serve_static(fd, filename, sbuf.st_size); //정적 파일 제공
  } else {    //동적 콘텐츠 제공
    if (!(S_ISREG(sbuf.st_mode)) || !(S_IXUSR & sbuf.st_mode)) {  //파일이 실행 가능한지 확인
      clienterror(fd, filename, "403", "Forbidden", "Tiny couldn't run the CGI program");
      return;
    }
    serve_dynamic(fd, filename, cgiargs);   //동적 파일 제공
  }
}

/*클라이언트로부터 추가 요청 헤더를 읽는 함수*/
void read_requesthdrs(rio_t *rp) {
  char buf[MAXLINE];

  Rio_readlineb(rp, buf, MAXLINE);  //요청 헤더 한 줄씩 읽기
  while(strcmp(buf, "\r\n")) {      //빈 줄이 나올 때까지 반복
    Rio_readlineb(rp, buf, MAXLINE);//계속 읽음
    printf("%s", buf);              //요청 헤더 출력
  }   
  return;
}

/*URI를 파싱하는 함수: 정적 콘텐츠인지 동적 콘텐츠인지 구분*/
int parse_uri(char *uri, char *filename, char *cgiargs) {
  char *ptr;

  if (!strstr(uri, "cgi-bin")) {  //정적 콘텐츠
    strcpy(cgiargs, "");          //CGI 인자 초기화
    strcpy(filename, ".");        //현재 디렉토리 기준으로 파일 이름 설정
    strcat(filename, uri);        //URI를 파일 이름에 추가
    if (uri[strlen(uri) - 1] == '/') {  //URI가 '/'로 끝나면 기본 파일 설정
      strcat(filename, "home.html");    //기본 파일로 home.html 설정
    }
    return 1;
  }

  else {  //동적 콘텐츠
    ptr = index(uri, '?');  //URI '?'가 있으면 CGI인자 처리
    if (ptr) {
      strcpy(cgiargs, ptr+1);//'?' 뒤의 문자열을 CGI 인자로 설정
      *ptr = '\0';           //URI에서 '?' 이하를 제거
    }
    else {
      strcpy(cgiargs, "");  //CGI 인자 없으면 빈 문자열
    }
    strcpy(filename, ".");  //현재 디렉토리 기준으로 파일 이름 설정
    strcat(filename, uri);  //URI를 파일 이름에 추가
    return 0;
  }
}

void serve_static(int fd, char *filename, int filesize) {
  int srcfd;
  char *srcp, filetype[MAXLINE], buf[MAXBUF];
  int start = 0, end = filesize - 1;  // 기본적으로 파일 전체를 전송
  char *range_header = getenv("HTTP_RANGE");  // Range 헤더 가져오기 (환경변수 사용)

  get_filetype(filename, filetype); // 파일 유형 결정 (MIME 타입)
  
  // 기본 응답 헤더 작성
  sprintf(buf, "HTTP/1.1 200 OK\r\n");
  sprintf(buf, "%sServer: Tiny Web Server\r\n", buf);
  sprintf(buf, "%sConnection: close\r\n", buf);
  sprintf(buf, "%sContent-length: %d\r\n", buf, filesize);
  sprintf(buf, "%sContent-type: %s\r\n", buf, filetype);

  // Range 헤더가 있는 경우
  if (range_header) {
      sscanf(range_header, "bytes=%d-%d", &start, &end);
      sprintf(buf, "HTTP/1.1 206 Partial Content\r\n");
      sprintf(buf, "%sContent-Range: bytes %d-%d/%d\r\n", buf, start, end, filesize);
  }

  sprintf(buf, "%sContent-Disposition: inline\r\n\r\n", buf);
  Rio_writen(fd, buf, strlen(buf)); // 응답 헤더 전송

  // 파일 전송
  srcfd = Open(filename, O_RDONLY, 0);
  srcp = (char *)Malloc(filesize);
  Rio_readn(srcfd, srcp, filesize);
  Close(srcfd);
  
  // Range 요청이 있다면 해당 범위만 전송
  Rio_writen(fd, srcp + start, end - start + 1);
  Free(srcp);
}


// /*정적 콘텐츠를 클라이언트에 제공하는 함수*/
// void serve_static(int fd, char *filename, int filesize) {
//   int srcfd;
//   char *srcp, filetype[MAXLINE], buf[MAXBUF];

//   get_filetype(filename, filetype); //파일 유형 결정(MIME 타입)
//   sprintf(buf, "HTTP/1.0 200 OK\r\n");
//   sprintf(buf, "%sServer: Tiny Web Server\r\n", buf);
//   sprintf(buf, "%sConnection: close\r\n", buf);
//   sprintf(buf, "%sContent-length: %d\r\n", buf, filesize);
//   sprintf(buf, "%sContent-type: %s\r\n\r\n", buf, filetype);
//   Rio_writen(fd, buf, strlen(buf)); //응답 헤더 작성 후 전송(csapp의 Rio_writen 함수 사용)
//   printf("Resoponse headers:\n");
//   printf("%s", buf);

//   srcfd = Open(filename, O_RDONLY, 0);  //파일 열기(csapp의 Open함수 사용)
//   srcp = Mmap(0, filesize, PROT_READ, MAP_PRIVATE, srcfd, 0); //파일을 메모리로 매핑(csapp의 Mmap 함수 사용)
//   Close(srcfd); //파일 디스크립터 닫기(csapp의 Close 함수 사용)
//   Rio_writen(fd, srcp, filesize); //파일 내용 전송(csapp의 Rio_writen 함수 사용)
//   Munmap(srcp, filesize); //메모리 매핑 해제(csapp의 Munmap 함수 사용)
// }

/*파일의 유형(MIME 타입)을 결정하는 함수*/
void get_filetype(char *filename, char *filetype) {
  if (strstr(filename, ".html")) {
    strcpy(filetype, "text/html");
  } else if (strstr(filename, ".gif")) {
    strcpy(filetype, "image/gif");
  } else if (strstr(filename, ".png")) {
    strcpy(filetype, "image/png");
  } else if (strstr(filename, ".jpg")) {
    strcpy(filetype, "image/jpeg");
  } else if (strstr(filename, ".mpg")) {
    strcpy(filetype, "video/mpg");
  } else if (strstr(filename, ".mp4")) {
    strcpy(filetype, "video/mp4");
  } else {
    strcpy(filetype, "text/plain");
  }
}

/*동적 콘텐츠를 클라이언트에 제공하는 함수*/
void serve_dynamic(int fd, char *filename, char *cgiargs) {
  char buf[MAXLINE], *emptylist[] = { NULL };

  sprintf(buf, "HTTP/1.0 200 OK\r\n");
  Rio_writen(fd, buf, strlen(buf)); //응답 헤더 작성 후 전송
  sprintf(buf, "Server: Tiny Web Server\r\n");
  Rio_writen(fd, buf, strlen(buf));

  if (Fork() == 0) {  //자식 프로세스 생성(csapp의 Fork 함수 사용)
    setenv("QUERY_STRING", cgiargs, 1); //CGI 인자를 환경 변수로 설정
    Dup2(fd, STDOUT_FILENO);  //표준 출력(STDOUT)을 소켓으로 변경(csapp의 Dup2함수 사용)
    Execve(filename, emptylist, environ); //CGI 프로그램 실행(csapp의 Execve 함수 사용)
  }
  Wait(NULL); //부모 프로세스는 자식이 종료될 때까지 대기
}

/*클라이언트에 에러 메시지를 전송하는 함수*/
void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg) {
  char buf[MAXLINE], body[MAXBUF];

  printf("clienterror");

  sprintf(body, "<html><title>Tiny Error</title>");
  sprintf(body, "%s<body bgcolor=""ffffff"">\r\n", body);
  sprintf(body, "%s%s: %s\r\n", body, errnum, shortmsg);
  sprintf(body, "%s<p>%s: %s\r\n", body, longmsg, cause);
  sprintf(body, "%s<hr><em>The Tiny Web server<\em>\r\n", body);

  sprintf(buf, "/HTTP1.0 %s %s\r\n", errnum, shortmsg);
  Rio_writen(fd, buf, strlen(buf)); //에러 응답 헤더 전송(csapp의 Rio_writen 함수 사용)
  sprintf(buf, "Content-type : text/html\r\n");
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "Content-length: %d\r\n\r\n", (int)strlen(body));
  Rio_writen(fd, buf, strlen(buf)); //에러 응답 바디 전송
  Rio_writen(fd, body, strlen(body));
}
