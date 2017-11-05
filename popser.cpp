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
#include <algorithm>
#include <sstream>
#include <mutex>  
#include <semaphore.h> 
#include <fcntl.h>   
#include <filesystem>
#include "md5.h"   

#define BUFFER 1024         // buffer for incoming messages
#define QUEUE 1             // queue length for  waiting connections

using namespace std;
string username;
string password;
sem_t *mutex1;
bool quitting=false;
std::ifstream file("log.txt");

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

//https://stackoverflow.com/questions/7755719/check-if-string-starts-with-another-string-find-or-compare
bool starts_with(const string& s1, const string& s2) {	
    return s2.size() <= s1.size() && s1.compare(0, s2.size(), s2) == 0;
}

void SignalHandler(int iSignalNum){
    switch(iSignalNum){
		case SIGINT:
		case SIGSEGV:
		case SIGTSTP:
				sem_close(mutex1);
				sem_unlink("mutex1");
		break;
     default:
        sem_close(mutex1);
		sem_unlink("mutex1");
    }
	exit(0); 
}    

string getTimestamp(){
	string timestamp = to_string(getpid());
	char hostname[1024];
    gethostname(hostname, 1024);
    //puts(hostname);
	timestamp = timestamp + "." + to_string(time(NULL)) + "@" +  hostname ;
	return timestamp;
}

string logOperations(){
	string str;
	int num;									//get the number of the last email in log file
	while (std::getline(file, str)){						
		string buf; 
		stringstream ss(str); 
		vector<string> tokens; 
		while (ss >> buf){
			tokens.push_back(buf); 
		}	
		num = atoi(tokens[0].c_str());
		
    }
	num++;
	
	
	
	
	
	return str;
}

string parseMsg(string message, Parameters params, string timestamp){
	static bool inTransaction = false;
	string response;
//	message.erase(std::remove(message.begin(), message.end(), '\r'), message.end());	//remove newlines at the end of the imput message
//	message.erase(std::remove(message.begin(), message.end(), '\n'), message.end());
	string arg1, arg2, arg3;															//two arguments that the user has sent
	istringstream iss(message);
	getline(iss, arg1, ' ');															//save the two args
	getline(iss, arg2, '\r');
	std::transform(arg1.begin(), arg1.end(), arg1.begin(), ::tolower);					//first arg is case insensitive
	static string user = "";
	if(params.c && !inTransaction){	 				//Plaintext authorization
		if(!arg1.compare("user")){
			user = arg2;
			response = "+OK\r\n";
		}else if (!arg1.compare("pass")){		//TODO spaces in password
			if(!username.compare(user) && !password.compare(arg2)){						//AUTH ok
				if(!sem_trywait(mutex1)){
					inTransaction=true;
					string str = logOperations();
					response="+OK maildrop locked and ready\r\n";
				}else{
					response="-ERR unable to lock maildrop\r\n";
				}
			}else{																		//AUTH fail
				response = "-ERR [AUTH] Authentication failed\r\n";
			}
			return response;
		}else if (starts_with(message,"quit")){
			response="+OK logging out\r\n";
			exit(0);
		}else{
			response = "-ERR\r\n";
		}
	}else if (!params.c && !inTransaction){			//Hashed authorization
		if (!arg1.compare("quit")){
			response="+OK logging out\r\n";
		}else if (!arg1.compare("apop")){
			istringstream iss2(arg2);
			getline(iss2, arg2, ' ');															
			getline(iss2, arg3, '\r');
			if(!username.compare(arg2) && !arg3.compare(md5("<" + timestamp + ">" + password))){
				if(!sem_trywait(mutex1)){
					inTransaction=true;
					string str = logOperations();
					response="+OK maildrop locked and ready\r\n";
				}else{
					response="-ERR unable to lock maildrop\r\n";
				}	
			}else{
				response="-ERR\r\n";
			}
		}else{
			response="-ERR\r\n";
		}
	}else if (inTransaction){
		if (starts_with(message,"quit")){
			response="+OK logging out\r\n";
			quitting = true;
		}else if (!arg1.compare("list")){	
		
		}else if (!arg1.compare("stat")){
			
		}else if (!arg1.compare("retr")){	
		
		}else if (!arg1.compare("dele")){
			
		}else if (!arg1.compare("rset")){

		}else if (!arg1.compare("noop")){		
		
		}else if (!arg1.compare("uidl")){
			
		}else{
			response = "-ERR\r\n";
		}	
	}
	return response;
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
			string timestamp = getTimestamp();
			string welcome = "+OK POP3 server ready <" + timestamp + ">\r\n";
			
			i = write(newsock,welcome.c_str(),welcome.length());    // send a converted message to the client
			if (i == -1)                           // check if data was successfully sent out
				err(1,"write() failed.");
			else if (i != (int) welcome.length())
				err(1,"write(): buffer written partially");

			while((msg_size = read(newsock, buffer, BUFFER)) > 0){ // read the message
				string message(buffer, msg_size);
				string response = parseMsg(message, params, timestamp);		
					
				i = write(newsock,response.c_str(),response.length());    // send a converted message to the client
				if (i == -1)                           // check if data was successfully sent out
					err(1,"write() failed.");
				else if (i != (int) response.length())
					err(1,"write(): buffer written partially");
				if(quitting) break;
			}
			// no other data from the client -> close the client and wait for another
			sem_post(mutex1);
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
	username=str.substr(11);
	
	getline(infile,str); 
	password=str.substr(11);
	
	infile.close();
}

int main(int argc, char* argv[]) {
	signal(SIGINT,  SignalHandler);
	signal(SIGSEGV, SignalHandler);
	signal(SIGTSTP, SignalHandler);
	mutex1 = sem_open("mutex1", O_CREAT | O_EXCL, 0644, 1);	
	Parameters params = validateParameters(argc, argv);
	getCredentials(params);
	connection(params);
	sem_close(mutex1);
	sem_unlink("mutex1");
    return 0;
}


