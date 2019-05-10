#define _POSIX_C_SOURCE 200112L
#define _GNU_SOURCE
#include <sys/epoll.h>
#include <fcntl.h>
#include <netdb.h>
#include "chat.h"
#include "serv.h"

#define ADDRSTRLEN (NI_MAXHOST + NI_MAXSERV + 10)
#define MAX_EVENTS 10

struct clients clients = {NULL, NULL, 0};

int nfds, epollfd;

static void useFd(int cfd) {
  char cmd[ARG_MAX];

  if (readLine(cfd, &cmd, ARG_MAX) > 0) {
    if (strncmp(cmd, "JOIN ", 5) == 0) {
      struct client *client = malloc(sizeof(struct client));
      client -> cfd = cfd;
      strcpy(client -> name, cmd + 5);
      client -> next = NULL;

      if (clients.first == NULL) {
        clients.first = client;
        clients.last = client;
      } else {
        clients.last -> next = client;
        clients.last = client;
      }
      clients.totClients++;

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
}

int main() {
  struct epoll_event ev, events[MAX_EVENTS];
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
    sfd = socket(rp -> ai_family, rp -> ai_socktype | SOCK_NONBLOCK, rp -> ai_protocol);
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

  epollfd = epoll_create1(0);
  if (epollfd == -1) {
    perror("epoll_create1");
    exit(EXIT_FAILURE);
  }

  ev.events = EPOLLIN;
  ev.data.fd = sfd;

  if (epoll_ctl(epollfd, EPOLL_CTL_ADD, sfd, &ev) == -1) {
    perror("epoll_ctl: listen_sock");
    exit(EXIT_FAILURE);
  }

  for (;;) {
    nfds = epoll_wait(epollfd, events, MAX_EVENTS, -1);
    if (nfds == -1) {
      perror("epoll_wait");
      exit(EXIT_FAILURE);
    }

    for (int n = 0; n < nfds; ++n) {
      if (events[n].data.fd == sfd) {
        char addrStr[ADDRSTRLEN];
        char host[NI_MAXHOST];
        char service[NI_MAXSERV];
        struct sockaddr_storage claddr;
        socklen_t addrlen;
        int cfd = accept(sfd, (struct sockaddr *) &claddr, &addrlen);
        if (cfd == -1) {
          perror("accept");
          exit(EXIT_FAILURE);
        }
        if (getnameinfo((struct sockaddr *) &claddr, addrlen,
              host, NI_MAXHOST, service, NI_MAXSERV, 0) == 0) {
          snprintf(addrStr, ADDRSTRLEN, "(%s, %s)", host, service);
        } else {
          snprintf(addrStr, ADDRSTRLEN, "(?UNKNOWN?)");
        }
        printf("Connection from %s\n", addrStr);

        ev.events = EPOLLIN;
        ev.data.fd = cfd;
        if (epoll_ctl(epollfd, EPOLL_CTL_ADD, cfd, &ev) == -1) {
          perror("epoll_ctl: conn_sock");
          exit(EXIT_FAILURE);
        }
      } else {
        useFd(events[n].data.fd);
      }
    }
  }

  return 0;
}
