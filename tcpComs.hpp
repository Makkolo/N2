#pragma once

#include "n2Interpreter.hpp"
#include <atomic>

//TCP socket
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <netinet/in.h>

#include <poll.h>
#include <chrono>
#include <atomic>

#include "tools.hpp"



int tcpInitHost();
int tcpGetClient(int serverSocket, int timeout, std::atomic<bool> *interrupt);
int tcpToN2(n2RequestData *tcpReq, std::string msg);
std::string n2ToNodeRed(const n2RequestData tcpAns);