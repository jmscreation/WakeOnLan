// std includes
#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <algorithm>
#include <stdio.h>
#include <filesystem>

// windows socket includes
#include <WinSock2.h>
#include <Windows.h>

#include "ifiterate.h"

namespace fs = std::filesystem;

// default values
// Default MAC address file name
std::string MACFileName{ "MACAddresses.txt" };
// Default port on receiving device
unsigned short PortAddress{ 9 };

void PrintUsage(char* msg)
{
	std::cout << "USAGE: " << msg << " MACAddresses.txt" << std::endl;
	std::cout << "For further information call: " << msg << " --help" << std::endl;
}

void PrintHelp()
{
	std::cout << std::endl;
	std::cout << "-- Wake On LAN --" << std::endl;
	std::cout << "For usage please create text file names 'MACAddresses.txt' with on MAC address per line.";
	std::cout << " For every MAC address a broadcast Wake-On-LAN call will be send.";
	std::cout << " A MAC address has the form 'XX:XX:XX:XX:XX:XX'. Every line beginning with '#' will be ignored." << std::endl;

	std::cout << std::endl << "-- Basic call --" << std::endl;
	std::cout << "WakeOnLAN.exe [options]" << std::endl;
	
	std::cout << std::endl << "-- Options --" << std::endl;
	std::cout << "-h or --help \t Shows this information"<< std::endl;
	std::cout << "-i or --interfaces \t Shows all interface information"<< std::endl;
	std::cout << "-f or --file [filename] \t Set other MAC address file (default: MACAddresses.txt)" << std::endl;
	std::cout << "-p or --port [portnumber]\t Set other port (default: 9)" << std::endl;
	std::cout << "-m or --mac [MAC address] \t Directly wake a single MAC address" << std::endl;
	std::cout << std::endl;
}

void CreateMACAddressFile()
{
	std::ofstream AddressFileStream;
	AddressFileStream.open(MACFileName);
	if (AddressFileStream.is_open())
	{
		AddressFileStream << "# Enter your the target MAC address here.\n# MAC addresses have the form: XX:XX:XX:XX:XX:XX\n# Comments can be made with '#' at begin of a line." << std::endl;
		AddressFileStream.close();
	}
	else
	{
		std::cout << "Could not create MAC address file." << std::endl;
	}
	
}

std::vector<std::string> ReadAddresses(std::string AddressFileName)
{
	std::vector<std::string> MACAddresses = std::vector<std::string>();
	std::ifstream FID(AddressFileName);
	if (FID.is_open())
	{
		std::string Address;
		while (std::getline(FID,Address))
		{
			Address.erase(std::remove_if(Address.begin(), Address.end(), isspace), Address.end());
			// skip commented lines
			if (Address[0] == '#') continue;
			if (Address.size() > 0)
				MACAddresses.push_back(Address);
		}
		FID.close();
	}
	else
	{
		std::cout << "File not found!" << std::endl;
	}

	if (MACAddresses.size() == 0)
	{
		std::cout << "No valid addresses in file!" << std::endl;
	}

	return MACAddresses;
}

void RemoveCharsFromString(std::string &Str, const char* CharsToRemove) {
	for (unsigned int i = 0; i < strlen(CharsToRemove); ++i) {
		Str.erase(std::remove(Str.begin(), Str.end(), CharsToRemove[i]), Str.end());
	}
}

