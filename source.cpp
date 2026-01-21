#include <iostream>

#include <string.h>
#include <thread>
#include <mutex>
#include <atomic>
#include <queue>
#include <errno.h>
#include <csignal>
#include <poll.h>

//TCP socket
#include <sys/socket.h>

//Serial
#include <unistd.h> 

#include "n2Interpreter.hpp"
#include "tcpComs.hpp"





//Global constants
const int tcpRxBufferSize = 128;
const int tcpTxBufferSize = 64;
const int serialRxBufferSize = 16;
const int serialTxBufferSize = 32;
const int serialRetries = 10;
const int pollTimeout_ms = 500;
const std::chrono::duration threadIdleTime = std::chrono::milliseconds(200);


//Variable for lambda threads to terminate threads
std::atomic<bool> terminateProgram = false;
std::atomic<bool> restartProgram = false;










int tcpRx(std::queue<std::string> *queue, std::mutex *lockQ, int *socket, std::atomic<int> *threadError, std::atomic<bool> *stop)
{
    //Loop variables
    int buffZ = 0, len, err, i;
    char buff[tcpRxBufferSize] = {0};
    std::string msg;

    //Poll with timeout
    struct pollfd pfd;
    pfd.fd = *socket;
    pfd.events = POLLIN;

    //Working loop
    while (!*stop)
    {
        err = poll(&pfd, 1, pollTimeout_ms);

        //Get inbound tcp data and size of data.
        if (pfd.revents & POLLIN)
        {
            len = recv(*socket, &buff[buffZ], tcpRxBufferSize - buffZ, 0);
            if (len > 0)
            {
                len+=buffZ;
                buffZ = 0;

                //
                while (true)
                {
                    for (i = buffZ; i < (len-1) && buff[i] != '\n'; i++)
                        ;
                        
                    if (buff[i] == '\n')
                    {
                        //Add new requests to the queue
                        msg.assign(&buff[buffZ], i-buffZ);
                        lockQ->lock();
                        queue->push(msg);
                        lockQ->unlock();
                        if (i == (len-1))
                        {
                            buffZ = 0;
                            break;
                        }
                        else
                            buffZ = i+1;
                    }
                    else
                    {
                        for (int j = 0; j < (len - buffZ); j++ )
                            buff[j] = buff[buffZ+j];
                        buffZ = len - buffZ;
                        break;
                    }
                }
            }
            else
            {
                std::cout << "ERROR: TCP Read error:" << errno << " \"" << strerror(errno) << "\" during tcp recv\n" << std::flush;
                (*threadError) += 0x101;
                return -1;
            }
        }
        else if (err < 0 || (pfd.revents & (POLLERR | POLLHUP | POLLNVAL)))
        {
            std::cout << "ERROR: Poll error:" << errno << " \"" << strerror(errno) << "\" during tcp recv\n" << std::flush;
            (*threadError) += 0x102;
            return -1;
        }

        //Old method for handling timeout
        /*
        //Size <= 0 indicates error
        if (len > 0)
        {
            //Add new requests to the queue
            msg.assign(buff, len);
            lockQ->lock();
            queue->push(msg);
            lockQ->unlock();
        }
        else if (!len || (errno != EWOULDBLOCK))        //Ignore timeout error
        {
            if (len)
                printf("Error %i from recv: %s\n", errno, strerror(errno));
            else
                std::cout << "TCP connection closed by client\n";

            std::cout.flush();
            (*threadError) += 0x102;
            return -1;
        }

        else
            errno = 0;      //Reset timeout error
        */
    }
    return 0;
}










int n2Coms(std::queue<std::string> *reqQ, std::mutex *lockReqQ, std::queue<std::string> *ansQ, std::mutex *lockAnsQ, char *comPort, std::atomic<int> *threadError, std::atomic<bool> *stop)
{
    //Define variables used in loop
    n2RequestData req;
    std::string msg;
    int sPort, err, errCount = 0;

    //Initialize serial port
    err = n2ComSetup(&sPort, comPort);
    if (err)
    {
        //printf("Error %i: %s\n", errno, strerror(errno));
        std::cout << "ERROR: " << errno << " \"" << strerror(errno) << "\" during n2ComSetup\n" << std::flush;
        (*threadError) += 0x1003 & err;
        return -1;
    }

    //Serial working loop
    while (!*stop)
    {
        //Only run if queue is not empty. Otherwise, sleep.
        if (!reqQ->empty())
        {
            //Get next request in inbound tcp queue
            lockReqQ->lock();
            if (!tcpToN2(&req, reqQ->front()))
            {
                lockReqQ->unlock();

                //Fulfil request
                err = n2Request(&req, sPort, serialTxBufferSize, serialRxBufferSize);
                
                //Handle errors from serial request.
                if (err & 0xFF00)
                {
                    (*threadError) += err;
                    break;
                }
                else if (err & 0xF)
                    errCount ++;
                else
                {
                    //Remove first item in queue
                    lockReqQ->lock();
                    reqQ->pop();
                    lockReqQ->unlock();

                    if (! ((0xF0 & err) || errCount > serialRetries))
                    {
                        //Parse reply to a tcp message and add it to the outbound tcp queue
                        msg = n2ToNodeRed(req);
                        lockAnsQ->lock();
                        ansQ->push(msg);
                        lockAnsQ->unlock();
                        errCount = 0;
                    }
                }
            }
            else
            {
                reqQ->pop();
                lockReqQ->unlock();
                std::cout << "TCP request is in an invalid format: \"" << reqQ->front() << "\"\n" << std::flush;
                lockAnsQ->lock();
                ansQ->push("Invalid request format");
                lockAnsQ->unlock();
            }
        }
        else
            std::this_thread::sleep_for(threadIdleTime);
    }

    //Close com port on exit
    close(sPort);
    return 0;
}










