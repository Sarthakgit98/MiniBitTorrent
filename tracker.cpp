#include<stdio.h>
#include<stdlib.h>
#include<netinet/in.h>
#include<sys/types.h>
#include<sys/socket.h>
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

//GLOBAL VARIABLES

string ip_g;
int portno_g;

class User{
public:
	string ip;
	int portno;
	string username;
	string password;
	unordered_set<string> groups_joined;
	
	User(string user, string pass){
		username = user;
		password = pass;
	}
	
	void joingroup(string group){
		groups_joined.insert(group);
	}
	
	void leavegroup(string group){
		if(groups_joined.find(group) != groups_joined.end()) groups_joined.erase(group);
	}
};

class File{
public:
	string filename;
	int bitlength;
	unordered_set<string> owners;
	unordered_set<string> loggedout;
	string hash;
	bool exist;
	
	File(string file, string user){
		filename = file;
		owners.insert(user);
		exist = true;
	}
	
	void logoutuser(string user){
		if(owners.find(user) != owners.end()){
			loggedout.insert(user);
			owners.erase(user);
			
			if(owners.size() == 0) exist = false;
		}
	}
	
	void adduser(string user){
		owners.insert(user);
		exist = true;
	}
	
	void loginuser(string user){
		if(loggedout.find(user) != loggedout.end()){
			owners.insert(user);
			loggedout.erase(user);
			exist = true;
		}
	}
	
	void unshareuser(string user){
		if(loggedout.find(user) != loggedout.end()) loggedout.erase(user);
		if(owners.find(user) != owners.end()) owners.erase(user);
		if(owners.size() == 0 && loggedout.size() == 0) exist = false;
	}
};

class Group{
public:
	string id;
	string owner;
	unordered_set<string> members;
	unordered_set<string> pending;
	unordered_map<string, File*> sharedfiles;
	
	Group(string id_name, string owner_name){
		id = id_name;
		owner = owner_name;
		members.insert(owner_name);
	}
	
	bool findmember(string req){
		return members.find(req) != members.end();
	}
	
	bool findpending(string req){
		return pending.find(req) != pending.end();
	}
	
	void removemember(string req){
		if(members.find(req) != members.end()) members.erase(req);
	}
	
	void removepending(string req){
		if(pending.find(req) != pending.end()) pending.erase(req);
	}
	
	void addfile(string file, string user){
		if(sharedfiles.find(file) != sharedfiles.end()) sharedfiles[file]->adduser(user);
		else{
			File* fileobj = new File(file, user);
			sharedfiles[file] = fileobj;
		}
	}
	
	void logoutuser(string user){
		for(auto filepair : sharedfiles){
			filepair.second->logoutuser(user);
		}
	}
	
	void unsharefile(string file, string user){
		if(sharedfiles.find(file) != sharedfiles.end()) sharedfiles[file]->unshareuser(user);
	}
};

unordered_map<string, Group*> grouplist;
unordered_map<string, User*> registeredlist;
unordered_map<string, User*> loggedin;

void error(string err){
	perror(err.c_str());
	exit(1);
}

//UTILITY FUNCTIONS

void writeData(int socket, string data){
	int n = send(socket, data.c_str(), 512, 0);
	if(n < 0) error("Error while writing to requestor");
}

