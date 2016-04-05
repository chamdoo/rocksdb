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
uint64_t GetCurrentTime();

struct FileSegInfo{
	uint64_t start_address;
	uint64_t size;
	FileSegInfo(uint64_t start_address_, uint64_t size_){
		this->start_address =start_address_;
		this->size =size_;
	}
};

class Node{
public:
	Node(std::string name_, bool isfile_, Node* parent_) :
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
	}
	Node* parent;
	std::string name;
	bool isfile;
	uint64_t last_modified_time;
	std::list<Node*>* children;
	std::vector<FileSegInfo*>* file_info;
	int link_count;
	uint64_t GetSize();
	static uint64_t page_size;
	static std::vector<unsigned char>* free_page_bitmap;
	bool lock;
};

class GlobalFileTableTree{
public:
	std::vector<unsigned char>* free_page_bitmap; // until now, it associates a char with a page

	GlobalFileTableTree(uint64_t page_size_){
		free_page_bitmap = new std::vector<unsigned char>();
		free_page_bitmap->push_back(0);
		root = new Node("", false, NULL);
		page_size = page_size_;
		CreateDir("tmp");
		CreateDir("rocksdbtest-1000");
		CreateDir("db_test");
		CreateFile("LOG");
		CreateDir("usr");
		CreateDir("var");
		CreateDir("sbin");
		CreateDir("srv");
		CreateDir("sys");
		CreateDir("proc");
		CreateDir("root");
		CreateDir("run");
		CreateDir("media");
		CreateDir("mnt");
		CreateDir("opt");
		CreateDir("lib32");
		CreateDir("lib64");
		CreateDir("dev");
		CreateDir("etc");
		CreateDir("home");
		CreateDir("cdrom");
		CreateDir("boot");
		CreateDir("bin");
	}
	~GlobalFileTableTree(){
		delete free_page_bitmap;
		RecursiveRemoveDir(root);
	}

	Node* CreateDir(std::string name);
	Node* CreateFile(std::string name);
	Node* Link(std::string src, std::string target);
	bool DeleteDir(std::string name);
	bool DeleteFile(std::string name);
	int Lock(std::string name, bool lock);

	Node* GetNode(std::string name);
	int FreeAllocatedPage(Node* node);
	// go to the target directory. and return the target directory node.
	Node* DirectoryTraverse(const std::string path, bool isCreate);
	Node* FindChild(Node* dir, std::string name);
	bool RecursiveRemoveDir(Node* cur);

	bool print(Node* cur, std::string indent);
	void printAll();
private:
	Node* root;
	uint64_t page_size;


};

} // namespace rocksdb
