#include <iostream>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <vector>
#include <chrono>
#include <sstream>
#include <WinSock2.h>
#include <iomanip> 
#include <ctime>   

#pragma comment(lib, "ws2_32.lib")

using namespace std;

// Class to represent a bond request
class BondRequest {
public:
    string id;
    string type; // "H" for Hydrogen, "O" for Oxygen
    string status; // "request" or "bonded"
    string timestamp;

    BondRequest(string id, string type, string status, string timestamp)
        : id(id), type(type), status(status), timestamp(timestamp) {}
};

class Server {
private:
    mutex mtx;
    condition_variable cv;
    vector<BondRequest> hydrogenRequests;
    vector<BondRequest> oxygenRequests;

public:
    void addHydrogenRequest(const BondRequest& request) {
        unique_lock<mutex> lock(mtx);
        hydrogenRequests.push_back(request);
        cv.notify_all();
    }

    void addOxygenRequest(const BondRequest& request) {
        unique_lock<mutex> lock(mtx);
        oxygenRequests.push_back(request);
        cv.notify_all();
    }

    void bond() {
        unique_lock<mutex> lock(mtx);
        cv.wait(lock, [this] {
            return hydrogenRequests.size() >= 2 && oxygenRequests.size() >= 1;
        });

        // bonding
        auto hydrogen1 = hydrogenRequests.back();
        hydrogenRequests.pop_back();
        auto hydrogen2 = hydrogenRequests.back();
        hydrogenRequests.pop_back();
        auto oxygen = oxygenRequests.back();
        oxygenRequests.pop_back();

        cout << "Bonded: " << hydrogen1.id << ", " << hydrogen2.id << ", " << oxygen.id << endl;
    }
};


class Client {
protected:
    Server& server;
    string type; // "H" for Hydrogen, "O" for Oxygen
    int count;
    mutex mtx;

public:
    Client(Server& server, string type, int count) : server(server), type(type), count(count) {}

    void sendRequests() {
    for (int i = 1; i <= count; ++i) {
        stringstream ss;
        ss << type << i;
        string id = ss.str();
        auto timestamp = chrono::system_clock::now();
        
        // Convert time_t to string
        std::string timestampStr;
        {
            std::time_t now_c = std::chrono::system_clock::to_time_t(timestamp);
    		std::tm * ptm = std::localtime(&now_c);
    		char buffer[20]; // assuming timestamp will fit within 20 characters
    		std::strftime(buffer, 20, "%Y-%m-%d %H:%M:%S", ptm);
    		timestampStr = buffer;
        }

        BondRequest request(id, type, "request", timestampStr);

        {
            lock_guard<mutex> lock(mtx);
            cout << "Sending request: " << request.id << endl;
        }

        if (type == "H") {
            server.addHydrogenRequest(request);
        } else {
            server.addOxygenRequest(request);
        }

        this_thread::sleep_for(chrono::milliseconds(100)); // Simulating some delay between requests
    }
	}
};

// Hydrogen Client class
class HydrogenClient : public Client {
public:
    HydrogenClient(Server& server, int count) : Client(server, "H", count) {}

    void start() {
        sendRequests();
    }
};

// Oxygen Client class
class OxygenClient : public Client {
public:
    OxygenClient(Server& server, int count) : Client(server, "O", count) {}

    void start() {
        sendRequests();
    }
};

int main() {
    WSADATA wsaData;
    int iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (iResult != 0) {
        cout << "WSAStartup failed: " << iResult << endl;
        return 1;
    }

    Server server;
    int hydrogenCount = 5; // num of Hydrogen molecules
    int oxygenCount = 2;   // num of Oxygen molecules

    
    thread hydrogenThread([&] {
        HydrogenClient hydrogenClient(server, hydrogenCount);
        hydrogenClient.start();
    });

    thread oxygenThread([&] {
        OxygenClient oxygenClient(server, oxygenCount);
        oxygenClient.start();
    });

    // Start bonding once all requests are sent
    hydrogenThread.join();
    oxygenThread.join();

    // Bonding phase
    for (int i = 0; i < hydrogenCount + oxygenCount; ++i) {
        server.bond();
    }

    WSACleanup();

    return 0;
}

