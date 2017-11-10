#include <arpa/inet.h>
#include <algorithm>
#include <errno.h>
#include <iostream>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <string>
#include <sys/select.h>
using namespace std;

int main(int argc, char *argv[])
{
  if (argc != 2) {
    fprintf(stderr, "*** Author: Ying Tsai (yingt)\n");
    exit(1);
  }
  string ip_addr = argv[1];
  string ip = ip_addr.substr(0, ip_addr.find(":"));
  string port = ip_addr.substr(ip_addr.find(":") + 1);
  // Create the socket for sending messages to server.
  int sock = socket(PF_INET, SOCK_DGRAM, 0);
  if (sock < 0) {
    fprintf(stderr, "Cannot open socket (%s)\n", strerror(errno));
    exit(1);
  }
  fd_set rfds;
  FD_ZERO(&rfds);
  struct sockaddr_in target;
  socklen_t target_size = sizeof(target);
  bzero(&target, sizeof(target));
  target.sin_family = AF_INET;
  target.sin_addr.s_addr = inet_addr(ip.c_str());
  target.sin_port = htons(stoi(port));
  char buffer[100];
  while (true) {
    FD_SET(sock, &rfds);
    FD_SET(STDIN_FILENO, &rfds);
    select(max(sock, STDIN_FILENO) + 1, &rfds, NULL, NULL, NULL);
    // Try to receive data from target server, if no data don't wait
    int rlen = recvfrom(sock, buffer, sizeof(buffer), MSG_DONTWAIT, (struct sockaddr*)&target, &target_size);
    buffer[rlen] = 0;
    if (rlen > 0) {
        printf("%s\n", buffer);
    } else {
      string input;
      getline(cin, input);
      strcpy(buffer, input.c_str());
      sendto(sock, buffer, strlen(buffer), 0, (struct sockaddr*)&target, target_size);
      transform(input.begin(), input.end(), input.begin(), ::tolower);
      if (input == "/quit") {
        break;
      }
    }
  }
  close(sock);
  return 0;
}
