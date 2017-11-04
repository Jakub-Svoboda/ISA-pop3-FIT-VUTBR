#include <iostream>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fstream>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>
#include <sys/socket.h>
#include <err.h>
#include <ctype.h>
#include <string.h>


#define BUFFER 1024         // buffer for incoming messages
#define QUEUE 1             // queue length for  waiting connections

using namespace std;
string username;
string password;

typedef struct Parameters{
	bool h;
	string a;
	bool c; 
	int p;
	string d;
	bool r;
} Parameters;

Parameters validateParameters(int argc, char* argv[]){
	Parameters params = {false,"", false, 0, "", false};
	int opt;
	
	while ((opt = getopt(argc, argv, "ha:cp:d:r")) != -1) {
		switch (opt) {
			case 'h':
				params.h = true;
				cerr << ("./popser [-h] [-a PATH] [-c] [-p PORT] [-d PATH] [-r]\n");
				exit(0);
				break;
			case 'a':
				params.a.assign(optarg);
				break;
			case 'c':
				params.c = true;
				break;
			case 'p':
				params.p = atoi(optarg);
				break;	
			case 'd':
				params.d = string(optarg);
				break;	
			case 'r':
				params.c = true;
				break;			
			default: //Unknown parameter
				cerr << ("./popser [-h] [-a PATH] [-c] [-p PORT] [-d PATH] [-r]\n");
				exit(-1);
		}
	}
	
	if(argc == 1 && params.r==true){ 				//TODO RESET
		
	}else{
		if(params.a != "" && params.p != 0 && params.d != ""){							//All required parameters are present
			ifstream ifile(params.a);
			if(!ifile) cerr << "Authentication file does not exist: "<<params.a<<"\n"; //Check if file exists
			struct stat dirInfo;
			if( stat( params.d.c_str(), &dirInfo ) != 0) cerr<<"Directory does not exist: "<<params.d.c_str()<<"\n";	//Check if folder exists

		}else{
			if (params.a == ""){
				cerr << ("Missing -a PAHT argument\n");
				cerr << ("./popser [-h] [-a PATH] [-c] [-p PORT] [-d PATH] [-r]\n");
				exit(-1);
			}
			if (params.p == 0){
				cerr << ("Missing -p PORT argument\n");
				cerr << ("./popser [-h] [-a PATH] [-c] [-p PORT] [-d PATH] [-r]\n");
				exit(-1);
			}
			if(params.d == ""){
				cerr << ("Missing -d PATH argument\n");
				cerr << ("./popser [-h] [-a PATH] [-c] [-p PORT] [-d PATH] [-r]\n");
				exit(-1);
			}
		}
	}
	
	return params;
}

void parseMsg(string message){
	static bool inTransaction = false;
	if(!inTransaction){
		
	}
	
}
//The majority of this function comes from the ISA examplary echo-server2.cpp file from https://wis.fit.vutbr.cz/FIT/st/course-files-st.php.cs?file=%2Fcourse%2FISA-IT%2Fexamples&cid=12191
//The author of this project is not the author of most of the code in the following function.
int connection (Parameters params){
	int fd;
	int newsock;
	int len, msg_size, i;
	struct sockaddr_in server;   // the server configuration (socket info)
	struct sockaddr_in from;     // configuration of an incoming client (socket info)
	char buffer[BUFFER];
	pid_t pid; 			           // process ID number (PID)
	struct sigaction sa;         // a signal action when CHILD process is finished
		 
	// handling SIG_CHILD for concurrent processes
	// necessary for correct processing of child's PID 

	sa.sa_handler = SIG_IGN;      // ignore signals - no specific action when SIG_CHILD received
	sa.sa_flags = 0;
	sigemptyset(&sa.sa_mask);     // no mask needed 
	if (sigaction(SIGCHLD,&sa,NULL) == -1)  // when SIGCHLD received, no action required
		err (1,"sigaction() failed");

	if ((fd = socket(AF_INET , SOCK_STREAM , 0)) == -1)   //create a server socket
		err(1,"socket(): could not create the socket");
	
	server.sin_family = AF_INET;             // initialize server's sockaddr_in structure
	server.sin_addr.s_addr = INADDR_ANY;     // wait on every network interface, see <netinet/in.h>
	server.sin_port = htons(params.p);  // set the port where server is waiting
		 
	if( bind(fd,(struct sockaddr *)&server , sizeof(server)) < 0)  //bind the socket to the port
		err(1,"bind() failed");
		 
	if (listen(fd,QUEUE) != 0)   //set a queue for incoming connections
		err(1,"listen() failed");
		 
	// accept connection from an incoming client
	// parameter "from" gets information about the client
	// - if we don't need it, put NULL instead; e.g., newsock=accept(fd,NULL,NULL);
	// newsock is a file descriptor to a new socket where incoming connection is processed

	len = sizeof(from);
	while(1){  // wait for incoming connections (concurrent server)
		// accept a new connection
		if ((newsock = accept(fd, (struct sockaddr *)&from, (socklen_t*)&len)) == -1)
			err(1,"accept failed");

		if ((pid = fork()) > 0){  // this is parent process
			close(newsock);
		}
		else if (pid == 0){  // this is a child process that will handle an incoming request
			//p = (long) getpid(); // current child's PID
			close(fd);

			// process incoming messages from the client using "newsock" socket
			// until the client stops sending data (CRLF)

			while((msg_size = read(newsock, buffer, BUFFER)) > 0){ // read the message
				string message = buffer;
				//printf("buffer = \"%.*s\"\n",msg_size,buffer);
				parseMsg(message);		
					
				i = write(newsock,buffer,msg_size);    // send a converted message to the client
				if (i == -1)                           // check if data was successfully sent out
					err(1,"write() failed.");
				else if (i != msg_size)
					err(1,"write(): buffer written partially");
			}
			// no other data from the client -> close the client and wait for another
		  
			close(newsock);                          // close the new socket
			exit(0);                                 // exit the child process
		} 
		else
			err(1,"fork() failed");
	} 
  
	// close the server 
	close(fd);                               // close an original server socket
	return 0;
}

void getCredentials(Parameters params){
	string str;
	ifstream infile;
	infile.open (params.a.c_str());

	getline(infile,str); 
	username=str.substr(11,std::string::npos);
	
	

	str = "";
	getline(infile,str); 
	password=str.substr(11);
	
	cerr<<username<<pasword<<endl;
	cerr<<username.c_str()[0]<<endl;
	cerr<<username.c_str()[1]<<endl;
	cerr<<username.c_str()[2]<<endl;
	cerr<<username.c_str()[3]<<endl;
	cerr<<username.c_str()[4]<<endl;
	//cerr<<username.c_str()[5]<<endl;
	infile.close();
}

int main(int argc, char* argv[]) 
{
	Parameters params = validateParameters(argc, argv);
	getCredentials(params);
	connection(params);
	
    return 0;
}