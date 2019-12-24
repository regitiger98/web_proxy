#include <arpa/inet.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <netinet/in.h>
#include <netdb.h>
#include <sys/socket.h>
#include <string>
#include <thread>
using namespace std;

void usage() {
	printf("syntax : web_proxy <tcp port>\n");
	printf("sample : web_proxy 8080\n");
}

string FindHost(string data) {
	string none = "";
	
	if(data.substr(0, 3) != "GET" &&
	   data.substr(0, 3) != "PUT" &&
	   data.substr(0, 4) != "POST" &&
	   data.substr(0, 4) != "HEAD" &&
	   data.substr(0, 6) != "DELETE" &&
	   data.substr(0, 6) != "OPTION") {
		return none;
	}
	
	size_t pos = data.find("Host: ");
	if(pos == string::npos) {
		return none;
	}
	
	string host = data.substr(pos + 6);
	pos = host.find("\r\n");
	host = host.substr(0, pos);
	return host;
}

void Relay(int recvfd, int sendfd) {
	const static int BUFSIZE = 4096;
	char buf[BUFSIZE];

	while(true) {
		ssize_t received = recv(recvfd, buf, BUFSIZE - 1, 0);
		if (received == 0 || received == -1) {
			perror("recv failed");
			break;
		}
		buf[received] = '\0';
		printf("[*] Received Msg from %d :\n%s\n", recvfd, buf);

		ssize_t sent = send(sendfd, buf, strlen(buf), 0);
		if (sent == 0) {
			perror("send failed");
			break;
		}
	}
	close(recvfd);
	close(sendfd);
}

int main(int argc, char *argv[]) {
	if (argc != 2) {
		usage();
		return -1;
	}
	int port = atoi(argv[1]);
	
	int sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd == -1) {
		perror("socket failed");
		return -1;
	}

	int optval = 1;
	setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR,  &optval , sizeof(int));

	struct sockaddr_in addr;
	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);
	addr.sin_addr.s_addr = htonl(INADDR_ANY);
	memset(addr.sin_zero, 0, sizeof(addr.sin_zero));

	int res = bind(sockfd, reinterpret_cast<struct sockaddr*>(&addr), sizeof(struct sockaddr));
	if (res == -1) {
		perror("bind failed");
		return -1;
	}

	res = listen(sockfd, 2);
	if (res == -1) {
		perror("listen failed");
		return -1;
	}

	while (true) {
		struct sockaddr_in addr;
		socklen_t clientlen = sizeof(sockaddr);
		int browserfd = accept(sockfd, reinterpret_cast<struct sockaddr*>(&addr), &clientlen);
		if (browserfd < 0) {
			perror("ERROR on accept");
			close(browserfd);
			continue;
		}
		printf("[+] Connected from Browser\n");
		
		const static int BUFSIZE = 4096;
		char buf[BUFSIZE];

		ssize_t received = recv(browserfd, buf, BUFSIZE - 1, 0);
		if (received == 0 || received == -1) {
			perror("recv failed");
			close(browserfd);
			continue;
		}
		buf[received] = '\0';
		printf("[*] Received Msg from %d :\n%s\n", browserfd, buf);
		
		string data = string((const char*)buf, strlen(buf));
		string host = FindHost(data);
		if(host == "") {
			close(browserfd);
			continue;
		}
		printf("[*] Host : %s\n", host.c_str());
		struct hostent* hostinfo;
		hostinfo = gethostbyname(host.c_str());
		
		int serverfd = socket(AF_INET, SOCK_STREAM, 0);
		if (serverfd == -1) {
			perror("socket failed");
			close(browserfd);
			close(serverfd);
			continue;
		}

		struct sockaddr_in addr2;
		addr2.sin_family = AF_INET;
		addr2.sin_port = htons(80);
		addr2.sin_addr.s_addr = *(unsigned int*)hostinfo->h_addr;
		memset(addr2.sin_zero, 0, sizeof(addr2.sin_zero));

		res = connect(serverfd, reinterpret_cast<struct sockaddr*>(&addr2), sizeof(struct sockaddr));
		if (res == -1) {
			perror("connect failed");
			close(browserfd);
			close(serverfd);
			continue;
		}
		printf("[+] Connected to Server\n");

		ssize_t sent = send(serverfd, buf, strlen(buf), 0);
		if (sent == 0) {
			perror("send failed");
			close(browserfd);
			close(serverfd);
			continue;
		}

		thread Thread1(Relay, browserfd, serverfd);
		thread Thread2(Relay, serverfd, browserfd);
	}

	close(sockfd);

}
