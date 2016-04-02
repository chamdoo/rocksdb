#include "nohost_fs.h"
#include <string.h>

namespace rocksdb{

int NoHostFs::Open(std::string name, char type){
	Node* ret;
	unsigned int start_address;

	switch(type){
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

unsigned int NoHostFs::Write(int fd, char* buf, unsigned int size){
	OpenFileEntry* entry = open_file_table->at(fd);
	FileSegInfo* finfo = NULL;
	char* curbuf = buf;
	int wsize = 0;

	unsigned int start_page = entry->w_offset / page_size;
	unsigned int last_page = (entry->w_offset + size - 1) / page_size;
	unsigned int offset = entry->w_offset % page_size;


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

unsigned int NoHostFs::SequentialRead(int fd, char* buf, unsigned int size){
	std::string data = "";
	char* tmp = new char[page_size];
	int left = size;
	int done = 0;
	while (left != 0) {
		done = ReadHelper(fd, tmp, left);
		if (done < 0) return false;
		if(tmp != NULL) data += tmp;
		left -= done;
	}
	strcpy(buf, data.data());
	return done;
}

unsigned int NoHostFs::ReadHelper(int fd, char* buf, unsigned int size){
	OpenFileEntry* entry = open_file_table->at(fd);
	FileSegInfo* finfo = NULL;
	int rsize = 0;

	unsigned int start_page = entry->r_offset / page_size;
	unsigned int last_page = (entry->r_offset + size - 1) / page_size;
	unsigned int offset = entry->r_offset % page_size;

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

unsigned int NoHostFs::Read(int fd, char* buf, unsigned int size){
	OpenFileEntry* entry = open_file_table->at(fd);
	FileSegInfo* finfo = NULL;
	int rsize = 0;

	unsigned int start_page = entry->r_offset / page_size;
	unsigned int last_page = (entry->r_offset + size - 1) / page_size;
	unsigned int offset = entry->r_offset % page_size;


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

unsigned int NoHostFs::GetFreeBlockAddress(){
	unsigned int i;

	for(i = 0; i < free_page_bitmap->size(); i++)
		if(free_page_bitmap->at(i) == 0) break;

	if(i == free_page_bitmap->size())
		free_page_bitmap->push_back(0);

	free_page_bitmap->at(i) = 1;
	return i*page_size;
}

}
