/*
Project: 	POP3 server
Author:		Jakub Svoboda
Login:		xsvobo0z
Email:		xsvobo0z@stud.fit.vutbr.cz
Date:		6. November 2017
Course:		ISA 2017
*/

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
#include <experimental/filesystem>
#include "md5.h"   

#define BUFFER 1024         // buffer for incoming messages
#define QUEUE 1             // queue length for  waiting connections
#define TIMEOUT 600			//10 minutes autologoff

using namespace std;
namespace fs = std::experimental::filesystem;

string username;								
string password;
sem_t *mutex1;								//a semaphore for the exclusive access to the Maildir folder
bool quitting=false;
std::fstream longLog("longLog.txt");

void  ALARMhandler(int sig){
	sem_post(mutex1);
	exit(0);                                 // exit the child process

}

//the structure containing parameters' values
typedef struct Parameters{
	bool h;
	string a;
	bool c; 
	int p;
	string d;
	bool r;
} Parameters;
Parameters params;

//Resets all logged file movements and removes log files
void resetFiles(){
	if(fs::exists("movements.txt")){				
		std::ifstream file;
		file.open("movements.txt");				//if file exists, open it and read the changes
		string line;
		while (std::getline(file, line)){				
			std::istringstream iss(line);
			vector<string> v;
			for(string word; iss >> word; )
				v.push_back(word);
			if(fs::exists(v[0])){
				fs::rename(v[0],v[1]);			//reverse the file movemens
			}
		}
		file.close();
	}
	if(fs::exists("mailInfo.txt")){				//remove log file
		fs::remove("mailInfo.txt");
	}
	if(fs::exists("movements.txt")){			//remove log file
		fs::remove("movements.txt");
	}

}

