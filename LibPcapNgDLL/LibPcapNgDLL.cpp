// LibPcapNgDLL.cpp : Defines the exported functions for the DLL application.
// History
// v.1:		first version tested with the Managed export (to CLI)
// v.1.1:	addition of the DEBUG_FAILED_TEST preprocessor directive
//			change of return value for EPOCH time from long double to double as Microsoft C++ treats the 2 equally

#include "stdafx.h"
#include "LibPcapNgDLL.h"
#include <iostream>
// preprocessor directive for debugging entrance in method
//#define DEBUG 
// preprocessor directive for debugging failed packet filters
//#define DEBUG_FAILED_TEST 

namespace LibPcapNg
{
	const int IPpayloadOffset = 0x0e; // Offset to the start of the IP packet from the start of the ethernet frame
	const int FSFB2OffSetPayLoad = 0x49; // Offset to the start of the FSFB2 payload from the start of the ethernet frame
	const int packetDataOffsetEnhPB = 28; // Offset of packet data position in enhanced Packet Block
	const int TimeStampHighOffsetEnhPB = 12; // Offset of TimeStamp (High) position in enhanced Packet Block
	const int TimeStampLowOffsetEnhPB = 16; // Offset of TimeStamp (Low) position in enhanced Packet Block
	const int IPpacketWidth = 0x10 + packetDataOffsetEnhPB; // Offset to the IP packet size from the start of the ethernet frame
	const int IPOffsetprotocol = 0x17 + packetDataOffsetEnhPB; // Offset to the IP protocol from the start of the ethernet frame
	const int IPSrcOffset = 0x1A + packetDataOffsetEnhPB; // Offset to the Source IP address from the start of the ethernet frame
	const int IPDstOffset = 0x1E + packetDataOffsetEnhPB; // Offset to the Destination IP address from the start of the ethernet frame
	const int PortSrcOffset = 0x22 + packetDataOffsetEnhPB; // Offset to the Source Port from the start of the ethernet frame
	const int PortDstOffset = 0x24 + packetDataOffsetEnhPB; // Offset to the Destination Port from the start of the ethernet frame
	const int SrcNodeIDOffset = 0x4A + packetDataOffsetEnhPB; // Offset to the Source NodeID from the start of the ethernet frame
	const int DstNodeIDOffset = 0x4C + packetDataOffsetEnhPB; // Offset to the Destination NodeID from the start of the ethernet frame
	const int MAXBuffer = 1 << 24; // Maximum size of file to load in memory is 2^24 bytes
	const int BSDcode = 0x80; // Application code for BSD message
	const std::string BLOCKTYPE_NG = { 0x0A, 0x0D, 0x0D, 0x0A }; // block type for pcapng file
	const std::string BYTEORDERMAGIC_BE = { 0x1A, 0x2B, 0x3C, 0x4D }; // Byte Order Magic for Big Endian machine
	const std::string BYTEORDERMAGIC_LE = { 0x4D, 0x3C, 0x2B, 0x1A }; // Byte Order Magic for Little Endian machine
	const int UDP = 0x11; // Code of UDP packet

	// Convert single char to (unsigned) int
	int char2int(char input) {
		int output;
		(int)input < 0 ? output = input + 256 : output = input;
		return output;
	}

	// Convert a number of char into (unsigned) int. start is the first element in the char[], width and Endianness are required
	int char2int(char *input, int start, int width, endianness iEndianess) {
		int output = 0;
		if (iEndianess == LITTLE_ENDIAN) {
			for (int cpt = 0; cpt < width; cpt++) {
				output += char2int(input[start + cpt]) * (1 << cpt * 8);
			}
		}
		if (iEndianess == BIG_ENDIAN) {
			for (int cpt = 0; cpt < width; cpt++) {
				output += char2int(input[start + cpt]) * (1 << (width - 1 - cpt) * 8);
			}
		}
		return output;
	}

