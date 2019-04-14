#define _POSIX_C_SOURCE 200112L
#define _DEFAULT_SOURCE
#include <pthread.h>
#include <netdb.h>
#include "chat.h"
#include "serv.h"

#define ADDRSTRLEN (NI_MAXHOST + NI_MAXSERV + 10)

struct clients clients = {NULL, NULL, 0};
pthread_mutex_t mtxClients = PTHREAD_MUTEX_INITIALIZER;

static void* useFd(void *arg) {
  int cfd = (*(int *) arg);
  free(arg);

  int s;
  char cmd[ARG_MAX];

  while (readLine(cfd, &cmd, ARG_MAX) > 0) {
    if (strncmp(cmd, "JOIN ", 5) == 0) {
      struct client *client = malloc(sizeof(struct client));
      client -> cfd = cfd;
      strcpy(client -> name, cmd + 5);
      client -> next = NULL;

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

      strcpy(cmd, "successfully joined\r\n");
      write(cfd, cmd, strlen(cmd));

    } else if (strcmp(cmd, "LIST") == 0) {
      struct client *cptr = clients.first;
      while (cptr != NULL) {
        write(cfd, cptr -> name, strlen(cptr -> name));
        if (cptr -> next != NULL) write(cfd, "\n", 1);
        cptr = cptr -> next;
      }
      write(cfd, "\r\n", 2);

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
        strcpy(cmd, "ERROR: not online\r\n");
        write(cfd, cmd, strlen(cmd));
      } else {
        readLine(cfd, cmd, ARG_MAX);
        write(mcfd, cmd, strlen(cmd));
        write(mcfd, "\r\n", 2);
      }

    } else if (strncmp(cmd, "BMSG ", 5) == 0) {
      struct client *cptr = clients.first;
      while (cptr != NULL) {
        if (cptr -> cfd != cfd) {
          write(cptr -> cfd, cmd + 5, strlen(cmd + 5));
          write(cptr -> cfd, "\r\n", 2);
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
        strcpy(cmd, "leave failed\r\n");
        write(cfd, cmd, strlen(cmd));
      }

    } else{
      strcpy(cmd, "wrong command\r\n");
      write(cfd, cmd, strlen(cmd));
    }
  }
  return 0;
}

int main() {
  int sfd, optval;
  struct addrinfo hints;
  struct addrinfo *result, *rp;

  struct sockaddr_storage svaddr;
  socklen_t addrlen;
  char addrStr[ADDRSTRLEN];
  char host[NI_MAXHOST];
  char service[NI_MAXSERV];

  memset(&hints, 0, sizeof(struct addrinfo));
  hints.ai_canonname = NULL;
  hints.ai_addr = NULL;
  hints.ai_next = NULL;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_family = AF_UNSPEC;
  hints.ai_flags = AI_PASSIVE | AI_NUMERICSERV;
  if (getaddrinfo(NULL, PORT_NUM, &hints, &result) != 0) {
    perror("getaddrinfo");
    exit(EXIT_FAILURE);
  }

  optval = 1;
  for (rp = result; rp != NULL; rp = rp->ai_next) {
    sfd = socket(rp -> ai_family, rp -> ai_socktype, rp -> ai_protocol);
    if (sfd == -1)
      continue;
    if (setsockopt(sfd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) == -1) {
      perror("setsockopt");
      exit(EXIT_FAILURE);
    }
    if (bind(sfd, rp -> ai_addr, rp -> ai_addrlen) == 0)
      break;
    close(sfd);
  }
  if (rp == NULL) {
    printf("Could not bind socket to any address\n");
    exit(EXIT_FAILURE);
  }
  if (listen(sfd, SOMAXCONN) == -1) {
    perror("listen");
    exit(EXIT_FAILURE);
  }
  freeaddrinfo(result);

  addrlen = sizeof(struct sockaddr_storage);
  getsockname(sfd, (struct sockaddr *) &svaddr, &addrlen);

  if (getnameinfo((struct sockaddr *) &svaddr, addrlen,
        host, NI_MAXHOST, service, NI_MAXSERV, 0) == 0) {
    snprintf(addrStr, ADDRSTRLEN, "(%s, %s)", host, service);
  } else {
    snprintf(addrStr, ADDRSTRLEN, "(?UNKNOWN?)");
  }
  printf("Listening to %s\n", addrStr);

  for (;;) {
    char addrStr[ADDRSTRLEN];
    char host[NI_MAXHOST];
    char service[NI_MAXSERV];
    struct sockaddr_storage claddr;
    socklen_t addrlen;

    int *cfd = malloc(sizeof(int));
    *cfd = accept(sfd, (struct sockaddr *) &claddr, &addrlen);
    if (*cfd == -1) {
      perror("accept");
      continue;
    }

    if (getnameinfo((struct sockaddr *) &claddr, addrlen,
          host, NI_MAXHOST, service, NI_MAXSERV, 0) == 0) {
      snprintf(addrStr, ADDRSTRLEN, "(%s, %s)", host, service);
    } else {
      snprintf(addrStr, ADDRSTRLEN, "(?UNKNOWN?)");
    }
    printf("Connection from %s\n", addrStr);

    pthread_t t1;
    int s = pthread_create(&t1, NULL, useFd, cfd);
    if (s != 0) {
      printf("pthread_create error\n");
      close(*cfd);
      free(cfd);
    }
  }

  return 0;
}
