#include <stdlib.h>
#include <stdio.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <string.h>
#include <sys/time.h>
#include <assert.h>
#include <errno.h>
#include <unistd.h>

#define panic(a...) do { fprintf(stderr, a); fprintf(stderr, "\n"); exit(1); } while (0)
#define logVerbose(a...) do { if (verbose) { fprintf(stderr, a); fprintf(stderr, "\n"); } } while (0)
#define warning(a...) do { fprintf(stderr, a); fprintf(stderr, "\n"); } while (0)
#define log(a...) do { struct timeval tv; gettimeofday(&tv, NULL); fprintf(stderr, "PRX %d.%03d ", (int)tv.tv_sec, (int)(tv.tv_usec/1000)); fprintf(stderr, a); fprintf(stderr, "\n"); } while(0)

#define MAX_SERVERS 10
#define MAX_MSG_LEN 1000
#define MAX_QUEUE_LEN 1000

struct {
  in_addr_t ip;
  int port;
  in_addr_t bindIP;
  int bindPort;
  int proxySocket;
} server[MAX_SERVERS];

struct {
  int srcServerIdx;
  int dstServerIdx;
  long long xmitTime;
  char buffer[MAX_MSG_LEN];
} holdbackQueue[MAX_QUEUE_LEN];

int numServers = 0;
int queueLength = 0;
bool verbose = false;

int findServer(in_addr_t ip, int port, bool useBind)
{
  for (int i=0; i<numServers; i++)
    if (((server[i].ip == ip) && (server[i].port == port) && !useBind) ||
        ((server[i].bindIP == ip) && (server[i].bindPort == port) && useBind))
      return i;

  return -1;
}

char *paddr(in_addr_t ip, int port, char *buf)
{
  struct in_addr addr;
  addr.s_addr = ip;
  sprintf(buf, "%s:%d", inet_ntoa(addr), port);
  return buf;
}

long long currentTimeMicros()
{
  struct timeval tv;
  gettimeofday(&tv, NULL);
  return (tv.tv_sec*1000000LL + tv.tv_usec);
}

void deliverQueuedMessage(int idx)
{
  struct sockaddr_in target;
  bzero((void*)&target, sizeof(target));
  target.sin_family = AF_INET;
  target.sin_addr.s_addr = server[holdbackQueue[idx].dstServerIdx].bindIP;
  target.sin_port = htons(server[holdbackQueue[idx].dstServerIdx].bindPort);

  char addrbuf1[200], addrbuf2[200];
  log("SEND %s->%s '%s'", 
    paddr(server[holdbackQueue[idx].srcServerIdx].ip, server[holdbackQueue[idx].srcServerIdx].port, addrbuf1), 
    paddr(server[holdbackQueue[idx].dstServerIdx].bindIP, server[holdbackQueue[idx].dstServerIdx].bindPort, addrbuf2), 
    holdbackQueue[idx].buffer
  );

  int w = sendto(server[holdbackQueue[idx].srcServerIdx].proxySocket, holdbackQueue[idx].buffer, strlen(holdbackQueue[idx].buffer), 0, (struct sockaddr*)&target, sizeof(target));
  if (w<0)
    panic("sendto() failed (%s)", strerror(errno));
}

