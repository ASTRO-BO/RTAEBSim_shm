/***************************************************************************
    copyright            : (C) 2014 Andrea Bulgarelli
                               2014 Andrea Zoli
    email                : bulgarelli@iasfbo.inaf.it
                           zoli@iasfbo.inaf.it
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "shm_common.h"

// shm
#include <sys/ipc.h>
#include <sys/shm.h>

// semaphore
#include <fcntl.h>
#include <sys/stat.h>
#include <semaphore.h>

#include <iostream>
#include <iomanip>
#include <string>
#include <cstdlib>
#include <packet/PacketBufferV.h>
#include <packet/File.h>

using namespace std;
using namespace PacketLib;

struct timespec start, stop;
unsigned long totbytes = 0;

double timediff(struct timespec stop, struct timespec start) {
    double secs = (stop.tv_sec - start.tv_sec) + (stop.tv_nsec - start.tv_nsec) / (double)1000000000.0;
    return secs;
}

void end(int ntimefilesize=1) {
	clock_gettime( CLOCK_MONOTONIC, &stop);
	double time = timediff(stop, start);
	//std::cout << "Read " << ncycread << " ByteSeq: MB/s " << (float) (ncycread * Demo::ByteSeqSize / time) / 1048576 << std::endl;
	cout << "Result: it took  " << time << " s" << endl;
	if(totbytes != 0)
		cout << "Result: rate: " << setprecision(10) << totbytes / 1000000 / time << " MiB/s" << endl;
	exit(1);
}

int main(int argc, char *argv[]) {

	if(argc != 5) {
		cout << "client file.raw test(0/10) memcpy(0/1) waveformExtraction(0/1)" << endl;
		cout << "where test is:" << endl;
		cout << "0: check data model loading" << endl;
		cout << "1: load data into a circular buffer" << endl;
		cout << "2: decoding for routing (identification of the type of packet)" << endl;
		cout << "3: decoding all the blocks of the packet" << endl;
		exit(0);
	}

	// Parse command line
	int test = atoi(argv[2]);
	int ntimes = 0;
	switch(test) {
		case 0: {
			cout << "Test 0: check data model loading" << endl;
			break;
		}
		case 1: {
			cout << "Test 1: check the loading of camera data packets" << endl;
			break;
		}
		case 2: {
			cout << "Test 2: decoding for routing (identification of the type of packet)" << endl;
			ntimes = 1000;
			break;
		}
		case 3: {
			cout << "Test 3: access to a pointer of the camera data (all pixels) as a single block (method 1 packetlib)" << endl;
			ntimes = 500;
			break;
		}
		case 4: {
			cout << "Test 4: packetlib access to an array of samples using packetlib to get the block" << endl;
			ntimes = 50;
			break;
		}
		case 5: {
			cout << "Test 5: direct acces to an array of samples using packetlib to get the block" << endl;
			ntimes = 2;
			break;
		}
		case 6: {
			cout << "Test 6: access to header and data field header with packetlib" << endl;
			ntimes = 2;
			break;
		}
		case 7: {
			cout << "Test 7: decoding all the blocks of the packet (method 2 packetlib::bytestream builder)" << endl;
			ntimes = 5000;
			break;
		}
		case 8: {
			cout << "Test 8: access to header, data field header and source data field (header)" << endl;
			ntimes = 500;
			break;
		}
		case 9: {
			cout << "Test 9: access to some structural information form source data field (packetlib)" << endl;
			ntimes = 10;
			break;
		}
		case 10: {
			cout << "Test 10: access to some values from source data field (packetlib)" << endl;
			ntimes = 10;
			break;
		}
		default:
		{
			cerr << "Wrong test number " << test << endl;
			return 0;
		}
	}

	// Load $CTARTA env variable
	string ctarta;
	const char* home = getenv("CTARTA");
	if(!home) {
		cerr << "ERROR: CTARTA environment variable is not defined." << endl;
		return 0;
	}
	ctarta = home;

	// Create a virtual memory from the shm (of key shmKey)
	key_t key = shmKey;
	int shmid = shmget(key, shmSize, 0666);
	if (shmid < 0) {
		cerr << "Failure in shmget" << endl;
		return 0;
	}
	unsigned char* shm = (unsigned char*) shmat(shmid, NULL, 0);
	unsigned char* shmPtr = shm;

	// Create semaphores
	sem_t* full = sem_open(semFullName, O_CREAT, 0644, 0);
	if (full == SEM_FAILED) {
		cerr << "Unable to create full semaphore" << endl;
		return 0;
	}
	sem_t* empty = sem_open(semEmptyName, O_CREAT, 0644, 1);
	if (empty == SEM_FAILED) {
		cerr << "Unable to create empty semaphore" << endl;
		return 0;
	}

	bool activatememorycopy = atoi(argv[3]);
	if(activatememorycopy)
		cout << "Test  : memcpy activated..." << endl;

	bool calcalg = atoi(argv[4]);
	if(calcalg)
		cout << "Test  : waveform extraction algorithm " << endl;

	// set test and calcalg value in the shm
	*((int*)shmPtr) = test;
	shmPtr += sizeof(int);
	*((bool*)shmPtr) = activatememorycopy;
	shmPtr += sizeof(bool);
	*((bool*)shmPtr) = calcalg;
	shmPtr += sizeof(bool);

	if(test == 0) {
		clock_gettime( CLOCK_MONOTONIC, &start);
		cout << "start Test 0 ..." << endl;
	}

	try {
		if(test == 0)
			end();

		//check the size of the file
		File f;
		f.open(argv[1]);
		f.close();

		if(test == 1) {
			clock_gettime(CLOCK_MONOTONIC, &start);
			cout << "Start Test 1 ..." << endl;
		}

		string configFile = ctarta + "/share/rtatelem/rta_fadc1.stream";
		PacketLib::PacketBufferV buff(ctarta + configFile, argv[1]);
		buff.load();
		int buffersize = buff.size();
		cout << "Loaded " << buffersize << " packets " << endl;

		if(test == 1) {
			for(long i=0; i<buffersize; i++) {
				ByteStreamPtr rawPacket = buff.getNext();
				totbytes += rawPacket->size();
			}
			end();
		}

		cout << "Start Test " << test << " ... " << ntimes << " runs " << endl;
		clock_gettime( CLOCK_MONOTONIC, &start);

		dword* sizeShmPtr = (dword*)shmPtr;
		byte* bufferShmPtr = (byte*)shmPtr+sizeof(dword);

		for(int i=0; i<ntimes; i++) {
			ByteStreamPtr rawPacket = buff.getNext();
			dword size = rawPacket->size();

			sem_wait(empty);
			*sizeShmPtr = size;
			memcpy(rawPacket->getStream(), bufferShmPtr, size);
			sem_post(full);

			totbytes += size;
		}

		end(ntimes);
	}
	catch(PacketException* e) {
        cout << e->geterror() << endl;
	}

	sem_close(full);
	sem_close(empty);
	shmctl(shmid, IPC_RMID, 0);

	return 0;
}