	// Convert Epoch time from pCapNG file format into EPOCH time
	double pCapEpoch2Epoch(int timeH, int timeL) {
		const time_t time_resolution = (time_t) 1e3; // for the result to be in msec - the resolution is usec in pCap
		std::time_t t;
		double returnTime;
		t = timeH;
		t = t << 32;
		t += timeL;
		returnTime = (double) t / time_resolution;
#ifdef DEBUG
		std::cout << "pCapEpoch2Epoch(): Entered function with EPOCH time: " << returnTime << std::endl;
#endif
		return(returnTime);
	}

	// Implementation of class FSFB2BSDpacket
	FSFB2BSDpacket::FSFB2BSDpacket() {
		nextFSFB2BSDPacket = NULL;
		payload = NULL;
		sizePayLoad = 0;
	}

	FSFB2BSDpacket::~FSFB2BSDpacket() {
		FSFB2BSDpacket *currentBSD = this;
		while (currentBSD != NULL) {
			if (currentBSD->payload != NULL) { delete currentBSD->payload; }
			currentBSD = currentBSD->nextFSFB2BSDPacket;
		}
	}

	FileManagement::FileManagement(std::string name_input) {
		namePcapFile = name_input;
		memblock = NULL;
		rootBSDptr = NULL;
		lastBSDptr = rootBSDptr;
		nbByteinBuffer = 0;
		globalHeaderSize = 0;
		currentPos = 0; // Temporary counter to track position within the pCap
		lastBSDptr = NULL; // Keep track of the last BSD (for quick addition to linked list)
		nbFilteredPacket = 0;
		nbPacket = 0;
		version = "1.1";
		currentTimeStamp = LDBL_MAX;
	}

	FileManagement::~FileManagement() {
		if (memblock != NULL) { 
			delete[] memblock; 
			memblock = NULL; 
#ifdef DEBUG 
			std::cout << "Deleted memblock" << std::endl; 
#endif 
		}
		delete rootBSDptr;
	}

	// Return the version of the class
	std::string FileManagement::getVersion() { 
#ifdef DEBUG
		std::cout << "FileManagement::getVersion(): Entered function" << std::endl; 
#endif
		return version; 
	}

	// Return the total number of packet filtered
	int FileManagement::getFilteredPacketNumber() { return nbPacket; }

	// Load the pCapNG file in memory
	bool FileManagement::Load() {
#ifdef DEBUG
		std::cout << "FileManagement::Load(): Entered function" << std::endl;
#endif
		std::ifstream file(namePcapFile, std::ios::in | std::ios::binary | std::ios::ate);
		if (file.is_open()) {
			std::streampos size = file.tellg(); // size of the file
			if (size > MAXINT32) { return false; } // maximum size of file that can be opened
			nbByteinBuffer = (int)size;
			memblock = new char[(int)size];
			file.seekg(0, std::ios::beg);
			file.read(memblock, size);
			file.close();
			return(true);
		}
#ifdef DEBUG
		std::cout << "FileManagement::Load(): file " << namePcapFile << " could not open" << std::endl;
#endif
		return(false);
	}

	// Parse the Section Header
	bool FileManagement::parseSectionHeader() {
		std::string BT;
#ifdef DEBUG
		std::cout << "FileManagement::parseSectionHeader(): Entered function" << std::endl;
#endif
		if (memblock == NULL) { return false; }
		BT.append(1, memblock[0]).append(1, memblock[1]).append(1, memblock[2]).append(1, memblock[3]); // concatenate bytes 0 to 3 to build Block Type
#ifdef DEBUG
		std::cout << "FileManagement::parseSectionHeader(): concatenate bytes 0 to 3 to build Block Type" << std::endl;
#endif
		if (BT != BLOCKTYPE_NG) { return(false); }
		else {
			globalHeaderSize = char2int(memblock, 4, 4, LITTLE_ENDIAN);
			std::string BOM;
			BOM.append(1, memblock[8]).append(1, memblock[9]).append(1, memblock[10]).append(1, memblock[11]); // concatenate bytes 8 to 11 to build Byte-Order Magic (endianness test) 
			if (BOM == BYTEORDERMAGIC_BE) { localEndianness = BIG_ENDIAN; }
			else if (BOM == BYTEORDERMAGIC_LE) { localEndianness = LITTLE_ENDIAN; }
			else { return(false); }
		}
		currentPos += globalHeaderSize;
		return(true);
	}

