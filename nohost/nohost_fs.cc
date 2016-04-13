#include "nohost_fs.h"
#include <string.h>

namespace rocksdb{

int NoHostFs::Lock(std::string name, bool lock){
	////printf("NoHostFs::Lock::name:%s\n", name.c_str());
	return global_file_tree->Lock(name, lock);
}

int NoHostFs::Close(int fd){
	if(fd < 0 || (int)open_file_table->size() <= fd){
		//errno = EBADF; // Bad file number
		return -1;
	}
	OpenFileEntry* entry = open_file_table->at(fd);
	if(entry == NULL){
		//errno = EBADF; // Bad file number
		return -1;
	}
	delete entry;
	open_file_table->at(fd) = NULL;

	return 0;
}
int NoHostFs::Access(std::string name){
	////printf("NoHostFs::Access::name:%s\n", name.c_str());
	Node* node = global_file_tree->GetNode(name);
	if(node == NULL){
		errno = ENOENT; // No such file or directory
		return -1;
	}
	return 0;
}
int NoHostFs::Rename(std::string old_name, std::string name){
	////printf("NoHostFs::Rename::%s, %s\n", old_name.c_str(), name.c_str());

	Node* old_node = global_file_tree->GetNode(old_name);
	std::vector<std::string> path_list = split(name, '/');
	if(old_node == NULL){
		errno = ENOENT; // No such file or directory
		return -1;

	}
	Node* new_node = global_file_tree->GetNode(name);
	if(new_node == NULL){
		*(old_node->name) = path_list.back();
		return 0;
	}
	else{
		if(new_node->isfile){
			if(DeleteFile(name) != 0) return -1;
			*(old_node->name) = path_list.back();
			return 0;
		}
		else{
			errno = EISDIR; // is a directory
			return -1;
		}
	}

}
Node* NoHostFs::ReadDir(int fd){
	////printf("NoHostFs::ReadDir::name:%d\n", fd);
	if(fd < 0 || (int)open_file_table->size() <= fd){
		errno = EBADF; // Bad file number
		return NULL;
	}
	OpenFileEntry* entry = open_file_table->at(fd);
	if(entry == NULL){
		errno = EBADF; // Bad file number
		return NULL;
	}
	if(entry->node->isfile){
		errno = ENOTDIR; // not a directory
		return NULL;
	}

	Node* ret = NULL;
	if(entry->entry_list_iter == entry->node->children->end())
		return ret;
	else
		ret = *(entry->entry_list_iter);

	entry->entry_list_iter++;

	return ret;
}
int NoHostFs::DeleteFile(std::string name){
	////printf("NoHostFs::DeleteFile::name:%s\n", name.c_str());
	return global_file_tree->DeleteFile(name);
}
int NoHostFs::DeleteDir(std::string name){
	////printf("NoHostFs::DeleteDir::name:%s\n", name.c_str());
	return global_file_tree->DeleteDir(name);
}
int NoHostFs::CreateDir(std::string name){
//	//printf("Enter NoHostFs::CreateDir(%s)\n", name.c_str());
	if(global_file_tree->CreateDir(name) == NULL)
		return -1;
	return 0;
}
int NoHostFs::CreateFile(std::string name){
	////printf("Enter NoHostFs::CreateFile(%s)\n", name.c_str());
	Node* newfile = NULL;
	if((newfile = global_file_tree->CreateFile(name)) == NULL)
		return -1;
	size_t start_address = GetFreeBlockAddress();
	newfile->file_info->push_back(new FileSegInfo(start_address, 0));
	return 0;
}
bool NoHostFs::DirExists(std::string name){
	////printf("NoHostFs::DirExists::name:%s\n", name.c_str());
	Node* node = global_file_tree->GetNode(name);
	if(node == NULL){
		errno = ENOENT; // No such file or directory
		return false;
	}
	if(node->isfile){
		errno = ENOTDIR; // Not a directory
		return false;
	}
	return true;
}
int NoHostFs::GetFileSize(std::string name){
	////printf("NoHostFs::GetFileSize::name:%s\n", name.c_str());
	Node* node = global_file_tree->GetNode(name);
	if(node == NULL){
		errno = ENOENT; // No such file or directory
		return -1;
	}
	return node->GetSize();
}
long int NoHostFs::GetFileModificationTime(std::string name){
	////printf("NoHostFs::GetFileModificationTime::name:%s\n", name.c_str());
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
	////printf("NoHostFs::IsEof::fd:%d\n", fd);
	if(fd < 0 || (int)open_file_table->size() <= fd){
		errno = EBADF; // Bad file number
		return -1;
	}
	OpenFileEntry* entry = open_file_table->at(fd);
	if(entry == NULL){
		errno = EBADF; // Bad file number
		return false;
	}
	if((size_t)(entry->r_offset) == entry->node->GetSize())
		return true;
	else
		return false;
}
off_t NoHostFs::Lseek(int fd, off_t n){
	////printf("NoHostFs::Lseek::name:%d\n", fd);
	if(fd < 0 || (int)open_file_table->size() <= fd){
		errno = EBADF; // Bad file number
		return -1;
	}
	OpenFileEntry* entry = open_file_table->at(fd);
	if(entry == NULL){
		errno = EBADF; // Bad file number
		return -1;
	}

	entry->r_offset = (entry->r_offset + n) % entry->node->GetSize();
	entry->w_offset = (entry->w_offset + n) % entry->node->GetSize();
	return entry->r_offset;
}



int NoHostFs::Open(std::string name, char type){
	////printf("NoHostFs::Open::name:%s\n", name.c_str());
	Node* ret = NULL;
	size_t start_address;

	ret = global_file_tree->GetNode(name);

	switch(type){
	case 'd' :
		if(ret == NULL){
			errno = ENOENT; // No such file or directory
			return -1;
		}
		global_file_tree->cwd = name;
		break;
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
		if(ret == NULL) return -1;
		start_address = GetFreeBlockAddress();
		ret->file_info->push_back(new FileSegInfo(start_address, 0));
		break;
	case 'a' :
		if(ret == NULL){
			ret = global_file_tree->CreateFile(name);
			if(ret == NULL) return -1;
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

long int NoHostFs::Write(int fd, const char* buf, size_t size){
	if(fd < 0 || (int)open_file_table->size() <= fd){
		errno = EBADF; // Bad file number
		return -1;
	}
	OpenFileEntry* entry = open_file_table->at(fd);
	FileSegInfo* finfo = NULL;
	ssize_t wsize = 0;
	if(entry == NULL){
		errno = EBADF; // Bad file number
		return -1;
	}

	size_t start_page = entry->w_offset / page_size;
	size_t last_page = (entry->w_offset + size - 1) / page_size;
	off_t offset = entry->w_offset % page_size;


	if(entry->node->file_info->size() - 1 < start_page){
		printf("NoHostFs::Write: fileinfo size:%zu, last page:%zu", entry->node->file_info->size(), last_page);
		entry->node->file_info->push_back(new FileSegInfo(GetFreeBlockAddress(), 0));
	}

	finfo = entry->node->file_info->at(start_page);
	off_t offsize = lseek(flash_fd, (finfo->start_address +offset), SEEK_SET);
	if(offsize == -1){ std::cout << "lseek error\n"; return offsize; }

	if(page_size - offset < size){
		wsize = write(flash_fd, buf, page_size - offset);
		if(wsize < 0){ std::cout << "write error\n"; return wsize; }
		finfo->size += wsize;
		entry->w_offset += wsize;
	}
	else{
		wsize = write(flash_fd, buf, size);
		if(wsize < 0){ std::cout << "write error\n"; return wsize; }
		finfo->size += (size_t)wsize;
		entry->w_offset += (off_t)wsize;
	}

	entry->node->last_modified_time = GetCurrentTime();
	//printf("NoHostFs::Write::name:%s fd:%d, buffer_size:%zu ,written_size=%zu, start_offset=%zu\n",
	//		entry->node->name->c_str(), fd, size, wsize, (finfo->start_address +offset));
	return wsize;
}

long int NoHostFs::Write(int fd, char* buf, size_t size){
	if(fd < 0 || (int)open_file_table->size() <= fd){
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
	ssize_t wsize = 0;

	size_t start_page = entry->w_offset / page_size;
	size_t last_page = (entry->w_offset + size - 1) / page_size;
	size_t offset = entry->w_offset % page_size;


	if(entry->node->file_info->size() - 1 < start_page){
		printf("NoHostFs::Write: fileinfo size:%zu, last page:%zu", entry->node->file_info->size(), last_page);
		entry->node->file_info->push_back(new FileSegInfo(GetFreeBlockAddress(), 0));
	}

	finfo = entry->node->file_info->at(start_page);
	off_t offsize = lseek(flash_fd, (finfo->start_address +offset), SEEK_SET);
	if(offsize == -1){ std::cout << "lseek error\n"; return -1; }

	if(page_size - offset < size){
		wsize = write(flash_fd, curbuf, page_size - offset);
		if(wsize < 0){ std::cout << "write error\n"; return wsize; }
		finfo->size += (size_t)wsize;
		entry->w_offset += (off_t)wsize;
	}
	else{
		wsize = write(flash_fd, curbuf, size);
		if(wsize < 0){ std::cout << "write error\n"; return wsize; }
		finfo->size += (size_t)wsize;
		entry->w_offset += (off_t)wsize;
	}

	entry->node->last_modified_time = GetCurrentTime();
	//printf("NoHostFs::Write::name:%s fd:%d, buffer_size:%zu ,written_size=%zu, start_offset=%zu\n",
	//		entry->node->name->c_str(), fd, size, wsize, (finfo->start_address +offset));
	return wsize;
}

size_t NoHostFs::SequentialRead(int fd, char* buf, size_t size){
	if(fd < 0 || (int)open_file_table->size() <= fd){
		errno = EBADF; // Bad file number
		return -1;
	}
	OpenFileEntry* entry = open_file_table->at(fd);
	size_t viable_rsize = 0;

	if(entry == NULL){
		errno = EBADF; // Bad file number
		return -1;
	}

	if(entry->node->GetSize() - entry->r_offset == 0){
		return 0;
	}

	viable_rsize = entry->node->GetSize() - entry->r_offset;

	if(viable_rsize > size)
		viable_rsize = size;
	char* tmp = buf;

	size_t left = viable_rsize;
	ssize_t done = 0;
	size_t sum = 0;
	while (left != 0) {
		done = ReadHelper(fd, tmp, left);
		if (done < 0) return 0;
		left -= done;
		sum += done;
        tmp += done;
	}

	////printf("NoHostFs::SequentialRead:: fd:%d, buffer_size:%zu ,readed_size=%zu\n", fd, size, sum);
	return sum;
}

long int NoHostFs::ReadHelper(int fd, char* buf, size_t size){
	OpenFileEntry* entry = open_file_table->at(fd);
	FileSegInfo* finfo = NULL;
	ssize_t rsize = 0;


	size_t start_page = entry->r_offset / page_size;
	size_t last_page = (entry->r_offset + size - 1) / page_size;
	size_t offset = entry->r_offset % page_size;
	if(last_page < start_page) return 0;


	finfo = entry->node->file_info->at(start_page);
	rsize = lseek(flash_fd, (finfo->start_address + offset), SEEK_SET);
	if(rsize < 0){ std::cout << "lseek error\n"; return -1; }
	if(page_size - offset < size){
		rsize = read(flash_fd, buf, page_size - offset);
		if(rsize < 0){ std::cout << "read error\n"; return rsize; }
		entry->r_offset += (off_t)rsize;
	}
	else{
		rsize = read(flash_fd, buf, size);
		if(rsize < 0){ std::cout << "read error\n"; return rsize; }
		entry->r_offset += (off_t)rsize;
	}
	return rsize;

}

long int NoHostFs::Pread(int fd, char* buf, size_t size, uint64_t absolute_offset){
	if(fd < 0 || (int)open_file_table->size() <= fd){
		errno = EBADF; // Bad file number
		return -1;
	}
	OpenFileEntry* entry = open_file_table->at(fd);
	FileSegInfo* finfo = NULL;
	ssize_t rsize = 0;
	if(entry == NULL){
		errno = EBADF; // Bad file number
		return -1;
	}
	//printf("NoHostFs::Pread::%s\n", entry->node->name->c_str());
	if(entry->node->GetSize() <= absolute_offset){
		errno =EFAULT;
		return -1;
	}
	uint64_t viable_size = entry->node->GetSize() - absolute_offset;
	if(viable_size > size)
		viable_size = size;
	////printf("viable size = total file size:%zu - bsolute_offset:%zu\n", entry->node->GetSize(), absolute_offset);


	size_t start_page = absolute_offset / page_size;
	size_t last_page = (absolute_offset + viable_size - 1) / page_size;
	size_t offset = absolute_offset % page_size;
	if(last_page < start_page) return 0;

	//printf("start_page=%zu, last_page=%zu, offset=%zu, page_size=%zu \n", start_page, last_page, offset, page_size);


	finfo = entry->node->file_info->at(start_page);

	rsize = lseek(flash_fd, (off_t)(finfo->start_address + offset), SEEK_SET);
	//printf("NoHostFs::Pread:: lseek(%d, %zu, SEEK_SET)\n", flash_fd, (off_t)(finfo->start_address + offset));
	if(rsize < 0){ std::cout << "lseek error\n"; return -1; }
	if(page_size - offset < viable_size){
		//printf("NoHostFs::Pread:: read(%d, %s, %zu), start_offset=%zu\n",
		//		flash_fd, buf, (size_t)(page_size - offset), (finfo->start_address + offset));
		rsize = read(flash_fd, buf, (size_t)(page_size - offset));
		if(rsize < 0){
			return rsize;
		}
	}
	else{
		//printf("NoHostFs::Pread:: read(%d, %s, %zu), start_offset=%zu\n",
		//		flash_fd, buf, (size_t)(viable_size), (finfo->start_address + offset));
		rsize = read(flash_fd, buf, viable_size);
		if(rsize < 0){
			return rsize;
		}
	}

	//////printf("NoHostFs::Pread:: read(%d, %s, %zu), start_offset=%zu\n", flash_fd, buf, viable_size, (finfo->start_address + offset));
	return rsize;
}

long int NoHostFs::Read(int fd, char* buf, size_t size){
	if(fd < 0 || (int)open_file_table->size() <= fd){
		errno = EBADF; // Bad file number
		return -1;
	}
	OpenFileEntry* entry = open_file_table->at(fd);
	FileSegInfo* finfo = NULL;
	ssize_t rsize = 0;
	if(entry == NULL){
		errno = EBADF; // Bad file number
		return -1;
	}

	size_t start_page = entry->r_offset / page_size;
	size_t last_page = (entry->r_offset + size - 1) / page_size;
	size_t offset = entry->r_offset % page_size;


	if((entry->node->file_info->size() - 1) < last_page){
		std::cout << "file data doesn't exist\n";
		return -1;
	}

	finfo = entry->node->file_info->at(start_page);
	rsize = lseek(flash_fd, (size_t)(finfo->start_address + offset), SEEK_SET);
	if(rsize < 0){ std::cout << "lseek error\n"; return -1; }
	if(page_size - offset < size){
		rsize = read(flash_fd, buf, page_size - (size_t)offset);
		if(rsize < 0){ std::cout << "read error\n"; return rsize; }
		entry->r_offset += (off_t)rsize;
	}
	else{
		rsize = read(flash_fd, buf, size);
		if(rsize < 0){ std::cout << "read error\n"; return rsize; }
		entry->r_offset += (off_t)rsize;
	}


	////printf("NoHostFs::Read:: fd:%d, buffer_size:%zu ,readed_size=%zu\n", fd, size, rsize);
	return rsize;
}

size_t NoHostFs::GetFreeBlockAddress(){
	size_t i;

	////printf("NoHostFs::GetFreeBlockAddress\n");
	for(i = 0; i < global_file_tree->free_page_bitmap->size(); i++)
		if(global_file_tree->free_page_bitmap->at(i) == 0) break;

	if(i == global_file_tree->free_page_bitmap->size())
		global_file_tree->free_page_bitmap->push_back(0);

	global_file_tree->free_page_bitmap->at(i) = 1;

/*	size_t j;
	printf("==========================================NoHostFs::GetFreeBlockAddress=======================================================\n");
	for(j = 0; j < global_file_tree->free_page_bitmap->size(); j++){
			printf("%zu th : %d ,  start address : %zu\n", j, global_file_tree->free_page_bitmap->at(i), j*page_size);
	}*/
	printf("==========================================NoHostFs::GetFreeBlockAddress=======================================================\n");
	global_file_tree->printAll();
	return i*page_size;
}
std::string NoHostFs::GetAbsolutePath(){
	return global_file_tree->cwd;
}



}
