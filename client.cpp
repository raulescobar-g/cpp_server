#include "common.h"
#include "TCPRequestChannel.h"
#include "BoundedBuffer.h"
#include "HistogramCollection.h"
#include <sys/wait.h>
#include <thread>
#include <mutex>
using namespace std;

// struct that the worker uses the package the patient number and data for that patient to send to the histogram workers via response buffer
struct packet {
	int p;
	double reply;
	packet(int _p, double _reply): p(_p), reply(_reply) {}
};

void patient_thread_function(int p, int n,BoundedBuffer& b){
	for (int i = 0 ; i < n; ++i){
		DataRequest d1(p,(double)i*0.004,1), d2(p,i*0.004,2); 							// create data packet
		vector<char> buf1(sizeof(DataRequest)), buf2(sizeof(DataRequest)); 				// create the buffer
		memcpy(buf1.data(), &d1, sizeof(DataRequest)); 									// store data in buffer
		memcpy(buf2.data(), &d2, sizeof(DataRequest));
		b.push(buf1);																	// push data onto bounded buffer for worker to consume
		b.push(buf2);
	}
}

void worker_thread_function(BoundedBuffer& reqbuf, BoundedBuffer& resbuf, TCPRequestChannel& chan, string filename){
	/*
		--has two behaviors--
			* if a normal data request then loop over the bounded buffer and make the requests to push onto response buffer
			* if file request then do file request and write to file
	*/

	if (filename == ""){ 
		while (true) {
			
			DataRequest requested(0,0.0,0);
			vector<char> req = reqbuf.pop();
			copy(req.begin(), req.end(), reinterpret_cast<char*>(&requested));	// pop data and cast into a data request struct

			if (requested.person == 0){											// if receive a pseudo quit message "patient 0"
				Request q(QUIT_REQ_TYPE);
				chan.cwrite(&q, sizeof(q));								// we quit from inside the worker thread
				break;
			}

			packet response_packet(requested.person, (double)0);
			vector<char> buf(sizeof(packet));
			chan.cwrite(&requested, sizeof(requested));						// make request
			chan.cread(&response_packet.reply, sizeof(double));					// read reply
								
			memcpy(buf.data(), &response_packet, sizeof(packet));				// put packet of data into buffer
			resbuf.push(buf);													// push data into response buffer for histogram worker
		}
	}
	else {
		int newfile;
		string real_filename = "received/"+filename;
		
		if ((newfile = open(real_filename.c_str(),O_WRONLY|O_CREAT,S_IRWXU)) < 0){
			perror("openning file to write to.");
			exit(1);
		}
		while (true){
			FileRequest requested(0,0);
			vector<char> req = reqbuf.pop();
			copy(req.begin(), req.end(), reinterpret_cast<char*>(&requested));	// pop data and cast into a file request struct

			if (requested.length == -1) {
				Request q(QUIT_REQ_TYPE);
				chan.cwrite(&q, sizeof(Request));								// we quit from inside the worker thread
				break;
			}

			int poopy_len = sizeof(FileRequest) + filename.size()+1;
			char poopy[poopy_len];
			memcpy(poopy, &requested, sizeof(FileRequest));
			strcpy(poopy + sizeof(FileRequest), filename.c_str());

			chan.cwrite(poopy, poopy_len);
			char read[requested.length];
			chan.cread(&read, sizeof(read));
			
			int writer;
			lseek(newfile, requested.offset,SEEK_SET);
			if ((writer = write(newfile,read,sizeof(read))) < 0){
				perror("error writing to new file");
				exit(1);
			}
		}
	}
}

void histogram_thread_function(BoundedBuffer& resbuf, HistogramCollection& hc){
	while (true) {
		packet response(0,0.0);
		vector<char> req = resbuf.pop();
		copy(req.begin(), req.end(), reinterpret_cast<char*>(&response)); 			// cast response into a packet struct

		if (response.p == 0){break;} else {hc.update(response.p, (double)response.reply);}	// we push data into histogram to be updated
	}
}

void file_thread_function(string filename, int filelen, BoundedBuffer& reqbuf, int m) {
	for (int i = 0; i < (filelen/m); ++i){
		FileRequest fm2(i*m, m);
		vector<char> poopy(sizeof(FileRequest));
		memcpy(poopy.data(), &fm2, sizeof(FileRequest));
		reqbuf.push(poopy);
	}
	int new_m = filelen - ((int)(filelen/m))*m;
	FileRequest fm2(((int)(filelen/m))*m, new_m);
	vector<char> poopy(sizeof(FileRequest));
	memcpy(poopy.data(), &fm2, sizeof(FileRequest));
	reqbuf.push(poopy);
}

