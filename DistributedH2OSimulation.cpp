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

//DEFAULT VALUES
int RECEIVING_PORT = 27020;
string RECEIVING_IP = "127.0.0.2";
int SENDING_PORT = 27015;
string SENDING_IP = "127.0.0.1";

// Class to represent a bond request
class BondRequest {
public:
    string id;
    string type; // "H" for Hydrogen, "O" for Oxygen
    string status; // "request" or "bonded"
    string timestamp;

    BondRequest(string id, string type, string status, string timestamp)
        : id(id), type(type), status(status), timestamp(timestamp) {}

    
    string serialize() const {
        stringstream ss;
        ss << id << "," << type << "," << status << "," << timestamp;
        return ss.str();
    }

    
    static BondRequest deserialize(const string& data) {
        stringstream ss(data);
        string id, type, status, timestamp;
        getline(ss, id, ',');
        getline(ss, type, ',');
        getline(ss, status, ',');
        getline(ss, timestamp, ',');
        return BondRequest(id, type, status, timestamp);
    }

    void log(){
        cout << "(" << type << id << ", " << status << ", " << timestamp << ")\n";
    }
};

// Class to represent both hydrogen and oxygen client
class Client {
protected:
    SOCKET clientSocket;
    string type; // "H" for Hydrogen, "O" for Oxygen
    int count;

public:
    Client(string type, int count) : type(type), count(count) {}

    virtual ~Client() {
        closesocket(clientSocket);
        WSACleanup();
    }

    virtual void sendRequests() = 0;
};

class HydrogenClient : public Client {
public:
    HydrogenClient(int count) : Client("H", count) {}

    void sendRequests() override {
        
        WSADATA wsaData;
        int iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
        if (iResult != 0) {
            cout << "WSAStartup failed: " << iResult << endl;
            return;
        }

        
        clientSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (clientSocket == INVALID_SOCKET) {
            cout << "Error creating socket: " << WSAGetLastError() << endl;
            WSACleanup();
            return;
        }

        
        sockaddr_in serverAddr;
        serverAddr.sin_family = AF_INET;
        serverAddr.sin_addr.s_addr = inet_addr(SENDING_IP.c_str());
        serverAddr.sin_port = htons(SENDING_PORT);

        if (connect(clientSocket, reinterpret_cast<sockaddr*>(&serverAddr), sizeof(serverAddr)) == SOCKET_ERROR) {
            cout << "Unable to connect to server: " << WSAGetLastError() << endl;
            closesocket(clientSocket);
            WSACleanup();
            return;
        }

        // Send bond requests
        for (int i = 1; i <= count; ++i) {
            stringstream ss;
            ss << type << i;
            string id = ss.str();
            auto timestamp = chrono::system_clock::now();
            std::time_t now_c = chrono::system_clock::to_time_t(timestamp);
            std::tm * ptm = std::localtime(&now_c);
            char buffer[20]; // assuming timestamp will fit within 20 characters
            std::strftime(buffer, 20, "%Y-%m-%d %H:%M:%S", ptm);
            string timestampStr = buffer;
            BondRequest request(id, type, "request", timestampStr);
            string serializedRequest = request.serialize();

            
            send(clientSocket, serializedRequest.c_str(), serializedRequest.size(), 0);

            //Log request
            request.log();

            this_thread::sleep_for(chrono::milliseconds(100)); // delay
        }
        
        closesocket(clientSocket);
		WSACleanup();
    }
};


class OxygenClient : public Client {
public:
    OxygenClient(int count) : Client("O", count) {}

    void sendRequests() override {
   
    WSADATA wsaData;
    int iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (iResult != 0) {
        cout << "WSAStartup failed: " << iResult << endl;
        return;
    }

  
    SOCKET clientSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (clientSocket == INVALID_SOCKET) {
        cout << "Error creating socket: " << WSAGetLastError() << endl;
        WSACleanup();
        return;
    }

    
    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = inet_addr(SENDING_IP.c_str());
    serverAddr.sin_port = htons(SENDING_PORT);

    if (connect(clientSocket, reinterpret_cast<sockaddr*>(&serverAddr), sizeof(serverAddr)) == SOCKET_ERROR) {
        cout << "Unable to connect to server: " << WSAGetLastError() << endl;
        closesocket(clientSocket);
        WSACleanup();
        return;
    }

    // Send bond requests
    for (int i = 1; i <= count; ++i) {
        stringstream ss;
        ss << type << i;
        string id = ss.str();
        auto timestamp = chrono::system_clock::now();
        std::time_t now_c = chrono::system_clock::to_time_t(timestamp);
        std::tm * ptm = std::localtime(&now_c);
        char buffer[20]; // assuming timestamp will fit within 20 characters
        std::strftime(buffer, 20, "%Y-%m-%d %H:%M:%S", ptm);
        string timestampStr = buffer;
        BondRequest request(id, type, "request", timestampStr);
        string serializedRequest = request.serialize();

        
        send(clientSocket, serializedRequest.c_str(), serializedRequest.size(), 0);

        //Log request
        request.log();

        this_thread::sleep_for(chrono::milliseconds(100)); // delay
    }

    
    closesocket(clientSocket);
    WSACleanup();
}
};

