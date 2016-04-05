#include "nohost_global_file_table.h"
#include "stdio.h"

#include <sstream>



namespace rocksdb{

// Node implementation
uint64_t Node::GetSize(){
	if(isfile){
		int total = 0;
		for(uint64_t i = 0; i < file_info->size(); i++){
			total += (file_info->at(i)->size);
			//printf("%dth,  size : %d\n", i, (file_info->at(i)->size));
		}
		return total;
	}
	else{
		return children->size();
	}
}


// GlobalFileTableTree implementation
Node* GlobalFileTableTree::DirectoryTraverse(const std::string path, bool isCreate){
	std::string subpath;
	if(path[0] == '/')
		subpath = path.substr(1, path.size()-1);
	else
		subpath = path;

	std::vector<std::string> path_list = split(subpath, '/');
	std::vector<std::string>::iterator iter = path_list.begin();
	Node* dir = root;
	Node* ret = NULL;

	if(path_list.size() == 0) return dir;
	if(isCreate) path_list.pop_back();

	if(path_list.size() == 0) return dir;

	for(; iter != path_list.end(); iter++){
		if(dir->isfile){
			if(iter ==path_list.end()) break;
			else ret = NULL;
		}
		if( (ret = FindChild(dir, *iter)) == NULL) break;
		dir = ret;
	}
	return ret;
}
Node* GlobalFileTableTree::FindChild(Node* dir, std::string name){
	Node* child = NULL;
	std::list<Node*>::iterator iter = dir->children->begin();

	for(; iter != dir->children->end(); iter++){
		if((*iter)->name == name)
			child = *iter;
	}
	return child;
}

Node* GlobalFileTableTree::CreateDir(std::string name){


	std::vector<std::string> path_list = split(name, '/');
	if(path_list.size() == 0){
		std::cout <<"A file must have a name."<<std::endl;
		return NULL;
	}
	std::string new_dir_name = path_list.back();

	Node* curdir = DirectoryTraverse(name, false);
	if(curdir != NULL){
		if(curdir->isfile){
			errno = ENOTDIR; // Not a directory
			return NULL;
		}
		errno = EEXIST; // File already exists
		std::cout << name <<" file already exist."<<std::endl;
		return curdir;
	}
	curdir = DirectoryTraverse(name, true);
	Node* newdir = new Node(new_dir_name, false, curdir);
	curdir->children->push_back(newdir);

	return newdir;
}
Node* GlobalFileTableTree::CreateFile(std::string name){
	std::vector<std::string> path_list = split(name, '/');
	if(path_list.size() == 0){
		std::cout <<"A file must have a name."<<std::endl;
		return NULL;
	}

	std::string new_file_name = path_list.back();

	Node* curdir = DirectoryTraverse(name, false);
	if(curdir != NULL){
		errno = EEXIST; // File already exists
		std::cout << name <<" file already exist."<<std::endl;
		return NULL;
	}

	curdir = DirectoryTraverse(name, true);
	Node* newfile = new Node(new_file_name, true, curdir);
	curdir->children->push_front(newfile);

	return newfile;
}

Node* GlobalFileTableTree::Link(std::string src, std::string target){
	std::vector<std::string> path_list = split(src, '/');
	if(path_list.size() == 0){
		std::cout <<"A file must have a name."<<std::endl;
		return NULL;
	}

	std::string new_file_name = path_list.back();

	Node* curdir = DirectoryTraverse(src, false);
	if(curdir != NULL){
		errno = EEXIST; // File already exists
		std::cout << src <<" file already exist."<<std::endl;
		return NULL;
	}
	curdir = DirectoryTraverse(src, true);

	Node* target_node = GetNode(target);
	if(target_node == NULL){
		errno = ENOENT; // No such file or directory
		std::cout << src <<" a target file doesn't exist."<<std::endl;
		return NULL;
	}

	target_node->link_count++;
	curdir->children->push_front(target_node);

	return target_node;
}

bool GlobalFileTableTree::DeleteDir(std::string name){
	std::vector<std::string> path_list = split(name, '/');
	if(path_list.size() == 0){
		std::cout <<"A file must have a name."<<std::endl;
		return false;
	}

	std::string remove_dir_name = path_list.back();

	Node* curdir = DirectoryTraverse(name, false);
	if(curdir == NULL){
		errno = ENOENT; // No such file or directory
		std::cout << name <<" file doesn't exist."<<std::endl;
		return false;
	}
	if(curdir->isfile){
		errno = ENOTDIR; // Not a directory
		return -1;
	}
	curdir->link_count--;
	if(curdir->link_count > 0)
		return true;

	RecursiveRemoveDir(curdir);

	curdir = curdir->parent;
	std::list<Node*>::iterator iter = curdir->children->begin();
	while(iter != curdir->children->end()){
		if((*iter)->name == remove_dir_name){
			curdir->children->erase(iter);
			delete (*iter);
			return true;
		}
		iter++;
	}

	return false;
}
bool GlobalFileTableTree::RecursiveRemoveDir(Node* cur){
	if(cur->isfile)
		return true;
	if(cur->children->size() == 0)
		return true;
	else{
		std::list<Node*>::iterator iter = cur->children->begin();
		while(iter != cur->children->end()){
			RecursiveRemoveDir((*iter));
			if( (*iter)->link_count > 1)
				(*iter)->link_count--;
			else{
				FreeAllocatedPage(*iter);
				delete (*iter);
			}
			iter++;
		}
		return true;
	}
}


bool GlobalFileTableTree::DeleteFile(std::string name){
	std::vector<std::string> path_list = split(name, '/');
	if(path_list.size() == 0){
		std::cout <<"A file must have a name."<<std::endl;
		return false;
	}
	std::string remove_file_name = path_list.back();

	Node* curfile = DirectoryTraverse(name, false);
	if(curfile == NULL){
		errno = ENOENT; // No such file or directory
		std::cout << name <<" file doesn't exist."<<std::endl;
		return false;
	}
	if(!curfile->isfile){
		errno = EISDIR; //Is a directory
		return -1;
	}

	curfile->link_count--;
	if(curfile->link_count > 0)
		return true;

	Node* curdir = curfile->parent;
	std::list<Node*>::iterator iter = curdir->children->begin();
	while(iter != curdir->children->end()){
		if((*iter)->name == remove_file_name){
			curdir->children->erase(iter);
			FreeAllocatedPage(*iter);
			delete (*iter);
			return true;
		}
		iter++;
	}

	return false;
}
int GlobalFileTableTree::Lock(std::string name, bool lock){
	Node* node = NULL;
	if( (node = DirectoryTraverse(name, false)) == NULL){
		errno = ENOENT; // No such file or directory
		std::cout << name <<" file doesn't exist."<<std::endl;
		return -1;
	}
	if(lock)
		node->lock = true;
	else
		node->lock = false;
	return 1;
}

Node* GlobalFileTableTree::GetNode(std::string name){
	Node* node = NULL;
	if( (node = DirectoryTraverse(name, false)) == NULL){
		errno = ENOENT; // No such file or directory
		std::cout << name <<" file doesn't exist."<<std::endl;
		return NULL;
	}
	return node;
}

int GlobalFileTableTree::FreeAllocatedPage(Node* node){

	if(!node->isfile) return -1;

	std::vector<FileSegInfo*>::iterator iter = node->file_info->begin();
	unsigned int i = 0;
	while(iter != node->file_info->end()){
		i = (*iter)->start_address / page_size;
		if(i < free_page_bitmap->size())
			free_page_bitmap->at(i) = 0;
		iter++;
	}
	return 0;
}

void GlobalFileTableTree::printAll(){
	print(root, "");
}
bool GlobalFileTableTree::print(Node* cur, std::string indent){
	if(cur->isfile)
		return true;
	else if(cur->children->size() == 0)
		return true;
	else{
		std::list<Node*>::iterator iter = cur->children->begin();
		while(iter != cur->children->end()){
			std::cout << indent << (*iter)->name << ", type:" << (*iter)->isfile
					<< std::endl;
			print((*iter), indent + "   ");
			iter++;
		}
		return true;
	}
}

std::vector<std::string> &split(const std::string &s, char delim, std::vector<std::string> &elems) {
    std::stringstream ss(s);
    std::string item;
    while (std::getline(ss, item, delim)) {
        elems.push_back(item);
    }
    return elems;
}

std::vector<std::string> split(const std::string &s, char delim) {
    std::vector<std::string> elems;
    split(s, delim, elems);
    return elems;
}

uint64_t GetCurrentTime(){
	struct timeval current;
	gettimeofday(&current, NULL);

	return (current.tv_sec * 1000000 + current.tv_usec);
}

}
