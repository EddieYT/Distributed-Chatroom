#include <regex>
using namespace std;
using namespace regex_constants;

extern int state;
extern bool vflag;

smatch matches;
regex reg_join ("^/join (\\d+)$", ECMAScript | icase);
regex reg_part ("^/part$",  ECMAScript | icase);
regex reg_nick ("^/nick (\\w+)$", ECMAScript | icase);
regex reg_quit ("^/quit$", ECMAScript | icase);

string no_such_room = "-ERR Please enter a number <= 10";
string already_exist = "-ERR You are alrealdy in room #";
string leave = "+OK You have left chat room #";
string no_such_user = "-ERR You are not in any chat room";
string join_room = "+OK You are now in chat room #";
string set_nick = "+OK Nickname set to ";
string nick_exist = "-ERR The nickname is taken in your room";

/* This function handles join command */
void handle_join(smatch matches, Server& my_server, Client& cli) {
  int room_id = stoi(matches[1]);
  int which_group;
  char buffer[100];
  map<int, Room*>::iterator it = my_server.rooms.find(room_id);
  // The room # is not supported
  if (room_id > my_server.max) {
    strcpy(buffer, no_such_room.c_str());
    my_server.sendMsg(cli, buffer);
    return;
  }
  // Client already exists in current chat room
  if ((which_group = my_server.hasClient(cli)) > 0) {
    string msg = already_exist + to_string(which_group);
    strcpy(buffer, msg.c_str());
    my_server.sendMsg(cli, buffer);
    return;
  }
  if (it != my_server.rooms.end()) {
    my_server.addClient(cli, room_id);
    string msg = join_room + to_string(room_id);
    strcpy(buffer, msg.c_str());
    my_server.sendMsg(cli, buffer);
  } else {
    my_server.createRoom(room_id);
    my_server.addClient(cli, room_id);
    string msg = join_room + to_string(room_id);
    strcpy(buffer, msg.c_str());
    my_server.sendMsg(cli, buffer);
  }
}

/* This function handles part command */
void handle_part(Server& my_server, Client& cli) {
  int group = my_server.hasClient(cli);
  char buffer[100];
  if (group > 0) {
    map<int, Room*>:: iterator it = my_server.rooms.find(group);
    bool res = it->second->removeClient(cli);
    string msg = leave + to_string(group);
    strcpy(buffer, msg.c_str());
    my_server.sendMsg(cli, buffer);
  } else {
    strcpy(buffer, no_such_user.c_str());
    my_server.sendMsg(cli, buffer);
  }
}

/* This function handles nick command */
void handle_nick(smatch matches, Server& my_server, Client& cli) {
  string name = matches[1];
  char buffer[100];
  int room_id = my_server.hasClient(cli);
  if (room_id > 0) {
    bool res = my_server.setNickname(cli, room_id, name);
    if (res) {
      string output = set_nick + "'" + name + "'";
      strcpy(buffer, output.c_str());
      my_server.sendMsg(cli, buffer);
    } else {
      string output = nick_exist;
      strcpy(buffer, output.c_str());
      my_server.sendMsg(cli, buffer);
    }
  } else {
    strcpy(buffer, no_such_user.c_str());
    my_server.sendMsg(cli, buffer);
  }
}

/* This function handles quit command */
void handle_quit(Server& my_server, Client& cli) {
  int which_group = my_server.hasClient(cli);
  if (which_group > 0) {
    Room* target = my_server.getRoom(which_group);
    target->removeClient(cli);
  }
}

/* This function handles general messages from client */
void handle_general(string command, Server& my_server, Client& cli) {
  char buff[100];
  int which_group = my_server.hasClient(cli);
  if (which_group > 0) {
    Client* user = my_server.getRoom(which_group)->getUser(cli);
    Msg m(command);
    m.setGroup(which_group);
    m.setNickname(user->nickname);
    if (state == DEFAULT) {
      my_server.multicast(m);
    } else if (state == FIFO) {
      my_server.FOmulticast(m);
    } else if (state == TOTAL){
      my_server.TOTALmulticast(m);
    }
  } else {
    strcpy(buff, no_such_user.c_str());
    my_server.sendMsg(cli, buff);
  }
}

/* This function handles all the msgs from client, return true if it needs to do
   multicast afterwards.
 */
void handle_client(char* cmd, Server& local, Client& cli) {
  string command(cmd);
  if (regex_search(command, matches, reg_join)) {
    handle_join(matches, local, cli);
  } else if (regex_search(command, matches, reg_part)) {
    handle_part(local, cli);
  } else if (regex_search(command, matches, reg_nick)) {
    handle_nick(matches, local, cli);
  } else if (regex_search(command, matches, reg_quit)) {
    handle_quit(local, cli);
  } else {
    handle_general(command, local, cli);
  }
}