// Server class
class Server {
private:
    SOCKET serverSocket;
    vector<SOCKET> clientSockets;
    mutex mtx;
    condition_variable cv;
    vector<BondRequest> hydrogenRequests;
    vector<BondRequest> oxygenRequests;

public:
    Server() {
        
        WSADATA wsaData;
        int iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
        if (iResult != 0) {
            cout << "WSAStartup failed: " << iResult << endl;
            return;
        }

        
        serverSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (serverSocket == INVALID_SOCKET) {
            cout << "Error creating socket: " << WSAGetLastError() << endl;
            WSACleanup();
            return;
        }

        // bind socket
        sockaddr_in serverAddr;
        serverAddr.sin_family = AF_INET;
        serverAddr.sin_addr.s_addr = inet_addr(RECEIVING_IP.c_str());
        serverAddr.sin_port = htons(RECEIVING_PORT);
        if (bind(serverSocket, reinterpret_cast<sockaddr*>(&serverAddr), sizeof(serverAddr)) == SOCKET_ERROR) {
            cout << "Bind failed with error: " << WSAGetLastError() << endl;
            closesocket(serverSocket);
            WSACleanup();
            return;
        }

        // listen for connections
        if (listen(serverSocket, SOMAXCONN) == SOCKET_ERROR) {
            cout << "Listen failed with error: " << WSAGetLastError() << endl;
            closesocket(serverSocket);
            WSACleanup();
            return;
        }

        cout << "Server Listening on " << RECEIVING_IP << ":" << RECEIVING_PORT << endl;
        start();
    }

    ~Server() {
        closesocket(serverSocket);
        for (SOCKET clientSocket : clientSockets) {
            closesocket(clientSocket);
        }
        WSACleanup();
    }

    void start() {
        //Split off thread that checks for bond requests

        while (true) {
            // accept client connection
            SOCKET clientSocket = accept(serverSocket, NULL, NULL);
            if (clientSocket == INVALID_SOCKET) {
                cout << "Accept failed with error: " << WSAGetLastError() << endl;
                closesocket(serverSocket);
                WSACleanup();
                return;
            }

            
            clientSockets.push_back(clientSocket);

            
            thread clientThread(&Server::handleClient, this, clientSocket);
            clientThread.detach();
        }
    }

    void handleClient(SOCKET clientSocket) {
        char buffer[1024];
        int bytesReceived;
        while ((bytesReceived = recv(clientSocket, buffer, sizeof(buffer), 0)) > 0) {
            buffer[bytesReceived] = '\0';
            string data(buffer);

           
            BondRequest request = BondRequest::deserialize(data);

            //Log request
            request.log();

            
            if (request.type == "H") {
                unique_lock<mutex> lock(mtx);
                hydrogenRequests.push_back(request);
            } else if (request.type == "O") {
                unique_lock<mutex> lock(mtx);
                oxygenRequests.push_back(request);
            }

            
            cv.notify_all();
        }

        // if recv returns 0, the client disconnected gracefully
        if (bytesReceived == 0) {
            cout << "Client disconnected" << endl;
        } else if (bytesReceived == SOCKET_ERROR) {
            cout << "recv failed with error: " << WSAGetLastError() << endl;
        }

        
        for (auto it = clientSockets.begin(); it != clientSockets.end(); ++it) {
            if (*it == clientSocket) {
                clientSockets.erase(it);
                break;
            }
        }

        closesocket(clientSocket);
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

int main() {
    string inputStr;
    int inputInt;

    //Ask if the program will run a client or a server
    while(true){
        cout << "Client(C) or Server(S)?: " ;
        cin >> inputStr;
        if(inputStr != "C" && inputStr != "S" &&
        inputStr != "c" && inputStr != "s")
            cout << "Invalid Input\n";
        else break;
    }

    //If client, what kind?
    if(inputStr == "C" || inputStr == "c"){
        while(true){
            cout << "Oxygen(O) or Hydrogen(H)?: ";
            cin >> inputStr;
            if(inputStr != "O" && inputStr != "o" &&
            inputStr != "H" && inputStr != "h")
                cout << "Invalid Input\n";
            else break;
        }

        //How many requests
        cout << "How many requests?: ";
        cin >> inputInt;

        //Client IP and PORT
        cout << "CLIENT IP: ";
        cin >> RECEIVING_IP;
        cout << "CLIENT PORT: ";
        cin >> RECEIVING_PORT;
        
        //Server IP and PORT
        cout << "SERVER IP: ";
        cin >> SENDING_IP;
        cout << "SERVER PORT: ";
        cin >> SENDING_PORT;

        if(inputStr == "O" || "o"){
            OxygenClient oxygenClient(inputInt);
            oxygenClient.sendRequests();
        }
        else if(inputStr == "H" || "h"){
            HydrogenClient hydrogenClient(inputInt);
            hydrogenClient.sendRequests();
        }
    }

    //If server, ask IP and PORT
    else if(inputStr == "S" || inputStr == "s"){
        cout << "IP: ";
        cin >> RECEIVING_IP;
        cout << "PORT: ";
        cin >> RECEIVING_PORT;

        Server server;
    }
    
    return 0;
}
