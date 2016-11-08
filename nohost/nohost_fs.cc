#include "nohost_fs.h"
#include <string.h>
#include <cassert>

//#define ENABLE_LIBFTL
#define ENABLE_FLASH_DB

#include "libmemio.h"

memio_t* mio = NULL;

namespace rocksdb {

std::string RecoverName(std::string name){
	std::vector<std::string> path_list = split(name, '/');
	std::vector<std::string>::iterator iter = path_list.begin();
	while(iter != path_list.end()){
		if(iter->compare("") == 0){
			iter = path_list.erase(iter); // update iter
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

NoHostFs::NoHostFs(size_t assign_size){
	global_file_tree = new GlobalFileTableTree(assign_size);
	open_file_table = new std::vector<OpenFileEntry*>();

#ifdef ENABLE_LIBFTL
	if ((mio = memio_open ()) == NULL) {
		fprintf (stderr, "oops! memio_open() failed!\n");
		exit (-1);
	}
#endif

#ifdef ENABLE_FLASH_DB
	flash_fd = open64("flash.db", O_CREAT | O_RDWR | O_TRUNC, 0666);
#endif

	this->page_size = assign_size;
}

NoHostFs::~NoHostFs(){
	delete global_file_tree;
	for(size_t i =0; i < open_file_table->size(); i++){
		if(open_file_table->at(i) != NULL)
			delete open_file_table->at(i);
	}
#ifdef ENABLE_FLASH_DB
	close(flash_fd);
	//unlink("flash.db");
#endif

#ifdef ENABLE_LIBFTL
	memio_close (mio);
#endif
}


int NoHostFs::Lock(std::string name, bool lock){
	name = RecoverName(name);
#ifdef ENABLE_DEBUG
	printf("NoHostFs::Lock::name:%s\n", name.c_str());
#endif
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
#ifdef ENABLE_DEBUG
	if(entry->node != NULL || entry->node->name != NULL) {
		printf("NoHostFs::Close::name:%s\n", entry->node->name->c_str());
	}
#endif
	delete entry;
	open_file_table->at(fd) = NULL;

	return 0;
}
int NoHostFs::Access(std::string name){
	name = RecoverName(name);
#ifdef ENABLE_DEBUG
	printf("NoHostFs::Access::name:%s\n", name.c_str());
#endif
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
#ifdef ENABLE_DEBUG
	printf("NoHostFs::Rename::%s, %s\n", old_name.c_str(), name.c_str());
#endif

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
#ifdef ENABLE_DEBUG
	printf("NoHostFs::ReadDir::name:%s\n", entry->node->name->c_str ());
#endif
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
	name = RecoverName(name);
#ifdef ENABLE_DEBUG
	printf("NoHostFs::DeleteFile::name:%s\n", name.c_str());
#endif
	return global_file_tree->DeleteFile(name);
}
int NoHostFs::DeleteDir(std::string name){
	name = RecoverName(name);
#ifdef ENABLE_DEBUG
	printf("NoHostFs::DeleteDir::name:%s\n", name.c_str());
#endif
	return global_file_tree->DeleteDir(name);
}
int NoHostFs::CreateDir(std::string name){
	name = RecoverName(name);
#ifdef ENABLE_DEBUG
	printf("Enter NoHostFs::CreateDir(%s)\n", name.c_str());
#endif
	if(global_file_tree->CreateDir(name) == NULL)
		return -1;
	return 0;
}
int NoHostFs::CreateFile(std::string name){
	name = RecoverName(name);
#ifdef ENABLE_DEBUG
	printf("Enter NoHostFs::CreateFile(%s)\n", name.c_str());
#endif
	Node* newfile = NULL;
	if((newfile = global_file_tree->CreateFile(name)) == NULL)
		return -1;
	size_t start_address = GetFreeBlockAddress();
	newfile->file_info->push_back(new FileSegInfo(start_address, 0));
	return 0;
}
bool NoHostFs::DirExists(std::string name){
	name = RecoverName(name);
#ifdef ENABLE_DEBUG
	printf("NoHostFs::DirExists::name:%s\n", name.c_str());
#endif
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
#ifdef ENABLE_DEBUG
	printf("NoHostFs::GetFileSize::name:%s\n", name.c_str());
#endif
	Node* node = global_file_tree->GetNode(name);
	if(node == NULL){
		errno = ENOENT; // No such file or directory
		return -1;
	}
	return node->GetSize();
}
long int NoHostFs::GetFileModificationTime(std::string name){
	name = RecoverName(name);
#ifdef ENABLE_DEBUG
	printf("NoHostFs::GetFileModificationTime::name:%s\n", name.c_str());
#endif
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
#ifdef ENABLE_DEBUG
	printf("NoHostFs::Link::%s => %s\n", src.c_str(), target.c_str());
#endif
	Node* node = global_file_tree->Link(src, target);
	if(node == NULL) return false;
	return true;
}
bool NoHostFs::IsEof(int fd){
	if(fd < 0 || (int)open_file_table->size() <= fd){
		errno = EBADF; // Bad file number
		return -1;
	}
	OpenFileEntry* entry = open_file_table->at(fd);
	if(entry == NULL){
		errno = EBADF; // Bad file number
		return false;
	}
#ifdef ENABLE_DEBUG
	printf("NoHostFs::IsEof::fd:%s\n", entry->node->name->c_str());
#endif
	if((size_t)(entry->r_offset) == entry->node->GetSize())
		return true;
	else
		return false;
}
off_t NoHostFs::Lseek(int fd, off_t n){
	if(fd < 0 || (int)open_file_table->size() <= fd){
		errno = EBADF; // Bad file number
		return -1;
	}
	OpenFileEntry* entry = open_file_table->at(fd);
	if(entry == NULL){
		errno = EBADF; // Bad file number
		return -1;
	}
#ifdef ENABLE_DEBUG
	printf("NoHostFs::Lseek::name:%s\n", entry->node->name->c_str());
#endif

	entry->r_offset = (entry->r_offset + n) % entry->node->GetSize();
	entry->w_offset = (entry->w_offset + n) % entry->node->GetSize();
	return entry->r_offset;
}

int NoHostFs::Open(std::string name, char type){
	name = RecoverName(name);
	Node* ret = NULL;
	size_t start_address;

#ifdef ENABLE_DEBUG
	printf("NoHostFs::Open::name:%s\n", name.c_str());
#endif

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



long int NoHostFs::BufferWrite(OpenFileEntry* entry, FileSegInfo* finfo, const char* buf, uint64_t dsize, uint64_t offset, size_t page_unit){
	ssize_t wsize=0;
	ssize_t wsizet=0;
	size_t bsize = entry->node->file_buf->b_size;
	size_t S = bsize + dsize;
	char* tbuf = entry->node->file_buf->buffer;

	if(S <= page_unit){
		tbuf += bsize;
		memcpy(tbuf, buf, dsize);
		entry->node->file_buf->b_size += dsize;
		entry->node->file_buf->start_address = finfo->start_address;
		entry->node->file_buf->offset = offset - finfo->start_address;
		wsize = dsize;
		if(S == page_unit){
			wsizet = page_unit; // write size
#ifdef ENABLE_FLASH_DB
			ssize_t wsizet_p = pwrite64(flash_fd, entry->node->file_buf->buffer, wsizet, offset);
			if(wsizet_p < 0){ 
				std::cout << "write error: errno " << errno << "wsizet " << wsizet << "offset " << offset << std::endl; 
				return wsizet_p; 
			}
#endif
#ifdef ENABLE_LIBFTL
			memio_write (mio, offset/8192, wsizet, (uint8_t*)entry->node->file_buf->buffer);
			memio_wait (mio);
#endif
			entry->w_offset += (off_t)wsizet;
			entry->node->size += (uint64_t)wsizet;
			finfo->size += (uint64_t)wsizet;
			entry->node->file_buf->b_size = 0;
		}
		return wsize;
	}
	else{
		if(bsize != 0){
			tbuf += bsize;
			memcpy(tbuf, buf, (page_unit - bsize));
			wsize = (page_unit - bsize);

			wsizet = page_unit; // write size
#ifdef ENABLE_FLASH_DB
			ssize_t wsizet_p = pwrite64(flash_fd, entry->node->file_buf->buffer, wsizet, offset);
			if(wsizet_p < 0){ 
				std::cout << "write error: errno " << errno << "wsizet " << wsizet << "offset " << offset << std::endl; 
				return wsizet_p; 
			}
#endif
#ifdef ENABLE_LIBFTL
			memio_write (mio, offset/8192, wsizet, (uint8_t*)entry->node->file_buf->buffer);
			memio_wait (mio);
#endif
			entry->w_offset += (off_t)wsizet;
			entry->node->size += (uint64_t)wsizet;
			finfo->size += (uint64_t)wsizet;
			entry->node->file_buf->b_size = 0;
			offset += (off_t)wsizet;
			buf += (page_unit - bsize);
			dsize = dsize - (page_unit - bsize);
		}

		//wsizet = pwrite(flash_fd, buf, dsize, offset);
		wsizet = (dsize/page_unit)*page_unit; // write size
#ifdef ENABLE_FLASH_DB
		ssize_t wsizet_p = pwrite64(flash_fd, buf, wsizet, offset);
		if(wsizet_p < 0){ 
			std::cout << "write error: errno " << errno << "wsizet " << wsizet << "offset " << offset << std::endl; 
			return wsizet_p; 
		}
#endif
#ifdef ENABLE_LIBFTL
		if (wsizet != 0) {
			memio_write (mio, offset/8192, wsizet, (uint8_t*)buf);
			memio_wait (mio);
		} else {
			/*
			uint8_t fuck_buf[8192];
			memcpy (fuck_buf, buf, dsize);
			printf ("FUCK YOU!!! -- %llu\n", dsize);
			printf ("pwrite-3: offset = %llu, page_unit = %zd\n", offset, ((dsize/page_unit)*page_unit));
			memio_write (mio, offset/8192, 8192, (uint8_t*)buf);
			memio_wait (mio);
			*/
		}
#endif
		entry->w_offset += (off_t)wsizet;
		finfo->size += (uint64_t)wsizet;
		entry->node->size += (uint64_t)wsizet;
		offset += (off_t)wsizet;
		wsize += wsizet;

		buf += (dsize/page_unit)*page_unit;
		memcpy(entry->node->file_buf->buffer, buf, dsize%page_unit);
		entry->node->file_buf->b_size = dsize % page_unit;
		entry->node->file_buf->start_address = finfo->start_address;
		entry->node->file_buf->offset = offset - finfo->start_address;
		wsize += dsize%page_unit;
	}
	return wsize;

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
	off_t offset = entry->w_offset % page_size;

	if(entry->node->file_info->size() - 1 < start_page)
		entry->node->file_info->push_back(new FileSegInfo(GetFreeBlockAddress(), 0));

	finfo = entry->node->file_info->at(start_page);

#ifdef ENABLE_DEBUG
	printf("NoHostFs::Write::name:%s fd:%d, buffer_size:%zu ,written_size=%zu, start_offset=%zu\n",
		entry->node->name->c_str(), fd, size, wsize, (finfo->start_address +offset));
#endif

	if(page_size - offset < size){
		wsize = BufferWrite(entry, finfo, buf, page_size - offset, (finfo->start_address +offset), 8192);
		//wsize = pwrite(flash_fd, buf, page_size - offset ,(finfo->start_address +offset));
		//if(wsize < 0){ std::cout << "write error\n"; return wsize; }
		//finfo->size += wsize;
		//entry->w_offset += wsize;
		//entry->node->size += (off_t)wsize;
	}
	else{
		wsize = BufferWrite(entry, finfo, buf, size, (finfo->start_address +offset), 8192);
		//wsize = pwrite(flash_fd, buf, size, (finfo->start_address +offset));
		//if(wsize < 0){ std::cout << "write error\n"; return wsize; }
		//finfo->size += (size_t)wsize;
		//entry->w_offset += (off_t)wsize;
		//entry->node->size += (off_t)wsize;
	}

	entry->node->last_modified_time = GetCurrentTime();
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
	ssize_t wsize = 0;

	size_t start_page = entry->w_offset / page_size;
	size_t offset = entry->w_offset % page_size;

	if(entry->node->file_info->size() - 1 < start_page)
		entry->node->file_info->push_back(new FileSegInfo(GetFreeBlockAddress(), 0));

	finfo = entry->node->file_info->at(start_page);

#ifdef ENABLE_DEBUG
	printf("NoHostFs::Write::name:%s fd:%d, buffer_size:%zu ,written_size=%zu, start_offset=%zu\n",
		entry->node->name->c_str(), fd, size, wsize, (finfo->start_address +offset));
#endif

	if(page_size - offset < size){
		wsize = BufferWrite(entry, finfo, buf, page_size - offset, (finfo->start_address +offset), 8192);
		//wsize = pwrite(flash_fd, curbuf, page_size - offset, (finfo->start_address +offset));
		//if(wsize < 0){ std::cout << "write error\n"; return wsize; }
		//finfo->size += (size_t)wsize;
		//entry->w_offset += (off_t)wsize;
	}
	else{
		wsize = BufferWrite(entry, finfo, buf, size, (finfo->start_address +offset), 8192);
		//wsize = pwrite(flash_fd, curbuf, size, (finfo->start_address +offset));
		//if(wsize < 0){ std::cout << "write error\n"; return wsize; }
		//finfo->size += (size_t)wsize;
		//entry->w_offset += (off_t)wsize;
	}

	entry->node->last_modified_time = GetCurrentTime();
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
#ifdef ENABLE_DEBUG
	printf("NoHostFs::SequentialRead:: %s\n", entry->node->name->c_str ());
#endif

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
		if (done < 0) return -1;
		left -= done;
		sum += done;
        tmp += done;
	}
	return sum;
}

extern int read_cnt;

long int NoHostFs::BufferRead(OpenFileEntry* entry, FileSegInfo* finfo, char* buf, uint64_t dsize, uint64_t offset, size_t page_unit, bool ispread){
	ssize_t rsize,rsizet;
	size_t bsize = entry->node->file_buf->b_size;
	size_t buf_start = entry->node->file_buf->start_address;
	size_t buf_offset = entry->node->file_buf->offset;
	size_t cur_start = finfo->start_address;
	char* tbuf = entry->node->file_buf->buffer;

	/* preparing data read for page unit*/
	size_t start_page = offset / page_unit;
	size_t last_page = (offset + (dsize-1)) / page_unit;
	size_t page_num = last_page - start_page + 1;
	char* unit_buffer = NULL;
	char* unit_buffer_i = NULL;

	if(bsize == 0 || buf_start != cur_start
			|| (dsize + offset < buf_start + buf_offset)){
		unit_buffer = new char[page_num*page_unit];
		unit_buffer_i = unit_buffer;

		printf ("[CASE1] sp: %d lp: %d offset: %llu dsize: %llu\n", 
			start_page, last_page, offset, dsize);
		
		rsizet = page_num*page_unit; // read size
#ifdef ENABLE_FLASH_DB
		//char* unit_buffer_fd = new char[page_num*page_unit];
		//ssize_t rsizet_p = pread64(flash_fd, unit_buffer_fd, rsizet, start_page*page_unit);
		ssize_t rsizet_p = pread64(flash_fd, unit_buffer_i, rsizet, start_page*page_unit);
		if(rsizet_p < 0){ std::cout << "read error: errno " << errno << std::endl; return rsizet_p; }
#endif
#ifdef ENABLE_LIBFTL
		memio_read (mio, start_page*page_unit/8192, rsizet, (uint8_t*)unit_buffer_i);
		memio_wait (mio);
#endif
#if 0
#ifdef ENABLE_FLASH_DB
		if (memcmp (unit_buffer_fd, unit_buffer_i, dsize) != 0) {
			printf ("[1] %zd = %zd * %zd / 8192, size=%zd\n",  start_page*page_unit/8192, start_page, page_unit, rsizet);
			/*
			printf ("oops! %x %x %x %x ...  %x %x %x %x != %x %x %x %x ... %x %x %x %x \n",
					unit_buffer_fd[0], unit_buffer_fd[1], unit_buffer_fd[2], unit_buffer_fd[3],
					unit_buffer_fd[rsizet-1], unit_buffer_fd[rsizet-2], unit_buffer_fd[rsizet-3], unit_buffer_fd[rsizet-4],
					unit_buffer_i[0], unit_buffer_i[1], unit_buffer_i[2], unit_buffer_i[3],
					unit_buffer_i[rsizet-1], unit_buffer_i[rsizet-2], unit_buffer_i[rsizet-3], unit_buffer_i[rsizet-4]);
			*/
			for (uint64_t i = 0; i < dsize; i+=8) {
				printf ("[%llu] %x %x %x %x %x %x %x %x | %x %x %x %x %x %x %x %x\n",
					i,
					unit_buffer_fd[i], unit_buffer_fd[i+1], unit_buffer_fd[i+2], unit_buffer_fd[i+3],
					unit_buffer_fd[i+4], unit_buffer_fd[i+5], unit_buffer_fd[i+6], unit_buffer_fd[i+7],
					unit_buffer_i[i], unit_buffer_i[i+1], unit_buffer_i[i+2], unit_buffer_i[i+3],
					unit_buffer_i[i+4], unit_buffer_i[i+5], unit_buffer_i[i+6], unit_buffer_i[i+7]);
			}
			fflush (stdout);
		}
		delete [] unit_buffer_fd;
#endif
#endif
		unit_buffer_i += (offset % page_unit);
		memcpy(buf, unit_buffer_i, dsize);
		delete [] unit_buffer;
		if(!ispread) entry->r_offset += (off_t)dsize;
		rsize = dsize;
	}
	else{
		if(offset >= buf_start + buf_offset){
			printf ("[CASE2] sp: %d lp: %d offset: %llu dsize: %llu\n", 
					start_page, last_page, offset, dsize);

			tbuf += (offset - (buf_start + buf_offset));
			memcpy(buf, tbuf, dsize);
			if(!ispread) entry->r_offset += (off_t)dsize;
			rsize = (off_t)dsize;
		}
		else{
			printf ("[CASE3] sp: %d lp: %d offset: %llu dsize: %llu\n", 
					start_page, last_page, offset, dsize);

			unit_buffer = new char[(buf_start + buf_offset) - (start_page*page_unit)];
			unit_buffer_i = unit_buffer;

			rsizet = ((buf_start + buf_offset) - (start_page*page_unit)); // read size
#ifdef ENABLE_FLASH_DB
			//char* unit_buffer_fd = new char[(buf_start + buf_offset) - (start_page*page_unit)];
			//ssize_t rsizet_p = pread64(flash_fd, unit_buffer_fd, rsizet, start_page*page_unit);
			ssize_t rsizet_p = pread64(flash_fd, unit_buffer_i, rsizet, start_page*page_unit);
			if(rsizet_p < 0){ std::cout << "read error: errno " << errno << std::endl; return rsizet_p; }
#endif
#ifdef ENABLE_LIBFTL
			memio_read (mio, start_page*page_unit/8192, rsizet, (uint8_t*)unit_buffer_i);
			memio_wait (mio);
#endif
#if 0
#ifdef ENABLE_FLASH_DB
			if (memcmp (unit_buffer_fd, unit_buffer_i, rsizet) != 0) {
				printf ("[2] %zd = %zd * %zd / 8192, size = %zd",  start_page*page_unit/8192, start_page, page_unit, rsizet);
				/*
				printf ("oops! %x %x %x %x ...  %x %x %x %x != %x %x %x %x ... %x %x %x %x \n",
						unit_buffer_fd[0], unit_buffer_fd[1], unit_buffer_fd[2], unit_buffer_fd[3],
						unit_buffer_fd[rsizet-1], unit_buffer_fd[rsizet-2], unit_buffer_fd[rsizet-3], unit_buffer_fd[rsizet-4],
						unit_buffer_i[0], unit_buffer_i[1], unit_buffer_i[2], unit_buffer_i[3],
						unit_buffer_i[rsizet-1], unit_buffer_i[rsizet-2], unit_buffer_i[rsizet-3], unit_buffer_i[rsizet-4]);
				*/
				for (ssize_t i = 0; i < rsizet; i+=8) {
					printf ("[%llu] %x %x %x %x %x %x %x %x | %x %x %x %x %x %x %x %x\n",
						i,
						unit_buffer_fd[i], unit_buffer_fd[i+1], unit_buffer_fd[i+2], unit_buffer_fd[i+3],
						unit_buffer_fd[i+4], unit_buffer_fd[i+5], unit_buffer_fd[i+6], unit_buffer_fd[i+7],
						unit_buffer_i[i], unit_buffer_i[i+1], unit_buffer_i[i+2], unit_buffer_i[i+3],
						unit_buffer_i[i+4], unit_buffer_i[i+5], unit_buffer_i[i+6], unit_buffer_i[i+7]);
				}
				fflush (stdout);
			}
			delete [] unit_buffer_fd;
#endif
#endif
			unit_buffer_i += (offset % page_unit);
			memcpy(buf, unit_buffer_i, (buf_start + buf_offset) - offset);
			if(!ispread) entry->r_offset += (off_t)((buf_start + buf_offset) - offset);
			delete [] unit_buffer;
			rsize = (buf_start + buf_offset) - offset;

			buf += ((buf_start + buf_offset) - offset);
			memcpy(buf, tbuf, (offset + dsize) - (buf_start + buf_offset));
			if(!ispread) entry->r_offset += (off_t)((offset + dsize) - (buf_start + buf_offset));
			rsize += (off_t)((offset + dsize) - (buf_start + buf_offset));
		}
	}
	return rsize;

}

long int NoHostFs::ReadHelper(int fd, char* buf, size_t size){
	OpenFileEntry* entry = open_file_table->at(fd);
	FileSegInfo* finfo = NULL;
	ssize_t rsize = 0;

#ifdef ENABLE_DEBUG
	printf ("NoHostFs::ReadHelper::name=%s\n", entry->node->name->c_str ());
#endif

	size_t start_page = entry->r_offset / page_size;
	size_t last_page = (entry->r_offset + size - 1) / page_size;
	size_t offset = entry->r_offset % page_size;
	if(last_page < start_page) return 0;


	finfo = entry->node->file_info->at(start_page);

	if(page_size - offset < size){
		rsize = BufferRead(entry, finfo, buf, page_size - offset, (finfo->start_address + offset), 8192, false);
		//rsize = pread(flash_fd, buf, page_size - offset, (finfo->start_address + offset));
		//if(rsize < 0){ std::cout << "read error\n"; return rsize; }
		//entry->r_offset += (off_t)rsize;
	}
	else{
		rsize = BufferRead(entry, finfo, buf, size, (finfo->start_address + offset), 8192, false);
		//rsize = pread(flash_fd, buf, size, (finfo->start_address + offset));
		//if(rsize < 0){ std::cout << "read error\n"; return rsize; }
		//entry->r_offset += (off_t)rsize;
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
#ifdef ENABLE_DEBUG
	printf("NoHostFs::Pread::%s\n", entry->node->name->c_str());
#endif
	if(entry->node->GetSize() <= absolute_offset){
		errno = 0;
		return 0; // eof
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

	if(page_size - offset < viable_size){
		rsize = BufferRead(entry, finfo, buf, page_size - offset, (finfo->start_address + offset), 8192, true);
		//rsize = pread(flash_fd, buf, (uint64_t)(page_size - offset), (off_t)(finfo->start_address + offset));
		//printf("NoHostFs::Pread:: read(%d, %s, %zu), start_offset=%zu\n", flash_fd, buf, viable_size, (finfo->start_address + offset));
		//if(rsize < 0) return rsize;
	}
	else{
		rsize = BufferRead(entry, finfo, buf, viable_size, (finfo->start_address + offset), 8192, true);
		//rsize = pread(flash_fd, buf, viable_size, offset == 0 ? finfo->start_address : (finfo->start_address + offset));
		//printf("NoHostFs::Pread:: read(%d, %s, %zu), start_offset=%zu\n", flash_fd, buf, viable_size, (finfo->start_address + offset));
		//if(rsize < 0) return rsize;
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

	printf("ASSERT\n");
	assert(1);

#ifdef ENABLE_DEBUG
	printf("NoHostFs::Read::%s\n", entry->node->name->c_str());
#endif

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
//	printf("==========================================NoHostFs::GetFreeBlockAddress=======================================================\n");
//	global_file_tree->printAll();
	return i*page_size;
}

std::string NoHostFs::GetAbsolutePath(){
	return global_file_tree->cwd;
}

int NoHostFs::GetFd(){
	return flash_fd;
}


} /* namespace rocksdb */