int main(int argc, char *argv[]){

	int opt, 					// argument buffer
		p = 15,					// number of patient data to fetch (cumulative)
		h = 15,					// number of histogram threads
		b = 2,				// bounded buffer size? still not sure come back to UPDATE
		w = 250,				// number of worker threads to create
		n = 15000,  			// number of data points per patient to fetch	
		m = 256; 				// default buffer memory for FIFO
	string filename = "",
			portno = "",
			hostname = "";		// default filename empty string used as condition for deciding if its a file request or data request

	while ((opt = getopt(argc, argv, "f:p:h:w:b:n:m:r:")) != -1) {
		switch (opt) {
			case 'f':
				filename = optarg;
				break;
			case 'p':
				p = atoi(optarg);
				break;
			case 'w':
				w = atoi(optarg);
				break;
			case 'b':
				b = atoi(optarg);
				break;
			case 'n':
				n = atoi(optarg);
				break;
			case 'm':
				m = atoi(optarg);
				break;
			case 'r':
				portno = optarg;
				break;
			case 'h':
				hostname = optarg;
				break;
		}
	}

	TCPRequestChannel chan(hostname,portno);
	BoundedBuffer request_buffer(b), response_buffers(b);
	HistogramCollection hc; 
	thread patient_threads[p], worker_threads[w], histogram_threads[h];
	TCPRequestChannel * chans[w];
	thread file_thread;
	

	for (int i = 0 ; i < p; ++i){
		Histogram * h = new Histogram(10,-2.0,2.0);
		hc.add(h);
	}

	for (int i = 0; i < w; ++i){ 
		chans[i] = new TCPRequestChannel(hostname, portno);
	}

	// STARTING THE TIMER NOW
	struct timeval start, end;
    gettimeofday (&start, 0);

    /* Starting all threads here */
	
	if (filename == "") {
		for (int i = 0; i < p; ++i){ 
			patient_threads[i] = thread(patient_thread_function,i+1,n,ref(request_buffer)); 
		}
	} 
	else {

		FileRequest fm (0,0);
		int len = sizeof (FileRequest) + filename.size()+1;
		char buf2 [len];
		memcpy (buf2, &fm, sizeof (FileRequest));
		strcpy (buf2 + sizeof (FileRequest), filename.c_str());
		chan.cwrite (buf2, len);  
		int64 filelen;
		chan.cread (&filelen, sizeof(int64));

		file_thread = thread(file_thread_function,filename,filelen,ref(request_buffer),m);
	}
	
	for (int i = 0; i < w; ++i){ 
		worker_threads[i] = thread(worker_thread_function,ref(request_buffer),ref(response_buffers),ref(*chans[i]),filename); 
	}
	if (filename == "") {
		for (int i = 0; i < h ; ++i){ 
			histogram_threads[i] = thread(histogram_thread_function,ref(response_buffers),ref(hc)); 
		}
	}
	
	

	/* Joining all threads here*/
	if (filename == "") {
		for (int i = 0; i < p; ++i){ 
			patient_threads[i].join();
		} 
		for (int i = 0 ; i < w; ++i){
			DataRequest q1 (0,0.0,0);
			vector<char> buf(sizeof(DataRequest));
			memcpy(buf.data(), &q1, sizeof(DataRequest));
			request_buffer.push(buf);
		} 
		for (int i = 0 ; i < w ; ++i){
			worker_threads[i].join();
			delete chans[i];
		}
		for (int i =0 ; i < h; ++i){
			vector<char> buf(sizeof(packet)); 
			packet quit(0,0.0);
			memcpy(buf.data(), &quit, sizeof(packet));
			response_buffers.push(buf);
		}
		for (int i = 0; i < h ; ++i){
			histogram_threads[i].join();
		}
	}
	else {
		file_thread.join();
		for (int i = 0 ; i < w; ++i){
			FileRequest q1 (-1,-1);
			vector<char> buf(sizeof(FileRequest));
			memcpy(buf.data(), &q1, sizeof(FileRequest));
			request_buffer.push(buf);
		} 
		for (int i = 0 ; i < w ; ++i){
			worker_threads[i].join();
			delete chans[i];
		}
	}
	
	// ENDING TIMER NOW	
    gettimeofday (&end, 0);


    // print the results and time difference
	if (filename == "") hc.print ();

    int secs = (end.tv_sec * 1e6 + end.tv_usec - start.tv_sec * 1e6 - start.tv_usec)/(int) 1e6;
    int usecs = (int)(end.tv_sec * 1e6 + end.tv_usec - start.tv_sec * 1e6 - start.tv_usec)%((int) 1e6);
    cout << "Took " << secs << " seconds and " << usecs << " micro seconds" << endl;
	
	 
	Request q (QUIT_REQ_TYPE);
    chan.cwrite (&q, sizeof (Request));
	// client waiting for the server process, which is the child, to terminate
	wait(0);
	cout << "Client process exited" << endl;

}
