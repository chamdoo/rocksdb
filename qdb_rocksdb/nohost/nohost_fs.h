#include <iostream>
#include <list>
#include <string>
#include <vector>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdint.h>
#include <errno.h>
#include <stdio.h>

#include "nohost_global_file_table.h"

namespace rocksdb{

struct OpenFileEntry{
	Node* node;
	off_t r_offset;
	off_t w_offset;
	std::list<Node*>::iterator entry_list_iter;
	OpenFileEntry(Node* node_, off_t r_offset_, off_t w_offset_){
		this->node =node_;
		this->r_offset =r_offset_;
		this->w_offset =w_offset_;
		if(!node->isfile)
			this->entry_list_iter = node->children->begin();
	}
};

class NoHostFs{
private:
	std::vector<OpenFileEntry*>* open_file_table;
	int flash_fd;
	uint64_t page_size;
    int nopen, nclose, nread, nwrite, ndelete, ncreate;
    uint64_t tdrs;
    uint64_t tdws;

public:
	GlobalFileTableTree* global_file_tree; // it must be private!!!!

	NoHostFs(uint64_t assign_size){
		global_file_tree = new GlobalFileTableTree(assign_size);
		open_file_table = new std::vector<OpenFileEntry*>();
		flash_fd = open("flash.db", O_CREAT | O_RDWR | O_TRUNC, 0666);
		this->page_size = assign_size;
        nopen = nclose = nread = nwrite = ndelete = ncreate = 0;
        tdrs = 0;
        tdws = 0;
	}
	~NoHostFs(){
		delete global_file_tree;
		for(uint64_t i =0; i < open_file_table->size(); i++){
			if(open_file_table->at(i) != NULL)
				delete open_file_table->at(i);
		}
		close(flash_fd);
//	unlink("flash.db");
	}
	uint64_t GetFreeBlockAddress();
	int Open(std::string name, char type);
	ssize_t Write(int fd, const char* buf, uint64_t size);
	ssize_t Write(int fd, char* buf, uint64_t size);
	ssize_t Read(int fd, char* buf, uint64_t size);
	ssize_t ReadHelper(int fd, char* buf, uint64_t size);
	uint64_t SequentialRead(int fd, char* buf, uint64_t size);
	ssize_t Pread(int fd, char* buf, uint64_t size, uint64_t absolute_offset);
	ssize_t Pwrite(int fd, const char* buf, uint64_t size, uint64_t absolute_offset);

	int Close(int fd);
	int Rename(std::string old_name, std::string name);
	int Access(std::string name);
	Node* ReadDir(int fd);
	int DeleteFile(std::string name);
	int DeleteDir(std::string name);
	int CreateFile(std::string name);
	int CreateDir(std::string name);
	bool DirExists(std::string name);
	uint64_t GetFileSize(std::string name);
	ssize_t GetFileModificationTime(std::string name);
	bool Link(std::string src, std::string target);
	bool IsEof(int dfd);
	off_t Lseek(int fd, off_t n);
	int Free(Node* node);
	int Lock(std::string name, bool lock);
	std::string GetAbsolutePath();
    int  GetFd();

};


}// namespace rocksdb
