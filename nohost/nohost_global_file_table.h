#include <iostream>
#include <list>
#include <string>
#include <vector>
#include <stdint.h>
#include <time.h>
#include <sys/time.h>
#include <errno.h>

namespace rocksdb{

std::vector<std::string> &split(const std::string &s, char delim, std::vector<std::string> &elems);
std::vector<std::string> split(const std::string &s, char delim);
long int GetCurrentTime();

struct FileSegInfo{
	size_t start_address;
	size_t size;
	FileSegInfo(size_t start_address_, size_t size_){
		this->start_address =start_address_;
		this->size =size_;
	}
};

class Node{
public:
	Node(std::string* name_, bool isfile_, Node* parent_) :
		children(NULL), file_info(NULL){
		this->name =name_;
		this->isfile = isfile_;
		this->parent = parent_;
		if(isfile_) file_info = new std::vector<FileSegInfo*>();
		else children = new std::list<Node*>();
		last_modified_time = GetCurrentTime();
		link_count = 1;
		lock = false;
	}
	~Node(){
		if(isfile) delete file_info;
		else delete children;
		delete name;
	}
	Node* parent;
	std::string* name;
	bool isfile;
	long int last_modified_time;
	std::list<Node*>* children;
	std::vector<FileSegInfo*>* file_info;
	int link_count;
	unsigned long int GetSize();
	bool lock;
};

class GlobalFileTableTree{
public:
	std::vector<unsigned char>* free_page_bitmap; // until now, it associates a char with a page

	GlobalFileTableTree(size_t page_size_){
		free_page_bitmap = new std::vector<unsigned char>();
		free_page_bitmap->push_back(0);
		root = new Node(new std::string(""), false, NULL);
		cwd = "/";
		page_size = page_size_;
		CreateDir("tmp");
        CreateDir("proc");
        CreateDir("proc/sys");
        CreateDir("proc/sys/kernel");
        CreateDir("proc/sys/kernel/random");
        CreateFile("proc/sys/kernel/random/uuid");
	}
	~GlobalFileTableTree(){
		delete free_page_bitmap;
		RecursiveRemoveDir(root);
	}

	Node* CreateDir(std::string name);
	Node* CreateFile(std::string name);
	Node* Link(std::string src, std::string target);
	int DeleteDir(std::string name);
	int DeleteFile(std::string name);
	int Lock(std::string name, bool lock);

	Node* GetNode(std::string name);
	int FreeAllocatedPage(Node* node);
	// go to the target directory. and return the target directory node.
	Node* DirectoryTraverse(const std::string path, bool isCreate);
	Node* FindChild(Node* dir, std::string name);
	bool RecursiveRemoveDir(Node* cur);

	bool print(Node* cur, std::string indent);
	void printAll();

	std::string cwd;
private:
	Node* root;
	size_t page_size;


};

} // namespace rocksdb
