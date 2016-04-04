#include "nohost_fs.h"
#include <string.h>

namespace rocksdb{
int NoHostFs::Close(int fd) {return 0;}
int NoHostFs::Rename(std::string old_name, std::string name) {return 0;}
int NoHostFs::Access(std::string name) {return 0;}
Node* NoHostFs::ReadDir(int fd) {return 0;}
int NoHostFs::DeleteFile(std::string name) {return 0;}
int NoHostFs::DeleteDir(std::string name) {return 0;}
int NoHostFs::CreateDir(std::string name) {return 0;}
bool NoHostFs::DirExists(std::string name) {return 0;}
int NoHostFs::GetFileSize(std::string name) {return 0;}
int NoHostFs::GetFileModificationTime(std::string name) {return 0;}
bool NoHostFs::Link(std::string src, std::string target) {return 0;}
bool NoHostFs::IsEof(int fd) {return 0;}
int NoHostFs::Lseek(int fd, uint64_t n) {return 0;}



int NoHostFs::Open(std::string name, char type){
	Node* ret = NULL;
	uint64_t start_address;

	switch(type){
	case 'd' :
		break;
	case 'r' :
		ret = global_file_tree->GetNode(name);
		break;
	case 'w' :
		global_file_tree->DeleteFile(name);
		global_file_tree->CreateFile(name);
		ret = global_file_tree->GetNode(name);
		start_address = GetFreeBlockAddress();
		ret->file_info->push_back(new FileSegInfo(start_address, 0));
		break;
	default:
		std::cout << "r:readonly, w:read/write, you must choose one of them\n";
	}
	open_file_table->push_back(new OpenFileEntry(ret, 0, 0));

	return open_file_table->size() - 1;
}

int NoHostFs::Write(int fd, const char* buf, uint64_t size){
	OpenFileEntry* entry = open_file_table->at(fd);
	FileSegInfo* finfo = NULL;
	int wsize = 0;

	uint64_t start_page = entry->w_offset / page_size;
	uint64_t last_page = (entry->w_offset + size - 1) / page_size;
	uint64_t offset = entry->w_offset % page_size;


	while(entry->node->file_info->size() - 1 < last_page){
		entry->node->file_info->push_back(new FileSegInfo(GetFreeBlockAddress(), 0));
	}

	finfo = entry->node->file_info->at(start_page);
	wsize = lseek(flash_fd, (finfo->start_address +offset), SEEK_SET);
	if(wsize < 0){ std::cout << "lseek error\n"; return wsize; }

	if(page_size - offset < size){
		wsize = write(flash_fd, buf, page_size - offset);
		if(wsize < 0){ std::cout << "write error\n"; return wsize; }
		finfo->size += wsize;
		entry->w_offset += wsize;
	}
	else{
		wsize = write(flash_fd, buf, size);
		if(wsize < 0){ std::cout << "write error\n"; return wsize; }
		finfo->size += wsize;
		entry->w_offset += wsize;
	}

	return wsize;
}

int NoHostFs::Write(int fd, char* buf, uint64_t size){
	OpenFileEntry* entry = open_file_table->at(fd);
	FileSegInfo* finfo = NULL;
	char* curbuf = buf;
	int wsize = 0;

	uint64_t start_page = entry->w_offset / page_size;
	uint64_t last_page = (entry->w_offset + size - 1) / page_size;
	uint64_t offset = entry->w_offset % page_size;


	while(entry->node->file_info->size() - 1 < last_page){
		entry->node->file_info->push_back(new FileSegInfo(GetFreeBlockAddress(), 0));
	}

	finfo = entry->node->file_info->at(start_page);
	wsize = lseek(flash_fd, (finfo->start_address +offset), SEEK_SET);
	if(wsize < 0){ std::cout << "lseek error\n"; return wsize; }

	if(page_size - offset < size){
		wsize = write(flash_fd, curbuf, page_size - offset);
		if(wsize < 0){ std::cout << "write error\n"; return wsize; }
		finfo->size += wsize;
		entry->w_offset += wsize;
	}
	else{
		wsize = write(flash_fd, curbuf, size);
		if(wsize < 0){ std::cout << "write error\n"; return wsize; }
		finfo->size += wsize;
		entry->w_offset += wsize;
	}

	return wsize;
}

int NoHostFs::SequentialRead(int fd, char* buf, uint64_t size){
	std::string data = "";
	char* tmp = new char[page_size];
	int left = size;
	int done = 0;
	int sum = 0;
	while (left != 0) {
		done = ReadHelper(fd, tmp, left);
		if (done < 0) return false;
		if(tmp != NULL) data += tmp;
		left -= done;
		sum += done;
	}
	strcpy(buf, data.data());
	return sum;
}

int NoHostFs::ReadHelper(int fd, char* buf, uint64_t size){
	OpenFileEntry* entry = open_file_table->at(fd);
	FileSegInfo* finfo = NULL;
	int rsize = 0;

	uint64_t start_page = entry->r_offset / page_size;
	uint64_t last_page = (entry->r_offset + size - 1) / page_size;
	uint64_t offset = entry->r_offset % page_size;

	if(entry->node->file_info->size() - 1 < last_page){
		std::cout << "file data doesn't exist\n";
		return -1;
	}

	finfo = entry->node->file_info->at(start_page);
	rsize = lseek(flash_fd, (finfo->start_address + offset), SEEK_SET);
	if(rsize < 0){ std::cout << "lseek error\n"; return rsize; }
	if(page_size - offset < size){
		rsize = read(flash_fd, buf, page_size - offset);
		if(rsize < 0){ std::cout << "read error\n"; return rsize; }
		entry->r_offset += rsize;
	}
	else{
		rsize = read(flash_fd, buf, size);
		if(rsize < 0){ std::cout << "read error\n"; return rsize; }
		entry->r_offset += rsize;
	}
	return rsize;

}

int NoHostFs::Read(int fd, char* buf, uint64_t size){
	OpenFileEntry* entry = open_file_table->at(fd);
	FileSegInfo* finfo = NULL;
	int rsize = 0;

	uint64_t start_page = entry->r_offset / page_size;
	uint64_t last_page = (entry->r_offset + size - 1) / page_size;
	uint64_t offset = entry->r_offset % page_size;


	if(entry->node->file_info->size() - 1 < last_page){
		std::cout << "file data doesn't exist\n";
		return -1;
	}

	finfo = entry->node->file_info->at(start_page);
	rsize = lseek(flash_fd, (finfo->start_address + offset), SEEK_SET);
	if(rsize < 0){ std::cout << "lseek error\n"; return rsize; }
	if(page_size - offset < size){
		rsize = read(flash_fd, buf, page_size - offset);
		if(rsize < 0){ std::cout << "read error\n"; return rsize; }
		entry->r_offset += rsize;
	}
	else{
		rsize = read(flash_fd, buf, size);
		if(rsize < 0){ std::cout << "read error\n"; return rsize; }
		entry->r_offset += rsize;
	}

	return rsize;
}

uint64_t NoHostFs::GetFreeBlockAddress(){
	uint64_t i;

	for(i = 0; i < free_page_bitmap->size(); i++)
		if(free_page_bitmap->at(i) == 0) break;

	if(i == free_page_bitmap->size())
		free_page_bitmap->push_back(0);

	free_page_bitmap->at(i) = 1;
	return i*page_size;
}

}