int tcpTx(std::queue<std::string> *queue, std::mutex *lockQ, int *socket, std::atomic<int> *threadError, std::atomic<bool> *stop)
{
    char buff[tcpTxBufferSize];
    std::string msg;
    int len = 0;

    //Working loop
    while (!*stop)
    {
        //Sleep if queue is empty
        if(!queue->empty())
        {
            //Get first queue entry and determine length
            lockQ->lock();
            msg = queue->front();
            lockQ->unlock();
            len = msg.length();
            len = (len < tcpTxBufferSize ? len : tcpTxBufferSize);
            
            //Copy message from string to buffer (char array)
            strncpy(buff, msg.c_str(), len);

            //Recheck stop condition to avoid exception
            if (*stop)
                break;
            //Send buffer, terminate on error
            if (send(*socket, buff, len, 0) < 0)
            {
                (*threadError) += 0x104;
                return -1;
            }

            //Lock the queue and remove the first entry
            lockQ->lock();
            queue->pop();
            lockQ->unlock();
        }
        else
            std::this_thread::sleep_for(threadIdleTime);
    }
    return 0;
}










int main(int argc, char* argv[])        //Input arguments from terminal, first argument is path to serial usb device
{

    //setvbuf(stdout, NULL, _IONBF, 0);     //Disable cout buffering
    
    //Abort if the program was started without input arguments
    if (argc < 2)
    {
        std::cout << "ERROR: No COM device path."
        <<  "Rerun program with the path to the serial device."
        << "ie. n2Gateway.exe \"/dev/ttyUSB0\"\n" << std::flush;
        return -1;
    }
    
    //If more than 1 argument was passed, assume that the second argument is path to a lookup table expansion file.
    if (argc > 2)
    {
        if (expandTables(argv[2]))
        {    
            std::cout << "Error ocured while reading file \"" << argv[2] << "\". Verify that the format is correct." << std::flush;
            return -1;
        }
    }

    //Lambda functions to terminate program on shutdown/interrupt
    std::signal(SIGTERM, [](int) { terminateProgram = true; });
    std::signal(SIGINT, [](int) { terminateProgram = true; });
    std::signal(SIGHUP, [](int) { terminateProgram = true; });
    std::signal(SIGPIPE, [](int) { restartProgram = true; });
       
    //Variables for error handling and thread termination
    std::atomic<int> threadError{0};
    int prevError, i;
    std::atomic<bool> stopThreads;

    //Variables for locking queues to prevent data corruption
    std::mutex lockReqQ;
    std::mutex lockAnsQ;

    //Queues for requests and replies
    std::queue<std::string> reqQ;
    std::queue<std::string> ansQ;

    int clientSocket = -1;

    //Break if a thread returns
    while (true)
    {
        //Resetting loop variables
        stopThreads = 0;
        i = 0;
        prevError = threadError;

        //Create tcp socket
        std::cout << "Initializing TCP socket and waiting for inbound connection.\n" << std::flush;
        clientSocket = tcpGetClient(tcpInitHost(), pollTimeout_ms, &terminateProgram);

        if (terminateProgram)
        {
            std::cout << "Program termination requested, aborting init\n" << std::flush;
            break;
        }
        if (clientSocket < 0)
        {
            std::cout << "ERROR: Failed to establish tcp connection\n" << std::flush;
            break;
        }
        

        //Create threads
        std::cout << "Starting all worker threads.\n" << std::flush;
        std::thread rx(tcpRx, &reqQ, &lockReqQ, &clientSocket, &threadError, &stopThreads);
        std::thread n2(n2Coms, &reqQ, &lockReqQ, &ansQ, &lockAnsQ, argv[1], &threadError, &stopThreads);
        std::thread tx(tcpTx, &ansQ, &lockAnsQ, &clientSocket, &threadError, &stopThreads);

        //Run worker threads until error
        std::cout << "Starting nested main loop.\n" << std::flush;

        //Loop until error in threads or termination
        while (! ((threadError - prevError) & 0xFF00))
        {
            //Terminate program if terminateProgram signal received
            if (terminateProgram)
            {
                std::cout << "terminateProgram signal received.\n" << std::flush;
                break;
            }
            else if (restartProgram)
            {
                std::cout << "restartProgram signal received, tcp socket closed?\n" << std::flush;
                break;
            }

            //Reset error variable after x iterations without new errors
            else if (threadError)
            {
                if (i > 1000)
                    threadError = prevError = 0;
                else if (prevError == threadError)
                    i++;
                else
                {
                    prevError = threadError;
                    i = 0;
                }
            }
            
            //Sleep main thread
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
        }

        //Abort and join all threads
        std::cout << "Terminating all worker threads\n" << std::flush;
        stopThreads = true;
        rx.join();
        n2.join();
        tx.join();

        std::cout << "Closing TCP socket.\n" << std::flush;
        close(clientSocket);

        //Abort if error is severe or terminateProgram signal is true
        if (terminateProgram || threadError & 0xF000)
            break;

        
        std::cout << "Main loop ended with error code \"" << threadError << "\". Restarting main loop\n" << std::flush;
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    }
    return threadError;
}