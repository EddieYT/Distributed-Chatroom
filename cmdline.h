using namespace std;

enum Order { DEFAULT, FIFO, TOTAL };
extern bool vflag;
extern int state;
extern string filename, serverId;

void process_cml(int argc, char *argv[]) {
  int opt;
  string order;
  while ((opt = getopt(argc, argv, "o:v")) != -1) {
    switch(opt) {
      case 'v':
        vflag = true;
        break;
      case 'o':
        order = string(optarg);
        transform(order.begin(), order.end(), order.begin(), ::tolower);
        if (order == "unordered") {
          state = DEFAULT;
        } else if (order == "fifo") {
          state = FIFO;
        } else if (order == "total"){
          state = TOTAL;
        } else {
          cout << "Wrong argument for selecting an order." << endl;
          exit(-1);
        }
        break;
      default:
        abort();
    }
  }

  int count = 0;
  while (count < 2) {
    if (optind >= argc) {
      cout << "Please enter a filename and a server index." << endl;
      exit(-1);
    }
    if (count == 0) filename = argv[optind];
    if (count == 1) serverId = argv[optind];
    count++;
    optind++;
  }
}
