#include<stdio.h>
#include<stdlib.h>
#include<netinet/in.h>
#include<sys/types.h>
#include<sys/socket.h>
#include<sys/stat.h>
#include<cstring>
#include<unistd.h>
#include<iostream>
#include<pthread.h>
#include<iostream>
#include<netdb.h>
#include<bits/stdc++.h>
#include<arpa/inet.h>
#include<fcntl.h>

using namespace std;

/*###############################################################


		GLOBAL DATA STRUCTURES FOR PEER


#################################################################*/

vector<pair<string, int>> ip_det;
int tracker_no, tracker_count;
string ip_g, server_ip_g;
int portno_g, server_portno_g;
string username = "$";

//File information
class File{
public:
	string filename;
	string path;
	string bitmap;

	File(string file, string p, string b = "1"){
		filename = file;
		path = p;
		bitmap = b;
	}
};

//Download progress per request
class Download{
public:
	string group;
	string filename;
	string bitmap;
	string state;
	int remaining_bits;
	
	Download(string file, string gid, int bitlength){
		string bitmap_temp = "";
		for(int i = 0; i < bitlength; i++){
			bitmap_temp += "0";
		}
		filename = file;
		group = gid;
		bitmap = bitmap_temp;
		remaining_bits = bitlength;
		state = "D";
	}
	
	void updateprogress(int chunk){
		bitmap[chunk] = '1';
		remaining_bits--;
		if(remaining_bits == 0) state = "C";
	}
	
	void printprogress(){
		cout << "[" << state << "][" << group << "] " << filename << endl; 
	}
};

//Data structures to access above objects
vector<Download*> downloads;
unordered_map<string, File*> sharedfiles;

//Arguments for threads
struct argstruct{
	string pinfo;
	string destination;
	Download* inst;
};

struct download_helper_t{
	string filename;
	string destination;
	Download* d_data;
	int chunk;
	string peerinfo;
};

struct download_t{
	vector<string> command_tok;
	vector<string> peerinfo;
};

/*###############################################################


			UTILITY FUNCTIONS


#################################################################*/

void error(string err){
	perror(err.c_str());
	exit(1);
}

void writeData(int socket, string data){
	int n = send(socket, data.c_str(), 512, 0);
	if(n < 0) error("Error while writing to requestor");
}

string readData(int socket){
	char buffer[512];
	memset(buffer, '\0', sizeof(buffer));
	int n = recv(socket, buffer, sizeof(buffer), 0);
	if(n < 0) error("Error while reading from socket");

	string b = string(buffer);
	return b;
}

vector<string> tokenize(string input){
	stringstream ss(input);
	string temp;
	vector<string> res;
	
	while(ss >> temp){
		res.push_back(temp);
	}
	
	return res;
}

pair<string, int> tokenize_ip(string input){
	pair<string, int> ip_det;
	
	ip_det.second = stoi(input.substr(input.size()-4, 4));
	ip_det.first = input.substr(0, input.size()-5);
	
	return ip_det;
}


void readTrackerInfo(string path){
	char tracker_c[512];
	int read_fd = open(path.c_str(), O_RDONLY);
	int read_ret;
	do{
		read_ret = read(read_fd, tracker_c, 512);
	}while(read_ret > 0);
	
	string tracker = string(tracker_c);
	
	vector<string> tokenized_tracker = tokenize(tracker);
	tracker_count = tokenized_tracker.size();
	
	for(int i = 0; i < tracker_count; i++){
		ip_det.push_back(tokenize_ip(tokenized_tracker[i]));
	}
	
	server_ip_g = ip_det[0].first;
	server_portno_g = ip_det[0].second;
}

/*###############################################################


		SERVER AND CLIENT OBJECTS


#################################################################*/

class Server{
public:
	string ip;
	int portno;
	int sock_listener;
	struct sockaddr_in self_addr;
	
	Server(string addr, int port){
		ip = addr;
		portno = port;
		int option = 1;
		
		char addr_c[addr.size()+1];
		strcpy(addr_c, addr.c_str());
		addr_c[addr.size()] = '\0';
	
		sock_listener = socket(AF_INET, SOCK_STREAM, 0);
		if(sock_listener <= 0) error("Could not create socket");
		
		int socket_option=setsockopt(sock_listener , SOL_SOCKET, SO_REUSEADDR, &option, sizeof(option));
		
		self_addr.sin_family = AF_INET;
		inet_pton(AF_INET, addr_c, &self_addr.sin_addr.s_addr);
		self_addr.sin_port = htons(port);
		
		if(bind(sock_listener, (struct sockaddr *) &self_addr, sizeof(self_addr)) < 0) error("Could not bind server");
		
		if(listen(sock_listener, 1000) < 0) error("Could not enter listening state");
	}
};