bool SendWakeOnLAN(std::string MACAddress, unsigned PortAddress)
{

	// Build message = magic packet
	// 6x 0xFF followed 16x MACAddress
	unsigned char Message[102];
	for (auto i = 0; i < 6; ++i)
		Message[i] = 0xFF;

	unsigned char MAC[6];
	std::string StrMAC = MACAddress;
	RemoveCharsFromString(StrMAC, ":-");
	try {
		for (auto i = 0; i < 6; ++i)
		{
			MAC[i] = static_cast<unsigned char>(std::stoul(StrMAC.substr(i * 2, 2), nullptr, 16));
		}
	} catch(std::invalid_argument e){
		std::cout << "Invalid MAC address given: " << MACAddress << "\n";
		return 1;
	}
	
	for (auto i = 1; i <= 16; ++i)
	{
		memcpy(&Message[i * 6], &MAC, 6 * sizeof(unsigned char));
	}
	
	// Create socket
	// Socket variables
	WSADATA WSAData;
	SOCKET SendingSocket = INVALID_SOCKET;
	struct sockaddr_in LANDestination {};

	// Initialize WinSock
	if (WSAStartup(MAKEWORD(2, 2), &WSAData) != 0)
	{
		std::cout << "WSA startup failed with error:" << std::endl;
		std::cout << WSAGetLastError() << std::endl;
		return 1;
	}
	else
	{
		// Initialize socket with protocol properties (internet protocol, datagram-based protocol (size limited to 512bytes))
		SendingSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

		if (SendingSocket == INVALID_SOCKET)
		{
			std::cout << "Socket is not initialized:" << std::endl;
			std::cout << WSAGetLastError() << std::endl;
			WSACleanup();
			return 1;
		}

		// Set socket options (broadcast)
		const bool optval = TRUE;
		if (setsockopt(SendingSocket, SOL_SOCKET, SO_BROADCAST, (char*)&optval, sizeof(optval)) != 0)
		{
			std::cout << "Socket startup failed with error:" << std::endl;
			std::cout << WSAGetLastError() << std::endl;
			WSACleanup();
			return 1;
		}

		std::vector<NetworkInterface> iflist;

		if(GetNetworkInterfaceInfos(iflist)){
			for(const NetworkInterface& ii : iflist){ // iterate over all interfaces

				LANDestination.sin_family = AF_INET;
				LANDestination.sin_addr.s_addr = inet_addr(ii.broadcast.data());
				LANDestination.sin_port = htons(PortAddress);

				// Send Wake On LAN packet
				if (sendto(SendingSocket, (char*) Message, 102, 0, reinterpret_cast<sockaddr*>(&LANDestination), sizeof(LANDestination)) == ERROR)
				{
					std::cout << "Sending magic packet failed with error: " << WSAGetLastError() << "\n On interface " << ii.name << " [" << ii.address << "]" << std::endl;
					return 1;
				}
			}
		} else {
			std::cout << "Failed to enumerate network interfaces!\n";
			closesocket(SendingSocket);
			WSACleanup();
			return 1;
		}

		// Close socket after sending
		if (closesocket(SendingSocket) != 0) {
			std::cout << "Closing socket fails with error:" << std::endl;
			std::cout << WSAGetLastError() << std::endl;
			WSACleanup();
			return 1;
		}

		// Deregister the WSAStartup call
		WSACleanup();

	}

	return 0;
}

int main(int argc, char* argv[])
{
	if(argc == 1 && !fs::exists(MACFileName)){
		PrintUsage(argv[0]);
		return 1;
	}
	std::string MACAddress;

	// Handle input arguments
	for (auto i = 1; i < argc; ++i)
	{
		
		if ( !strcmp(argv[i],"--help") || !strcmp(argv[i],"-h"))
		{
			PrintHelp();
			return 1;
		}
		if ( !strcmp(argv[i],"--interfaces") || !strcmp(argv[i],"-i"))
		{
			PrintNetworkInterfaceInfos();
			return 1;
		}
		if (!strcmp(argv[i], "--mac") || !strcmp(argv[i], "-m"))
		{
			MACAddress = argv[i + 1];
			std::cout << "Direct Wake MAC: " << argv[i+1] << std::endl;
			++i;
		}
		if (!strcmp(argv[i], "--file") || !strcmp(argv[i], "-f"))
		{
			MACFileName = argv[i + 1];
			std::cout << "MACFileName changed to " << argv[i+1] << std::endl;
			++i;
		}
		if (!strcmp(argv[i], "--port") || !strcmp(argv[i], "-p"))
		{
			PortAddress = static_cast<unsigned short>(std::stoi(argv[i + 1]));
			std::cout << "Wake On LAN port changed to " << argv[i + 1] << std::endl;
			++i;
		}
			
	}
	std::vector<std::string> MACAddressList;

	if(MACAddress.empty()){
		// Check if MAC address file name exist
		if (!fs::exists(MACFileName))
		{
			std::cout << "MAC file is not found. MAC file is created in current directory." << std::endl;
			CreateMACAddressFile();
			return EXIT_FAILURE;
		}
		// Read all addresses from address file
		MACAddressList = ReadAddresses(MACFileName);
	} else {
		// Waking up single MAC address from CLI
		MACAddressList.push_back(MACAddress);
	}


	// Sending Wake On LAN signals to all listed MAC addresses
	auto AddressIter = MACAddressList.cbegin();
	for (; AddressIter != MACAddressList.cend(); ++AddressIter)
	{
		if (!SendWakeOnLAN((*AddressIter), PortAddress))
		{
			std::cout << "Magic packet sent successful to : "<< *AddressIter <<" :: "<< PortAddress << std::endl;
		}
		else
		{
			return EXIT_FAILURE;
		}
	}

	return EXIT_SUCCESS;

}