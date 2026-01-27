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
    uint8_t byteSwap;//Adds a shift in address calculation such that ABCD -> CDAB. Er kjent for AI, AO og AO2
    //                  Name        offset
    std::unordered_map<std::string, uint8_t> items;
};










//Default values used to initialize module table.

//For modules of type 1 BYTE
//Uses the item name to determine the output data format
const std::unordered_map<std::string, uint8_t> byteDefault =
{
    {"BOOL",        0},           //uint8_t
    {"BYTE",        0}              //uint8_t
};





//For modules of type "Number" OR "2 Bytes"
//Uses the item name to determine the output data format
const std::unordered_map<std::string, uint8_t> wordDefault =
{
    {"BOOL",        0},         //uint8_t
    {"WORD",        0},         //uint16_t
    {"FLOAT",       0}          //Float
};





//For modules of type "4 Bytes"
//Uses the item name to determine the output data format
const std::unordered_map<std::string, uint8_t> dwordDefault =
{
    {"BOOL",        0},         //uint8_t
    {"DWORD",       0}         //WORD (uint16_t)
};





//PGM module specifiers
const std::unordered_map<std::string, uint8_t> pgmDefaultItems =
{
    //PID
    {"PID.LSP", 26},
    {"PID.KP", 27},
    {"PID.TI", 28},
    {"PID.TD", 29},
    {"PID.WSP", 61},
    {"PID.PV", 63},
    {"PID.RSP", 66},


    //Four channel segment function
    {"CMP1.X1", 26},
    {"CMP1.Y1", 27},
    {"CMP1.X2", 28},
    {"CMP1.Y2", 29},
    {"CMP1.X3", 30},
    {"CMP1.Y3", 31},
    {"CMP1.X4", 32},
    {"CMP1.Y4", 33}
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
    {"DO1", 70},
    {"DO2", 70},
    {"DO3", 70},
    {"DO4", 70},
    {"DO5", 70},
    {"DO6", 70},
    {"DO7", 70},
    {"DO8", 70},

    //DI
    {"DI", 71},
    {"DI1", 71},
    {"DI2", 71},
    {"DI3", 71},
    {"DI4", 71},
    {"DI5", 71},
    {"DI6", 71},
    {"DI7", 71},
    {"DI8", 71},
};









//Lookup table for N2 modules
inline std::unordered_map<std::string, n2ModuleInfo> n2Modules =
{
    //Name      Address     Size        Count       byteSwap       Items:       Name        Offset       
    {"DO"      ,{0x005     ,0x01       ,1           ,0              ,{          byteDefault             }}},

    {"DI"      ,{0x006     ,0x01       ,1           ,0              ,{          byteDefault             }}},

    {"LRS"     ,{0x008     ,0x01       ,2           ,0              ,{          wordDefault             }}},
    {"LRS2"    ,{0x02c     ,0x01       ,2           ,0              ,{          wordDefault             }}},

    {"DCO"     ,{0x00a     ,0x01       ,2           ,0              ,{          wordDefault             }}},

    {"ACO"     ,{0x022     ,0x01       ,8           ,0              ,{{          "CV"   ,0             }}}},

    {"PGM"     ,{0x040     ,0x60       ,12          ,0              ,{          pgmDefaultItems         }}},

    {"AI"      ,{0x4c0     ,0x10       ,8           ,1              ,{{         "CV"     ,7          }}}},

    {"AO"      ,{0x540     ,0x10       ,2           ,1              ,{{         "CV"     ,6          }}}},
    {"AO2"     ,{0x900     ,0x10       ,6           ,1              ,{{         "CV"     ,6          }}}},
    
    {"EXT"     ,{0x5c0     ,0x50       ,8           ,0              ,{      extDefaultItems             }}},

    {"TS"      ,{0x840     ,0x10       ,8           ,0              ,{{         "TIME"      ,4          }}}}
};