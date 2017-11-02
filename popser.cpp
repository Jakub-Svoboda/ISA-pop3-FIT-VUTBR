#include <iostream>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fstream>

using namespace std;

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

int main(int argc, char* argv[]) 
{
	Parameters params = validateParameters(argc, argv);
	
	
	
	
	
	
    return 0;
}