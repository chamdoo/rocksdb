#include <iostream>
#include <list>
#include <string>
#include <vector>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include "nohost_global_file_table.h"

namespace rocksdb{

struct OpenFileEntry{
	Node* node;
	unsigned int offset;
	OpenFileEntry(Node* node, unsigned int offset){
		this->node =node;
		this->offset =offset;
	}
};

class NoHostFs{
private:
	std::vector<OpenFileEntry*>* open_file_table;
	std::vector<unsigned char>* free_page_bitmap; // until now, it associates a char with a page
	int flash_fd;
	int page_size;

public:
	GlobalFileTableTree* global_file_tree; // it must be private!!!!
	NoHostFs(unsigned int assign_size){
		global_file_tree = new GlobalFileTableTree();
		open_file_table = new std::vector<OpenFileEntry*>();
		free_page_bitmap = new std::vector<unsigned char>();
		free_page_bitmap->push_back(0);
		std::cout << free_page_bitmap->size() << std::endl;
		flash_fd = open("flash.db", O_CREAT | O_RDWR | O_TRUNC, 0666);
		this->page_size = assign_size;
	}
	~NoHostFs(){
		delete global_file_tree;
		for(unsigned int i =0; i < open_file_table->size(); i++){
			delete open_file_table->at(i);
		}
		close(flash_fd);
	    unlink("flash.db");
	}
	unsigned int GetFreeBlockAddress();
	int Open(std::string name, char type);
	int Write(int fd, char* buf, unsigned int size);
	int Read(int fd, char* buf, unsigned int size);




};


}// namespace rocksdb
