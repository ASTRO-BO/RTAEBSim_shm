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

#include <limits>
#include <CTADecoder.h>

// shm
#include <sys/ipc.h>
#include <sys/shm.h>

// semaphore
#include <fcntl.h>
#include <sys/stat.h>
#include <semaphore.h>

using namespace std;
using namespace PacketLib;

bool iszero(double someValue) {
	if(someValue == 0)
		return true;
	if (someValue <  numeric_limits<double>::epsilon() &&
		someValue > -numeric_limits<double>::epsilon())
		return true;
    return false;
}

void printBuffer(word* c, int npixels, int nsamples) {
	for(int pixel = 0; pixel<npixels; pixel++) {
		cout << pixel << " ";
		for(int j=0; j<nsamples; j++)
			cout << c[pixel * nsamples + j] << " ";
		cout << endl;
	}
}

#ifdef PRINTALG
int flag = 0;
#endif

void calcWaveformExtraction1(byte* buffer, int npixels, int nsamples, int ws ) {
	word *b = (word*) buffer; //should be pedestal subtractred
	//printBuffer(b, npixels, nsamples);

	/*
	vector<int> maxresv;
	maxresv.reserve(npixels);
	vector<double> timev;
	timev.reserve(npixels);
	int* maxres = &maxresv[0];
	double* time = &timev[0];
	*/

	int* maxres = new int[npixels];
	double* time = new double[npixels];

	//word bl[npixels*nsamples];
	//memcpy(bl, b, npixels*nsamples*sizeof(word));

	for(int pixel = 0; pixel<npixels; pixel++) {
		word* s = b + pixel * nsamples;

#ifdef PRINTALG
		if(flag == 0) {

			cout << pixel << " ";
			for(int k=0; k<nsamples; k++)
				cout << s[k] << " ";
			cout << endl;
		}
#endif

		long max = 0;
		double maxt = 0;
		long sumn = 0;
		long sumd = 0;
		long maxj = 0;
		double t = 0;
		//long sumres[nsamples-ws];

		for(int j=0; j<=ws-1; j++) {
			sumn += s[j] * j;
			sumd += s[j];
		}
		//sumres[0] = sum;
		for(int j=1; j<nsamples-ws; j++) {

			sumn = sumn - s[j-1] * (j-1) + s[j+ws] * (j+ws);
			sumd = sumd - s[j-1] + s[j+ws-1];

			if(!iszero(sumd))
				t = sumn / (double)sumd;
			//sumres[j] = sum;
			if(sumd > max) {
				max = sumd;
				maxt = t;
				maxj = j;
			}
		}

		/*for(int j=0; j<nsamples-ws; j++)
			if(sumres[j]>max) {
				max = sumres[j];
				maxj = j;
			}
		 */

		//maxres.push_back(max);
		//time.push_back(maxt);

		maxres[pixel] = max;
		time[pixel] = maxt;

#ifdef PRINTALG
		//>9000
		if(flag == 0) cout << pixel << " " << maxj << " " << maxres[pixel] << " " << time[pixel] << " " << endl;
#endif
		/*
		for(int k=0; k<nsamples; k++)
			cout << s[k] << " ";
		cout << endl;
		 */
	}
	//SharedPtr<double> shtime(maxt);

#ifdef PRINTALG
	flag++;
#endif
}

void calcWaveformExtraction3(byte* buffer, int npixels, int nsamples, int ws) {
	word *b = (word*) buffer; //should be pedestal subtractred
	/*
	 vector<int> maxres;
	 maxres.reserve(npixels);
	 vector<int> time;
	 time.reserve(npixels);
	 */
	word bl[npixels*nsamples];
	memcpy(bl, b, npixels*nsamples);
	for(int pixel = 0; pixel<npixels; pixel++) {
		//word* s = bl + pixel * nsamples;
		long indexstart = pixel * nsamples;

		long max = 0;
		long maxj = 0;
		long sum = 0;
		for(int j=indexstart; j<indexstart+nsamples-ws; j++) {
			if(j==indexstart)
				for(int i=j; i<=j+ws-1; i++)
					sum += bl[i];
			else
				sum = sum - bl[j-1] + bl[j+ws];
			if(sum>max) {
				max = sum;
				maxj = j;
			}
		}
		//maxres.push_back(max);
		//time.push_back(maxj);
	}
}

