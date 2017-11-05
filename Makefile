all: popser.cpp md5.cpp md5.h
	g++ -Wall -pedantic -std=c++17 -pthread -o popser popser.cpp md5.cpp