//Checkes the validity of parameters and fills tha Parameters structure with data
void validateParameters(int argc, char* argv[]){
	params = {false,"", false, 0, "", false};		//default values
	int opt;
	
	while ((opt = getopt(argc, argv, "ha:cp:d:r")) != -1) {		//parse with getopt()
		switch (opt) {
			case 'h':											//help parameter
				params.h = true;
				cout << ("./popser [-h] [-a PATH] [-c] [-p PORT] [-d PATH] [-r]\n");
				exit(0);
				break;
			case 'a':											//the authentication file parameter
				params.a.assign(optarg);
				break;
			case 'c':											//clear text parameter
				params.c = true;
				break;
			case 'p':											//port parameter
				params.p = atoi(optarg);
				break;	
			case 'd':											//Maildir directory parameter
				params.d = string(optarg);
				break;	
			case 'r':											//reset
				params.r = true;
				break;			
			default: 											//Unknown parameter
				cerr << ("./popser [-h] [-a PATH] [-c] [-p PORT] [-d PATH] [-r]\n");
				exit(-1);
		}
	}
	
	if(argc == 2 && params.r==true){ 				//Server resets
		resetFiles();
		sem_close(mutex1);
		sem_unlink("mutex1");
		exit(0);
	}else{
		if(params.a != "" && params.p != 0 && params.d != ""){	//All required parameters are present
			if(params.r){						//server resets
				resetFiles();
				sem_close(mutex1);
				sem_unlink("mutex1");
				exit(0);
			}
			ifstream ifile(params.a);
			if(!ifile) cerr << "Authentication file does not exist: "<<params.a<<"\n"; //Check if file exists
			struct stat dirInfo;
			if( stat( params.d.c_str(), &dirInfo ) != 0) cerr<<"Directory does not exist: "<<params.d.c_str()<<"\n";	//Check if folder exists

		}else{
			if (params.a == ""){					//Incorrect parameter data, print help text and exit with -1
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
}

//https://stackoverflow.com/questions/7755719/check-if-string-starts-with-another-string-find-or-compare
bool starts_with(const string& s1, const string& s2) {	
    return s2.size() <= s1.size() && s1.compare(0, s2.size(), s2) == 0;
}

//https://stackoverflow.com/questions/20446201/how-to-check-if-string-ends-with-txt
bool has_suffix(const string &str, const string &suffix){
    return str.size() >= suffix.size() && str.compare(str.size() - suffix.size(), suffix.size(), suffix) == 0;
}

//Handler for SIGINT SIGSEGV  and SIGTSTP
void SignalHandler(int iSignalNum){
    switch(iSignalNum){
		case SIGINT:
		case SIGSEGV:
		case SIGTSTP:
				sem_close(mutex1);				//clear semaphores
				sem_unlink("mutex1");
		break;
     default:
        sem_close(mutex1);
		sem_unlink("mutex1");
    }
	exit(0); 
}    

//Obtains timestamp for the md5 hash in apop function
string getTimestamp(){
	string timestamp = to_string(getpid());	//process ID
	char hostname[1024];						
    gethostname(hostname, 1024);			//Host name	
	timestamp = timestamp + "." + to_string(time(NULL)) + "@" +  hostname ;
	return timestamp;
}

//Calculates a size of a file using the filesystem library
int fileSize(const std::string &fileName){
    ifstream file2(fileName.c_str(), ifstream::in | ifstream::binary);
    file2.seekg(0, ios::end);
    int size = file2.tellg();
    file2.close();
    return size;
}

//Converts ann \n to \r\n. This is required for the correct size computation
void unix2dos(){
	string path = params.d;
	char c = path.back();
	if(c != '/'){
		path += "/new" ;
	}else{
		path += "new" ;
	}
	
	for (auto & p : fs::directory_iterator(path)){
		string line,text;
		std::ifstream myfile;
		myfile.open(p.path().c_str());				//open for each file
		int i = 0;
		while (std::getline(myfile, line)){
			i++;
			std::istringstream iss(line);
			if(has_suffix(line,string("\r"))){		//if line ends with \n, replace it with \r\n
				line+="\n";
			}else{
				line+="\r\n";
			}
			text+=line;
		}	
		myfile.close();
		std::ofstream file;							//write modified email text back to the file and close it
		file.open(p.path().c_str());
		file<<text;
	}
}

//Creates a unique id for an email
string getid(){
	string id = to_string(getpid());			//process ID
    int random_variable = std::rand();			//random value
	id = to_string(random_variable) + id + to_string(time(NULL));
	return id;
}

//Handles the Authentication phase of the process.
string logOperations(){
	srand(time(NULL));			//randomize see
	unix2dos();			//convert to \r\n newlines
	string str;
	string path = params.d;
	char c = path.back();
	if(c != '/'){
		path += "/new" ;
	}else{
		path += "new" ;
	}
	int numOfEmails = 0;
	int totalSize = 0;
								//open file for logging email movements
	std::ofstream movements("movements.txt",std::ios_base::app | std::ios_base::out);
	
	for (auto & p : fs::directory_iterator(path)){
		numOfEmails++;
		totalSize += fileSize(p.path());			//calculate total size
		string str = p.path().c_str();
		size_t index = 0;
		index = str.find("Maildir/new", index);
		str.replace(index, 11, "Maildir/cur");
		movements<<str<<" "<<p.path().c_str()<<endl;	
		fs::rename(p.path().c_str(),str);			//move each file from new to the cur folder
	}
	movements.close();
	str = "+OK "+username+"'s maildrop has " + to_string(numOfEmails) + " new messages (" + to_string(totalSize) + " octets)\r\n";
	//message for the user after a successfull login
	
	std::ofstream mailInfo;
	mailInfo.open("mailInfo.txt");			//open log file for email metadata
	path = params.d;
	c = path.back();
	if(c != '/'){
		path += "/cur" ;
	}else{
		path += "cur" ;
	}
	
	int number =0;							//generate a log for each email file
	for (auto & p : fs::directory_iterator(path)){
		number++;
		int size = fileSize(p.path());
		string str = p.path().c_str();
		string id = getid();
		mailInfo<<number<< " " <<size<<" "<<p.path().c_str()<<" "<<id<<" A"<<endl;	
	}
	mailInfo.close();			
	return str;
}

//DELE command
string dele(string arg){
	arg += " ";
	std::ifstream mailInfo;
	mailInfo.open("mailInfo.txt");		//open the log file
	string text,line, line2, find;
	bool didWeDoIt = false;
	while (std::getline(mailInfo, line)){
		std::istringstream iss(line);
		text+=line + "\n";
		if(!strncmp(line.c_str(), arg.c_str(), arg.size())){
			if(line.back() == 'D'){					// an already deleted email cannot be deleted again
				return "-ERR message " + arg + "already deleted\r\n";
			}
			find = line;
			line2=line;
			line2.pop_back();
			line2+="D\n";						//Mark for deletion in log file
			didWeDoIt=true;
		}
	}	
	mailInfo.close();					
	if(!didWeDoIt)
		return "-ERR\r\n";
	size_t pos = text.find(find);
	
	text.replace(pos, line2.size(), line2);			
	std::ofstream mailInfo2;
	mailInfo2.open("mailInfo.txt");				//write modified text to the log gile
	mailInfo2<<text;
	mailInfo2.close();
	return "+OK message "+ arg + "deleted\r\n";	//send positive message to the user
}

//LIST command
string list(){
	std::ifstream mailInfo;
	mailInfo.open("mailInfo.txt");		//open log file
	std::string text,line;
	int numOfEmails = 0;				
	while (std::getline(mailInfo, line)){	//for each logged email
		std::istringstream iss(line);
		if (line.back() == 'D'){			//skip deleted email
			continue;
		}else{
			numOfEmails++;					//calculate the # of emails
			vector<string> v;
			for(string word; iss >> word; )
				v.push_back(word);
			text += v[0] + " " + v[1] + "\n";	//return number and size
		}
	}	
	text+=".\r\n";
	mailInfo.close();	
	text = "+OK " + to_string(numOfEmails) + " messages:\n" +text;
	return text;
}

//STAT command
string stat(){
	std::ifstream mailInfo;
	mailInfo.open("mailInfo.txt");	//open log file
	std::string line;
	int numOfEmails = 0;
	int sizeTotal = 0;
	while (std::getline(mailInfo, line)){	//for each logged email
		std::istringstream iss(line);
		if (line.back() == 'D'){			//skip deleted emails
			continue;
		}else{
			numOfEmails++;
			vector<string> v;
			for(string word; iss >> word; )
				v.push_back(word);
			sizeTotal += atoi(v[1].c_str());	//return their size
		}
	}	

	mailInfo.close();							//positive message with # of emails and size
	return "+OK " + to_string(numOfEmails) + " " + to_string(sizeTotal) + "\r\n";

}

//RETR command
string retr(string arg){
	arg += " ";
	std::ifstream mailInfo;
	mailInfo.open("mailInfo.txt");		//open log file
	string text,line;
	bool didWeDoIt = false;
	string path;
	string size;
	while (std::getline(mailInfo, line)){	//for each logged email
		std::istringstream iss(line);
		if(!strncmp(line.c_str(), arg.c_str(), arg.size())){
			if(line.back() == 'D'){			//email is marked for deletion -> negative message
				return "-ERR\r\n";
			}
			vector<string> v;
			for(string word; iss >> word; )
				v.push_back(word);
			if(v[0] + " " == arg){
				path = v[2];			//save path to file and size
				size = v[1];
				didWeDoIt = true;
			}
		}
	}	
	if(!didWeDoIt)
		return "-ERR\r\n";
	
	mailInfo.close();
	
	std::ifstream mail;
	mail.open(path);					//read the email
	while (std::getline(mail, line)){
		std::istringstream iss(line);
		text+=line + "\r\n";
	}
										//return positive message with size and content
	return "+OK " + size + " octets \n" + text + ".\r\n";
}

//RSET command
string rset(){
	std::ifstream mailInfo;
	mailInfo.open("mailInfo.txt");	//open logs
	string text,line;
	while (std::getline(mailInfo, line)){		//for each log
		std::istringstream iss(line);
		if(line.back() == 'D'){					//when the email is marked for deletion
			vector<string> v;
			for(string word; iss >> word; )
				v.push_back(word);
			line = v[0] + " " + v[1] + " " + v[2] + " " + v[3] +" A";	//Mark as active
		}
		text += line + "\n";
	}
	mailInfo.close();						
	std::ofstream mailInfo2;
	mailInfo2.open("mailInfo.txt");			
	mailInfo2<<text;					//write the new text to the file
	return "+OK \r\n";
}

//UIDL command
string uidl(string arg){
	string str = "+OK ";
	string::iterator end_pos = std::remove(arg.begin(), arg.end(), ' ');
	arg.erase(end_pos, arg.end());
	if(arg == ""){									//IDs for all emails
		str+="\r\n";
		std::ifstream mailInfo;
		mailInfo.open("mailInfo.txt");
		string text,line;
		while (std::getline(mailInfo, line)){		//for each logged email
			std::istringstream iss(line);
			vector<string> v;
			for(string word; iss >> word; ){
				v.push_back(word);
			}	
			str += v[0] + " " + v[3] + "\r\n";	
		}	
		str+=".\r\n";
		mailInfo.close();	
	}else{												//for only one email
		std::ifstream mailInfo;
		mailInfo.open("mailInfo.txt");
		string text,line;
		while (std::getline(mailInfo, line)){			//for each logged email
			std::istringstream iss(line);
			vector<string> v;
			for(string word; iss >> word; ){
				v.push_back(word);
			}	
			if(v[0] == arg){							//if email # matches the requested #
				str += v[0] + " " + v[3] + "\r\n";	
				mailInfo.close();
				return str;
			}	
		}	
		mailInfo.close();
		return "-ERR No email with this number.\r\n";
	}
	
	return str;
}

//The optional TOP command
string top(string arg2, string arg3){
	string::iterator end_pos = std::remove(arg2.begin(), arg2.end(), ' ');		//remove whitespaces in arguments
	arg2.erase(end_pos, arg2.end());
	end_pos = std::remove(arg3.begin(), arg3.end(), ' ');
	arg3.erase(end_pos, arg3.end());
	if(arg2=="" || atoi(arg2.c_str())<=0){								//negative requested #
		return "-ERR Invalid message number\r\n";
	}
	if(arg3=="" ||atoi(arg3.c_str())<0){								//negative line number
		return "-ERR Noise after message number\r\n";
	}
	string line,line2,text;
	std::ifstream mailInfo;
	mailInfo.open("mailInfo.txt");						//open log file
	while (std::getline(mailInfo, line)){				//for each line
		std::istringstream iss(line);
		if(!strncmp(line.c_str(), arg2.c_str(), arg2.size())){	//if email number matches
			if(line.back() == 'D'){								//and if not deleted
				return "-ERR message " + arg2 + " is marked as deleted\r\n";
			}else{					
				text ="+OK \r\n";
				vector<string> v;
				for(string word; iss >> word; )			//tokenize the line
					v.push_back(word);
				std::ifstream mail;
				mail.open(v[2]);						//read for the requested number of lines
				for(int i = 1; i<12 + atoi(arg3.c_str()); i++){
					if(std::getline(mail, line2)){
						text+=line2+"\r\n";
					}
				}										//return the content
				text+=".\r\n";
				return text;
			}
		}	
	}
	return "-ERR there is no message " + arg2 + ".\r\n"	;
}

//run update state operation after a quit command from transaction stat
void updateState(){
	std::ifstream mailInfo;
	mailInfo.open("mailInfo.txt");		//open log file
	string line;
	while (std::getline(mailInfo, line)){	//read logs line by line
		std::istringstream iss(line);

		if(line.back() == 'D'){			//and delete the emails for goood
			vector<string> v;
			for(string word; iss >> word; )
				v.push_back(word);
			fs::remove(v[2]);
		}
		
	}	
	mailInfo.close();
}

//opens the authentication file and gets credentials
void getCredentials(){
	string str;
	ifstream infile;
	infile.open (params.a.c_str());

	getline(infile,str); 
	username=str.substr(11);		//save username
	
	getline(infile,str); 
	password=str.substr(11);		//save password
	
	infile.close();
}

//Parser for the users command
string parseMsg(string message, string timestamp){
	static bool inTransaction = false;
	string response;
	string arg1, arg2, arg3;															//two arguments that the user has sent
	istringstream iss(message);
	getline(iss, arg1, ' ');															//save the two args
	getline(iss, arg2, '\r');
	std::transform(arg1.begin(), arg1.end(), arg1.begin(), ::tolower);					//first arg is case insensitive
	static string user = "";
	getCredentials();
	if(params.c && !inTransaction){	 				//Plaintext authorization
		if(!arg1.compare("user")){
			user = arg2;
			response = "+OK <any>\r\n";
		}else if (!arg1.compare("pass")){		//TODO spaces in password
			if(!username.compare(user) && !password.compare(arg2)){						//AUTH ok
				if(!sem_trywait(mutex1)){
					inTransaction=true;
					response = logOperations();
					alarm(TIMEOUT);
				}else{
					response="-ERR unable to lock maildrop\r\n";
				}
			}else{																		//AUTH fail
				user = "";
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
					response = logOperations();
					alarm(TIMEOUT);
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
			updateState();
			response="+OK logging out\r\n";
			quitting = true;
		}else if (starts_with(message,"list")){	
			response = list();
		}else if (starts_with(message,"stat")){
			response = stat();
		}else if (!arg1.compare("retr")){	
			response = retr(arg2);
		}else if (!arg1.compare("dele")){
			response = dele(arg2);
		}else if (starts_with(message,"rset")){
			response = rset();
		}else if (starts_with(message,"noop")){		
			response="+OK \r\n";
		}else if (starts_with(message,"uidl")){
			response = uidl(arg2);
		}else if (!arg1.compare("top")){
			istringstream iss2(arg2);
			getline(iss2, arg2, ' ');															
			getline(iss2, arg3, '\r');	
			response = top(arg2,arg3);
		}else{
			response = "-ERR\r\n";
		}
		alarm(TIMEOUT);	
	}
	return response;
}

//The majority of this function's code comes from the ISA examplary echo-server2.cpp file from https://wis.fit.vutbr.cz/FIT/st/course-files-st.php.cs?file=%2Fcourse%2FISA-IT%2Fexamples&cid=12191
//The author of this project does not claim authorship of most of the code in the following function.
int connection (){
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
				string response = parseMsg(message, timestamp);		
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

//creates the log files if they do not exist
void createFiles(){
	if(!fs::exists("mailInfo.txt")){
		std::ofstream outfile ("mailInfo.txt");
		outfile.close();
	}
	if(!fs::exists("movements.txt")){
		std::ofstream outfile ("movements.txt");
		outfile.close();
	}
}

int main(int argc, char* argv[]) {
	signal(SIGINT,  SignalHandler);		//create signal handlers for unexpected program kills
	signal(SIGSEGV, SignalHandler);
	signal(SIGTSTP, SignalHandler);
	signal(SIGALRM, ALARMhandler);		//and for the autologoff alarm
	mutex1 = sem_open("mutex1", O_CREAT | O_EXCL, 0644, 1);		//open a semaphore for Maildir
	createFiles();
	validateParameters(argc, argv);			//check if parameters are OK
	getCredentials();
	connection();											//connect
	sem_close(mutex1);											//clear the semaphore
	sem_unlink("mutex1");										
    return 0;
}