	//TODO: check that the allocation has succeeded
	bool FileManagement::addFSFB2BSDPacket() {
		if (rootBSDptr == NULL) {
			rootBSDptr = new FSFB2BSDpacket();
			lastBSDptr = rootBSDptr;
		}
		else {
			lastBSDptr->nextFSFB2BSDPacket = new FSFB2BSDpacket();
			lastBSDptr = lastBSDptr->nextFSFB2BSDPacket;
		}
		nbPacket++;
		return true;
	}

	bool FileManagement::addFSFB2BSDPacket(std::string IPsrc, std::string IPdst, int portSrc, int portDst) {
		addFSFB2BSDPacket();
		lastBSDptr->IPsrc = IPsrc;
		lastBSDptr->IPdst = IPdst;
		lastBSDptr->portsrc = portSrc;
		lastBSDptr->portdst = portDst;
		return true;
	}

	// Parse the Interface Description
	bool FileManagement::parseInterfaceDescription() {
		int interfaceBlockLength = 0;

		interfaceBlockLength = char2int(memblock, currentPos + 4, 4, localEndianness);
		currentPos += interfaceBlockLength;
		return true;
	}

	// Parse the Enhanced Packet Block
	bool FileManagement::parseEnhancedPacketBlock(std::string IPsrc, std::string IPdst, int portSrc, int portDst, 
		int srcNodeID, int dstNodeID) {
		int interfaceBlockLength = 0;
#ifdef DEBUG_FAILED_TEST
		std::cout << "FileManagement::parseEnhancedPacketBlock(): function entered with IPsrc: " << IPsrc << " IPdst: " << IPdst << std::endl;
#endif
		interfaceBlockLength = char2int(memblock, currentPos + 4, 4, localEndianness);
		if (char2int(memblock[currentPos + IPOffsetprotocol]) != UDP) {
			currentPos += interfaceBlockLength;
#ifdef DEBUG_FAILED_TEST
			std::cout << "FileManagement::parseEnhancedPacketBlock(): Test UDP failed" << std::endl;
#endif
			return(true); // if the packet is not UDP go to next
		}

		std::string pCapIPsrc = std::to_string(char2int(memblock[currentPos + IPSrcOffset]))
			+ "." + std::to_string(char2int(memblock[currentPos + IPSrcOffset + 1]))
			+ "." + std::to_string(char2int(memblock[currentPos + IPSrcOffset + 2]))
			+ "." + std::to_string(char2int(memblock[currentPos + IPSrcOffset + 3]));
		if ((pCapIPsrc != IPsrc) && (IPsrc != "*")) { 
			currentPos += interfaceBlockLength;
#ifdef DEBUG_FAILED_TEST
		std::cout << "FileManagement::parseEnhancedPacketBlock(): Test IPsrc failed => IPSrc " << pCapIPsrc << " vs. " << IPsrc << std::endl;
#endif
		return true;
		}
		std::string pCapIPdst = std::to_string(char2int(memblock[currentPos + IPDstOffset]))
			+ "." + std::to_string(char2int(memblock[currentPos + IPDstOffset + 1]))
			+ "." + std::to_string(char2int(memblock[currentPos + IPDstOffset + 2]))
			+ "." + std::to_string(char2int(memblock[currentPos + IPDstOffset + 3]));
		if ((pCapIPdst != IPdst) && (IPdst != "*")) { 
			currentPos += interfaceBlockLength; 
#ifdef DEBUG_FAILED_TEST
		std::cout << "FileManagement::parseEnhancedPacketBlock(): Test IPdst failed = > IPDst " << pCapIPdst << " vs. " << IPdst << std::endl;
#endif
		return true;
		}
		// Note: Network convention is big endian - not related to the capture endianness of the machine
		int pCapPortsrc = char2int(memblock, currentPos + PortSrcOffset, 2, BIG_ENDIAN);
		if (pCapPortsrc != portSrc) { 
			currentPos += interfaceBlockLength; 
#ifdef DEBUG_FAILED_TEST
		std::cout << "FileManagement::parseEnhancedPacketBlock(): Test PortSrc failed" << std::endl;
#endif
		return true;
		}
		int pCapPortdst = char2int(memblock, currentPos + PortDstOffset, 2, BIG_ENDIAN);
		if (pCapPortdst != portDst) { 
			currentPos += interfaceBlockLength; 
#ifdef DEBUG_FAILED_TEST
			std::cout << "FileManagement::parseEnhancedPacketBlock(): Test PortDst failed" << std::endl;
#endif
			return true;
		}
		// Data representation (inside the FSFB2 message) shall follow little-endian format ([SML400GP_FSFB2_SyID_0005])
		int pCapSrcNodeID = char2int(memblock, currentPos + SrcNodeIDOffset, 2, LITTLE_ENDIAN);
		int pCapDstNodeID = char2int(memblock, currentPos + DstNodeIDOffset, 2, LITTLE_ENDIAN);
		if ( (pCapSrcNodeID != srcNodeID) && (srcNodeID != 0) || (pCapDstNodeID != dstNodeID) && (dstNodeID != 0) ) { 
			currentPos += interfaceBlockLength; 
#ifdef DEBUG_FAILED_TEST
			std::cout << "FileManagement::parseEnhancedPacketBlock(): Test SrcNodeID/DstNodeID failed" << std::endl;
#endif
			return true; }
		// All filters have passed and the packet can be added
		addFSFB2BSDPacket(pCapIPsrc, pCapIPdst, pCapPortsrc, pCapPortdst);
		nbFilteredPacket++;
		lastBSDptr->timestampH = char2int(memblock, currentPos + TimeStampHighOffsetEnhPB, 4, localEndianness);
		lastBSDptr->timestampL = char2int(memblock, currentPos + TimeStampLowOffsetEnhPB, 4, localEndianness);
		int payloadWidth = char2int(memblock, currentPos + IPpacketWidth, 2, BIG_ENDIAN); // size of the IP payload
		lastBSDptr->payload = new unsigned char[payloadWidth];
		lastBSDptr->sizePayLoad = payloadWidth;
		memcpy(lastBSDptr->payload, memblock + currentPos + IPpayloadOffset + packetDataOffsetEnhPB, payloadWidth);
		currentPos += interfaceBlockLength;
		return true;
	}
	
