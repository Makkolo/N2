#include "n2Interpreter.hpp"



float n2numberToFloat(const uint16_t msg)        //Convert N2 number to float.
{
    int8_t sign = ((msg & 0x8) ? 0xFF : 0x1 );

	uint8_t e = (msg & 0x00f0)>>4;

	uint16_t m = ((msg&0x7)<<8)|((msg&0xff00)>>8);


	float mVal = 0;

	for (int i = 0; i < 11; i++)
	{
		if ((m>>i)&0x1)
			mVal += 1.0 / (0x0002<<(10-i));
	}
	float eVal = 1;

	for (int i = 0; i < e; i++)
		eVal *= 2;

	
	return (eVal * mVal * sign);
}










uint16_t floatToN2number(float value)        //Convert float to N2 number.
{
    int8_t sign = (value < 0 ? 0xFF : 0x1);
	value*= sign;

	uint8_t exp = 0;
	while (1 <= value && exp < 15)
	{
		value /= 2.0;
		exp++;
	}
	uint16_t m = (value * (0x1 << 11));
	if (2047 < m)
		m = 2047;

	return (sign & 0x8) | ((m & 0xff) << 8) | (m >> 8) | (exp << 4);
}










uint32_t n2ChkCalc(__uint128_t msg, uint8_t len)                            //Calculate message checksum.
{
    //CHK byte 1 and 2
	uint16_t x1 = 0, x2 = 0;
	uint8_t temp = 2;

	for (int i = 0; i < 8 && (msg>>i * 8); i++)
	{
		temp = msg >> (i * 8);
		x1 -= temp * (2 + i);
		x2 += temp * (1 + i);
	}

	while (x1 & 0xff00)
		x1 = (x1 & 0xff) + (x1 >> 8);
	while (x2 & 0xf00)
		x2 = (x2 & 0xff) + (x2 >> 8);

	x1--;

	x1 &= 0xff;
	x2 &=0xff;

	//CHK byte 3, ASCII sum
	msg = (msg << 16) | (x1 << 8) | x2;
	len+= 4;
	uint16_t sum = 0;
	for (int i = 0; i < len; i++)
	{
		temp = (msg >> i*4) & 0xf;
		sum += 48 + temp;
		if (0x9 < temp)
			sum += 7;
	}



    return (x1 << 16) | (x2 << 8) | (sum & 0xff);
}










int n2AdrCalc(uint32_t *adr, n2RequestData *req)
{
	n2ModuleInfo modInfo = n2Modules.at(req->module);
	*adr = ((req->deviceId)<<0x10) | (modInfo.address + modInfo.items.at(req->item));

	if (req->item == "BOOL")
	{
		int size;
		if (req->item == "WORD")
			size = 16;
		else if (req->item == "DWORD")
			size = 32;
		else
			size = 8;
		if (req->index / (size+1))
		{
			(req->index)-= size;
			(*adr)++;
		}
		
	}
	else
		(*adr)+= modInfo.size * (req->index -1);

	//Exception for extension modules, to be able to read single boolean
	if (req->module == "EXT" && req->item.substr(0,1) == "D" && req->item.length() > 2)
		req->index = std::stoi(req->item.substr(2,1));
	
	if (!std::holds_alternative<std::monostate>(req->data))
		(*adr) |= n2WriteCmd;
	else
	{
		//Exception for AI and AO
		if (req->module == "AI")
			*adr = ((req->deviceId)<<24) | 0x840000 | (((*adr) & 0xff) << 0x8) | (*adr)>>0x8;
		else if (req->module == "AO")
			*adr = ((req->deviceId)<<24) | 0x840000 | (((*adr) & 0xff) << 0x8) | (*adr)>>0x8;
		else
			(*adr) |= n2ReadCmd;
	}
	return 0;
}


















int n2BuildMsg(std::string *msg, n2RequestData *req)
{
	int err = 0;
	uint64_t imsg = 0;
	uint32_t chk = 0;

	//Calculate the Johnson memory address
	uint32_t adr = 0;
	err = n2AdrCalc(&adr, req);
	imsg = adr;
	if(err)
	{
		std::cout << "Error: Failed to calculate N2 adr\n" << std::flush;
		return err;
	}
		
	

	//Calculate/convert the value, if its a write request, otherwise skipped.
	if (!std::holds_alternative<std::monostate>(req->data))
	{
		if (std::holds_alternative<uint8_t>(req->data))
			imsg = (imsg<<8) | std::get<uint8_t>(req->data);
		else if (std::holds_alternative<uint16_t>(req->data))
			imsg = (imsg<<16) | std::get<uint16_t>(req->data);
		else if (std::holds_alternative<float>(req->data))
			imsg = (imsg<<16) | floatToN2number(std::get<float>(req->data));
		else
			return 0x1;
	}

	std::stringstream msgSs;
	if (req->deviceId < 0x10)
		msgSs << "0";
	msgSs << std::uppercase << std::hex << imsg;

	uint8_t len = msgSs.str().length();

	chk = n2ChkCalc(imsg, len);
	for (int i = 0; i < 6 && !(chk & (0xF00000 >> (4*i) )); i++)
		msgSs << "0";
	msgSs << std::uppercase << std::hex << chk;
    
    
    *msg = ">" + msgSs.str() + "\r";

    return 0;
}










