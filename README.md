## Introduction
- A distributed chat room application which supports up to 10 groups, and messages 
can be delievered in Unordered/FIFO/TOTAL ordering.

- Two major components in the application:  
1. chatclient: A client first connects to a server, and it can join a room/set nickname/leave a room/quit.
2. chatserver: 
The server responds to client's requests and then do corresponding message processing.  
Modules include __Msg.h__, __Client.h__, __Room.h__, __Server.h__, __cmdline.h__, __process_client.h__, __process_server.h__


## How to run this program?
### Client
#### Command line: ./chatclient <Server's IP address:port>
##### What a user can do after connecting to a server?
- JOIN: <chat room's ID>: join a chosen chat room
- PART: leave the current chat room
- NICK <chosen nickname>: set a nickname
- QUIT: quit this program

### Server
#### Command line: ./chatserver <a filename contains other servers' information> < Server ID >
1. -v Debug mode: This mode will show debug messages for you.
2. -o <ordering> Assign an order: You can decide the order of messages being delivered to clients.  
Options: __Unordered(default)/FIFO/TOTAL__
