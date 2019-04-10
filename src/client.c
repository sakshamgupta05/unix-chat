#include <signal.h>
#include "chat.h"

int main() {
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
    while (fgets(cmd, ARG_MAX, stdin) != NULL) {
      write(sfd, cmd, strlen(cmd));
    }

  } else {
    while (readLine(sfd, cmd, ARG_MAX) > 0) {
      printf("%s\n", cmd);
    }
    close(sfd);
    kill(pid, SIGKILL);
  }
}