class Client{
public:
	int sockfd;
	vector<string> fileOwners;
	
	Client(){
		sockfd = socket(AF_INET, SOCK_STREAM, 0);
		if(sockfd <= 0) error("Could not create socket");
	}
	
	string parseReplyExpected(int sock, vector<string> request_tok){
		string output;
		if (request_tok[0] == "list_groups"){
			string temp;
			while((temp = readData(sock)) != "$"){
				cout << temp << endl;
			}
			output = temp;
		}
		else if (request_tok[0] + " " + request_tok[1] == "requests list_requests" || request_tok[0] + " " + request_tok[1] == "requests list_members"){
			string temp;
			while((temp = readData(sock)) != "$"){
				cout << temp << endl;
			}
			output = temp;
		}
		else if (request_tok[0] == "list_files"){
			string temp;
			while((temp = readData(sock)) != "$"){
				cout << temp << endl;
			}
			output = temp;
		}
		else if (request_tok[0] == "download_file"){
			string temp;
			while((temp = readData(sock)) != "$"){
				fileOwners.push_back(temp);
			}
			output = temp;
		}
		
		else output = readData(sock);
		return output;
	}
	
	void sendRequest(int sockfd, string request_string){
		vector<string> request_tok = tokenize(request_string);
		if(request_tok[0] == "login"){
			writeData(sockfd, request_string);
			writeData(sockfd, ip_g + " " + to_string(portno_g));
		}
		else{
			writeData(sockfd, request_string);
		}
	}
	
