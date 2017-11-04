#include <arpa/inet.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/time.h>

#define panic(a...) do { fprintf(stderr, a); fprintf(stderr, "\n"); exit(1); } while (0)
#define logVerbose(a...) do { if (verbose) { struct timeval tv; gettimeofday(&tv, NULL); printf("TST %d.%03d ", (int)tv.tv_sec, (int)(tv.tv_usec/1000)); printf(a); printf("\n"); } } while(0)
#define warning(a...) do { fprintf(stderr, "WARNING: "); fprintf(stderr, a); fprintf(stderr, "\n"); } while (0)

#define ORDER_UNORDERED 0
#define ORDER_FIFO 1
#define ORDER_TOTAL 2

#define MAX_SERVERS 10
#define MAX_CLIENTS 100
#define MAX_MSG_LEN 50
#define MAX_MESSAGES 10000
#define MAX_GROUPS 10

struct {
  in_addr_t ip;
  int port;
} server[MAX_SERVERS];

struct {
  int sock;
  int serverIdx;
  int groupID;
  int nextRecvSeq;
} client[MAX_CLIENTS];

struct {
  int senderIdx;
  char text[MAX_MSG_LEN+1];
  int groupID;
  int recvSeq[MAX_CLIENTS];
} message[MAX_MESSAGES];

bool verbose = false;
int ordering = ORDER_UNORDERED;
int numServers;
int numClients = 10;
int numGroups = 1;
int numMessages = 0;
int maxMessages = 10;
int finalDelaySeconds = 5;
long long xmitIntervalMicros = 100000;

void readServerList(const char *filename)
{
  FILE *infile = fopen(filename, "r");
  if (!infile)
    panic("Cannot read server list from '%s'", filename);
  char linebuf[1000];
  while (fgets(linebuf, sizeof(linebuf), infile)) {
    char *sproxyaddr = strtok(linebuf, ",\r\n");
    char *srealaddr = sproxyaddr ? strtok(NULL, ",\r\n") : NULL;
    char *serveraddr = srealaddr ? srealaddr : sproxyaddr;
    char *sip = strtok(serveraddr, ":");
    char *sport = strtok(NULL, ":\r\n");
    struct in_addr ip;
    inet_aton(sip, &ip);
    if (numServers >= MAX_SERVERS)
      panic("Too many servers defined in '%s' (max %d)", filename, MAX_SERVERS);
    server[numServers].ip = ip.s_addr;
    server[numServers].port = atoi(sport);
    numServers ++;
  }
  fclose(infile);
  logVerbose("%d server(s) found in '%s'", numServers, filename);
}

void sendToServer(int clientIdx, int serverIdx, const char *text)
{
  assert((0<=clientIdx) && (clientIdx<numClients));
  assert((0<=serverIdx) && (serverIdx<numServers));
  assert(text != NULL);

  struct sockaddr_in dest;
  bzero((void*)&dest, sizeof(dest));
  dest.sin_family = AF_INET;
  dest.sin_addr.s_addr = server[serverIdx].ip;
  dest.sin_port = htons(server[serverIdx].port);

  int w = sendto(client[clientIdx].sock, text, strlen(text), 0, (struct sockaddr*)&dest, sizeof(dest));
  if (w<0)
    panic("sendto() failed in sendToServer(\"%s\"): %s", text, strerror(errno));
}

long long currentTimeMicros()
{
  struct timeval tv;
  gettimeofday(&tv, NULL);
  return (tv.tv_sec*1000000LL + tv.tv_usec);
}

