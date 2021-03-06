#include "nohost_global_file_table.h"
#include "stdio.h"

#include <sstream>



namespace rocksdb{

// Node implementation
uint64_t Node::GetSize(){
	if(isfile){
		return size;
	}
	else{
		return children->size();
	}
}


// GlobalFileTableTree implementation
Node* GlobalFileTableTree::DirectoryTraverse(const std::string path, bool isCreate){
/*	std::string subpath;
	if(path[0] == '/')
		subpath = path.substr(1, path.size()-1);
	else
		subpath = path;*/

	std::vector<std::string> path_list = split(path, '*');
	std::vector<std::string>::iterator iter = path_list.begin();
	while(iter != path_list.end()){
		if(iter->compare("") == 0){
			path_list.erase(iter);
			continue;
		}
		iter++;
	}
	Node* ret = root;

	if(path_list.size() == 0) return root;
	if(isCreate) path_list.pop_back();
	if(path_list.size() == 0) return root;

	for(unsigned int i = 0; i < path_list.size(); i++){
		ret = FindChild(ret, path_list[i]);

		if(ret == NULL){
			errno = ENOENT;
			return NULL;
		}
	}
	return ret;
}
Node* GlobalFileTableTree::FindChild(Node* dir, std::string name){
	Node* child = NULL;

	if(dir->isfile)
		return NULL;
	std::list<Node*>::iterator iter = dir->children->begin();

	for(; iter != dir->children->end(); iter++){
		if((*iter)->name->compare(name) == 0){
			child = *iter;
			break;
		}
	}
	return child;
}

Node* GlobalFileTableTree::CreateDir(std::string name){


	std::vector<std::string> path_list = split(name, '*');
	if(path_list.size() == 0){
		std::cout <<"A file must have a name."<<std::endl;
		return NULL;
	}
	std::string new_dir_name = path_list.back();

	Node* curdir = DirectoryTraverse(name, false);
	if(curdir != NULL){
/*		if(curdir->isfile){
			errno = ENOTDIR; // Not a directory
			return NULL;
		}*/
		errno = EEXIST; // File already exists
		return NULL;
	}
	curdir = DirectoryTraverse(name, true);
	if(curdir == NULL){
		errno = ENOENT; // No such file or directory
		return NULL;
	}

	Node* newdir = new Node(new std::string(new_dir_name), false, curdir);
	curdir->children->push_back(newdir);

	return newdir;
}
Node* GlobalFileTableTree::CreateFile(std::string name){
	std::vector<std::string> path_list = split(name, '*');
	if(path_list.size() == 0){
//		std::cout <<"A file must have a name."<<std::endl;
		return NULL;
	}

	std::string new_file_name = path_list.back();

	Node* curdir = DirectoryTraverse(name, false);
	if(curdir != NULL){
		errno = EEXIST; // No such file or directory
		return NULL;
	}

	curdir = DirectoryTraverse(name, true);
	if(curdir == NULL){
		errno = ENOENT; // No such file or directory
		return NULL;
	}
	Node* newfile = new Node(new std::string(new_file_name), true, curdir);
	curdir->children->push_back(newfile);

	return newfile;
}

Node* GlobalFileTableTree::Link(std::string src, std::string target){
	std::vector<std::string> path_list = split(target, '*');
	if(path_list.size() == 0){
//		std::cout <<"A file must have a name."<<std::endl;
		return NULL;
	}

	std::string new_file_name = path_list.back();

	Node* targetnode = GetNode(target);
	if(targetnode != NULL){
		errno = EEXIST; // File already exists
		return NULL;
	}

	Node* srcnode = GetNode(src);
	if(srcnode == NULL){
		errno = ENOENT; // No such file or directory
		return NULL;
	}

	if(srcnode->isfile){
		srcnode->file_info->at(0)->link_count++;
		targetnode = CreateFile(target);
		delete targetnode->file_info;
		targetnode->file_info = srcnode->file_info;
		targetnode->size = srcnode->size;
		targetnode->last_modified_time = srcnode->last_modified_time;
	}
	else{
		srcnode->link_count++;
		targetnode = CreateDir(target);
		delete targetnode->children;
		targetnode->children = srcnode->children;
		targetnode->size = srcnode->size;
		targetnode->last_modified_time = srcnode->last_modified_time;
	}


	return targetnode;
}

int GlobalFileTableTree::DeleteDir(std::string name){
	std::vector<std::string> path_list = split(name, '*');
	if(path_list.size() == 0){
		errno = ENOENT; // No such file or directory
//		std::cout <<"A file must have a name."<<std::endl;
		return -1;
	}

	std::string remove_dir_name = path_list.back();

	Node* curdir = DirectoryTraverse(name, false);
	if(curdir == NULL){
		errno = ENOENT; // No such file or directory
//		std::cout << name <<" file doesn't exist."<<std::endl;
		return -1;
	}
	if(curdir->isfile){
		errno = ENOTDIR; // Not a directory
		return -1;
	}
	if(curdir->children->size() != 0){
		std::list<Node*>::iterator iter = curdir->children->begin();
/*		printf("children list: \n");
		while(iter != curdir->children->end()){
			printf("  : %s\n", (*iter)->name->c_str());
			iter++;
		}*/

		errno = ENOTEMPTY;
		return -1;
	}

	curdir->link_count--;

	curdir = curdir->parent;
	std::list<Node*>::iterator iter = curdir->children->begin();
	while(iter != curdir->children->end()){
		if((*iter)->name->compare(remove_dir_name) == 0){
			delete (*iter);
			curdir->children->erase(iter);
			return 0;
		}
		iter++;
	}

	return -1;
}
bool GlobalFileTableTree::RecursiveRemoveDir(Node* cur){
/*	if(cur->isfile)
		return true;
	if(cur->children->size() == 0)
		return true;
	else{
 		std::list<Node*>::iterator iter = cur->children->begin();
 		while(iter != cur->children->end()){
			//RecursiveRemoveDir((*iter));
			if( (*iter)->link_count > 1)
				(*iter)->link_count--;
			else{
				FreeAllocatedPage(*iter);
				delete (*iter);
			}
			iter++;
		}
		return true;
	}*/
	return true;
}


int GlobalFileTableTree::DeleteFile(std::string name){
	std::vector<std::string> path_list = split(name, '*');
	if(path_list.size() == 0){
//		std::cout <<"A file must have a name."<<std::endl;
		return -1;
	}
	std::string remove_file_name = path_list.back();

	Node* curfile = DirectoryTraverse(name, false);
	if(curfile == NULL){
		errno = ENOENT; // No such file or directory
//		std::cout << name <<" file doesn't exist."<<std::endl;
		return -1;
	}
	if(!curfile->isfile){
		errno = EISDIR; //Is a directory
		return -1;
	}

	curfile->file_info->at(0)->link_count--;

	Node* curdir = curfile->parent;
	std::list<Node*>::iterator iter = curdir->children->begin();
	while(iter != curdir->children->end()){
		if((*iter)->name->compare(remove_file_name) == 0){
			if(curfile->file_info->at(0)->link_count == 0) FreeAllocatedPage(*iter);
			delete (*iter);
			curdir->children->erase(iter);
			return 0;
		}
		iter++;
	}

	return -1;
}
int GlobalFileTableTree::Lock(std::string name, bool lock){
	Node* node = NULL;
	if( (node = DirectoryTraverse(name, false)) == NULL){
		errno = ENOENT; // No such file or directory
//		std::cout << name <<" file doesn't exist."<<std::endl;
		return -1;
	}
	if(lock)
		node->lock = true;
	else
		node->lock = false;
	return 0;
}

Node* GlobalFileTableTree::GetNode(std::string name){
	Node* node = NULL;
	if( (node = DirectoryTraverse(name, false)) == NULL){
		errno = ENOENT; // No such file or directory
//		std::cout << name <<" file doesn't exist."<<std::endl;
		return NULL;
	}
	return node;
}

int GlobalFileTableTree::FreeAllocatedPage(Node* node){
	if(!node->isfile) return -1;
	std::vector<FileSegInfo*>::iterator iter = node->file_info->begin();
	uint64_t i = 0;
	while(iter != node->file_info->end()){
		i = (*iter)->start_address / page_size;
		if(i < free_page_bitmap->size())
			free_page_bitmap->at(i) = 0;
		iter++;
	}
/*	printf("=====================================GlobalFileTableTree::FreeAllocatedPage===================================================\n");
	i = 0;
	for(i = 0; i < free_page_bitmap->size(); i++){
			printf("%zu th : %d ,  start address : %zu\n", i, free_page_bitmap->at(i), i*page_size);
	}*/

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
		uint64_t size = 0;
		while(iter != cur->children->end()){
			size += (*iter)->GetSize();
			iter++;
		}

		FILE* fp = fopen("pdf_size", "a");
		fprintf(fp, "%zu\n", size);
		fclose(fp);
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

long int GetCurrentTime(){
	struct timeval current;
	gettimeofday(&current, NULL);

	return (current.tv_sec * 1000000 + current.tv_usec);
}

}
