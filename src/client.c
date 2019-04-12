#define _POSIX_SOURCE
#include <signal.h>
#include <sys/wait.h>
#include <time.h>
#include "chat.h"

enum mode{manual, bench};

void client(enum mode m, int M) {
  int sfd;
  struct sockaddr_in svaddr;
  char cmd[ARG_MAX];

  sfd = socket(AF_INET, SOCK_STREAM, 0);
  if (sfd == 0) {
    perror("socket");
    exit(EXIT_FAILURE);
  }

  struct in_addr inaddr_any;
  inaddr_any.s_addr = htonl(INADDR_LOOPBACK);

  memset(&svaddr, 0, sizeof(struct sockaddr_in));
  svaddr.sin_family = AF_INET;
  svaddr.sin_addr = inaddr_any;
  svaddr.sin_port = htons(PORT_NUM);

  if (connect(sfd, (struct sockaddr *) &svaddr, sizeof(struct sockaddr_in)) == -1) {
    perror("connect");
    exit(EXIT_FAILURE);
  }

  pid_t pid = fork();
  if (pid == 0) {
    if (m == manual) {
      while (fgets(cmd, ARG_MAX, stdin) != NULL) {
        write(sfd, cmd, strlen(cmd));
      }
    } else {
      strcpy(cmd, "JOIN name\n");
      write(sfd, cmd, strlen(cmd));
      strcpy(cmd, "BMSG hello\n");
      for (int i = 0; i < M; i++) {
        write(sfd, cmd, strlen(cmd));
      }
      strcpy(cmd, "LEAV\n");
      write(sfd, cmd, strlen(cmd));
    }
    close(sfd);
    exit(EXIT_SUCCESS);
  } else {
    while (readLine(sfd, cmd, ARG_MAX) > 0) {
      if (m == manual) {
        printf("%s\n", cmd);
      }
    }
    close(sfd);
    kill(pid, SIGKILL);
  }
}

void client_bench(int N, int M, int T) {
  int n = 0;
  for (int i = 0; i < T; i++) {
    printf("client %d connected\n", i);
    pid_t pid = fork();
    if (pid == -1) {
      perror("fork");
      exit(EXIT_FAILURE);
    } else if (pid == 0) {
      client(bench, M);
      exit(EXIT_SUCCESS);
    }

    if (++n >= N) {
      wait(NULL);
      n--;
    }
  }
}

int main(int argc, char *argv[]) {
  if (argc == 1) {
    printf("starting in manual mode\n");
    client(manual, 0);
  } else if (argc == 4) {
    printf("starting in benchmark mode\n");
    int N = atoi(argv[1]);
    int M = atoi(argv[2]);
    int T = atoi(argv[3]);
    client_bench(N, M, T);
  } else {
    printf("provide 0 args for manual mode or 3 args (N, M, T) for benchmark mode\n");
    exit(EXIT_FAILURE);
  }
  return 0;
}
