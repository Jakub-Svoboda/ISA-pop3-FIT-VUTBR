all: popser.cpp 
	g++ -Wall -pedantic -std=c++1y -pthread -o popser popser.cpp 