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
#include <queue>
#include <atomic>

namespace async {

using namespace std;

struct Bulk {
    vector<string> commands;
    time_t block_time;
};

class IPrinter {
protected:
    std::queue<Bulk> q;
    std::mutex m;
    std::condition_variable cv;
    bool stopped = false;

    void print_to(ostream& stream, const Bulk& bulk) {
        stream << "bulk: ";
        for (int i = 0; i < bulk.commands.size(); i++) {
            stream << bulk.commands[i];
            if (i != bulk.commands.size()-1) {
                stream << ", ";
            }
        }
        stream << endl;
    }

public:
    virtual void print(const Bulk& bulk) = 0;
    void worker() {
        while (true) {
            std::unique_lock<std::mutex> lock(m);
            cv.wait(lock, [this](){ return !q.empty() || stopped; });
            if (stopped) {
                return;
            }
            Bulk bulk = q.front();
            q.pop();
            lock.unlock();
            // Process task
            print(bulk);
        }
    }
    std::thread spawn() {
        return std::thread(&IPrinter::worker, this);
    }
    void stop_work() {
        while (!q.empty()) {
            this_thread::sleep_for(chrono::seconds(1));
        }
        stopped = true;
        cv.notify_all();
    }
    void add_task(Bulk& bulk) {
        std::unique_lock<std::mutex> lock(m);
        q.push(bulk);
        lock.unlock();
        cv.notify_one();
    }
};

class ConsolePrinter : public IPrinter {
public:
    void print(const Bulk& bulk) {
        print_to(cout, bulk);
    }
};

class FilePrinter : public IPrinter {
    std::atomic_int id = 0;
public:
    void print(const Bulk& bulk) {
        ofstream file;
        file.open("bulk" + to_string(bulk.block_time) + "_" + to_string(id++) + ".log");
        print_to(file, bulk);
        file.close();
    }
};

class PacketHandler {
    const size_t N;
    Bulk bulk;
    int dyn_block_nesting = 0;
    list<IPrinter*> printers;
public:
    PacketHandler(size_t N) : N(N) {}
    bool is_zero_nesting() {
        return true ? dyn_block_nesting == 0 : false;
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
            if (bulk.commands.empty()) {
                bulk.block_time = time(nullptr);
            }
            bulk.commands.push_back(line);
            if (dyn_block_nesting == 0 && bulk.commands.size() == N) {
                pop_print();
            }
        }
    }

    void pop_print() {
        if (!bulk.commands.empty()) {
            notify();
            bulk.commands.clear();
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
            //printer->print(bulk);
            printer->add_task(bulk);
        }
    }
};

class HandleDispatch {
private:
    std::mutex mtx;
    bool stopped = false;
    ConsolePrinter cp;
    FilePrinter fp;
    thread w1, w2, w3;
    HandleDispatch()
    {
        w1 = cp.spawn();
        w2 = fp.spawn();
        w3 = fp.spawn();
    }
    ~HandleDispatch() {
        cp.stop_work();
        fp.stop_work();
        w1.join();
        w2.join();
        w3.join();
    }
public:
    HandleDispatch(HandleDispatch &other) = delete;
    void operator=(const HandleDispatch &) = delete;

    static HandleDispatch& get_instance() {
        static HandleDispatch instance;
        return instance;
    }
    PacketHandler* create_handler(size_t bulk) {
        auto ph = new PacketHandler(bulk);
        ph->attach(&cp);
        ph->attach(&fp);
        return ph;
    }

    void execute_handler(PacketHandler* ph, const char* data) {
        string input = string(data);
        ph->process(data);
    }

    void destroy_handler(PacketHandler* ph) {
        if (ph->is_zero_nesting()) {
            ph->pop_print();
        }
        delete ph;
    }
};

handle_t connect(std::size_t bulk) {
    auto& hd = HandleDispatch::get_instance();
    auto ph = hd.create_handler(bulk);
    
    return ph;
}

void receive(handle_t handle, const char *data, std::size_t size) {
    auto& hd = HandleDispatch::get_instance();
    hd.execute_handler(static_cast<PacketHandler*>(handle), data);
}

void disconnect(handle_t handle) {
    auto& hd = HandleDispatch::get_instance();
    hd.destroy_handler(static_cast<PacketHandler*>(handle));
}

}
