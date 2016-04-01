#include <iostream>
#include <list>
#include <string>
#include <vector>

namespace rocksdb{

struct FileSegInfo{
	unsigned int start_address;
	unsigned int size;
	FileSegInfo(unsigned int start_address, unsigned int size){
		this->start_address =start_address;
		this->size =size;
	}
};

class Node{
public:
	Node(std::string name, bool isfile, Node* parent) :
		children(NULL), file_info(NULL){
		this->name =name;
		this->isfile = isfile;
		this->parent = parent;
		if(isfile) file_info = new std::vector<FileSegInfo*>();
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
	unsigned int GetSize();
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