string readData(int socket){
	char buffer[512];
	memset(buffer, '\0', sizeof(buffer));
	int n = recv(socket, buffer, sizeof(buffer), 0);
	if(n < 0) error("Error while reading from socket");
	return string(buffer);
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


void readTrackerInfo(int choice, string path){
	char tracker_c[512];
	int read_fd = open(path.c_str(), O_RDONLY);
	int read_ret;
	do{
		read_ret = read(read_fd, tracker_c, 512);
	}while(read_ret > 0);
	
	string tracker = string(tracker_c);
	
	vector<string> tokenized_tracker = tokenize(tracker);
	
	pair<string, int> ip_det = tokenize_ip(tokenized_tracker[choice]);
	ip_g = ip_det.first;
	portno_g = ip_det.second;
	
}

//SERVER SIDE OBJECT

class Server{
public:
	string ip;
	int portno;
	int sock_listener;
	struct sockaddr_in self_addr;
	
	Server(string addr, int port){
		ip = addr;
		portno = port;
	
		char addr_c[addr.size()+1];
		strcpy(addr_c, addr.c_str());
		addr_c[addr.size()] = '\0';
	
		sock_listener = socket(AF_INET, SOCK_STREAM, 0);
		if(sock_listener <= 0) error("Could not create socket");
		
		self_addr.sin_family = AF_INET;
		inet_pton(AF_INET, addr_c, &self_addr.sin_addr.s_addr);
		self_addr.sin_port = htons(port);
		
		if(bind(sock_listener, (struct sockaddr *) &self_addr, sizeof(self_addr)) < 0) error("Could not bind server");
		
		if(listen(sock_listener, 20) < 0) error("Could not enter listening state");
		
	}
};

//############################################################


//			COMMAND PARSING


//############################################################

string signup(vector<string> command){
	string username = command[1];
	string password = command[2];
	
	User* user = new User(username, password); 
	registeredlist[username] = user;
	
	return "Registered " + username;
}

string login(vector<string> command, int sockfd){
	string ip;
	int portno;
	string buffer = readData(sockfd);
	
	if(loggedin.find(command[1]) != loggedin.end()){
		return "This user is already logged in from a different location";
	}
	
	vector<string> temp = tokenize(buffer);
	ip = temp[0];
	portno = stoi(temp[1]);
	
	if(registeredlist.find(command[1]) != registeredlist.end()){
		if(registeredlist[command[1]]->password == command[2]){
			loggedin[command[1]] = registeredlist[command[1]];
			loggedin[command[1]]->ip = ip;
			loggedin[command[1]]->portno = portno;
			for(auto group : grouplist){
				for(auto file : group.second->sharedfiles){
					file.second->loginuser(command[1]);
				}
			}
			return "Logged in";
		}
		else{
			return "Invalid password";
		}
	}
	return "$";
}

string connect(vector<string> command){
	if(loggedin.find(command[1]) != loggedin.end()){
		return loggedin[command[1]]->ip + " " + to_string(loggedin[command[1]]->portno);
	}
	else{
		return "Error";
	}
}

string logout(vector<string> command){
	if(loggedin.find(command[1]) != loggedin.end()){
		loggedin.erase(command[1]);
		for(auto mypair: grouplist){
			mypair.second->logoutuser(command[1]);
		}
		return command[1] + " has logged out";
	}
	else{
		return "Not logged in";
	}
}

string create_group(vector<string> command){
	string group_id = command[1];
	string owner = command[2];
	
	if(grouplist.find(group_id) != grouplist.end()) return "Group already exists";
	
	Group* group = new Group(group_id, owner);
	grouplist[group_id] = group;
	loggedin[owner]->joingroup(group_id);
	return "Group created: " + group_id + " with owner " + owner;
}

string join_group(vector<string> command){
	string group_id = command[1];
	string requestor = command[2];
	if(grouplist.find(group_id) != grouplist.end()){
		loggedin[requestor]->joingroup(group_id);
		grouplist[group_id]->pending.insert(requestor);
		return "Request sent successfully";
	}
	
	return "Group does not exist";
}

string leave_group(vector<string> command){
	string group_id = command[1];
	string requestor = command[2];
	Group* temp = grouplist[group_id];
	
	if(grouplist.find(group_id) != grouplist.end()){
		Group* temp = grouplist[group_id];
		
		if(temp->findmember(requestor)){
			temp->removemember(requestor);
			temp->logoutuser(requestor);
			loggedin[requestor]->leavegroup(group_id);
			
			if(temp->owner == requestor){
				for(auto mypair : loggedin){
					mypair.second->leavegroup(group_id);
				}
				grouplist.erase(group_id);
			}
			return "Left group successfully";
		}
		else return "User not in this group";
	}
	return "Group does not exist";
}

string list_groups(vector<string> command, int sockfd){
	writeData(sockfd, "Group_id/Owner");
	for(auto data : grouplist){
		writeData(sockfd, data.first + "/" + data.second->owner);
	}
	return "$";
}

string list_requests(vector<string> command, int sockfd){
	if(grouplist.find(command[2]) != grouplist.end()){
		if(grouplist[command[2]]->owner == command[3]){
			writeData(sockfd, "Pending requests for group: " + command[2]);
			for(string req : grouplist[command[2]]->pending){
				writeData(sockfd, req);
			}
			return "$";
		}
		else{
			writeData(sockfd, "No permission");
			return "$";
		}
	}
	else{
		writeData(sockfd, "Group does not exist");
		return "$";
	}
}

string list_members(vector<string> command, int sockfd){
	if(grouplist.find(command[2]) != grouplist.end()){
		writeData(sockfd, "Current members in group: " + command[2]);
		for(string req : grouplist[command[2]]->members){
			writeData(sockfd, req);
		}
		return "$";
	}
	else{
		writeData(sockfd, "Group does not exist");
		return "$";
	}
}

string accept_request(vector<string> command){
	string gid = command[1];
	string uid = command[2];
	string requestor = command[3];
	
	if(grouplist.find(gid) != grouplist.end()){
		if(grouplist[gid]->owner == requestor){
			if(grouplist[gid]->findpending(uid)){
				grouplist[gid]->removepending(uid);
				grouplist[gid]->members.insert(uid);
				loggedin[uid]->joingroup(gid);
				return "Request accepted";
			}
			else{
				return "No such request exists";
			}
		}
		else{
			return "No permission";
		}
	}
	else{
		return "No such group exists";
	}
}

string list_files(vector<string> command, int sockfd){
	string gid = command[1]; 
	if(grouplist.find(gid) != grouplist.end()){
		for(auto mypair : grouplist[gid]->sharedfiles){
			if(mypair.second->exist) writeData(sockfd, mypair.first);
		}
		return "$";
	}
	else{
		writeData(sockfd, "Group does not exist");
		return "$";
	}
}

string upload_file(vector<string> command, int client){
	string file = command[1];
	string gid = command[2];
	string sharing_user = command[3];
	
	if(grouplist.find(gid) != grouplist.end()){
		if(grouplist[gid]->findmember(sharing_user)){
			grouplist[gid]->addfile(file, sharing_user);
			return "Uploaded";
		}
		else return "Not a member of this group";
	}
	else return "Group does not exist";
}

string fetch_download_peer(vector<string> command, int client){
	string gid = command[1];
	string filename = command[2];
	
	if(grouplist.find(gid) != grouplist.end()){
		Group* temp = grouplist[gid];
		if(!temp->findmember(command[4])){
			return "Not a member of this group";
		}
		if(temp->sharedfiles.find(filename) != temp->sharedfiles.end()){
			string user;
			string ip;
			string port;
			string res;
			for(auto owner : temp->sharedfiles[filename]->owners){
				user = owner;
				ip = loggedin[owner]->ip;
				port = to_string(loggedin[owner]->portno);
				res = user + " " + ip + " " + port;
				//cout << res << endl;
				writeData(client, res);
			}
			return "$";
		}
		else return "File does not exist";
	}
	else return "Group does not exist";
}

string stop_share(vector<string> command){
	string gid = command[1];
	string filename = command[2];
	string uid = command[3];
	
	if(grouplist.find(gid) != grouplist.end()){
		grouplist[gid]->unsharefile(filename, uid);
		return "Successful";
	}
	else{
		return "Group does not exist";
	}
}

string quit_peer(vector<string> command){
	string uid = command[1];
	User* u_obj = loggedin[uid];
	for(string gid : u_obj->groups_joined){
		Group* g_obj = grouplist[gid];
		for(auto file_pair : g_obj->sharedfiles){
			g_obj->unsharefile(file_pair.first, uid);
		}
	}
	logout(command);
	return "Quit successfully";
}

string parseCommand(vector<string> command, int client){
	//parse command and reply accordingly here;
	string output;
	if(command[0] == "create_user"){
		output = signup(command);
	}
	else if(command[0] == "login"){
		output = login(command, client);
	}
	else if(command[0] == "connect"){
		output = connect(command);
	}
	else if(command[0] == "logout"){
		output = logout(command);
	}
	else if(command[0] == "create_group"){
		output = create_group(command);
	}
	else if(command[0] == "join_group"){
		output = join_group(command);
	}
	else if(command[0] == "leave_group"){
		output = leave_group(command);
	}
	else if(command[0] == "list_groups"){
		output = list_groups(command, client);
	}
	else if(command[0] + " " + command[1] == "requests list_requests"){
		output = list_requests(command, client);
	}
	else if(command[0] + " " + command[1] == "requests list_members"){
		output = list_members(command, client);
	}
	else if(command[0] == "accept_request"){
		output = accept_request(command);
	}
	else if(command[0] == "list_files"){
		output = list_files(command, client);
	}
	else if(command[0] == "upload_file"){
		output = upload_file(command, client);
	}
	else if(command[0] == "download_file"){
		output = fetch_download_peer(command, client);
	}
	else if(command[0] == "stop_share"){
		output = stop_share(command);
	}
	else if(command[0] == "quit"){
		output = quit_peer(command);
	}
	return output;
}

//#########################################################################


//			SERVER SIDE


//#########################################################################

void *handle_server_connection(void* args){
	int client = *((int*)args);
	string buffer;
	buffer = readData(client);
	vector<string> tokens = tokenize(buffer);
	
	buffer = parseCommand(tokens, client);
	writeData(client, buffer);
	close(client);
	return NULL;
}

void *server_function(void* args){
	Server server(ip_g, portno_g);
	struct sockaddr_in address;
	memset((char*)&address, '\0', sizeof(address));
	socklen_t addlen = (socklen_t) sizeof(address);
	cout << "Waiting for connections" << endl;
	while(true){
		int client = accept(server.sock_listener, (struct sockaddr*) &address, &addlen);
		pthread_t threadv;
		pthread_create(&threadv, NULL, handle_server_connection, &client);
	}
	return NULL;
}

int main(int argc, char** argv){
	readTrackerInfo(atoi(argv[2]), string(argv[1]));
	
	cout << "Starting server..." << endl;
	cout << "IP: " << ip_g << endl;
	cout << "Port: " << portno_g << endl;
	pthread_t server_thread;
	pthread_create(&server_thread, NULL, server_function, NULL);
	
	string command;
	while(true){
		cin >> command;
		if (command == "quit") break;
	}
	
	return 0;
}
