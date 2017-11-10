using namespace std;

extern int state;
extern bool vflag;
extern string filename, serverId;

void handle_server(char* msg, Server& my_server, Client& c) {
  if (state == DEFAULT) {
    Msg m;
    m.decode(msg);
    my_server.deliverUnordered(m);
  } else if (state == FIFO) {
    Msg m;
    m.FIFOdecode(msg);
    my_server.deliverFIFO(m, c);
  } else if (state == TOTAL) {
    Msg m;
    m.decodeTOTAL(msg);
    if (m.state == 0) {
      my_server.respond(c, m);
    } else if (m.state == 1) {
      my_server.collect(c, m);
    } else if (m.state == 2) {
      my_server.update(c, m);
    }
  }
}

/* Check if the incoming msg is from other servers */
bool from_server(struct sockaddr_in input, vector<Server*>& servers) {
  for (int i = 0; i < servers.size(); i++) {
    struct sockaddr_in my_server = servers.at(i)->local;
    if (my_server.sin_addr.s_addr == input.sin_addr.s_addr && my_server.sin_port == input.sin_port) {
      return true;
    }
  }
  return false;
}

/* Gets all servers' forward address */
vector<Server*> get_forwards(string filename) {
  fstream fs;
  vector<string> temp;
  string buff;
  fs.open("./" + filename, fstream::in);
  while (getline(fs, buff)) {
    temp.push_back(buff.substr(0, buff.find(",")));
  }
  vector<Server*> all;
  for (int i = 0; i < temp.size(); i++) {
    Server* cur = new Server(temp.at(i));
    all.push_back(cur);
  }
  fs.close();
  return all;
}

/* Gets all servers' binding address */
Server get_binding(string filename, string server_id) {
  fstream fs;
  vector<string> temp;
  string buff;
  int id = stoi(server_id);
  fs.open("./" + filename, fstream::in);
  while (getline(fs, buff)) {
    temp.push_back(buff.substr(buff.find(",") + 1));
  }
  Server server = Server(temp.at(id - 1));
  fs.close();
  return server;
}