int main(int argc, char *argv[])
{
  long long maxDelayMicros = 5000;
  double lossProbability = 0;

  /* Parse arguments */

  int c;
  while ((c = getopt(argc, argv, "d:l:v")) != -1) {
    switch (c) {
      case 'd':
        maxDelayMicros = atoll(optarg);
        break;
      case 'l':
        lossProbability = atof(optarg);
        break;
      case 'v':
        verbose = true;
        break;
      default:
        fprintf(stderr, "Syntax: %s [-v] [-d maxDelayMicroseconds] [-l lossProbability] serverListFile\n", argv[0]);
        exit(1);
    }
  }

  if (optind != (argc-1)) {
    fprintf(stderr, "Error: Name of the server list file is missing!\n");
    return 1;
  }

  /* Read the server list */

  FILE *infile = fopen(argv[optind], "r");
  if (!infile)
    panic("Cannot read server list from '%s'", argv[optind]);
  char linebuf[1000];
  while (fgets(linebuf, sizeof(linebuf), infile)) {
    char *sproxyaddr = strtok(linebuf, ",\r\n");
    char *srealaddr = sproxyaddr ? strtok(NULL, ",\r\n") : NULL;
    if (!srealaddr)
      panic("Each line of '%s' needs to have two IP addresses separated by a comma!", argv[optind]);
    char *sip = strtok(sproxyaddr, ":");
    char *sport = strtok(NULL, ":\r\n");
    struct in_addr ip;
    inet_aton(sip, &ip);
    if (numServers >= MAX_SERVERS)
      panic("Too many servers defined in '%s' (max %d)", argv[optind], MAX_SERVERS);
    server[numServers].ip = ip.s_addr;
    server[numServers].port = atoi(sport);
    char *sip2 = strtok(srealaddr, ":");
    char *sport2 = strtok(NULL, ":\r\n");
    struct in_addr ip2;
    inet_aton(sip2, &ip2);
    server[numServers].bindIP = ip2.s_addr;
    server[numServers].bindPort = atoi(sport2);
    numServers ++;
  }
  fclose(infile);
  logVerbose("%d server(s) read from '%s'", numServers, argv[optind]);

  /* Open server sockets */

  for (int i=0; i<numServers; i++) {
    server[i].proxySocket = socket(AF_INET, SOCK_DGRAM, 0);
    if (server[i].proxySocket<0)
      panic("Cannot open proxy socket (%s)", strerror(errno));

    char addrbuf[200];
    struct sockaddr_in serverAddress;
    bzero((void*)&serverAddress, sizeof(serverAddress));
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_addr.s_addr = server[i].ip;
    serverAddress.sin_port = htons(server[i].port);
    if (bind(server[i].proxySocket, (struct sockaddr*)&serverAddress, sizeof(serverAddress)) < 0) 
      panic("Cannot bind to %s (%s)", paddr(server[i].ip, server[i].port, addrbuf), strerror(errno));
    logVerbose("Listening on %s", paddr(server[i].ip, server[i].port, addrbuf));

    int yes = 1;
    if (setsockopt(server[i].proxySocket, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) < 0)
      panic("Cannot set SO_REUSEADDR option on server socket (%s)", strerror(errno));
  }

  /* Main loop */

  char addrbuf1[200], addrbuf2[200];
  while (true) {

    fd_set rdset;
    FD_ZERO(&rdset);
    int maxFD = 0;
    for (int i=0; i<numServers; i++) {
      FD_SET(server[i].proxySocket, &rdset);
      if (server[i].proxySocket > maxFD)
        maxFD = server[i].proxySocket;
    }

    long long earliestDelivery = currentTimeMicros() + 1000000;
    for (int i=0; i<queueLength; i++)
      if (holdbackQueue[i].xmitTime < earliestDelivery)
        earliestDelivery = holdbackQueue[i].xmitTime;

    long long maxWaitMicros = earliestDelivery - currentTimeMicros();
    if (maxWaitMicros < 1)
      maxWaitMicros = 1;

    struct timeval tv;
    tv.tv_sec = maxWaitMicros / 1000000ULL;
    tv.tv_usec = maxWaitMicros % 1000000ULL;

    log("Sleep %lld micros", maxWaitMicros);

    int ret = select(maxFD+1, &rdset, NULL, NULL, &tv);
    if (ret<0)
      panic("select() failed (%s)", strerror(errno));

    long long now = currentTimeMicros();
    for (int i=0; i<queueLength; ) {
      if (holdbackQueue[i].xmitTime <= now) {
        deliverQueuedMessage(i);
        holdbackQueue[i] = holdbackQueue[--queueLength];
      } else {
        i++;
      }
    }

    /* Receive a new message */

    for (int i=0; i<numServers; i++) {
      if (FD_ISSET(server[i].proxySocket, &rdset)) {       // server[i].bindIP,server[i].bindPort is the 'real' destination of this packet
        struct sockaddr_in sender;
        socklen_t senderLength = sizeof(sender);
        char buffer[65535];
        int len = recvfrom(server[i].proxySocket, &buffer, sizeof(buffer), 0, (struct sockaddr*)&sender, &senderLength);
        if (len < 0)
          panic("Cannot recvfrom (%s)", strerror(errno));

        buffer[len] = 0;

        int senderIdx = findServer(sender.sin_addr.s_addr, ntohs(sender.sin_port), true);  // server[senderIdx].proxySocket is where it should come from
        if (senderIdx < 0)
          panic("Received a packet from %s, but this isn't an actual bind port", paddr(sender.sin_addr.s_addr, ntohs(sender.sin_port), addrbuf1));

        log("RECV %s->%s '%s'", paddr(sender.sin_addr.s_addr, ntohs(sender.sin_port), addrbuf1), paddr(server[i].ip, server[i].port, addrbuf2), buffer);

        if (queueLength >= MAX_QUEUE_LEN)
          panic("Too many queued messages!");

        bool isLost = (rand()%1000)<(int)(lossProbability * 1000);
        if (!isLost) {
          holdbackQueue[queueLength].srcServerIdx = senderIdx;
          holdbackQueue[queueLength].dstServerIdx = i;
          holdbackQueue[queueLength].xmitTime = currentTimeMicros() + (rand() % maxDelayMicros);
          strncpy(holdbackQueue[queueLength].buffer, buffer, sizeof(holdbackQueue[queueLength].buffer)-1);
          queueLength ++;
        }

      }
    }
  }

  return 0;
}  