	// Parse the pcap file
	int FileManagement::parsePcapNG(std::string ipSrc, std::string ipDst, int portSrc, int portDst, int srcNodeID, int dstNodeID) {
		bool EOFreached = false;
#ifdef DEBUG
		std::cout << "FileManagement::parsePcapNG: Entered function" << std::endl;
#endif
		if (parseSectionHeader() == false) { return -3; }
#ifdef DEBUG
		std::cout << "FileManagement::parsePcapNG: parseSectionHeader() == true" << std::endl;
#endif
		while (!EOFreached) {
			int blockType = char2int(memblock, currentPos, 4, LITTLE_ENDIAN);
			
			if (blockType == 1) {
				if (parseInterfaceDescription() == false) { return -1; }
			}
			if (blockType == 6) {
				if (parseEnhancedPacketBlock(ipSrc, ipDst, portSrc, portDst, srcNodeID, dstNodeID) == false) { return -2; }
			}
			if (currentPos >= nbByteinBuffer) { EOFreached = true; }
		}
		// Once the file loaded in memory has been used the allocated memory can be returned to the system
		if (memblock != NULL) {
			delete[] memblock;
			memblock = NULL;
#ifdef DEBUG 
			std::cout << "Deleted memblock" << std::endl;
#endif 
		}
		return 0;
	}

