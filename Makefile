all : web_proxy

web_proxy: main.cpp
	g++ -o web_proxy main.cpp -lpcap -lpthread

clean:
	rm -f web_proxy *.o
