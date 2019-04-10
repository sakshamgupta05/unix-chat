#define MAX_CLIENT_NAME 64

struct client {
  int cfd;
  char name[MAX_CLIENT_NAME];
  struct client *next;
};

struct clients {
  struct client *first;
  struct client *last;
  int totClients;
};
