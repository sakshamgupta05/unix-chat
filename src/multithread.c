#include <pthread.h>
#include "chat.h"
#include "serv.h"

struct clients clients = {NULL, NULL, 0};
pthread_mutex_t mtxClients = PTHREAD_MUTEX_INITIALIZER;

static void* useFd(void *arg) {
  int cfd = (*(int *) arg);
  free(arg);

  int s;
  struct sockaddr_in claddr;
  socklen_t addrlen;
  char claddrStr[INET_ADDRSTRLEN];
  char cmd[ARG_MAX];

  addrlen = sizeof(struct sockaddr_in);
  getpeername(cfd, (struct sockaddr *) &claddr, &addrlen);

  if (inet_ntop(AF_INET, &claddr.sin_addr, claddrStr, INET_ADDRSTRLEN) == NULL) {
    printf("Couldn't convert client address to string\n");
  } else {
    printf("Server connected to (%s, %u)\n", claddrStr, ntohs(claddr.sin_port));
  }

  while (readLine(cfd, &cmd, ARG_MAX) > 0) {
    if (strncmp(cmd, "JOIN ", 5) == 0) {
      struct client *client = malloc(sizeof(struct client));
      client -> cfd = cfd;
      strcpy(client -> name, cmd + 5);

      // critical section
      s = pthread_mutex_lock(&mtxClients);
      if (s != 0) {
        printf("error pthread_mutex_lock\n");
      }
      if (clients.first == NULL) {
        clients.first = client;
        clients.last = client;
      } else {
        clients.last -> next = client;
        clients.last = client;
      }
      clients.totClients++;
      s = pthread_mutex_unlock(&mtxClients);
      if (s != 0) {
        printf("error pthread_mutex_unlock\n");
      }
      // end critical section

      write(cfd, "successfully joined\n", 20);

    } else if (strcmp(cmd, "LIST") == 0) {
      struct client *cptr = clients.first;
      while (cptr != NULL) {
        write(cfd, cptr -> name, strlen(cptr -> name));
        write(cfd, "\t", 1);
        cptr = cptr -> next;
      }
      write(cfd, "\n", 1);

    } else if (strncmp(cmd, "UMSG ", 5) == 0) {
      int mcfd = -1;
      struct client *cptr = clients.first;
      while (cptr != NULL) {
        if (strcmp(cmd + 5, cptr -> name) == 0) {
          mcfd = cptr -> cfd;
          break;
        }
        cptr = cptr -> next;
      }
      if (mcfd == -1) {
        write(cfd, "ERROR: not online\n", 18);
      } else {
        readLine(cfd, cmd, ARG_MAX);
        write(mcfd, cmd, strlen(cmd));
        write(mcfd, "\n", 1);
      }

    } else if (strncmp(cmd, "BMSG ", 5) == 0) {
      struct client *cptr = clients.first;
      while (cptr != NULL) {
        if (cptr -> cfd != cfd) {
          write(cptr -> cfd, cmd + 5, strlen(cmd + 5));
          write(cptr -> cfd, "\n", 1);
        }
        cptr = cptr -> next;
      }

    } else if (strcmp(cmd, "LEAV") == 0) {
      int leav = 0;
      struct client *cptr = clients.first;
      struct client *pcptr = NULL;
      while (cptr != NULL) {
        if (cptr -> cfd == cfd) {

          // critical section
          s = pthread_mutex_lock(&mtxClients);
          if (s != 0) {
            printf("error pthread_mutex_lock\n");
          }
          if (pcptr == NULL) {
            clients.first = cptr -> next;
          } else {
            pcptr -> next = cptr -> next;
          }
          free(cptr);
          if (cptr -> next == NULL) {
            clients.last = pcptr;
          }
          clients.totClients--;
          s = pthread_mutex_unlock(&mtxClients);
          if (s != 0) {
            printf("error pthread_mutex_unlock\n");
          }
          // end critical section

          leav = 1;
          break;
        }
        pcptr = cptr;
        cptr = cptr -> next;
      }

      if (leav) {
        close(cfd);
      } else {
        write(cfd, "leave failed\n", 5);
      }

    } else{
      write(cfd, "wrong command\n", 14);
    }
  }
  return 0;
}

int main() {
  int sfd, optval;
  struct sockaddr_in svaddr;
  char svaddrStr[INET_ADDRSTRLEN];

  sfd = socket(AF_INET, SOCK_STREAM, 0);
  if (sfd == 0) {
    perror("socket");
    exit(EXIT_FAILURE);
  }

  struct in_addr inaddr_any;
  inaddr_any.s_addr = htonl(INADDR_ANY);

  memset(&svaddr, 0, sizeof(struct sockaddr_in));
  svaddr.sin_addr = inaddr_any;
  svaddr.sin_port = htons(PORT_NUM);

  optval = 1;
  if (setsockopt(sfd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) == -1) {
    close(sfd);
    return -1;
  }

  if (bind(sfd, (struct sockaddr *) &svaddr, sizeof(struct sockaddr_in)) == -1) {
    perror("bind");
    exit(EXIT_FAILURE);
  }

  if (listen(sfd, SOMAXCONN) == -1) {
    perror("listen");
    exit(EXIT_FAILURE);
  }

  if (inet_ntop(AF_INET, &svaddr.sin_addr, svaddrStr, INET_ADDRSTRLEN) != NULL) {
    printf("listening to ip %s port %u\n", svaddrStr, ntohs(svaddr.sin_port));
  }

  for (;;) {
    int *cfd = malloc(sizeof(int));
    *cfd = accept(sfd, NULL, NULL);
    if (*cfd == -1) {
      perror("accept");
      continue;
    }
    pthread_t t1;
    int s = pthread_create(&t1, NULL, useFd, cfd);
    if (s != 0) {
      printf("pthread_create error\n");
      continue;
    }

  }

  return 0;
}
