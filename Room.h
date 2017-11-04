using namespace std;

class Room {
public:
  int room_id;
  vector<Client*> clients;
  Room(int group) {
    this->room_id = group;
  };
  bool addClient(Client& c) {
    Client* client = new Client(c.addr);
    this->clients.push_back(client);
    client->nickname = c.toString();
    return true;
  }
  /* Check if this chat room has this client as a member */
  bool hasClient(Client& c) {
    for (int i = 0; i < clients.size(); i++) {
      Client* cur = clients.at(i);
      if (cur->equals(c)) return true;
    }
    return false;
  }
  /* Check if someone already used this nickname */
  bool hasNickname(string name) {
    for (int i = 0; i < clients.size(); i++) {
      Client* cur = clients.at(i);
      if (cur->nickname == name) return true;
    }
    return false;
  }
  Client* getUser(Client& c) {
    for (int i = 0; i < clients.size(); i++) {
      Client* cur = clients.at(i);
      if (cur->equals(c)) return cur;
    }
    return NULL;
  }

  void setNickname(Client& c, string name) {
    Client* user = getUser(c);
    if (user != NULL) {
      user->nickname = name;
    }
  }

  bool removeClient(Client& c) {
    for (int i = 0; i < clients.size(); i++) {
      Client* cur = clients.at(i);
      if (cur->equals(c)) {
        clients.erase(clients.begin() + i);
        return true;
      }
    }
    return false;
  }
};
