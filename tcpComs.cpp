#include "tcpComs.hpp"










int tcpInitHost()
{
    int serverSocket = socket(AF_INET, SOCK_STREAM, 0);

    if (serverSocket < 0)
        return -1;
    sockaddr_in adr;//{ 0 };
    adr.sin_family = AF_INET;
    adr.sin_port = htons(8888);
    adr.sin_addr.s_addr = INADDR_ANY;

    if (bind(serverSocket, (struct sockaddr*)&adr, sizeof(adr)))
        return -1;

    return serverSocket;
}










int tcpGetClient(int serverSocket, int timeout_ms, std::atomic<bool> *interrupt)
{
    //Return error if invalid server socket
    if (serverSocket < 0)
        return serverSocket;

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










int tcpToN2(n2RequestData *req, std::string msg)
{
    const char seperator = ',';
    //Extract first entry as Request ID
    if (extractString(&msg, &(req->requestId), seperator))
        return 0x10;

    std::string temp;
    //Extract second entry as PLC ID
    if (extractString(&msg, &temp, seperator))
        return 0x10;
    req->deviceId = std::stoi(temp);
    
    //Extract third entry as module
    if (extractString(&msg, &(req->module), seperator))
        return 0x10;


    //Verify that module exists, return error code if not
    if (n2Modules.find(req->module) == n2Modules.end())
        return 0x10;

    //Get module parameters
    //n2ModuleInfo modInfo = n2Modules.at(req->module);


    //Extract fourth entry as item
    if(extractString(&msg, &(req->item), seperator))
        return 0x10;

    //Verify that item exists, return error code if not
    if (n2Modules.at(req->module).items.find(req->item) == n2Modules.at(req->module).items.end())
        return 0x10;
    
    

    //Extract fifth as index and return if there is no data
    if(extractString(&msg, &temp, seperator))
        return 0x10;
    req->index = std::stoi(temp);
    
    //If string is empty, no data was sent, otherwise assume remaining string is data
    if (msg.length() > 0)
    {
        if (req->item == "BOOL")
            req->data = uint8_t(msg == "true" || msg == "1");
        else
            req->data = std::stof(msg);
    }
    else
        req->data = std::monostate{};

    //Verify that the index is valid 
    if (req->index <= n2Modules.at(req->module).count || req->item == "BOOL")
        return 0;
	else
        return 0x10;
}










std::string n2ToNodeRed(const n2RequestData req)
{
	std::string msg = req.requestId + ",";
	if (std::holds_alternative<float>(req.data))
		(msg)+= std::to_string(std::get<float>(req.data));
	else if (std::holds_alternative<uint8_t>(req.data))
        (msg)+= std::to_string(std::get<uint8_t>(req.data));
	else if (std::holds_alternative<uint16_t>(req.data))
        (msg)+= std::to_string(std::get<uint16_t>(req.data));
	else
		(msg)+= std::get<std::string>(req.data);

	(msg) += "\n";

	return msg;
}