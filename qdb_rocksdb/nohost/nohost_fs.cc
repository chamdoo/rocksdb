#include "nohost_fs.h"
#include <string.h>
#include <sys/mman.h>

namespace rocksdb{

std::string RecoverName(std::string name){
	std::vector<std::string> path_list = split(name, '/');
	std::vector<std::string>::iterator iter = path_list.begin();
	while(iter != path_list.end()){
		if(iter->compare("") == 0){
			path_list.erase(iter);
			continue;
		}
		iter++;
	}
	std::string re_name = "";
	iter = path_list.begin();
	while(iter != path_list.end()){
		re_name = re_name + "/" + (*iter);
		iter++;
	}
	if(re_name.substr(0, 2).compare("/.") == 0){
		char the_path[256];
		std::string cwd(getcwd(the_path, 256));
		re_name = cwd + re_name.substr(2, re_name.size()-2);
	}
	return re_name;
}

int NoHostFs::GetFd(){
	return flash_fd;
}

int NoHostFs::Lock(std::string name, bool lock){
	name = RecoverName(name);
	//printf("NoHostFs::Lock::name:%s\n", name.c_str());
	return global_file_tree->Lock(name, lock);
}

int NoHostFs::Close(int fd){
	if(fd < 0 || (int)open_file_table->size() <= fd){
		errno = EBADF; // Bad file number
		return -1;
	}
	OpenFileEntry* entry = open_file_table->at(fd);
	if(entry == NULL){
		errno = EBADF; // Bad file number
		return -1;
	}
	FILE* fp= fopen("open.txt", "a");
	fprintf(fp, "%s  %d\n",entry->node->name->c_str(),  ++nclose);
	fclose(fp);
	delete entry;
	open_file_table->at(fd) = NULL;

	return 0;
}
int NoHostFs::Access(std::string name){
	name = RecoverName(name);
	//printf("NoHostFs::Access::name:%s\n", name.c_str());
	Node* node = global_file_tree->GetNode(name);
	if(node == NULL){
		errno = ENOENT; // No such file or directory
		return -1;
	}
	return 0;
}
int NoHostFs::Rename(std::string old_name, std::string name){
	name = RecoverName(name);
	old_name = RecoverName(old_name);
	//printf("NoHostFs::Rename::%s, %s\n", old_name.c_str(), name.c_str());
	if(old_name.compare(name) == 0)
		return 0;

	Node* old_node = global_file_tree->GetNode(old_name);
	std::vector<std::string> path_list = split(name, '*');
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

	//printf("NoHostFs::ReadDir::name:%s\n", ret->name->c_str());
	return ret;
}
int NoHostFs::DeleteFile(std::string name){
	name = RecoverName(name);
	//printf("NoHostFs::DeleteFile::name:%s\n", name.c_str());
	FILE* fp= fopen("delete.txt", "a");
	fprintf(fp, "%s  %d\n",name.c_str(),  ++ndelete);
	fclose(fp);
	return global_file_tree->DeleteFile(name);
}
int NoHostFs::DeleteDir(std::string name){
	name = RecoverName(name);
	//printf("NoHostFs::DeleteDir::name:%s\n", name.c_str());
	return global_file_tree->DeleteDir(name);
}
int NoHostFs::CreateDir(std::string name){
	name = RecoverName(name);
	//printf("Enter NoHostFs::CreateDir(%s)\n", name.c_str());
	if(global_file_tree->CreateDir(name) == NULL)
		return -1;
	return 0;
}
int NoHostFs::CreateFile(std::string name){
	name = RecoverName(name);
	//printf("Enter NoHostFs::CreateFile(%s)\n", name.c_str());
	Node* newfile = NULL;
	if((newfile = global_file_tree->CreateFile(name)) == NULL)
		return -1;

	FILE* fp= fopen("create.txt", "a");
	fprintf(fp, "%s  %d\n",name.c_str(),  ++ncreate);
	fclose(fp);
	uint64_t start_address = GetFreeBlockAddress();
	newfile->file_info->push_back(new FileSegInfo(start_address, 0));
	return 0;
}
bool NoHostFs::DirExists(std::string name){
	name = RecoverName(name);
	//printf("NoHostFs::DirExists::name:%s\n", name.c_str());
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
uint64_t NoHostFs::GetFileSize(std::string name){
	name = RecoverName(name);
	Node* node = global_file_tree->GetNode(name);
	if(node == NULL){
		errno = ENOENT; // No such file or directory
		return -1;
	}
	//printf("NoHostFs::GetFileSize::name:%s size=%zu\n", name.c_str(), node->GetSize());
	return node->GetSize();
}
ssize_t NoHostFs::GetFileModificationTime(std::string name){
	name = RecoverName(name);
	//printf("NoHostFs::GetFileModificationTime::name:%s\n", name.c_str());
	Node* node = global_file_tree->GetNode(name);
	if(node == NULL){
		errno = ENOENT; // No such file or directory
		return -1;
	}
	return node->last_modified_time;
}

bool NoHostFs::Link(std::string src, std::string target){
	src = RecoverName(src);
	target = RecoverName(target);
	Node* node = global_file_tree->Link(src, target);
	if(node == NULL) return false;
	return true;
}
bool NoHostFs::IsEof(int fd){
	//printf("NoHostFs::IsEof::fd:%d\n", fd);
	if(fd < 0 || (int)open_file_table->size() <= fd){
		errno = EBADF; // Bad file number
		return -1;
	}
	OpenFileEntry* entry = open_file_table->at(fd);
	if(entry == NULL){
		errno = EBADF; // Bad file number
		return false;
	}
	if((uint64_t)(entry->r_offset) == entry->node->GetSize())
		return true;
	else
		return false;
}
off_t NoHostFs::Lseek(int fd, off_t n){
	//printf("NoHostFs::Lseek::name:%d\n", fd);
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
	name = RecoverName(name);
    //printf("NoHostFs::Open::name:%s\n", name.c_str());
	Node* ret = NULL;
	uint64_t start_address;

	ret = global_file_tree->GetNode(name);

	FILE* fp= fopen("open.txt", "a");
	fprintf(fp, "%s  %d\n",name.c_str(),  ++nopen);
	fclose(fp);
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
		if(ret != NULL){
			global_file_tree->DeleteFile(name);
			fp= fopen("delete.txt", "a");
			fprintf(fp, "%s  %d\n",name.c_str(),  ++ndelete);
			fclose(fp);
		}

		ret = global_file_tree->CreateFile(name);
		fp= fopen("create.txt", "a");
		fprintf(fp, "%s  %d\n",name.c_str(),  ++ncreate);
		fclose(fp);
		if(ret == NULL) return -1;
		start_address = GetFreeBlockAddress();
		ret->file_info->push_back(new FileSegInfo(start_address, 0));
		break;
	case 'a' :
		if(ret == NULL){
			ret = global_file_tree->CreateFile(name);
			fp= fopen("create.txt", "a");
			fprintf(fp, "%s  %d\n",name.c_str(),  ++ncreate);
			fclose(fp);
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

ssize_t NoHostFs::Write(int fd, const char* buf, uint64_t size){
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
	ssize_t wsize = 0;
	uint64_t start_page = entry->w_offset / page_size;
	//uint64_t last_page = (entry->w_offset + size - 1) / page_size;
	off_t offset = entry->w_offset % page_size;

	FILE* fp= fopen("write_trace.txt", "a");
	FILE* fp2 = fopen("rw_trace.txt", "a");

	if(entry->node->file_info->size() - 1 < start_page)
		entry->node->file_info->push_back(new FileSegInfo(GetFreeBlockAddress(), 0));

	finfo = entry->node->file_info->at(start_page);

	if(page_size - offset < size){
		wsize = pwrite(flash_fd, buf, page_size - offset ,(finfo->start_address +offset));
		if(wsize < 0){ std::cout << "write error\n"; return wsize; }
		finfo->size += (uint64_t)wsize;
		entry->w_offset += (off_t)wsize;
		entry->node->size += (uint64_t)wsize;
        fprintf(fp, "%s %zu %zu %zu\n",entry->node->name->c_str(), (finfo->start_address +offset), page_size - offset, entry->node->size);
        fprintf(fp2, "%s, %zu, ,%zu, %zu\n",entry->node->name->c_str(), (finfo->start_address +offset), page_size - offset, entry->node->size);
	}
	else{
		wsize = pwrite(flash_fd, buf, size, (finfo->start_address +offset));
		if(wsize < 0){ std::cout << "write error\n"; return wsize; }
		finfo->size += (uint64_t)wsize;
		entry->w_offset += (off_t)wsize;
		entry->node->size += (uint64_t)wsize;
        fprintf(fp, "%s %zu %zu %zu\n", entry->node->name->c_str(), (finfo->start_address +offset), size, entry->node->size);
        fprintf(fp2, "%s, %zu, ,%zu, %zu\n", entry->node->name->c_str(), (finfo->start_address +offset), size, entry->node->size);
	}
	fclose(fp);
	fclose(fp2);

	fp= fopen("no_write.txt", "a");
	fprintf(fp, "%s  %d\n",entry->node->name->c_str(),  ++nwrite);
	fclose(fp);

	fp= fopen("cdf_write.txt", "a");
	fprintf(fp, "%zu %zu\n", GetCurrentTime(), (tdws+=wsize));
	fclose(fp);
	fp= fopen("cdf_rw.txt", "a");
	fprintf(fp, "%zu %zu\n", (tdws), tdrs);
	fclose(fp);
	global_file_tree->printAll();
    //printf("NoHostFs::Write::name:%s fd:%d, buffer_size:%zu ,written_size=%zu, start_offset=%zu\n", entry->node->name->c_str(), fd, size, wsize, (finfo->start_address +offset));
	entry->node->last_modified_time = GetCurrentTime();
	return wsize;
}

ssize_t NoHostFs::Pwrite(int fd, const char* buf, size_t size, uint64_t absolute_offset){
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
	ssize_t wsize = 0;

	size_t start_page = absolute_offset / page_size;
	size_t offset = absolute_offset % page_size;

	FILE* fp= fopen("write_trace.txt", "a");
	FILE* fp2= fopen("rw_trace.txt", "a");

	if(entry->node->file_info->size() - 1 < start_page)
		entry->node->file_info->push_back(new FileSegInfo(GetFreeBlockAddress(), 0));

	finfo = entry->node->file_info->at(start_page);

	if(page_size - offset < size){
		wsize = pwrite(flash_fd, buf, page_size - offset, (finfo->start_address +offset));
		if(wsize < 0){ std::cout << "write error\n"; return wsize; }
		finfo->size += (uint64_t)wsize;
		entry->node->size += (off_t)wsize;
        fprintf(fp, "%s %zu %zu %zu\n",entry->node->name->c_str(), (finfo->start_address +offset), page_size - offset, entry->node->size);
        fprintf(fp2, "%s, %zu, ,%zu, %zu\n",entry->node->name->c_str(), (finfo->start_address +offset), page_size - offset, entry->node->size);
	}
	else{
		wsize = pwrite(flash_fd, buf, size, (finfo->start_address +offset));
		if(wsize < 0){ std::cout << "write error\n"; return wsize; }
		finfo->size += (uint64_t)wsize;
		entry->node->size += (off_t)wsize;
        fprintf(fp, "%s %zu %zu %zu\n", entry->node->name->c_str(), (finfo->start_address +offset), size, entry->node->size);
        fprintf(fp2, "%s, %zu, ,%zu, %zu\n", entry->node->name->c_str(), (finfo->start_address +offset), size, entry->node->size);
	}
	fclose(fp);
	fclose(fp2);

	fp= fopen("no_write.txt", "a");
	fprintf(fp, "%s  %d\n",entry->node->name->c_str(),  ++nwrite);
	fclose(fp);

	fp= fopen("cdf_write.txt", "a");
	fprintf(fp, "%zu %zu\n", GetCurrentTime(), (tdws+=wsize));
	fclose(fp);

	fp= fopen("cdf_rw.txt", "a");
	fprintf(fp, "%zu %zu\n", (tdws), tdrs);
	fclose(fp);
	global_file_tree->printAll();
	entry->node->last_modified_time = GetCurrentTime();
	return wsize;
}

ssize_t NoHostFs::Write(int fd, char* buf, uint64_t size){
	if(fd < 0 || (int)open_file_table->size() <= fd){
		errno = EBADF; // Bad file number
		return -1;
	}
	OpenFileEntry* entry = open_file_table->at(fd);
	if(entry == NULL){ errno = EBADF; // Bad file number
		return -1;
	}
FileSegInfo* finfo = NULL;
	char* curbuf = buf;
	ssize_t wsize = 0;

	uint64_t start_page = entry->w_offset / page_size;
	//uint64_t last_page = (entry->w_offset + size - 1) / page_size;
	uint64_t offset = entry->w_offset % page_size;

	if(entry->node->file_info->size() - 1 < start_page)
		entry->node->file_info->push_back(new FileSegInfo(GetFreeBlockAddress(), 0));

	finfo = entry->node->file_info->at(start_page);

	if(page_size - offset < size){
		wsize = pwrite(flash_fd, curbuf, page_size - offset, (finfo->start_address +offset));
		if(wsize < 0){ std::cout << "write error\n"; return wsize; }
        //fprintf(tfp, "%zu %zu\n", (finfo->start_address +offset), page_size - offset);
		finfo->size += (uint64_t)wsize;
		entry->w_offset += (off_t)wsize;
		entry->node->size += (off_t)wsize;
	}
	else{
		wsize = pwrite(flash_fd, curbuf, size, (finfo->start_address +offset));
		if(wsize < 0){ std::cout << "write error\n"; return wsize; }
      //  fprintf(tfp, "%zu %zu\n", (finfo->start_address +offset), size);
		finfo->size += (uint64_t)wsize;
		entry->w_offset += (off_t)wsize;
		entry->node->size += (off_t)wsize;
	}
//	fprintf(tfpnowrite, "%s  %d\n",entry->node->name->c_str(),  ++nwrite);
	entry->node->last_modified_time = GetCurrentTime();
	return wsize;
}

uint64_t NoHostFs::SequentialRead(int fd, char* buf, uint64_t size){
	if(fd < 0 || (int)open_file_table->size() <= fd){
        errno = EBADF; // Bad file number
        return -1;
	}
	OpenFileEntry* entry = open_file_table->at(fd);
	uint64_t viable_rsize = 0;

	if(entry == NULL){
		errno = EBADF; // Bad file number
		return -1;
	}
	//===================================================================
	std::string tname = *(entry->node->name);
	if(tname.compare("uuid") == 0){
		int fd2 = open("/proc/sys/kernel/random/uuid", O_RDONLY);
	    char tmpbuf[100];
	    ssize_t tsize=0;
	    if(fd2 != -1) tsize = read(fd2, tmpbuf, sizeof(tmpbuf));
	    if(tsize < 0) memcpy(buf, tmpbuf, tsize);
	    close(fd2);
	    return tsize;
	}
	//===================================================================

	if(entry->node->GetSize() - entry->r_offset <= 0){
		return 0;
	}

	viable_rsize = entry->node->GetSize() - entry->r_offset;

	if(viable_rsize > size)
		viable_rsize = size;
	char* temp = buf;

	uint64_t left = viable_rsize;
	ssize_t done = 0;
	uint64_t sum = 0;
	while (left != 0) {
		done = ReadHelper(fd, temp, left);
		if (done < 0){
			return -1;
		}
		left -= done;
		sum += done;
        temp += done;
	}

	FILE* fp= fopen("no_read.txt", "a");
	fprintf(fp, "%s  %d\n",entry->node->name->c_str(),  ++nread);
	fclose(fp);
	//printf("NoHostFs::SequentialRead:: fd:%d, buffer_size:%zu ,readed_size=%zu\n", fd, size, sum);
	return sum;
}

ssize_t NoHostFs::ReadHelper(int fd, char* buf, uint64_t size){
	OpenFileEntry* entry = open_file_table->at(fd);
	FileSegInfo* finfo = NULL;
	ssize_t rsize = 0;


	uint64_t start_page = entry->r_offset / page_size;
	uint64_t last_page = (entry->r_offset + size - 1) / page_size;
	uint64_t offset = entry->r_offset % page_size;
	if(last_page < start_page) return 0;


	FILE* fp= fopen("read_trace.txt", "a");
	FILE* fp2= fopen("rw_trace.txt", "a");

	finfo = entry->node->file_info->at(start_page);
	if(rsize < 0){ std::cout << "lseek error\n"; return -1; }
	if(page_size - offset < size){
		rsize = pread(flash_fd, buf, page_size - offset, (finfo->start_address + offset));
		if(rsize < 0){ std::cout << "read error\n"; return rsize; }
        fprintf(fp, "%s %zu %zu %zu\n",entry->node->name->c_str(), (finfo->start_address +offset), page_size - offset, entry->r_offset);
        fprintf(fp2, "%s, ,%zu, %zu, %zu\n",entry->node->name->c_str(), (finfo->start_address +offset), page_size - offset, entry->r_offset);
		entry->r_offset += (off_t)rsize;
	}
	else{
		rsize = pread(flash_fd, buf, size, (finfo->start_address + offset));
		if(rsize < 0){ std::cout << "read error\n"; return rsize; }
        fprintf(fp, "%s %zu %zu %zu\n",entry->node->name->c_str(), (finfo->start_address +offset), size, entry->r_offset);
        fprintf(fp2, "%s, ,%zu, %zu, %zu\n",entry->node->name->c_str(), (finfo->start_address +offset), size, entry->r_offset);
		entry->r_offset += (off_t)rsize;
	}
	fclose(fp);
	fclose(fp2);

	fp= fopen("cdf_read.txt", "a");
	fprintf(fp, "%zu %zu\n", GetCurrentTime(), (tdrs+=rsize));
	fclose(fp);

	fp= fopen("cdf_rw.txt", "a");
	fprintf(fp, "%zu %zu\n", tdws, (tdrs));
	fclose(fp);

	return rsize;

}

ssize_t NoHostFs::Pread(int fd, char* buf, uint64_t size, uint64_t absolute_offset){
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
		errno = 0;
		return 0; // eof
	}
	uint64_t viable_size = entry->node->GetSize() - absolute_offset;
	if(viable_size > size)
		viable_size = size;
	//printf("viable size = total file size:%zu - bsolute_offset:%zu\n", entry->node->GetSize(), absolute_offset);


	uint64_t start_page = absolute_offset / page_size;
	uint64_t last_page = (absolute_offset + viable_size - 1) / page_size;
	off_t offset = absolute_offset % page_size;
	if(last_page < start_page) return 0;

	//printf("start_page=%zu, last_page=%zu, offset=%zu, page_size=%zu \n", start_page, last_page, offset, page_size);

	FILE* fp= fopen("read_trace.txt", "a");
	FILE* fp2= fopen("rw_trace.txt", "a");

	finfo = entry->node->file_info->at(start_page);

	if(page_size - offset < viable_size){
		rsize = pread(flash_fd, buf, (uint64_t)(page_size - offset), (off_t)(finfo->start_address + offset));
        fprintf(fp, "%s %zu %zu\n",entry->node->name->c_str(), (finfo->start_address +offset), page_size - offset);
        fprintf(fp2, "%s, ,%zu, %zu\n",entry->node->name->c_str(), (finfo->start_address +offset), page_size - offset);
	//printf("NoHostFs::Pread:: read(%d, %s, %zu), start_offset=%zu\n", flash_fd, buf, viable_size, (finfo->start_address + offset));
		if(rsize < 0) return rsize;
	}
	else{
		rsize = pread(flash_fd, buf, viable_size, offset == 0 ? finfo->start_address : (finfo->start_address + offset));
        fprintf(fp, "%s %zu %zu\n",entry->node->name->c_str(), (finfo->start_address +offset), viable_size);
        fprintf(fp2, "%s, ,%zu, %zu\n",entry->node->name->c_str(), (finfo->start_address +offset), viable_size);
	//printf("NoHostFs::Pread:: read(%d, %s, %zu), start_offset=%zu\n", flash_fd, buf, viable_size, (finfo->start_address + offset));
		if(rsize < 0) return rsize;
	}
	fclose(fp);
	fclose(fp2);

	fp= fopen("no_read.txt", "a");
	fprintf(fp, "%s  %d\n",entry->node->name->c_str(),  ++nread);
	fclose(fp);

	fp= fopen("cdf_read.txt", "a");
	fprintf(fp, "%zu %zu\n", GetCurrentTime(), (tdrs+=rsize));
	fclose(fp);


	fp= fopen("cdf_rw.txt", "a");
	fprintf(fp, "%zu %zu\n", tdws, tdrs);
	fclose(fp);

	return rsize;
}

ssize_t NoHostFs::Read(int fd, char* buf, uint64_t size){
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

	uint64_t start_page = entry->r_offset / page_size;
	uint64_t last_page = (entry->r_offset + size - 1) / page_size;
	uint64_t offset = entry->r_offset % page_size;


	if((entry->node->file_info->size() - 1) < last_page){
		std::cout << "file data doesn't exist\n";
		return -1;
	}

	finfo = entry->node->file_info->at(start_page);
	rsize = lseek(flash_fd, (uint64_t)(finfo->start_address + offset), SEEK_SET);
	if(rsize < 0){ std::cout << "lseek error\n"; return -1; }
	if(page_size - offset < size){
		rsize = read(flash_fd, buf, page_size - (uint64_t)offset);
		if(rsize < 0){ std::cout << "read error\n"; return rsize; }
		entry->r_offset += (off_t)rsize;
	}
	else{
		rsize = read(flash_fd, buf, size);
		if(rsize < 0){ std::cout << "read error\n"; return rsize; }
		entry->r_offset += (off_t)rsize;
	}


	//printf("NoHostFs::Read:: fd:%d, buffer_size:%zu ,readed_size=%zu\n", fd, size, rsize);
	return rsize;
}

uint64_t NoHostFs::GetFreeBlockAddress(){
	uint64_t i;

	//printf("NoHostFs::GetFreeBlockAddress\n");
	for(i = 0; i < global_file_tree->free_page_bitmap->size(); i++)
		if(global_file_tree->free_page_bitmap->at(i) == 0) break;

	if(i == global_file_tree->free_page_bitmap->size())
		global_file_tree->free_page_bitmap->push_back(0);

	global_file_tree->free_page_bitmap->at(i) = 1;

	return i*page_size;
}
std::string NoHostFs::GetAbsolutePath(){
	return global_file_tree->cwd;
}



}
