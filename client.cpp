/*
	Original author of the starter code
    Tanzir Ahmed
    Department of Computer Science & Engineering
    Texas A&M University
    Date: 2/8/20
	
	Please include your Name, UIN, and the date below
	Name: Emma Fernandez
	UIN: 833007846
	Date: 9/16/25
*/
#include "common.h"
#include "FIFORequestChannel.h"
#include <iostream>
#include <fstream>

using namespace std;

// NOTES: diff BIMDC/<filename.csv> received/<samefilename.csv>
// should return nothing if they are the same (the goal for -f)

int main (int argc, char *argv[]) {
	int opt;
	int p = -1;
	double t = -1;
	int e = -1;
	// creating -m variable
	// MAX_MESSAGE = default value for buffer capacity
	int m = MAX_MESSAGE;
	bool new_chan = false;
	vector<FIFORequestChannel*> channels;
		
	string filename = "";

	while ((opt = getopt(argc, argv, "p:t:e:f:m:c")) != -1) {
		switch (opt) {
			case 'p':
				p = atoi (optarg);
				break;
			case 't':
				t = atof (optarg);
					break;
			case 'e':
				e = atoi (optarg);
				break;
			case 'f':
				filename = optarg;
				break;
			// adding case m
			case 'm':
				m = atoi (optarg);
				break;
			case 'c':
				new_chan = true;
				break;
		}
	}
	
	// give arguments for the server
	// char* cmd1[] = {(char*) "ls", (char*) "-al", (char*) "/", nullptr};
	char m_str[12];
    snprintf(m_str, sizeof(m_str), "%d", m);
	char* execArgs[] = {(char*) "./server", (char*) "-m", m_str, nullptr};

	// fork
	// create child process for server
	pid_t pid = fork();

	if (pid < 0) {
		cerr << "Fork Failed." << endl;
		return 1;
	} else if (pid == 0) {
		// child process
		
		// in child, run execvp using the server arguments
		// executes child process (server code)
		// execlp("./server", "./server", "-m", m, nullptr);
		execvp(execArgs[0], execArgs);

		// if error in execv
		cerr << "Execv failed." << endl;
		
		return 1;
	} else {
		sleep(1);

		// parent proccess (client) code
		FIFORequestChannel const_chan("control", FIFORequestChannel::CLIENT_SIDE);
		channels.push_back(&const_chan);

		// create channel
		if (new_chan) {
			cout << "CREATING NEW CHANNEL" << endl;
			// send new channel request to server
			MESSAGE_TYPE nc = NEWCHANNEL_MSG;
			const_chan.cwrite(&nc, sizeof(MESSAGE_TYPE));

			// receive name from server
			// create variable to hold name

			// string channel_name;
			// // cread response from server
			// const_chan.cread(&channel_name, 30);
			char buffer[31] = {0};
			const_chan.cread(buffer, 30);
			string channel_name(buffer);

			// call the FIFORequestChannel constructor with the name from the server
			// -- dynamically create channel (bc we inside if statement but want it to exist outside)
			// -- call 'new' with constructor
			FIFORequestChannel* recent_channel = new FIFORequestChannel(channel_name, FIFORequestChannel::CLIENT_SIDE);

			// push the new channel into the vector
			channels.push_back(recent_channel);
		}

		// at this point, we know whether we creating a new channel or not
		// want to use the last channel in vector to send our requests
		FIFORequestChannel* chan = channels.back();

		if (p != -1 && t != -1 && e != -1) {
			// only wanna run single data point when p, t, e != -1
			char buf[MAX_MESSAGE]; // 256
			datamsg x(p, t, e);
		
			// send request to server
			memcpy(buf, &x, sizeof(datamsg));
			chan->cwrite(buf, sizeof(datamsg)); // question
			double reply;
			chan->cread(&reply, sizeof(double)); //answer
			cout << "For person " << p << ", at time " << t << ", the value of ecg " << e << " is " << reply << endl;

		} else if (p != -1) { //else if (p != -1 && t == -1 && e == -1) {
			// else, if p != -1, request 1000 data points

			// create file to write to
			ofstream outputFile;
			outputFile.open("received/x1.csv");

			// loop over first 1000 lines
			double t_temp = 0;
			if (outputFile.is_open()) {
				for (int i = 0; i < 1000; ++i) {

					char bufecg1[MAX_MESSAGE]; // 256
					outputFile << t_temp << ",";

					// send request for ecg 1
					datamsg ecg1(p, t_temp, 1);
					memcpy(bufecg1, &ecg1, sizeof(datamsg));
					chan->cwrite(bufecg1, sizeof(datamsg)); // question
					double reply1;
					chan->cread(&reply1, sizeof(double)); //answer
					outputFile << reply1 << ",";

					// send request for ecg 2
					char bufecg2[MAX_MESSAGE]; // 256
					datamsg ecg2(p, t_temp, 2);
					memcpy(bufecg2, &ecg2, sizeof(datamsg));
					chan->cwrite(bufecg2, sizeof(datamsg)); // question
					double reply2;
					chan->cread(&reply2, sizeof(double)); //answer
					outputFile << reply2 << endl;

					t_temp += 0.004;
				}
			}
			else {
				cerr << "Error opening file" << endl;
			}

			outputFile.close();
		}
		
		if (!filename.empty()) {
			// // creates filemsg instance
			filemsg fm(0, 0);
			// set fname to filename read in w flag -f
			string fname = filename;
		
			int len = sizeof(filemsg) + (fname.size() + 1);
			// creates buf2 that is size of length (size of filemsg + size of filename + 1 (null terminator))
			// -------- fopen to get size of file, file size is filename.size() + 1 ------------
			char* buf2 = new char[len];
			memcpy(buf2, &fm, sizeof(filemsg)); // cpy filemsg
			strcpy(buf2 + sizeof(filemsg), fname.c_str()); // cpy filename

			chan->cwrite(buf2, len);  // want the file length;

			int64_t filesize = 0;

			// receives file length
			chan->cread(&filesize, sizeof(int64_t));

			// loop over with filesize / buff capacity (-m) ----------- use while loop
			// create filemsg instance (can reuse buff2)
			// casts buff2 to filemsg pointer
			ofstream outputFile("received/" + fname, ios::binary);
			if (!outputFile.is_open()) {
				cerr << "Failed to open output file: received/" << fname << endl;
				delete[] buf2;
				return 1;
			}

			// response buffer at size buff capacity (m)
			char* buf3 = new char[m];
			int64_t offset = 0;

			while (offset < filesize) {
				int chunk_size = m;

				if (filesize - offset < m) {
					chunk_size = filesize - offset;
				}

				filemsg* file_req = new filemsg(offset, chunk_size);

				// Copy new filemsg into buf2
				memcpy(buf2, file_req, sizeof(filemsg));
				// Keep the filename in the second half
				strcpy(buf2 + sizeof(filemsg), fname.c_str());

				// send the request for chunk
				chan->cwrite(buf2, len);

				// receive cunk
				chan->cread(buf3, chunk_size);

				// write buf3 into file received/filename
				outputFile.write(buf3, chunk_size);

				// update remaining bytes to continue looping
				offset += chunk_size;

				delete file_req;
			}

			// close file
			outputFile.close();
			delete[] buf2;
			delete[] buf3;
		}

		// if necessary, close and delete the new channel
		if (new_chan) {
			// close and delete
			FIFORequestChannel* new_chan_ptr = channels.back();
    
    		MESSAGE_TYPE m = QUIT_MSG;
    		new_chan_ptr->cwrite(&m, sizeof(MESSAGE_TYPE));
    
    		delete new_chan_ptr;
		} else {
			// closing the channel    
			MESSAGE_TYPE m = QUIT_MSG;
			chan->cwrite(&m, sizeof(MESSAGE_TYPE));
		}
	}
	return 0;
}
