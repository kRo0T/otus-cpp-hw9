#include "async.h"
#include <iostream>

namespace async {

handle_t connect(std::size_t bulk) {
    std::cout << "Connect\n";
    return nullptr;
}

void receive(handle_t handle, const char *data, std::size_t size) {
    std::cout << "Receive\n";
}

void disconnect(handle_t handle) {
    std::cout << "Disconnect\n";
}

}