int main(int argc, char *argv[]) {

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

	// Allocate space for activatememorycopy
	byte* buffermemory = new byte[2000*50*sizeof(word)];

	// Create decoder
	string configFile = ctarta + "/share/rtatelem/rta_fadc1.stream";
	RTATelem::CTADecoder decoder(configFile);

	int test = *((int*)shmPtr);
	shmPtr += sizeof(int);
	bool activatememorycopy = *((bool*)shmPtr);
	shmPtr += sizeof(bool);
	bool calcalg = *((bool*)shmPtr);
	shmPtr += sizeof(bool);

	dword* sizeShmPtr = (dword*)shmPtr;
	byte* bufferShmPtr = (byte*)shmPtr+sizeof(dword);

	bool firstLoop = true;
	while(1)
	{
		word npixels = 0;
		word nsamples = 0;

		sem_wait(full);
		ByteStreamPtr rawPacket = ByteStreamPtr(new ByteStream(bufferShmPtr, *sizeShmPtr, false));
		RTATelem::CTAPacket& packet = decoder.getPacket(rawPacket);
		RTATelem::CTACameraTriggerData1& trtel = (RTATelem::CTACameraTriggerData1&) packet;

		enum RTATelem::CTAPacketType type = packet.getPacketType();
		//cout << "Packet #" << npacketsread2 << " size: " << size << " byte. type: " << type << endl;
		if(type == RTATelem::CTA_CAMERA_TRIGGERDATA_1) {

			long npacketsread2;
			if(firstLoop) {
				trtel.decode(true);
				npixels = trtel.getNumberOfPixels();
				int pixel = 0;
				nsamples = trtel.getNumberOfSamples(pixel);
				cout << npixels << " " << nsamples << endl;
				firstLoop = false;
			}

			switch(test) {
				case 3: {
					//access to a pointer of the camera data (all pixels) as a single block
					trtel.decode(true);

					//word subtype = trtel.header->getSubType();
					ByteStreamPtr camera = trtel.getCameraDataSlow();
					//cout << rawPacket->size() << " " << camera->size() << endl;
					//							word *c = (word*) camera->stream;
					//if(activatememorycopy) {
					//	memcpy(buffermemory, camera->stream, camera->size());
					//}

					if(calcalg) {
						//word npixels = trtel.getNumberOfPixels();
						//int pixel = 0;
						//word nsamples = trtel.getNumberOfSamples(pixel);
						//cout << npixels << " " << nsamples << endl;
						if(activatememorycopy)
							calcWaveformExtraction1(buffermemory, npixels, nsamples, 6);
						else
							calcWaveformExtraction1(camera->stream, npixels, nsamples, 6);
					}
					break;
				}

				case 4: {
					//packetlib access to an array of samples using packetlib to get the block
					trtel.decode(true);
					int pixel = 0;
					for(int i=0; i<nsamples; i++) {
						word sample = trtel.getSampleValue(pixel, i);
#ifdef PRINTALG
						cout << sample << " ";
#endif
					}
#ifdef PRINTALG
					cout << endl;
#endif
					pixel = npixels - 1;
					for(int i=0; i<nsamples; i++) {
						word sample = trtel.getSampleValue(pixel, i);
#ifdef PRINTALG
						cout << sample << " ";
#endif
					}
#ifdef PRINTALG
					cout << endl;
					cout << "---" << endl;
#endif
					break;
				}

				case 5: {
					//direct acces to an array of samples using packetlib to get the block
					trtel.decode(true);
					int pixel = 0;
					ByteStreamPtr samplebs = trtel.getPixelData(pixel);
					word* sample = (word*) samplebs->stream;
					word s;
					for(int i=0; i < nsamples; i++) {
						s = sample[i];
#ifdef PRINTALG
						cout <<  s << " ";
#endif
					}
#ifdef PRINTALG
					cout << endl;
#endif
					pixel = npixels - 1;
					samplebs = trtel.getPixelData(pixel);
					sample = (word*) samplebs->stream;
					for(int i=0; i<nsamples; i++) {
						s = sample[i];
#ifdef PRINTALG
						cout << s << " ";
#endif
					}
#ifdef PRINTALG
					cout << endl;
					cout << "---" << endl;
#endif
					break;
				}

				case 6: {
					//access to header and data field header with packetlib
					trtel.decode(true);

					word arrayID;
					word runNumberID;
					word ssc;

					trtel.header->getMetadata(arrayID, runNumberID);
					ssc = trtel.header->getSSC();
					word subtype = trtel.header->getSubType();
					double time = trtel.header->getTime();
#ifdef PRINTALG
					cout << "ssc: " << ssc << endl;
					cout << "metadata: arrayID " << arrayID << " and runNumberID " << runNumberID << " " << endl;
					cout << "subtype " << subtype  << endl;
					//trigger time
					cout << "Telescope Time " << time << endl;
#endif
					break;
				}

				case 7: {
					//access to blocks using only ByteStream
					ByteStreamPtr camera = trtel.getCameraData(rawPacket);
					/*
					word npixels;
					npixels = 1141;
					int pixel=0;
					word nsamples;
					nsamples = 40;
					*/
					//cout << npixels << " " << nsamples << endl;
					//cout << camera->size() << endl;

					//							word *c = (word*) camera->stream;
					//printBuffer(c, npixels, nsamples);
					//exit(0);
					if(activatememorycopy) {
						memcpy(buffermemory, camera->stream, camera->size());
					}
					if(calcalg) {
						if(activatememorycopy)
							calcWaveformExtraction1(buffermemory, npixels, nsamples, 6);
						else
							calcWaveformExtraction1(camera->stream, npixels, nsamples, 6);
					}
					//cout << "value of first sample " << c[0] << endl;
					break;
				}

				case 8: {
					//access to header, data field header and source data field (header)
					trtel.decode(true);

					word arrayID;
					word runNumberID;
					word ssc;

					trtel.header->getMetadata(arrayID, runNumberID);
					ssc = trtel.header->getSSC();
					word subtype = trtel.header->getSubType();
					double time = trtel.header->getTime();

					word ntt = trtel.getNumberOfTriggeredTelescopes();
					word tt = trtel.getIndexOfCurrentTriggeredTelescope();
					word telid = trtel.getTelescopeId();
					word evtnum =  trtel.getEventNumber();
#ifdef PRINTALG
					cout << "ssc: " << ssc << endl;
					cout << "metadata: arrayID " << arrayID << " and runNumberID " << runNumberID << " " << endl;
					cout << "subtype " << subtype  << endl;
					cout << "eventNumber:" << evtnum << endl;
					//trigger time
					cout << "Telescope Time " << time << endl;
					//the number of telescopes that have triggered
					cout << "Triggered telescopes: " <<  ntt << endl;
					//the index (zero-based) of the telescope that has triggerd
					cout << "Index Of Current Triggered Telescope " << tt << endl;
					//the id of the telescope that has triggered
					cout << "TelescopeId " << telid << endl;
#endif
					break;
				}

				case 9: {
					//access to some structural information form source data field (packetlib)
					trtel.decode(true);
					word npixels = trtel.getNumberOfPixels();
					int pixel=0;
					word nsamples = trtel.getNumberOfSamples(pixel);
#ifdef PRINTALG
					cout << npixels << " " << nsamples << endl;
#endif
					break;
				}

				case 10:
				{
					//access to some values form source data field (packetlib)
					trtel.decode(true);
					trtel.getSampleValue(0, 0);
					/*
					for(int pixel=0; pixel<npixels; pixel++)
						for(int sample=0; sample < nsamples; sample++)
							trtel.getSampleValue(pixel, sample);
					*/
					break;
				}
			}
			sem_post(empty);
		}
	}

	//wait for the lock of sync mutex
/*	clock_gettime(CLOCK_MONOTONIC, &stop);

	double MB = ((double) NPACKETS * (double) PACKETSZ) / (double) 1000000;
	double secs = (stop.tv_sec - start.tv_sec)
			+ (stop.tv_nsec - start.tv_nsec) / (double) 1000000000.0;
	double MBs = MB / secs;
	printf("throughput: %.3f MB/s\n", MBs);
	sem_close(mutex);
	sem_close(mutexsync);
	shmctl(shmid, IPC_RMID, 0);*/

	return 0;
}
