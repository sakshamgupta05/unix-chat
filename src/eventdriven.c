#define _GNU_SOURCE
#include <sys/epoll.h>
#include <fcntl.h>
#include "chat.h"
#include "serv.h"

#define MAX_EVENTS 10

struct clients clients = {NULL, NULL, 0};

static void useFd(int cfd) {
  char cmd[ARG_MAX];

  while (readLine(cfd, &cmd, ARG_MAX) > 0) {
    if (strncmp(cmd, "JOIN ", 5) == 0) {
      struct client *client = malloc(sizeof(struct client));
      client -> cfd = cfd;
      strcpy(client -> name, cmd + 5);

      if (clients.first == NULL) {
        clients.first = client;
        clients.last = client;
      } else {
        clients.last -> next = client;
        clients.last = client;
      }
      clients.totClients++;

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
        write(cfd, "leave failed\n", 5);
      }

    } else{
      write(cfd, "wrong command\n", 14);
    }
  }
}

int main() {
  int sfd, optval;
  struct sockaddr_in svaddr;
  char svaddrStr[INET_ADDRSTRLEN];

  struct epoll_event ev, events[MAX_EVENTS];
  int nfds, epollfd;

  sfd = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);
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
        struct sockaddr_in claddr;
        socklen_t addrlen;
        char claddrStr[INET_ADDRSTRLEN];
        int cfd = accept4(sfd, (struct sockaddr *) &claddr, &addrlen, SOCK_NONBLOCK);
        if (cfd == -1) {
          perror("accept");
          exit(EXIT_FAILURE);
        }
        if (inet_ntop(AF_INET, &claddr.sin_addr, claddrStr, INET_ADDRSTRLEN) == NULL) {
          printf("Couldn't convert client address to string\n");
        } else {
          printf("Server connected to (%s, %u)\n", claddrStr, ntohs(claddr.sin_port));
        }
        ev.events = EPOLLIN | EPOLLET;
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