int n2DecodeMsg(n2RequestData *req, std::string msg) //val must be float = index for boolean values.
{
	req->data = msg;
	
	//Check for sucess/failure from PLC
	int err = msg.substr(0,1) != "A";
	if (err)
		return 0;

	msg.erase(0,1);// = msg.substr(1, msg.length() - 1);		//
	
	//Check if data was received, or only status
	int len = msg.length() - 6;
	if (len < 1)
		return 0;

	//Convert message to binary
	std::istringstream iss(msg);
	uint64_t imsg = 0;
	iss >> std::hex >> imsg;

	//Check the checksum to verify message integrity
	uint32_t chk = n2ChkCalc((imsg>>24), len);
	if(chk != (imsg&0xFFFFFF))
	{
		
		std::cout << "Error: Checksum in reply from N2 was incorrect.\n" << std::flush;
		return 0x2;
	}
	
	//Remove checksum
	imsg = (imsg>>24);


	//Return raw byte
	if (req->item == "BYTE")
		req->data = uint8_t(imsg);

	//Return raw WORD
	else if (req->item == "WORD")
		req->data = uint16_t(imsg);

	//Return bool
	else if (req->item == "BOOL")
		req->data = uint8_t((imsg & (0x1 << (req->index - 1))) && 1);

	//Return float
	else
		req->data = n2numberToFloat(imsg);
	
	return 0;
}










int n2ComSetup(int *sPort, char *comPort)
{
	//Open port
	*sPort = open(comPort, O_RDWR);
    if (*sPort < 0)
    {
		std::cout << "Error in serial open: ";
        return 0x100;
    }

	//Copy current com port parameters
    struct termios tty;
    if(tcgetattr(*sPort, &tty) != 0)
    {
		std::cout << "Error from tcgetattr: ";
        return 0x200;
    }

	//Modify control parameters for N2 serial bus
	tty.c_cflag &= ~(PARENB | CSTOPB | CSIZE | CRTSCTS);		//no parity, 1 stop bit, no flow control, reset size, not canonical mode, no echo, no input signal interpreter
	tty.c_cflag |= (CS8 | CREAD | CLOCAL);		//8 bits, enable read, local??


	//Modify local parameters
	tty.c_lflag &= ~(ECHO | ECHOE | ECHONL | ISIG);
	tty.c_lflag &= ~ICANON;		//disables canonical mode, raw data fills buffer
	//tty.c_lflag |= ICANON;		//Enables canonical mode. returns msg on read() at \n or similar. 

	//Modify canonical parameters
	//tty.c_cc[VEOL] = '\r';

	//Modify input parameters for N2 serial bus
	tty.c_iflag &= ~(IXON | IXOFF | IXANY);		//Disable sw flow control
	tty.c_iflag &= ~(IGNBRK|BRKINT|PARMRK|ISTRIP|INLCR|IGNCR|ICRNL);		//Disable interpreter (get raw data)

	//Modify output parameters for N2 serial bus
	tty.c_oflag &= ~(OPOST | ONLCR);		//Dont interpret newline, CR, lf ...	???

	//Configure read() blocking
	tty.c_cc[VTIME] = n2RxBlockTime;		//block for 1 s
	tty.c_cc[VMIN] = n2MinRxBytes;			//or until x bytes of data received

	//Set baud rate
	cfsetspeed(&tty, B9600);

	if (tcsetattr(*sPort, TCSANOW, &tty))
	{
		std::cout << "Error from tcsetattr: ";
        return 0x300;
	}


	return 0;
}









int n2Write(const std::string msg, const int bufferSize, const int sPort)
{
	char buff[bufferSize];
	int len = msg.length();
	len = (len < bufferSize ? len : bufferSize);

	strncpy(buff, msg.c_str(), len);

	return write(sPort, buff, len);
}










