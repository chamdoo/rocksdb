#include "nohost_fs.h"
#include <string.h>

namespace rocksdb{

int NoHostFs::Lock(std::string name, bool lock){
	return global_file_tree->Lock(name, lock);
}

int NoHostFs::Close(int fd){
	if(fd < 0){
		errno = EBADF; // Bad file number
		return -1;
	}
	OpenFileEntry* entry = open_file_table->at(fd);
	if(entry == NULL){
		errno = EBADF; // Bad file number
		return -1;
	}
	delete entry;
	open_file_table->at(fd) = NULL;

	return 1;
}
int NoHostFs::Access(std::string name){
	Node* node = global_file_tree->GetNode(name);
	if(node == NULL){
		errno = ENOENT; // No such file or directory
		return -1;
	}
	return 1;
}
int NoHostFs::Rename(std::string old_name, std::string name){
	Node* node = global_file_tree->GetNode(old_name);
	if(node == NULL){
		errno = ENOENT; // No such file or directory
		return -1;
	}
/*	if(node == NULL){
		node = global_file_tree->CreateDir(name);
		if(node == NULL) return -1;
	}*/
	std::vector<std::string> path_list = split(name, '/');
	node->name = path_list.back();

	return 1;
}
Node* NoHostFs::ReadDir(int fd){
	if(fd < 0){
		errno = EBADF; // Bad file number
		return NULL;
	}
	OpenFileEntry* entry = open_file_table->at(fd);
	if(entry == NULL){
		errno = EBADF; // Bad file number
		return NULL;
	}
	uint64_t loop_count = 0;
	if(entry->node->isfile){
		errno = ENOTDIR; // not a directory
		return NULL;
	}
	std::list<Node*>::iterator iter = entry->node->children->begin();

	while(loop_count < entry->r_offset){
		iter++;
		loop_count++;
	}
	entry->r_offset++;
	if(iter == entry->node->children->end())
		return NULL;

	return *iter;
}
int NoHostFs::DeleteFile(std::string name){
	return global_file_tree->DeleteFile(name);
}
int NoHostFs::DeleteDir(std::string name){
	return global_file_tree->DeleteDir(name);
}
int NoHostFs::CreateDir(std::string name){
	printf("Enter NoHostFs::CreateDir(%s)\n", name.c_str());
	if(global_file_tree->CreateDir(name) == NULL)
		return -1;
	return 1;
}
int NoHostFs::CreateFile(std::string name){
	printf("Enter NoHostFs::CreateFile(%s)\n", name.c_str());
	if(global_file_tree->CreateFile(name) == NULL)
		return -1;
	return 1;
}
bool NoHostFs::DirExists(std::string name){
	if(Access(name) == -1) return false;
	return true;
}
int NoHostFs::GetFileSize(std::string name){
	Node* node = global_file_tree->GetNode(name);
	if(node == NULL){
		errno = ENOENT; // No such file or directory
		return -1;
	}
	return node->GetSize();
}
uint64_t NoHostFs::GetFileModificationTime(std::string name){
	Node* node = global_file_tree->GetNode(name);
	if(node == NULL){
		errno = ENOENT; // No such file or directory
		return -1;
	}
	return node->last_modified_time;
}
bool NoHostFs::Link(std::string src, std::string target){
	Node* node = global_file_tree->Link(src, target);
	if(node == NULL) return false;
	return true;
}
bool NoHostFs::IsEof(int fd){
	if(fd < 0){
		errno = EBADF; // Bad file number
		return false;
	}
	OpenFileEntry* entry = open_file_table->at(fd);
	if(entry == NULL){
		errno = EBADF; // Bad file number
		return false;
	}
	if(entry->r_offset == entry->node->GetSize())
		return true;
	else
		return false;
}
int NoHostFs::Lseek(int fd, uint64_t n){
	if(fd < 0){
		errno = EBADF; // Bad file number
		return -1;
	}
	OpenFileEntry* entry = open_file_table->at(fd);
	if(entry == NULL){
		errno = EBADF; // Bad file number
		return -1;
	}
	entry->r_offset = n;
	entry->w_offset = n;
	return entry->r_offset;
}



int NoHostFs::Open(std::string name, char type){
	Node* ret = NULL;
	uint64_t start_address;

	ret = global_file_tree->GetNode(name);

	switch(type){
	case 'd' :
	case 'r' :
		if(ret == NULL){
			errno = ENOENT; // No such file or directory
			return -1;
		}
		break;
	case 'w' :
		if(ret != NULL)
			global_file_tree->DeleteFile(name);

		ret = global_file_tree->CreateFile(name);
		start_address = GetFreeBlockAddress();
		ret->file_info->push_back(new FileSegInfo(start_address, 0));
		break;
	case 'a' :
		if(ret == NULL){
			ret = global_file_tree->CreateFile(name);
			start_address = GetFreeBlockAddress();
			ret->file_info->push_back(new FileSegInfo(start_address, 0));
		}
		break;
	default:
		std::cout << "r:readonly, w:read/write, you must choose one of them\n";
		errno = EINVAL; // invalid argument
		return -1;

	}
	open_file_table->push_back(new OpenFileEntry(ret, 0, 0));

	return open_file_table->size() - 1;
}

int NoHostFs::Write(int fd, const char* buf, uint64_t size){
	if(fd < 0){
		errno = EBADF; // Bad file number
		return -1;
	}
	OpenFileEntry* entry = open_file_table->at(fd);
	FileSegInfo* finfo = NULL;
	int wsize = 0;
	if(entry == NULL){
		errno = EBADF; // Bad file number
		return -1;
	}

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

	entry->node->last_modified_time = GetCurrentTime();
	return wsize;
}

int NoHostFs::Write(int fd, char* buf, uint64_t size){
	if(fd < 0){
		errno = EBADF; // Bad file number
		return -1;
	}
	OpenFileEntry* entry = open_file_table->at(fd);
	if(entry == NULL){
		errno = EBADF; // Bad file number
		return -1;
	}
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

	entry->node->last_modified_time = GetCurrentTime();
	return wsize;
}

int NoHostFs::SequentialRead(int fd, char* buf, uint64_t size){
	if(fd < 0){
		errno = EBADF; // Bad file number
		return -1;
	}
	std::string data = "";
	char* tmp = new char[page_size];
	int left = size;
	int done = 0;
	int sum = 0;
	while (left != 0) {
		done = ReadHelper(fd, tmp, left);
		if (done < 0) return -1;
		if(tmp != NULL) data += tmp;
		left -= done;
		sum += done;
	}

	strcpy(buf, data.data());
	return sum;
}

int NoHostFs::ReadHelper(int fd, char* buf, uint64_t size){
	if(fd < 0){
		errno = EBADF; // Bad file number
		return -1;
	}
	OpenFileEntry* entry = open_file_table->at(fd);
	FileSegInfo* finfo = NULL;
	int rsize = 0;
	if(entry == NULL){
		errno = EBADF; // Bad file number
		return -1;
	}

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
	if(fd < 0){
		errno = EBADF; // Bad file number
		return -1;
	}
	OpenFileEntry* entry = open_file_table->at(fd);
	FileSegInfo* finfo = NULL;
	int rsize = 0;
	if(entry == NULL){
		errno = EBADF; // Bad file number
		return -1;
	}

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

	for(i = 0; i < global_file_tree->free_page_bitmap->size(); i++)
		if(global_file_tree->free_page_bitmap->at(i) == 0) break;

	if(i == global_file_tree->free_page_bitmap->size())
		global_file_tree->free_page_bitmap->push_back(0);

	global_file_tree->free_page_bitmap->at(i) = 1;
	return i*page_size;
}

}
