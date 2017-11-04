using namespace std;

struct cmp {
    bool operator()(const Msg& a, const Msg& b) const {
        return a.msgID < b.msgID;
    }
};

class Server {
public:
  struct sockaddr_in local;
  int id;
  int max;
  int sock;
  bool* vflag;
  string addr;
  map<int, Room*> rooms;
  vector<Server*> servers;

  int seq[10];
  map<string, vector<int> > seqRecent ;
  map<string, vector<map<int, Msg> > >holdback;

  int Pg[10];
  int Ag[10];
  priority_queue<Msg, vector<Msg>, Compare> totalHold;
  map<Msg, priority_queue<pair<int, int> >, cmp> proposalCount;

  Server(string str_addr) {
    string ip = str_addr.substr(0, str_addr.find(":"));
    string port = str_addr.substr(str_addr.find(":") + 1);
    this->addr = str_addr;
    struct sockaddr_in servaddr;
    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = inet_addr(ip.c_str());
    servaddr.sin_port = htons(stoi(port));
    this->local = servaddr;
  }
  void initTOTAL() {
    for (int i = 0; i < 10; i++) {
      this->Pg[i] = 0;
      this->Ag[i] = 0;
    }
  }
  /* Initialize essential resources for FIFO */
  void initFIFO() {
    int N = servers.size();
    for (int i = 0; i < 10; i++) {
      this->seq[i] = 0;
    }
    for (int i = 0; i < N; i++) {
      Server* cur = this->servers.at(i);
      string addr = cur->addr;
      vector<map<int, Msg> > holdback;
      vector<int> seqRecent;
      for (int i = 0; i < 10; i++) {
        map<int, Msg> queue;
        holdback.push_back(queue);
        seqRecent.push_back(0);
      }
      this->holdback[addr] = holdback;
      this->seqRecent[addr] = seqRecent;
    }
  }
  /* Send msg to a client */
  bool sendMsg(Client cli, char* msg) {
    struct sockaddr_in target;
    bzero(&target, sizeof(target));
    target = cli.addr;
    int res = sendto(sock, msg, strlen(msg), 0, (struct sockaddr*)&target, sizeof(target));
    if (res < 0) return false;
    if (*vflag) printf("S[%d] sent a message to [%s]\n", this->id, cli.toString().c_str());
    return true;
  }
  /* Create a chat room with a romm id */
  bool createRoom(int room_id) {
    Room* room = new Room(room_id);
    rooms.insert(pair<int, Room*>(room_id, room));
    return true;
  }
  /* Add a client to a chat room */
  bool addClient(Client client, int room_id) {
    map<int, Room*>:: iterator it = rooms.find(room_id);
    if (it != rooms.end()) {
      it->second->addClient(client);
      return true;
    } else {
      return false;
    }
  }
  /* Check if this client belongs the server */
  int hasClient(Client c) {
    for (map<int, Room*>:: iterator it = rooms.begin(); it != rooms.end(); ++it) {
      if (it->second->hasClient(c)) {
        return it->first;
      }
    }
    return -1;
  }
  /* Get the room with this room_id */
  Room* getRoom(int room_id) {
    if (this->rooms.find(room_id) != this->rooms.end()) {
      return this->rooms.find(room_id)->second;
    } else {
      return NULL;
    }
  }
  /* Set nickname for a client belonging local server */
  bool setNickname(Client c, int room_id, string name) {
    Room* target = this->getRoom(room_id);
    if (target->hasNickname(name)) {
      return false;
    }
    target->setNickname(c, name);
    return true;
  }
  /* broadcast a message to other servers */
  void broadcast(char* msg) {
    for(int i = 0; i < servers.size(); i++) {
      struct sockaddr_in target;
      bzero(&target, sizeof(target));
      Server* cur = servers.at(i);
      target = cur->local;
      int res = sendto(sock, msg, strlen(msg), 0, (struct sockaddr*)&target, sizeof(target));
      if (res < 0) {
        if (*vflag) printf("S[%d] failed sending msg to [%s]\n", this->id, cur->addr.c_str());
      } else {
        if (*vflag) printf("S[%d] sent a msg to [%s]\n", this->id, cur->addr.c_str());
      }
    }
  }
  /* Server multicasts a message to other servers */
  void multicast(Msg m) {
    char* msg = m.encoding();
    this->broadcast(msg);
    free(msg);
  }
  /* Deliver msg to all clients in a specific room */
  void deliverRoom(Msg m) {
    Room* target = this->getRoom(m.room_id);
    if (target == NULL) return;
    char* content = m.toString();
    vector<Client*>& clients = target->clients;
    for (int i = 0; i < clients.size(); i++) {
      Client* cur = clients.at(i);
      this->sendMsg(*cur, content);
      if (*vflag) printf("S[%d] delivered msg to [%s] in room [%d]\n", this->id, cur->nickname.c_str(), target->room_id);
    }
    free(content);
  }
  /* Server delivers an unordered msg */
  void deliverUnordered(Msg m) {
    this->deliverRoom(m);
  }
  /* Server do FO-multicast to other servers */
  void FOmulticast(Msg m) {
    int group = m.room_id;
    int msgID = ++this->seq[group - 1];
    m.setMsgID(msgID);
    char* msg = m.FIFOencoding();
    if (*vflag) printf("S[%d] is ready to FOmulticast [%s]\n", this->id, msg);
    this->broadcast(msg);
    free(msg);
  }
  /* Server delievers an unordered msg */
  void deliverFIFO(Msg m, Client c) {
    string sender = c.toString();
    int group = m.room_id;
    int msgID = m.msgID;
    vector<map<int, Msg> >& holdback = this->holdback[sender];
    vector<int>& seqRecent = this->seqRecent[sender];
    map<int, Msg>& queue = holdback[group - 1];
    queue[msgID] = m;
    int nextID = seqRecent.at(group - 1) + 1;
    while(!queue.empty() && queue.begin()->first == nextID) {
      Msg curMsg = queue.begin()->second;
      if (*vflag) printf("S[%d] delivered a msg to room [%d]\n", this->id, group);
      this->deliverRoom(curMsg);
      queue.erase(nextID);
      seqRecent.at(group - 1) = nextID;
      nextID++;
    }
  }
  /* Server do TOTAL-multicast to other servers */
  void TOTALmulticast(Msg m) {
    m.state = 0;
    char* msg = m.encodingTOTAL();
    if (*vflag) printf("S[%d] is ready to TOTALmulticast [%s]\n", this->id, msg);
    this->broadcast(msg);
    free(msg);
    priority_queue<pair<int, int> > queue;
    this->proposalCount[m] = queue;
  }
  /* Respond to the incoming request of a proposal */
  void respond(Client c, Msg m) {
    int group = m.room_id;
    int Pg_new = ::max(this->Pg[group - 1], this->Ag[group - 1]) + 1;
    this->Pg[group - 1] = Pg_new;
    // Add current Msg to holdback queue
    m.deliverable = false;
    m.msgID = Pg_new;
    this->totalHold.push(m);
    m.state = 1;
    m.nodeID = this->id;
    char* msg = m.encodingTOTAL();
    if (*vflag) printf("S[%d] is ready to make a proposal [%s]\n", this->id, msg);
    this->sendMsg(c, msg);
    free(msg);
  }
  /* Collect proposal from other nodes and make a decision multicast */
  void collect(Client& c, Msg& m) {
    for (map<Msg,priority_queue<pair<int, int> > >::iterator it=this->proposalCount.begin(); it!=this->proposalCount.end(); ++it) {
      Msg cur = it->first;
      if (m.equals(cur)) {
        priority_queue<pair<int, int> >& pq = it->second;
        int msgID = m.msgID;
        int nodeID = m.nodeID;
        pair<int, int> proposal(msgID, nodeID);
        pq.push(proposal);
        printf("The current PQ size is [%lu]\n", pq.size());
        if (pq.size() == this->servers.size()) {
          proposal = pq.top();
          Msg out = cur;
          out.msgID = proposal.first;
          out.nodeID = proposal.second;
          out.state = 2;
          char* msg = out.encodingTOTAL();
          if (*vflag) printf("S[%d] is ready to send a decision: [%s]\n", this->id, msg);
          this->broadcast(msg);
          free(msg);
          this->proposalCount.erase(it);
        }
        break;
      }
    }
  }
  /* Update a decision for a specific msg and deliver */
  void update(Client & c, Msg& m) {
    int group = m.room_id;
    priority_queue<Msg, vector<Msg>, Compare>& holdback = this->totalHold;
    priority_queue<Msg, vector<Msg>, Compare> temp;
    while (!holdback.empty()) {
      Msg cur = holdback.top();
      holdback.pop();
      cout << "[Server.h/update/line 241]: " << cur.content << endl;
      if (cur.equals(m)) {
        cur.nodeID = m.nodeID;
        cur.msgID = m.msgID;
        cur.deliverable = true;
        this->Ag[group-1] = ::max(m.msgID, this->Ag[group-1]);
      }
      temp.push(cur);
    }
    this->totalHold = temp;
    holdback = this->totalHold;
    Msg check = holdback.top();
    while (check.deliverable) {
      holdback.pop();
      this->deliverRoom(check);
      check = holdback.top();
    }
  }
};
