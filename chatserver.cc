#include <arpa/inet.h>
#include <algorithm>
#include <fstream>
#include <iostream>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <strings.h>
#include <string>
#include <vector>
#include <map>
#include <queue>
#include "Msg.h"
#include "Client.h"
#include "Room.h"
#include "Server.h"
#include "cmdline.h"
#include "process_client.h"
#include "process_server.h"

extern char *optarg;
extern int optind, opterr, optopt;
int state = DEFAULT;
bool vflag = false;
string filename, serverId;

int main(int argc, char *argv[])
{
  if (argc < 2) {
    fprintf(stderr, "*** Author: Ying Tsai (yingt)\n");
    exit(1);
  }
  process_cml(argc, argv);
  vector<Server*> servers = get_forwards(filename);
  int sock = socket(PF_INET, SOCK_DGRAM, 0);
  Server my_server = get_binding(filename, serverId);
  my_server.id = stoi(serverId);
  my_server.max = 10;
  my_server.sock = sock;
  my_server.servers = servers;
  my_server.vflag = &vflag;
  if (state == FIFO) my_server.initFIFO();
  if (state == TOTAL) my_server.initTOTAL();
  bind(sock, (struct sockaddr*)&my_server.local, sizeof(my_server.local));

  struct sockaddr_in client;
  socklen_t clientlen = sizeof(client);
  char buff[100];
  while (true) {
    int rlen = recvfrom(sock, buff, sizeof(buff)-1, 0, (struct sockaddr*)&client, &clientlen);
    buff[rlen] = 0;
    // Evaluate if the message is from client or other servers by the incoming address
    if (rlen > 0) {
      if (from_server(client, servers)) {
        Client c(client);
        if (vflag) printf("S[%d] got a message from [%s]\n", my_server.id, c.toString().c_str());
        handle_server(buff, my_server, c);
      } else {
        Client c(client);
        if (vflag) printf("S[%d] got a message from [%s]\n", my_server.id, c.toString().c_str());
        handle_client(buff, my_server, c);
      }
    }
  }
  return 0;
}
