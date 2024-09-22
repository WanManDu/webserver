/*
 * adder.c - a minimal CGI program that adds two numbers together
 */
/* $begin adder */
//tiny의 자식 프로세서
//부모 프로세서인 tiny가 fork를 통해 만든 별도의 실행 흐름을 가짐
//부모와 독립된 일을 수행할 수 있는 새 일꾼을 뽑는것
#include "csapp.h"

int main(void) {
  char *buf, *p;
  char arg1[MAXLINE], arg2[MAXLINE], content[MAXLINE];
  int n1 = 0, n2 = 0;

  /*QUERY_STRING 환경 변수에서 인자 추출*/
  if ((buf = getenv("QUERY_STRING")) != NULL) {
    p = strchr(buf, '&'); //'&' 문자를 찾아서 인자 구분
    *p = '\0';            //첫 번째 인자와 두 번째 인자 분리
    strcpy(arg1, buf);    //첫 번째 인자 복사
    strcpy(arg2, p + 1);  //두 번째 인자 복사
    n1 = atoi(arg1);      //문자열을 정수로 변환
    n2 = atoi(arg2);      
  }

  /*동적 콘텐츠 생성*/
  sprintf(content, "QUERY_STRING=%s", buf);
  sprintf(content, "Welcome to add.com: ");
  sprintf(content, "%sTHE Internet addition portal. \r\n<p>", content);
  sprintf(content, "%sThe answer is: %d + %d = %d\r\n<p>", content, n1, n2, n1 + n2);
  sprintf(content, "%sThanks for visiting!\r\n", content);

  /*HTTP 응답 생성*/
  printf("connection; close\r\n");
  printf("Content-length: %d\r\n", (int)strlen(content));
  printf("Content-type: text/html\r\n\r\n");
  printf("%s", content);

  fflush(stdout); //출력 버퍼 비우기

  exit(0);
}
/* $end adder */
