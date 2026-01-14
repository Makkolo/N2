#pragma once
#include <bitset>
#include <string>
#include <unordered_map>
#include <variant>

#include <iostream>
#include <cmath>
#include <string.h>
#include <sstream>
#include <fstream>

//Serial coms
#include <fcntl.h> // Contains file controls like O_RDWR
#include <errno.h> // Error integer and strerror() function
#include <termios.h> // Contains POSIX terminal control definitions
#include <unistd.h> // write(), read(), close()

#include "tools.hpp"










//Structs
struct n2RequestData
{
    std::string requestId;                                                          //Request identifier
    uint8_t deviceId;                                                               //PLC ID (1-255)
    std::string module;                                                             //AI, AO, DI, DO, LRS, PGM
    std::string item;                                  //Program module sub type, only relevant for PGM (for example pid.kp)
    uint8_t index;                                                                  //AI1, AI2 and so on
    bool binary;                                                                    //True if data is binary/boolean
    std::variant<std::monostate, uint8_t, uint16_t, float, std::string> data;       //Data to write (optional)
};










//Functions
float n2numberToFloat(const uint16_t msg);                                                      //Convert N2 number to float.
uint16_t floatToN2number(float value);                                                          //Convert float to N2 number.
uint32_t n2ChkCalc(__uint128_t msg, uint8_t len);                                               //Extract and validate checksum in received message.
int n2AdrCalc(uint32_t *adr, const n2RequestData req);                                          //Calculate the adress for the serial request
int n2BuildMsg(std::string *msg, const n2RequestData req);                                      //Build message for serial request
int n2DecodeMsg(n2RequestData *ans, std::string msg);                                           //Decode reply from serial N2
int n2ComSetup(int *sPort, char *comPort);                                                      //Initialize com port for serial N2
int n2Write(const std::string msg, const int bufferSize, const int sPort);                      //Write data to serial N2 bus
int n2Read(std::string *msg, const int bufferSize, const int sPort);                            //Read inbound data from serial N2 bus
int n2Request(n2RequestData *req, int serialPort, int serialTxBuffSz, int serialRxBuffSz);      //Perform serial request
int expandTables(std::string filePath);










//Constants
const uint16_t n2ReadCmd = 0x8000;      //Command to read data
const uint16_t n2WriteCmd = 0xc000;     //Command to write data
const uint16_t n2Retries = 5;           //Number of retries on a failed request
const int n2RxBlockTime = 2;		    //Time to block program when reading inbound data (not used in cannonical mode)
const int n2MinRxBytes = 0;			//Dont block program if x ammount fo bytes received (not used in cannonical mode)










//Struct for N2 modules
struct n2ModuleInfo{
    uint16_t address;
    uint8_t size;
    uint8_t count;
    //                  Name        offset
    std::unordered_map<std::string, uint8_t> items;
};










//Default values used to initialize module table.

//For modules of type 1 BYTE
//Uses the item name to determine the output data format
const std::unordered_map<std::string, uint8_t> byteDefault =
{
    {"CV",      0},     //Bool
    {"BYTE",    0}      //Byte (uint8_t)
};





//For modules of type "Number" OR "2 Bytes"
//Uses the item name to determine the output data format
const std::unordered_map<std::string, uint8_t> wordDefault =
{
    {"CV",      0},     //Float
    {"WORD",    0}      //WORD (uint16_t)
};





//PGM module specifiers
const std::unordered_map<std::string, uint8_t> pgmDefaultItems =
{
    {"PID.LSP", 26},
    {"PID.KP", 27},
    {"PID.TI", 28},
    {"PID.TD", 29},
    {"PID.WSP", 61},
    {"PID.PV", 63},
    {"PID.RSP", 66},
};





//Extension module specifiers
const std::unordered_map<std::string, uint8_t> extDefaultItems =
{
    //AI
    {"AI1", 45},
    {"AI2", 46},
    {"AI3", 47},
    {"AI4", 48},
    {"AI5", 49},
    {"AI6", 50},
    {"AI7", 51},
    {"AI8", 52},

    //AO
    {"AO1", 53},
    {"AO1", 54},
    {"AO1", 55},
    {"AO1", 56},
    {"AO1", 57},
    {"AO1", 58},
    {"AO1", 59},
    {"AO1", 60},

    //DO
    {"DO", 70},

    //DI
    {"DI", 71}
};









//Lookup table for N2 modules
inline std::unordered_map<std::string, n2ModuleInfo> n2Modules =
{
    //Name      Address     Size        Count       Items:       Name        Offset        
    {"DO"      ,{0x005     ,0x00       ,8           ,{          byteDefault             }}},

    {"DI"      ,{0x006     ,0x00       ,8           ,{          byteDefault             }}},

    {"DCO"     ,{0x00a     ,0x00       ,16          ,{          byteDefault             }}},

    {"LRS"     ,{0x00b     ,0x00       ,16          ,{          byteDefault             }}},

    {"ACO"     ,{0x022     ,0x01       ,8           ,{          wordDefault             }}},

    {"PGM"     ,{0x040     ,0x60       ,12          ,{          pgmDefaultItems         }}},

    {"PMnI"    ,{0x04a     ,0x01       ,16          ,{          wordDefault             }}},

    {"PMnK"    ,{0x05a     ,0x01       ,34          ,{          wordDefault             }}},

    {"PMnO"    ,{0x07c     ,0x01       ,8           ,{          wordDefault             }}},

    {"AI"      ,{0x4c0     ,0x10       ,8           ,{{         "CV"        ,7          },
                                                    {           "WORD"      ,7          }}}},

    {"AO"      ,{0x540     ,0x10       ,8           ,{{         "CV"        ,6          },
                                                    {           "WORD"      ,6          }}}},
    
    {"EXT"     ,{0x5c0     ,0x50       ,8           ,{      extDefaultItems             }}},

    {"TS"      ,{0x840     ,0x10       ,8           ,{{         "TIME"      ,4          }}}}
};