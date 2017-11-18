#!/bin/bash

# Spustí make v nadřazeném adresáři a poté spustí testy

dir=$(pwd)
cd ..
make "$@" || exit
cd "$dir"
./test.py