	string request(string ip, int port, string request_string){
		
		struct sockaddr_in serv_addr;
		memset((char*)&serv_addr, '\0', sizeof(serv_addr));
		serv_addr.sin_family = AF_INET;
		serv_addr.sin_port = htons(port);
		inet_pton(AF_INET, ip.c_str(), &serv_addr.sin_addr.s_addr);
		
		if(connect(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0){
			tracker_no++;
			ip_g = ip_det[tracker_no%tracker_count].first;
			portno_g = ip_det[tracker_no%tracker_count].second;
			return "##FailedToConnect## Try again";
		}
		
		vector<string> request_tok = tokenize(request_string);
		
		sendRequest(sockfd, request_string);
		string buffer = parseReplyExpected(sockfd, request_tok);
		
		close(sockfd);
		
		return buffer;
	}
};

string parseRequest(int sock){
	string command = readData(sock);
	vector<string> command_tok = tokenize(command);
	
	if(command_tok[0] == "download_file"){
		string filename = command_tok[1];
		string path = sharedfiles[filename]->path;
		
		char buffer[512*1024];
		int fd = open((path + "/" + filename).c_str(), O_RDONLY);
		int size = pread(fd, buffer, sizeof(buffer), stoi(command_tok[2])*512*1024);
		
		int n = send(sock, buffer, size, 0);
		if(n < 0) error("Error while writing to requestor");
		return "";
	}
	else if(command_tok[0] == "get_bitmap"){
		return (sharedfiles[command_tok[1]]->bitmap);
	}
	return "Hello I am " + username;
}

void *handle_server_connect(void* args){
	int client = *((int*)args);
	string buffer;
	buffer = parseRequest(client);
	if(buffer != "$" && buffer.size() > 0) writeData(client, buffer);
	close(client);
	return NULL;
}

void *server_function(void* args){
	Server server(ip_g, portno_g);
	struct sockaddr_in address;
	memset((char*)&address, '\0', sizeof(address));
	socklen_t addlen = (socklen_t) sizeof(address);
	while(true){
		int client = accept(server.sock_listener, (struct sockaddr*) &address, &addlen);
		pthread_t threadv;
		pthread_create(&threadv, NULL, handle_server_connect, &client);
	}
	return NULL;
}

string client_function(string ip, int port, string request){
	Client client;
	string reply = client.request(ip, port, request);
	return reply;
}

/*###############################################################


		PEER/TRACKER REQUEST INTERMEDIATES


#################################################################*/

string make_request(string command){
	return client_function(server_ip_g, server_portno_g, command);
}

vector<string> make_download_request(string command){
	Client client;
	string reply = client.request(server_ip_g, server_portno_g, command);
	return client.fileOwners;
}

string connect_peer(string ip, int portno, string command){
	return client_function(ip, portno, command);
}

vector<string> make_bitmap_request(string filename, vector<string> peerinfo){
	string bitmap;
	for(int i = 0; i < peerinfo.size(); i++){
		vector<string> peer_tokenized = tokenize(peerinfo[i]);
		bitmap = connect_peer(peer_tokenized[1], stoi(peer_tokenized[2]), "get_bitmap " + filename);
		peerinfo[i] = peerinfo[i] + " " + bitmap;
	}
	return peerinfo;
}

/*###############################################################


			FILE DOWNLOAD


#################################################################*/

void download_file_helper(struct download_helper_t* args){
	struct download_helper_t arg = *(args);
	delete args;
	vector<string> p_info = tokenize(arg.peerinfo);
	string filepath = arg.destination + "/" + arg.filename;

	//Make connection
	int sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if(sockfd <= 0) error("Could not create socket");
	
	struct sockaddr_in serv_addr;
	memset((char*)&serv_addr, '\0', sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(stoi(p_info[2]));
	inet_pton(AF_INET, p_info[1].c_str(), &serv_addr.sin_addr.s_addr);
	
	if(connect(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) error("Could not connect to peer");
	
	writeData(sockfd, "download_file " + arg.filename + " " + to_string(arg.chunk));
	
	char* buffer = new char[512*1024];
	int i = 0;
	while(1){
		int n = recv(sockfd, buffer+i, 512*1024, 0);
		if(n <= 0) break;
		i = i+n;
	}
	//cout << "Downloaded chunk " << arg.chunk << endl;
	int fd = open(filepath.c_str(), O_WRONLY | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH);
	
	int writecount = pwrite(fd, buffer, i, arg.chunk*512*1024);
	if(writecount < 0) error("Could not download file");
	
	close(fd);
	close(sockfd);
	
	arg.d_data->updateprogress(arg.chunk);
	sharedfiles[arg.filename]->bitmap[arg.chunk] = '1';
}

bool compare_piece(pair<int, vector<string>> a, pair<int, vector<string>> b){
	return a.second.size() < b.second.size();
}

vector<pair<int, string>> piecewiseSelect(vector<string> peerinfo){
	int bitsize = tokenize(peerinfo[0])[3].size();
	vector<pair<int, vector<string>>> chunkpossessed(bitsize);
	for(int i = 0; i < bitsize; i++){
		chunkpossessed[i].first = i;
	}
	for(string x : peerinfo){
		vector<string> temp = tokenize(x);
		for(int i = 0; i < bitsize; i++){
			if(temp[3][i] == '1'){
				chunkpossessed[i].second.push_back(x);
			}
		}
	}
	
	sort(chunkpossessed.begin(), chunkpossessed.end(), compare_piece);
	
	vector<pair<int, string>> chunkrequest(bitsize);
	for(int i = 0; i < bitsize; i++){
		vector<string> temp = chunkpossessed[i].second;
		int choice = rand()%temp.size();
		
		chunkrequest[i].first = chunkpossessed[i].first;
		chunkrequest[i].second = temp[choice];
	}
	
	return chunkrequest;
}

void* download_file(void* args){
	download_t arg = *((download_t*)args);
	delete (download_t*)args;
	vector<string> command_tok = arg.command_tok;
	vector<string> peerinfo = arg.peerinfo;
	vector<string> s_user = tokenize(peerinfo[0]);
	int bitlength = s_user[3].size();
	
	Download* instance = new Download(command_tok[2], command_tok[1], bitlength);
	File* file = new File(command_tok[2], command_tok[3], instance->bitmap);
	
	downloads.push_back(instance);
	sharedfiles[command_tok[2]] = file;
	
	make_request("upload_file " + command_tok[2] + " " + command_tok[1] + " " + username);
	
	vector<pair<int, string>> chunkrequest = piecewiseSelect(peerinfo);
	
	pthread_t threads[bitlength];
	
	for(int i = 0; i < chunkrequest.size(); i++){
		struct download_helper_t* req = new download_helper_t;
		req->filename = command_tok[2];
		req->destination = command_tok[3];
		req->d_data = instance;
		req->chunk = chunkrequest[i].first;
		req->peerinfo = chunkrequest[i].second;
		
		download_file_helper(req);
	}
	
	return NULL;
}

/*###############################################################


			COMMAND PARSING


#################################################################*/

void parseCommand(string command){
	vector<string> command_tok = tokenize(command);
	int commlen = command_tok.size();
	
	if(command_tok[0] == "create_user" && commlen == 3){
		string info = make_request(command);
		cout << info << endl;
	}
	else if(command_tok[0] == "login" && commlen == 3){
		string info = make_request(command);
		if (info != "$"){
			username = command_tok[1];
			cout << info << endl;
		}
		else{
			cout << info << endl;
		}
	}
	else if(commlen == 1 && command_tok[0] == "show_downloads"){
		if (downloads.size() == 0){
			cout << "No downloads yet" << endl;
			return;
		}
		for(Download* d : downloads){
			d->printprogress();
		}
	}
	else if(username != "$"){
		if(commlen == 2 && command_tok[0] == "connect"){
			string info = make_request(command);
			vector<string> address = tokenize(info);
			if(address.size() > 1){
				info = connect_peer(address[0], stoi(address[1]), command);
				cout << info << endl;
			}
			else{
				cout << "No such user logged in" << endl;
			}
		}
		
		else if(commlen == 1 && command_tok[0] == "logout"){
			string info = make_request(command + " " + username);
			cout << info << endl;
			username = "$";
		}
		else if(commlen == 2 && command_tok[0] == "create_group"){
			string info = make_request(command + " " + username);
			cout << info << endl;
		}
		else if(commlen == 2 && command_tok[0] == "join_group"){
			string info = make_request(command + " " + username);
			cout << info << endl;
		}
		else if(commlen == 2 && command_tok[0] == "leave_group"){
			string info = make_request(command + " " + username);
			cout << info << endl;
		}
		else if(commlen == 1 && command_tok[0] == "list_groups"){
			make_request(command);
		}
		else if(commlen == 3 && command_tok[0] + " " + command_tok[1] == "requests list_requests"){
			string info = make_request(command + " " + username);
			if(info != "$") cout << info << endl;
		}
		else if(commlen == 3 && command_tok[0] + " " + command_tok[1] == "requests list_members"){
			make_request(command);
		}
		else if(commlen == 3 && command_tok[0] == "accept_request"){
			string info = make_request(command + " " + username);
			cout << info << endl;
		}
		else if(commlen == 2 && command_tok[0] == "list_files"){
			make_request(command);
		}
		else if(commlen == 3 && command_tok[0] == "upload_file"){
			string path = command_tok[1];
			
			struct stat filestat;
			int filesize;
			if(lstat(path.c_str(), &filestat) == -1){
				cout << "File does not exist" << endl;
				return;
			}
			
			int index = path.size()-1;
			int len = 0;
			while(index > 0 && path[index] != '/' && path[index] != '\\'){
				index--;
				len++;
			}
			string file;
			if(index > 0){
				file = path.substr(index+1, len);
				path = path.substr(0, index+1);
			}
			else{
				file = path.substr(index, len+1);
				path = "./";
			}
			
			long long int bitlength = (intmax_t) (filestat.st_size/(512*1024));
			if(filestat.st_size%(512*1024) != 0) bitlength += 1;
			string bitmap = "";
			for(int i = 0; i < bitlength; i++){
				bitmap += "1";
			}
			
			File* temp = new File(file, path, bitmap);
			string info = make_request(command_tok[0] + " " + file + " " + command_tok[2] + " " + username);
			
			if(info == "Uploaded"){
				if(sharedfiles.find(file) == sharedfiles.end()) sharedfiles[file] = temp;
			}
			cout << info << endl;
		}
		else if(commlen == 4 && command_tok[0] == "download_file"){
			if(sharedfiles.find(command_tok[2]) != sharedfiles.end()){
				cout << "File already being seeded" << endl;
				return;
			}
			
			vector<string> peerinfo = make_download_request(command + " " + username);
			if(peerinfo.size() == 1 && peerinfo[0] == "Not a member of this group"){
				cout << peerinfo[0] << endl;
				return;
			}
			
			peerinfo = make_bitmap_request(command_tok[2], peerinfo);
			
			if(peerinfo.size() == 1 && peerinfo[0] == "File does not exist"){
				cout << peerinfo[0] << endl;
				return;
			}
			
			if(peerinfo.size() == 1 && peerinfo[0] == "Group does not exist"){
				cout << peerinfo[0] << endl;
				return;
			}
			
			download_t* args = new download_t;
			args->peerinfo = peerinfo;
			args->command_tok = command_tok;
			
			cout << "Starting download: " << command_tok[2] << endl;
			pthread_t main_dthread;
			pthread_create(&main_dthread, NULL, download_file, args);
		}
		else if(commlen == 3 && command_tok[0] == "stop_share"){
			if(sharedfiles.find(command_tok[2]) != sharedfiles.end()) sharedfiles.erase(command_tok[2]);
			string info = make_request(command + " " + username);
			cout << info << endl;
		}
		else if(commlen == 1 && command_tok[0] == "quit"){
			string info = make_request(command + " " + username);
			cout << info << endl;
			exit(0);
		}
	}
	else{
		cout << "Not logged in/invalid command" << endl;
	}	
	return;
}

/*###############################################################


			DRIVER FUNCTION


#################################################################*/

int main(int argc, char** argv){
	pair<string, int> temp = tokenize_ip(string(argv[1]));
	ip_g = temp.first;
	portno_g = temp.second;
	readTrackerInfo(argv[2]);
	
	cout << "Started peer" << endl;
	pthread_t server_thread;
	pthread_create(&server_thread, NULL, server_function, NULL);
	usleep(500);
	
	string command;
	//Parse command
	while(true){
		getline(cin>>ws, command);
		parseCommand(command);
		cout << endl;
	}
	//Parse command: end
	return 0;
}
