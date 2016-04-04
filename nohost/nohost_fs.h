#include <iostream>
#include <list>
#include <string>
#include <vector>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdint.h>

#include "nohost_global_file_table.h"

namespace rocksdb{

struct OpenFileEntry{
	Node* node;
	uint64_t r_offset;
	uint64_t w_offset;
	OpenFileEntry(Node* node_, uint64_t r_offset_, uint64_t w_offset_){
		this->node =node_;
		this->r_offset =r_offset_;
		this->w_offset =w_offset_;
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
	NoHostFs(uint64_t assign_size){
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
		for(uint64_t i =0; i < open_file_table->size(); i++){
			delete open_file_table->at(i);
		}
		close(flash_fd);
		//unlink("flash.db");
	}
	uint64_t GetFreeBlockAddress();
	int Open(std::string name, char type);
	int Write(int fd, const char* buf, size_t size);
	int Write(int fd, char* buf, size_t size);
	int Read(int fd, char* buf, size_t size);
	int ReadHelper(int fd, char* buf, size_t size);
	int SequentialRead(int fd, char* buf, size_t size);


	int Close(int fd);
	int Rename(std::string old_name, std::string name);
	int Access(std::string name);
	Node* ReadDir(int fd);
	int DeleteFile(std::string name);
	int DeleteDir(std::string name);
	int CreateDir(std::string name);
	bool DirExists(std::string name);
	int GetFileSize(std::string name);
	int GetFileModificationTime(std::string name);
	bool Link(std::string src, std::string target);
	bool IsEof(int dfd);
	int Lseek(int fd, uint64_t n);

};


}// namespace rocksdb
