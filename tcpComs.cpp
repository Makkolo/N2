#include "tcpComs.hpp"
#include "n2Interpreter.hpp"

//TCP socket
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <netinet/in.h>

#include <poll.h>
#include <chrono>
#include <atomic>










int tcpInitHost()
{
    int serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    sockaddr_in adr;//{ 0 };
    adr.sin_family = AF_INET;
    adr.sin_port = htons(8888);
    adr.sin_addr.s_addr = INADDR_ANY;
    
    bind(serverSocket, (struct sockaddr*)&adr, sizeof(adr));

    return serverSocket;
}










int tcpGetClient(int serverSocket, int timeout_ms, std::atomic<bool> *interrupt)
{
    //Loop variables
    int clientSocket = -1, err;
    
    //Poll with timeout
    struct pollfd pfd;
    pfd.fd = serverSocket;
    pfd.events = POLLIN;

    //Start listening for new connections
    listen(serverSocket, 1);

    //Wait for and Accept inbound connection, return -1 on error or interrupt
    while (! (*interrupt))
    {
        err = poll(&pfd, 1, timeout_ms);
        if (pfd.revents & POLLIN)
        {
            clientSocket = accept(serverSocket, nullptr, nullptr);
            break;
        }
        else if ((pfd.revents & (POLLERR | POLLHUP | POLLNVAL)) || err < 0)
        {
            clientSocket = -1;
            break;
        }
    }

    //Stop listening for new connections
    close(serverSocket);

    //Set receive timeout
    /*struct timeval tv;
    tv.tv_sec = 1;
    tv.tv_usec = 0;
    setsockopt(clientSocket, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));*/

    return clientSocket;
}










int nodeRedToN2(n2RequestData *req, std::string msg)
{
    //Remove \n if it was not removed from the message
    /*int end = msg.find("\n");
    if (end >= 0)
        msg.erase(end, 1);*/

    //Extract first entry as Request ID
	int end = msg.find(",");
    if (end < 0)
        return 0x10;
    req->requestId = msg.substr(0, end);
	msg.erase(0, end+1);

    //Extract second entry as PLC ID
    end = msg.find(",");
    if (end < 0)
        return 0x10;
    req->deviceId = std::stoi(msg.substr(0, end));
    msg.erase(0, end+1);

    
    //Extract third entry as module
    end = msg.find(",");
    if (end < 0)
        return 0x10;
    req->module = (msg.substr(0, end));
    msg.erase(0, end+1);
    
    if (moduleSize.find(req->module) == moduleSize.end())
        return 0x10;
    
    req->binary = ((!(moduleSize.at(req->module)) || req->module == "MAN.B"));

    //Extract PGM submodule + item as fourth entry if module = PGM
    if (req->module.find("PGM") != std::string::npos)
    {
        end = msg.find(",");
        if (end < 0)
            return 0x10;
    	req->pgm = (msg.substr(0, end));
    	msg.erase(0, end+1);
    }
	else
		req->pgm = std::monostate{};


    //Extract index and return if there is no data
    end = msg.find(",");
	if ((size_t)end == std::string::npos)
	{
		req->data = std::monostate{};
    	req->index = std::stoi(msg);
		return 0;
	}

    //Extract index
	req->index = stoi(msg.substr(0, end));
    msg.erase(0, end+1);
	end = msg.length();

    //Extract data
    if (req->binary)
        req->data = uint8_t(msg == "true" || msg == "1");
    else
		req->data = std::stof(msg.substr(0, end));

	return 0;
}










std::string n2ToNodeRed(const n2RequestData req)
{
	std::string msg = req.requestId + ",";
	if (std::holds_alternative<float>(req.data))
		(msg)+= std::to_string(std::get<float>(req.data));
	else if (std::holds_alternative<uint8_t>(req.data))
		(msg)+= std::to_string((std::get<uint8_t>(req.data)) & ((0x1 << (req.index - 1)) == 1));
	else if (std::holds_alternative<uint16_t>(req.data))
		(msg)+= std::to_string((std::get<uint16_t>(req.data)) & ((0x1 << (req.index - 1 )) == 1));
	else
		(msg)+= std::get<std::string>(req.data);

	(msg) += "\n";

	return msg;
}