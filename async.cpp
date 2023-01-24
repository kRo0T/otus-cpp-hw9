#include "async.h"
#include <iostream>
#include <string>
#include <list>
#include <vector>
#include <ctime>
#include <fstream>
#include <unordered_map>
#include <unordered_set>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <sstream>

namespace async {

using namespace std;

class IPrinter {
protected:
    void print_to(ostream& stream, const vector<string>& bulk) {
        stream << "bulk: ";
        for (int i = 0; i < bulk.size(); i++) {
            stream << bulk[i];
            if (i != bulk.size()-1) {
                stream << ", ";
            }
        }
        stream << endl;
    }
public:
    virtual void print(const vector<string>& bulk, time_t block_time) = 0;
};

class ConsolePrinter : public IPrinter {
public:
    void print(const vector<string>& bulk, time_t block_time) {
        print_to(cout, bulk);
    }
};

class FilePrinter : public IPrinter {
public:
    void print(const vector<string>& bulk, time_t block_time) {
        ofstream file;
        file.open("bulk" + to_string(block_time) + ".log");
        print_to(file, bulk);
        file.close();
    }
};

class PacketHandler {
    const int N = 3;
    vector<string> bulk;
    int dyn_block_nesting = 0;
    std::time_t block_time;
    list<IPrinter*> printers;
public:
    PacketHandler() = default;
    ~PacketHandler() {
        cout << "Packet handler dctr" << endl;
    }
    void add_packet(string line) {
        if (line == "{") {
            dyn_block_nesting++;
            if (dyn_block_nesting == 1) {
                pop_print();
            }
        } else if (line == "}") {
            dyn_block_nesting--;
            if (dyn_block_nesting == 0) {
                pop_print();
            }
        } else {
            if (bulk.empty()) {
                block_time = time(nullptr);
            }
            bulk.push_back(line);
            if (dyn_block_nesting == 0 && bulk.size() == N) {
                pop_print();
            }
        }
    }

    void pop_print() {
        if (!bulk.empty()) {
            notify();
            bulk.clear();
        }
    }

    void process() {
        for (string line; getline(cin, line);) {
            add_packet(line);
        }
        if (dyn_block_nesting == 0) {
            pop_print();
        }
    }

    void process(std::string input) {
        std::istringstream istream(input);
        for (string line; getline(istream, line);) {
            if (!line.empty())
                add_packet(line);
        }
    }

    void attach(IPrinter* printer) {
        printers.push_back(printer);
    }

    void detach(IPrinter* printer) {
        printers.remove(printer);
    }

    void notify() {
        for (auto printer : printers) {
            printer->print(bulk, block_time);
        }
    }
};

class HandleDispatch {
private:
    std::mutex mutex_;
    ConsolePrinter cp;
    FilePrinter fp;
    HandleDispatch()
    {
        cout << "Handle dispatch ctr" << endl;
    }
    ~HandleDispatch() {
        cout << "Handle dispatch dctr" << endl;
    }
public:
    HandleDispatch(HandleDispatch &other) = delete;
    void operator=(const HandleDispatch &) = delete;

    static HandleDispatch& get_instance() {
        static HandleDispatch instance;
        return instance;
    }
    PacketHandler* create_handler(size_t bulk) {
        auto ph = new PacketHandler();
        ph->attach(&cp);
        return ph;
    }

    void execute_handler(PacketHandler* ph, const char* data) {
        string input = string(data);
        ph->process(data);
    }

    void destroy_handler(PacketHandler* ph) {
        if (dyn_block_nesting == 0) {
            //pop_print();
        }
        delete ph;
    }
};

int notmain() {
    PacketHandler ph;
    ConsolePrinter cp;
    FilePrinter fp;
    ph.attach(&cp);
    ph.attach(&fp);
    ph.process();
    return 0;
}

handle_t connect(std::size_t bulk) {
    std::cout << "Connect\n";
    auto& hd = HandleDispatch::get_instance();
    auto ph = hd.create_handler(3);
    
    return ph;
}

void receive(handle_t handle, const char *data, std::size_t size) {
    std::cout << "Receive\n";
    auto& hd = HandleDispatch::get_instance();
    hd.execute_handler(static_cast<PacketHandler*>(handle), data);
}

void disconnect(handle_t handle) {
    std::cout << "Disconnect\n";
    auto& hd = HandleDispatch::get_instance();
    hd.destroy_handler(static_cast<PacketHandler*>(handle));
}

}
