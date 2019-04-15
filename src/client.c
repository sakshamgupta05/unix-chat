#define _POSIX_C_SOURCE 200112L
#include <signal.h>
#include <sys/wait.h>
#include <netdb.h>
#include <time.h>
#include "chat.h"

enum mode{manual, bench};

void client(char* addr, enum mode m, int M) {
  int cfd;
  char cmd[ARG_MAX];
  struct addrinfo hints;
  struct addrinfo *result, *rp;

  memset(&hints, 0, sizeof(struct addrinfo));
  hints.ai_canonname = NULL;
  hints.ai_addr = NULL;
  hints.ai_next = NULL;
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = AI_NUMERICSERV;

  if (getaddrinfo(addr, PORT_NUM, &hints, &result) != 0) {
    perror("getaddrinfo");
    exit(EXIT_FAILURE);
  }

  for (rp = result; rp != NULL; rp = rp->ai_next) {
    cfd = socket(rp -> ai_family, rp -> ai_socktype, rp -> ai_protocol);
    if (cfd == -1) continue;
    if (connect(cfd, rp -> ai_addr, rp -> ai_addrlen) != -1)
      break;
    close(cfd);
  }

  if (rp == NULL){
    printf("Could not connect socket to any address\n");
    exit(EXIT_FAILURE);
  }
  freeaddrinfo(result);

  pid_t pid = fork();
  if (pid == 0) {
    if (m == manual) {
      while (fgets(cmd, ARG_MAX-1, stdin) != NULL) {
        int len = strlen(cmd);
        cmd[len - 1] = '\r';
        cmd[len] = '\n';
        cmd[len + 1]  = '\0';
        write(cfd, cmd, len + 1);
      }
    } else {
      strcpy(cmd, "JOIN name\r\n");
      write(cfd, cmd, strlen(cmd));
      strcpy(cmd, "BMSG hello\r\n");
      for (int i = 0; i < M; i++) {
        write(cfd, cmd, strlen(cmd));
      }
      strcpy(cmd, "LEAV\r\n");
      write(cfd, cmd, strlen(cmd));
    }
    close(cfd);
    exit(EXIT_SUCCESS);
  } else {
    while (readLine(cfd, cmd, ARG_MAX) > 0) {
      if (m == manual) {
        printf("%s\n", cmd);
      }
    }
    close(cfd);
    kill(pid, SIGKILL);
  }
}

void client_bench(char *addr, int N, int M, int T) {
  int n = 0;
  for (int i = 0; i < T; i++) {
    printf("client %d connected\n", i);
    pid_t pid = fork();
    if (pid == -1) {
      perror("fork");
      exit(EXIT_FAILURE);
    } else if (pid == 0) {
      client(addr, bench, M);
      exit(EXIT_SUCCESS);
    }

    if (++n >= N) {
      wait(NULL);
      n--;
    }
  }
  while (n > 0) {
    wait(NULL);
    n--;
  }
}

static void usageError(char *progName, char *msg, int opt) {
  if (msg != NULL && opt != 0)
  fprintf(stderr, "%s (-%c)\n", msg, printable(opt));
  fprintf(stderr, "Usage: %s [-a arg] [-b \"N M T\"]\n", progName);
  exit(EXIT_FAILURE);
}

int main(int argc, char *argv[]) {
  enum mode mode;
  int opt;
  char *benStr = NULL;
  char *addrStr = NULL;
  while ((opt = getopt(argc, argv, ":a:b:")) != -1) {
    switch (opt) {
    case 'a':
      addrStr = optarg;
      break;
    case 'b':
      benStr = optarg;
      break;
    case ':':
      usageError(argv[0], "Missing argument", optopt);
    case '?':
      usageError(argv[0], "Unrecognized option", optopt);
    default:
      printf("Unexpected case in switch()\n");
      exit(EXIT_FAILURE);
    }
  }
  if (addrStr == NULL) {
    addrStr = ADDR_LOOPBACK;
  }
  if (benStr == NULL) mode = manual;
  else mode = bench;

  if (mode == manual) {
    printf("starting in manual mode\n");
    client(addrStr, mode, 0);
  } else {
    printf("starting in benchmark mode\n");
    int N, M, T;
    sscanf(benStr, "%d %d %d", &N, &M, &T);
    client_bench(addrStr, N, M, T);
  }
  return 0;
}