bool checkMessageOrdering(int msgIdx, int clientIdx)
{
  assert((0<=msgIdx) && (msgIdx<numMessages));
  assert((0<=clientIdx) && (clientIdx<numClients));
  assert(message[msgIdx].recvSeq[clientIdx] >= 1);

  // For unordered delivery, we don't have to check anything

  if (ordering == ORDER_UNORDERED)
  	return true;

  // For total ordering, we need to check whether all clients receive the messages in the same order

  if (ordering == ORDER_TOTAL) {
  	for (int i=0; i<numClients; i++) {
      if ((message[msgIdx].recvSeq[i] != -1) && (message[msgIdx].recvSeq[i] != message[msgIdx].recvSeq[clientIdx])) {
      	warning("Message M%03d was received as #%d by client C%02d, but as #%d by client C%02d",
      	  1+msgIdx, 1+message[msgIdx].recvSeq[i], 1+i, 1+message[msgIdx].recvSeq[clientIdx], 1+clientIdx
      	);
      	return false;
      }
  	}
  }

  // For FIFO ordering, we need to check whether the client has already received a later message by the same sender

  if (ordering == ORDER_FIFO) {
  	for (int i=msgIdx+1; i<numMessages; i++) {
  		if ((message[i].senderIdx == message[msgIdx].senderIdx) && (message[i].recvSeq[clientIdx] >= 0)) {
        warning("Client C%02d sent message M%03d before message M%03d, but client C%02d received them in the reverse order",
        	message[i].senderIdx+1, msgIdx+1, i+1, clientIdx+1
        );
        return false;
  		}
  	}
  }

  return true;
}

int countMissingMessages()
{
	int numMissing = 0;
	for (int i=0; i<numMessages; i++) {
		for (int j=0; j<numClients; j++) {
			if ((client[j].groupID == message[i].groupID) && (message[i].recvSeq[j] < 0)) {
				warning("Message M%03d was not delivered to client C%02d", i+1, j+1);
				numMissing ++;
			}
		}
	}

	return numMissing;
}