	// Return the current timestamp of the pCap as a long double in msec
	long double FileManagement::getTimeStamp() {
		FSFB2BSDpacket* currentPacket = NULL;
		if ((currentBSDptr == NULL) && (rootBSDptr == NULL)) { return (LDBL_MAX); }
		if ((currentBSDptr == NULL) && (rootBSDptr != NULL)) { currentPacket = rootBSDptr; }
		if (currentBSDptr != NULL ) { currentPacket = currentBSDptr; }
		return(pCapEpoch2Epoch(currentPacket->timestampH, currentPacket->timestampL));
	}

	// Return the first packet of the list, NULL if none
	int FileManagement::getFirstPacket(unsigned char* &inputArray) {
		if (rootBSDptr != NULL) {
			currentTimeStamp = getTimeStamp();
			currentBSDptr = rootBSDptr->nextFSFB2BSDPacket;
			inputArray = rootBSDptr->payload;
			return rootBSDptr->sizePayLoad;
		}
		else { inputArray = NULL;  return 0; }
	}

	// Return the next packet of the list, NULL if none
	int FileManagement::getNextPacket(unsigned char* &inputArray) {
		if (currentBSDptr != NULL) {
			currentTimeStamp = getTimeStamp();
			int size = currentBSDptr->sizePayLoad;
			inputArray = currentBSDptr->payload;
			currentBSDptr = currentBSDptr->nextFSFB2BSDPacket;
			return size;
		}
		else { inputArray = NULL;  return 0; }
	}

	long double FileManagement::getCurrentTimeStamp() {
		return(currentTimeStamp);
	}

	// Export of all necessary methods to be used by C# caller
	
	// Construct is the constructor	
	FileManagement* MngConstruct(char* input) {
		std::string name_input;
		name_input = std::string(input);
		return new FileManagement(name_input);
	}

	// stringToChar function
	// Used by Managed gateway function
	const char* stringToChar(std::string inputString, char* buffer) {
		if (buffer != NULL) { delete buffer; buffer = NULL; } // if not null, delete preceding allocation 
		buffer = new char[inputString.length() + 1];
		memcpy(buffer, inputString.c_str(), inputString.length() + 1);
		buffer[inputString.length() + 1] = '\0';
		return(buffer);
	}
	
	// Dispose is the destructor
	void MngDispose(FileManagement* objectToDispose) {
		if (objectToDispose != NULL) { delete objectToDispose; objectToDispose = NULL; }
	}
	// MngGetVersion is the managed version of getVersion()
	const char* MngGetVersion(FileManagement* object, char* buffer) {
		return (stringToChar(object->getVersion(), buffer));
	}
	// MngGetFilteredPacketNumber is the managed version of getFilteredPacketNumber()
	int MngGetFilteredPacketNumber(FileManagement* object) {
		return(object->getFilteredPacketNumber());
	}
	// MngParsePcapNG is the managed version of parsePcapNG()
	int MngParsePcapNG(FileManagement* object, char* ipSrc, char* ipDst, int portSrc, int portDst,
		int srcNodeID, int dstNodeID) {
#ifdef DEBUG
		std::cout << "MngParsePcapNG: " << ipSrc << " " << ipDst << " " << portSrc << " " << portDst << std::endl;
#endif
		return(object->parsePcapNG(ipSrc, ipDst, portSrc, portDst, srcNodeID, dstNodeID));
	}
	// MngLoad is the managed version of Load()
	bool MngLoad(FileManagement* object) {
		if (object->Load() == false) {
			return(false);
		}
		else { return true; }
	}
	// MngGetFirstPacket is the managed version of getFirstPacket()
	const int MngGetFirstPacket(FileManagement* object, unsigned char* &buffer) {
		return(object->getFirstPacket(buffer));
	}
	// MngGetNextPacket is the managed version of getNextPacket()
	const int MngGetNextPacket(FileManagement* object, unsigned char* &buffer) {
		return(object->getNextPacket(buffer));
	}
	// MngGetTimeStamp is the managed version for getting the pCap timestamp of the last packet extracted (MAXINT64 else)
	const double MngGetTimeStamp(FileManagement* object) {
		return(object->getCurrentTimeStamp());
	}
}
