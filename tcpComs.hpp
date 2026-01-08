#pragma once

#include "n2Interpreter.hpp"
#include <atomic>



int tcpInitHost();
int tcpGetClient(int serverSocket, int timeout, std::atomic<bool> *interrupt);
int nodeRedToN2(n2RequestData *tcpReq, std::string msg);
std::string n2ToNodeRed(const n2RequestData tcpAns);