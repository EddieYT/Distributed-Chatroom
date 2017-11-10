using namespace std;

class Msg {
public:
  int room_id;
  int msgID;
  int nodeID;
  int state;
  bool deliverable;
  string nickname;
  string content;
  Msg() {

  }
  /* Constructor */
  Msg(string content) {
      this->content = content;
  }
  /* Check if two msgs are equal */
  bool equals(Msg& m) {
    if (m.room_id == this->room_id && m.nickname == this->nickname
      && m.content == this->content) {
      return true;
    }
    return false;
  }
  /* Convert the Msg into a chat room syntax */
  char* toString() {
    string output = "<" + nickname + "> " + content;
    char temp[100];
    strcpy(temp, output.c_str());
    char* res = (char*) malloc(sizeof(temp));
    strcpy(res, temp);
    return res;
  }
  /* Encoding the Msg for sending to other servers */
  char* encoding() {
    string output = to_string(room_id) + "," + nickname + "," + content;
    char temp[100];
    strcpy(temp, output.c_str());
    char* res = (char*) malloc(sizeof(temp));
    strcpy(res, temp);
    return res;
  }
  /* Decoding data from other servers into Msg */
  void decode(char* input) {
    string temp = input;
    vector<string> split;
    size_t pos = temp.find(",");
    size_t pre = 0;
    int count = 0;
    while (pos != string::npos) {
      split.push_back(temp.substr(pre, pos-pre));
      count++;
      if (count == 2) {
        split.push_back(temp.substr(++pos));
        break;
      }
      pre = ++pos;
      pos = temp.find(",", pos);
    }
    for (int i = 0; i < split.size(); i++) {
      if (i == 0) this->room_id = stoi(split.at(i));
      if (i == 1) this->nickname = split.at(i);
      if (i == 2) this->content = split.at(i);
    }
  }
  /* Encoding the Msg for FIFO */
  char* FIFOencoding() {
    string output = to_string(room_id) + "," + to_string(msgID) + "," + nickname + "," + content
    + "," + to_string(nodeID) + "," + to_string(this->state);
    char temp[256];
    strcpy(temp, output.c_str());
    char* res = (char*) malloc(sizeof(temp));
    strcpy(res, temp);
    return res;
  }
  /* Decoding data from other servers into FIFO Msg */
  void FIFOdecode(char* msg) {
    string temp = msg;
    vector<string> split;
    size_t pos = temp.find(",");
    size_t pre = 0;
    int count = 0;
    while (pos != string::npos) {
      split.push_back(temp.substr(pre, pos-pre));
      count++;
      pre = ++pos;
      pos = temp.find(",", pos);
      if (count == 3) {
        split.push_back(temp.substr(pre, pos-pre));
        break;
      }
    }
    for (int i = 0; i < split.size(); i++) {
      if (i == 0) this->room_id = stoi(split.at(i));
      if (i == 1) this->msgID = stoi(split.at(i));
      if (i == 2) this->nickname = split.at(i);
      if (i == 3) this->content = split.at(i);
    }
  }
  /* Encoding the Msg for TOTAL ordering */
  char* encodingTOTAL() {
    string state;
    if (this->state == 0) {
      // From sender's request
      state= "0";
    } else if (this->state == 1){
      // From responder's proposal
      state = "1";
    } else if (this->state == 2) {
      // From sender's decision
      state = "2";
    }
    string output = to_string(room_id) + "," + to_string(msgID) + "," + nickname + "," + content
    + "," + to_string(nodeID) + "," + state;
    char temp[256];
    strcpy(temp, output.c_str());
    char* res = (char*) malloc(sizeof(temp));
    strcpy(res, temp);
    return res;
  }
  /* Decoding data from other servers into TOTAL ordering Msg */
  void decodeTOTAL(char* msg) {
    string temp = msg;
    vector<string> split;
    size_t pos = temp.find(",");
    size_t pre = 0;
    int count = 0;
    while (pos != string::npos) {
      split.push_back(temp.substr(pre, pos-pre));
      count++;
      if (count == 5) {
        split.push_back(temp.substr(++pos));
        break;
      }
      pre = ++pos;
      pos = temp.find(",", pos);
    }
    for (int i = 0; i < split.size(); i++) {
      if (i == 0) this->room_id = stoi(split.at(i));
      if (i == 1) this->msgID = stoi(split.at(i));
      if (i == 2) this->nickname = split.at(i);
      if (i == 3) this->content = split.at(i);
      if (i == 4) this->nodeID = stoi(split.at(i));
      if (i == 5) this->state= stoi(split.at(i));
    }
  }
  /* Set the nickname inside this msg */
  void setNickname(string name) {
    this->nickname = name;
  }
  /* Set the room id inside this msg */
  void setGroup(int room_id) {
    this->room_id = room_id;
  }
  /* Set the room id inside this msg */
  void setMsgID(int id) {
    this->msgID = id;
  }
private:
};

/* Comparator for holdback queue*/
class Compare {
public:
  bool operator() (const Msg& lhs, const Msg&rhs) const
  {
    if (lhs.msgID > rhs.msgID) {
      return true;
    } else if (lhs.msgID == rhs.msgID) {
      return lhs.nodeID < rhs.nodeID;
    }
    return false;
  }
};