int main(int argc, char *argv[])
{
  /* Parse arguments */

  int c;
  while ((c = getopt(argc, argv, "o:c:g:m:i:f:v")) != -1) {
    switch (c) {
      case 'o':
        if (!strcmp(optarg, "unordered"))
          ordering = ORDER_UNORDERED;
        else if (!strcmp(optarg, "fifo"))
          ordering = ORDER_FIFO;
        else if (!strcmp(optarg, "total"))
          ordering = ORDER_TOTAL;
        else
          panic("Unknown ordering: '%s' (supported: unordered, fifo, total)", optarg);
        break;
      case 'c':
        numClients = atoi(optarg);
        break;
      case 'f':
        finalDelaySeconds = atoi(optarg);
        break;
      case 'g':
        numGroups = atoi(optarg);
        break;
      case 'i':
        xmitIntervalMicros = atoll(optarg)*1000LL;
        break;
      case 'm':
        maxMessages = atoi(optarg);
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

  /* Initialize the random number generator, and read the server list from the file */

  srand(time(0));
  readServerList(argv[optind]);

  fprintf(stderr, "Sending %d messages from %d clients to %d groups in %dms intervals, checking for %s ordering\n",
  	maxMessages, numClients, numGroups, (int)(xmitIntervalMicros/1000LL), ((ordering==ORDER_UNORDERED) ? "no particular" : ((ordering==ORDER_FIFO) ? "FIFO" : "total")));

  /* Open client sockets and make each client /join one of the groups */

  for (int i=0; i<numClients; i++) {
    client[i].sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (client[i].sock<0)
      panic("Cannot open client socket (%s)", strerror(errno));
    client[i].serverIdx = rand()%numServers;
    client[i].groupID = 1+rand()%numGroups;
    client[i].nextRecvSeq = 1;

    char joinCommand[100];
    sprintf(joinCommand, "/join %d", client[i].groupID);
    logVerbose("Client C%02d joins group G%d", 1+i, client[i].groupID);
    sendToServer(i, client[i].serverIdx, joinCommand);

    struct sockaddr_in sender;
    socklen_t senderLength = sizeof(sender);
    char buffer[65535];
    int len = recvfrom(client[i].sock, &buffer, sizeof(buffer), 0, (struct sockaddr*)&sender, &senderLength);
    if (len < 0)
      panic("Cannot recvfrom (%s)", strerror(errno));
  }

  /* Main loop */

  long long nextXmit = currentTimeMicros();
  int numErrors = 0;
  int maxWaitCycles = finalDelaySeconds * 1000000/xmitIntervalMicros;
  int numWaitCycles = 0;
  while ((numMessages<maxMessages) || (numWaitCycles < maxWaitCycles)) {
    fd_set rdset;
    FD_ZERO(&rdset);
    int maxFD = 0;
    for (int i=0; i<numClients; i++) {
      FD_SET(client[i].sock, &rdset);
      if (client[i].sock > maxFD)
        maxFD = client[i].sock;
    }

    /* Compute a timeout, so we can wake up when the next message needs to be sent */

    long long maxWaitMicros = nextXmit - currentTimeMicros();
    if (maxWaitMicros < 1)
      maxWaitMicros = 1;

    struct timeval tv;
    tv.tv_sec = maxWaitMicros / 1000000ULL;
    tv.tv_usec = maxWaitMicros % 1000000ULL;

    /* Wait for an incoming packet and/or the timeout */

    int ret = select(maxFD+1, &rdset, NULL, NULL, &tv);
    if (ret<0)
      panic("select() failed (%s)", strerror(errno));

    /* If it is time to send another message, do that. Once all the messages have been sent,
       we still wait a little bit, so that any 'stragglers' (delayed messages) can be received */

    while (nextXmit <= currentTimeMicros()) {
      if (numMessages < maxMessages) {
  	    message[numMessages].senderIdx = rand()%numClients;
      	message[numMessages].groupID = client[message[numMessages].senderIdx].groupID;
      	sprintf(message[numMessages].text, "M%d-S%d-G%d-%06d", 
      	  numMessages+1, 
      	  message[numMessages].senderIdx+1,
      	  message[numMessages].groupID, 
      	  rand()%100000
        );
        for (int i=0; i<MAX_CLIENTS; i++)
          message[numMessages].recvSeq[i] = -1;
        logVerbose("Client C%02d sends message M%03d (%s) to group G%d", 
        	message[numMessages].senderIdx+1,
        	numMessages+1,
        	message[numMessages].text,
          message[numMessages].groupID
        );
        sendToServer(
          message[numMessages].senderIdx, 
          client[message[numMessages].senderIdx].serverIdx, 
          message[numMessages].text
        );
        numMessages ++;
      } else {
      	numWaitCycles ++;
      	if (numWaitCycles == 1)
      		logVerbose("Waiting %d seconds for stragglers...", finalDelaySeconds);
      }
      nextXmit += xmitIntervalMicros;	
    }

    /* Receive new messages, and do a couple of sanity checks */

    for (int i=0; i<numClients; i++) {
      if (FD_ISSET(client[i].sock, &rdset)) {
        struct sockaddr_in sender;
        socklen_t senderLength = sizeof(sender);
        char buffer[65535];
        int len = recvfrom(client[i].sock, &buffer, sizeof(buffer), 0, (struct sockaddr*)&sender, &senderLength);
        if (len < 0)
          panic("Error during recvfrom (%s)", strerror(errno));

        buffer[len] = 0;

        if (buffer[0] == '<') {
          char *mptr = &buffer[1];
          while (*mptr && (*mptr != '>'))
          	mptr++;

          if (*mptr != 0) {
            mptr ++;
            if (*mptr == ' ')
            	mptr ++;

            if (mptr[0] == 'M') {
              char buf2[strlen(mptr)+1];
              strcpy(buf2, &mptr[1]);
              strtok(buf2, "-");
              int msgID = atoi(buf2)-1;
              if ((0<=msgID) && (msgID < numMessages)) {
                if (!strcmp(mptr, message[msgID].text)) {
                	logVerbose("Client C%02d receives message M%03d (%s) as seq #%d", 1+i, 1+msgID, mptr, client[i].nextRecvSeq);
                  message[msgID].recvSeq[i] = client[i].nextRecvSeq ++;
      
                  if (!checkMessageOrdering(msgID, i))
                  	numErrors ++;
                } else {
                	warning("Client C%02d received a corrupted message (%s); no such message has been sent", 1+i, mptr);
                }
              } else {
                warning("Client C%02d received a message with an invalid message ID (%s)", 1+i, mptr);
              }
            } else {
              warning("Client C%02d received a message that was never sent (%s)", 1+i, mptr);
            }
          } else {
            warning("Client C%02d received a message that contained an opening '<', but no closing '>' (%s)", 1+i, buffer);
          }
        } else {
          warning("Client C%02d received a message that did not contain a sender ID <...> (%s)", 1+i, buffer);
        }
      }
    }
  }

  /* We've already checked the message ordering as we went. Now it is time to check whether
     all the messages have been delivered to all the clients */

  numErrors += countMissingMessages();
 
  if (!numErrors)
  	fprintf(stderr, "Ordering OK\n");
  else
  	fprintf(stderr, "%d ordering error(s) found\n", numErrors);

  return 0;
}