#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <linux/limits.h>
#include <ctype.h>
#include "read_line.h"

#define PORT_NUM 8001
#define PORT_NUM_S "8001"

#define ADDR_LOOPBACK "127.0.0.1"

#define printable(ch) (isprint((unsigned char) ch) ? ch : '#')
