#pragma once
#include <bitset>
#include <string>
#include <unordered_map>
#include <variant>










//Structs
struct n2RequestData
{
    std::string requestId;                                                          //Request identifier
    uint8_t deviceId;                                                               //PLC ID (1-255)
    std::string module;                                                             //AI, AO, DI, DO, LRS, PGM
    std::variant<std::monostate, std::string> pgm;                                  //Program module sub type, only relevant for PGM (for example pid.kp)
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











//Constants
const uint16_t n2ReadCmd = 0x8000;      //Command to read data
const uint16_t n2WriteCmd = 0xc000;     //Command to write data
const uint16_t n2Retries = 5;           //Number of retries on a failed request
const int n2RxBlockTime = 2;		    //Time to block program when reading inbound data (not used in cannonical mode)
const int n2MinRxBytes = 0;			//Dont block program if x ammount fo bytes received (not used in cannonical mode)










//Lookup tables
const std::unordered_map<std::string, uint16_t> moduleAdr =         //ADR + RI.
{
    {"MAN.A", 0x1},            //Manual adress choice
    {"MAN.B", 0x1},            //Manual adress choice
    {"DI",  0x006},         //Digital input
    {"DO",  0x05},          //Digital output
    {"AI",  0x4C0 + 7},     //Analog input
    {"AO",  0x540 + 6},     //Analog output
    {"DCO", 0x00a},         //Digital constant
    {"ACO", 0x022},         //Analog constant
    {"PGM", 0x040},         //Program module
    {"LRS", 0x00b}          //Logic result
};

const std::unordered_map<std::string, uint8_t> moduleSize =         //Ammount of bytes. (0 for bool)
{
    {"MAN.A", 0x1},            //Manual adress choice (Analog value)
    {"MAN.B", 0x1},            //Manual adress choice (Binary value)
    {"DI", 0x00},
    {"DO", 0x00},
    {"AI", 0x10},
    {"AO", 0x10},
    {"DCO", 0x0},
    {"ACO", 0x1},
    {"PGM", 0x60},
    {"LRS", 0x00}
};

const std::unordered_map<std::string, uint16_t> pgmAdr =        //RI of sub-item in PGM
{
    {"PID.LSP", 26},
    {"PID.KP", 27},
    {"PID.TI", 28},
    {"PID.TD", 29},
    {"PID.WSP", 61},
    {"PID.PV", 63},
    {"PID.RSP", 66}
};