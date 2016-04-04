#include <iostream>
#include <list>
#include <string>
#include <vector>
#include <stdint.h>

namespace rocksdb{

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
	}
	~Node(){
		if(isfile) delete file_info;
		else delete children;
	}
	Node* parent;
	std::string name;
	bool isfile;
	std::list<Node*>* children;
	std::vector<FileSegInfo*>* file_info;
	uint64_t GetSize();
};

class GlobalFileTableTree{
public:
	GlobalFileTableTree() : cwd(NULL){
		root = new Node("", false, NULL);
	}
	~GlobalFileTableTree(){
		RecursiveRemoveDir(root);
	}

	bool CreateDir(std::string name);
	bool CreateFile(std::string name);
	bool DeleteDir(std::string name);
	bool DeleteFile(std::string name);

	Node* GetNode(std::string name);
	// go to the target directory. and return the target directory node.
	Node* DirectoryTraverse(const std::string path, bool isCreate);
	Node* FindChild(Node* dir, std::string name);
	bool RecursiveRemoveDir(Node* cur);

	bool print(Node* cur, std::string indent);
	void printAll();
private:
	Node* root;
	// before you start all methods, you must set cwd using DicrectoryTraverse().
	Node* cwd;


};

std::vector<std::string> &split(const std::string &s, char delim, std::vector<std::string> &elems);
std::vector<std::string> split(const std::string &s, char delim);

} // namespace rocksdb
