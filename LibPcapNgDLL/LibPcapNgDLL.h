#pragma once

#ifdef LIBPCAPNG_EXPORTS
#define LIBPCAPNG_API __declspec(dllimport)
#else
#define LIBPCAPNG_API __declspec(dllexport)
#endif

#include "stdafx.h"
#include <fstream>
#include <string>
#include <ctime> // For converting the epoch time to local time

namespace LibPcapNg
{
	extern "C" {
	enum endianness { BIG_ENDIAN, LITTLE_ENDIAN };


	class FSFB2BSDpacket {
	public:
		std::string IPsrc;
		std::string IPdst;
		int portsrc, portdst;
		int timestampH, timestampL;
		unsigned char* payload;
		int sizePayLoad;
		FSFB2BSDpacket *nextFSFB2BSDPacket;

		LIBPCAPNG_API FSFB2BSDpacket();
		LIBPCAPNG_API ~FSFB2BSDpacket();
	};

	class FileManagement {
	private:
		char* memblock; // Temporary buffer for reading pCapNG file
		int nbByteinBuffer; // Size (in byte) of the input file
		endianness localEndianness; // Temporary variable for storing the endianness of the machine capture the pCap
		int globalHeaderSize; // Size of the global header
		int currentPos; // Temporary counter to track position within the pCap
		std::string namePcapFile; // Name of the pCapNG file
		FSFB2BSDpacket* rootBSDptr; // Root of the list of filtered packets
		FSFB2BSDpacket* lastBSDptr; // Keep track of the last BSD (for quick addition to linked list)
		FSFB2BSDpacket* currentBSDptr; // Keep track of the current BSD (for navigating linked list)
		double currentTimeStamp; // Keep track of the current timestamp of the pCap
		int nbFilteredPacket; // Keep track of the total number of packet found according to provided filter
		std::string version;
		int nbPacket; // Number of packets filtered
		
		long double getTimeStamp(); // internal method to compute the current time stamp of the pCap

	public:
		LIBPCAPNG_API FileManagement(std::string name_input);
		LIBPCAPNG_API ~FileManagement();
		LIBPCAPNG_API std::string getVersion();
		LIBPCAPNG_API int getFilteredPacketNumber();
		LIBPCAPNG_API bool Load();
		bool parseSectionHeader();
		bool addFSFB2BSDPacket();
		bool addFSFB2BSDPacket(std::string IPsrc, std::string IPdst, int portSrc, int portDst);
		bool parseInterfaceDescription();
		bool parseEnhancedPacketBlock(std::string IPsrc, std::string IPdst, int portSrc, int portDst,
			int srcNodeID, int dstNodeID);
		LIBPCAPNG_API int parsePcapNG(std::string ipSrc, std::string ipDst, int portSrc, int portDst,
			int srcNodeID, int dstNodeID);
		LIBPCAPNG_API int getFirstPacket(unsigned char* &inputArray);
		LIBPCAPNG_API int getNextPacket(unsigned char* &inputArray);
		LIBPCAPNG_API long double getCurrentTimeStamp();

	};
		// Export of necessary methods to managed code to call from C#

		LIBPCAPNG_API FileManagement* __cdecl MngConstruct(char* input);
		LIBPCAPNG_API void __cdecl MngDispose(FileManagement* objectToDispose);
		LIBPCAPNG_API const char* __cdecl MngGetVersion(FileManagement* objectToDispose, char* buffer);
		LIBPCAPNG_API int MngGetFilteredPacketNumber(FileManagement* object);
		LIBPCAPNG_API int MngParsePcapNG(FileManagement* object, char* ipSrc, char* ipDst, int portSrc, int portDst,
			int srcNodeID, int dstNodeID);
		LIBPCAPNG_API bool MngLoad(FileManagement* object);
		LIBPCAPNG_API const int MngGetFirstPacket(FileManagement* object, unsigned char* &buffer);
		LIBPCAPNG_API const int MngGetNextPacket(FileManagement* object, unsigned char* &buffer);
		LIBPCAPNG_API const double MngGetTimeStamp(FileManagement* object);
	}


}