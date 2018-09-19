#include <sys/socket.h> 
#include <sys/types.h> 
#include <netinet/in.h>
#include <arpa/inet.h>
#include <iostream>
#include <string>
#include <sstream>
#include <stdlib.h> 
#include <netdb.h>
#include <unistd.h>
#include <regex>

#define SERVER_ADDR "www.varlabs.org"
#define REQUEST_FILE "/yourip.php"
using namespace std;

void sendHttpGetRequest(string server_url, string request_file);

// Get IP Address of Server
string getServerIp(string server_addr) {
	struct addrinfo hints, *infoptr;
	hints.ai_family = AF_INET;
	memset(&hints, 0, sizeof hints);
	
	int result = getaddrinfo(server_addr.c_str(), NULL, &hints, &infoptr);
	if (result) { // If failed to get IP Address
		cout << "Error Getting " << server_addr << " Info: " << gai_strerror(result) << endl;
		exit(1);
	}

	char server_ip[256];
	getnameinfo(infoptr->ai_addr, infoptr->ai_addrlen,
				server_ip, sizeof(server_ip),
				NULL, 0, NI_NUMERICHOST);

	return string(server_ip);
}

// Create client socket
int createMySocket() {
	int sock = socket(AF_INET, SOCK_STREAM, 0);
	if (sock == -1) {
		perror("Socket");
		exit(1);
	}
	return sock;
}

// Set parameters of server port
sockaddr_in setupServerSocket(int server_port, string server_ip) {
	sockaddr_in server_addr;
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(server_port);
	inet_pton(AF_INET, server_ip.c_str(), &(server_addr.sin_addr));
	return server_addr;
}

// Connect to server's socket
void connectToServerSocket(int my_socket, sockaddr_in server_addr) {
	int connection = connect(my_socket, (struct sockaddr *)&server_addr, sizeof(struct sockaddr));
	if (connection < 0) {
		perror("Connect");
		exit(1);
	}
}

// Create http get request
string createHttpGetRequest(string server_addr, string request_file) {
	stringstream ss;
	string crlf = "\r\n";
	ss << "GET " << request_file << " HTTP/1.1" << crlf;
	ss << "Host: " << server_addr << crlf;
	ss << "Connection: keep-alive" << crlf;
	ss << "Accept: text/html" << crlf;
	ss << crlf;
	return ss.str();
}

// Extract response code
string extractResponseCode(char http_response[]) {
	char response_code[4]; 
	copy(http_response + 9, http_response + 12, response_code);
	response_code[3] = '\0';
	return string(response_code);
}

// extract new location
void extractNewLocation(char http_response[], string &server_addr, string &request_file) {
	string rcv_string = string(http_response);
	regex rgx("Location: http:\\/\\/([\\w+\\.?]+)([\\/\\w\\.]+)");
	smatch match;
	if(regex_search(rcv_string, match, rgx)) {
		server_addr = match[1];
		request_file = match[2];
	}
}

// Error handling
void handleError(char rcv_data[]) {
	cout << "Error handling HTTP Response: " << endl;
	cout << rcv_data << endl;
	exit(-1);
}

// Handle redirection
void handle302(char rcv_data[]) {

	string new_location, request_file;
	extractNewLocation(rcv_data, new_location, request_file);
	sendHttpGetRequest(new_location, request_file);
}

// Handle Ok!
void handle200(char rcv_data[]) {
	string rcv_string = string(rcv_data);
	regex rgx("([0-9]{1,3}\\.){3}[0-9]{1,3}");
	smatch match;
	if(regex_search(rcv_string, match, rgx)) {
		cout << "My public IP address is " << match[0] << endl;
	} else {
		cout << "NOT A 'YOUR IP' WEBPAGE" << endl << "SHOWING HTTP RESPONSE" << endl << endl;
		cout << rcv_data << endl;
	}
	exit(0);
}

// Entry point to different response handler
void handleHttpResponse(char rcv_data[]) {
	string response_code = extractResponseCode(rcv_data);
	if (response_code == "302" || response_code == "301") {
		handle302(rcv_data);
	} else if (response_code == "200") {
		handle200(rcv_data);
	} else {
		handleError(rcv_data);
	}
}

// Send GET request
void sendHttpGetRequest(string server_url, string request_file) {
	int my_socket = createMySocket();
	string server_ip;
	char rcv_data[1024];
	server_ip = getServerIp(server_url);
	sockaddr_in server_addr = setupServerSocket(80, server_ip);
	connectToServerSocket(my_socket, server_addr);
	string snd_data = createHttpGetRequest(server_url, request_file);
	send(my_socket, snd_data.c_str(), snd_data.length(), 0);
	recv(my_socket, rcv_data, 1024, 0);
	close(my_socket);
	handleHttpResponse(rcv_data);
}

void getServerAddrAndRequestFileFromUrl(string url, string &server_addr, string &request_file) {
	string delimiter = "/";
	int pos;
	if ((pos = url.find(delimiter)) > -1) {
		server_addr = url.substr(0, pos);
		request_file = url.substr(pos, url.length());
	} else {
		server_addr = url;
		request_file = delimiter;
	}
}

int main(int argc, char const *argv[]) {	
	string server_addr, request_file;
	if (argc != 2) {
		server_addr = SERVER_ADDR;
		request_file = REQUEST_FILE;
	} else {
		getServerAddrAndRequestFileFromUrl(string(argv[1]), server_addr, request_file);
	}
	sendHttpGetRequest(server_addr, request_file);
	return 0;
}