using namespace std;

class Client {
public:
  int group;
  string nickname;
  struct sockaddr_in addr;
  /* The constructor of Client */
  Client(struct sockaddr_in input) {
    this->addr = input;
  };
  /* Check if two clients are equal */
  bool equals(Client& c) {
    if (addr.sin_addr.s_addr == c.addr.sin_addr.s_addr && this->addr.sin_port == c.addr.sin_port) {
      return true;
    }
    return false;
  }
  string toString() {
    string ip = inet_ntoa(addr.sin_addr);
    string port = std::to_string(ntohs(addr.sin_port));
    string src = ip + ":" + port;
    return src;
  }
private:
};
