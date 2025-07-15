FLAGS=-std=c++17 -ggdb `pkg-config --cflags --libs opencv4` -lcurl -Wno-unused-result -O3 -Ideps

target:
	g++ src/*.cpp $(FLAGS)