int n2Read(std::string *msg, const int bufferSize, const int sPort)
{
	char buff[bufferSize];
	int len, reads = 0, i;
	bool cr = 0;
	msg->clear();
	do
	{
		len = read(sPort, buff, bufferSize);
		for(i = 0; i<len && !cr; i++)
			cr = (buff[i] == '\r');
		msg->append(buff, i - cr);
		reads++;
		if (n2Retries < reads)
		{
			std::cout << "Error: No reply from N2 after " << n2Retries * n2RxBlockTime * 0.1 << " seconds since request was sent\n" << std::flush;
			return 0x0010;
		}
	}while(!cr);

	if (i < len)
	{
		std::cout << "Warning: Data from N2 bus contained more than 1 reply. Reply was " << i 
		<< " Bytes long, while there was a total amount of data: " << len << " Bytes."
		<< "Second message truncated\n" << std::flush;
	}
	
	return 0;
}










int n2Request(n2RequestData *req, int serialPort, int serialTxBuffSz, int serialRxBuffSz)
{
	std::string msg;
	int err = 0;

	//If binary data is to be written, read current binary states, before overwriting binary data.
	if ((req->item == "BOOL") && !std::holds_alternative<std::monostate>(req->data))
	{
		//Store write data in a temporary variable, remove it from struct
		uint16_t dwTemp = std::get<uint8_t>(req->data);
		req->data = std::monostate{};
		
		//Read current data
		err = n2Request(req, serialPort, serialTxBuffSz, serialRxBuffSz);
		if (err)
			return err;
		
		//Combine current states with data to write and store it in request struct
		if (std::holds_alternative<uint8_t>(req->data))
			if (dwTemp)
				req->data = uint8_t(std::get<uint8_t>(req->data) | (dwTemp<<(req->index - 1)));
			else
				req->data = uint8_t(std::get<uint8_t>(req->data) & ~(dwTemp<<(req->index - 1)));
		else if (std::holds_alternative<uint16_t>(req->data))
			if (dwTemp)
				req->data = uint16_t(std::get<uint16_t>(req->data) | (dwTemp<<(req->index - 1)));
			else
				req->data = uint16_t(std::get<uint16_t>(req->data) & ~(dwTemp<<(req->index - 1)));
		else
			return 0x4;
	}


	//Build message and send to serial N2 bus
	err = n2BuildMsg(&msg, req);
	if (err)
		return err;

	if (n2Write(msg, serialTxBuffSz, serialPort) < 1)
	{
		std::cout << "Error: n2Write did not write any data. Com port error?\n" << std::flush;
		return 0x500;
	}
	
	
	//Wait for and read reply from N2 serial bus
	err = n2Read(&msg, serialRxBuffSz, serialPort);
	if (err)
		return err;

	err = n2DecodeMsg(req, msg);

	return err;
}










//Simple file reading function to expand lookup table.
int expandTables(std::string filePath)
{
    std::string line;
    std::ifstream file(filePath);
	const char space[2] = {' ', '\t'};
	std::string module, item, temp;
	uint16_t address;
	uint8_t size, count, offset;

	//Iterate through file, line by line.
    while(std::getline(file, line))
    {
		//Remove spaces
		cleanString(&line, space, 2);

		while(line.length() > 0)
		{
			//Get module name
			if (extractString(&line, &module, ','))
				return 0x10;
			

			//Check if module allready exists, add item if it does.
			if(n2Modules.find(module) == n2Modules.end())
			{

				//Get address
				if (extractString(&line, &temp, ','))
					return 0x10;
				address = std::stoi(temp);


				//Get size
				if (extractString(&line, &temp, ','))
					return 0x10;
				size = std::stoi(temp);


				//Get count
				if (extractString(&line, &temp, ','))
					return 0x10;
				count = std::stoi(temp);


				//Get item
				if (extractString(&line, &item, ','))
					return 0x10;


				//Get offset
				if (extractString(&line, &temp, ','))
					return 0x10;
				offset = std::stoi(temp);

				//Append the new module to the lookup table
				n2Modules.insert({ module, { address, size, count, {{ item, offset }} } });			
			}
			else
			{
				//Get item
				if (extractString(&line, &item, ','))
					return 0x10;


				if (n2Modules.at(module).items.find(item) == n2Modules.at(module).items.end())
				{
					//Get offset
					if (extractString(&line, &temp, ','))
						return 0x10;
					offset = std::stoi(temp);
					
					//Append item to the lookup table of the corresponding module
					n2Modules.at(module).items.insert({item, std::stoi(temp)});
				}
				else
					std::cout << "WARNING: expandTables: Skipping this entry as item \"" << item << "\" is allready defined for module \"" << module << "\". Verify that the format is correct" << std::flush;
			}
		}
    }
	return 0;
}