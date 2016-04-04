#include "nohost_global_file_table.h"

#include <sstream>



namespace rocksdb{

// Node implementation
uint64_t Node::GetSize(){
	if(isfile){
		int total = 0;
		for(uint64_t i = 0; i < file_info->size(); i++){
			total += (file_info->at(i)->size);
		}
		return total;
	}
	else{
		return children->size();
	}
}


// GlobalFileTableTree implementation
Node* GlobalFileTableTree::DirectoryTraverse(const std::string path, bool isCreate){
	std::vector<std::string> path_list = split(path, '/');
	std::vector<std::string>::iterator iter = path_list.begin();
	Node* dir = root;

	if(isCreate) path_list.pop_back();

	if(path_list[0] == "") iter++;

	for(; iter != path_list.end(); iter++){
		if(dir->isfile) break;
		dir = FindChild(dir, *iter);
	}

	cwd = dir;
	return dir;
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

bool GlobalFileTableTree::CreateDir(std::string name){
	std::vector<std::string> path_list = split(name, '/');
	if(path_list.size() == 0){
		std::cout <<"A file must have a name."<<std::endl;
		return false;
	}
	std::string new_dir_name = path_list.back();

	Node* curdir = DirectoryTraverse(name, false);
	if(curdir != NULL){
		std::cout << name <<" file already exist."<<std::endl;
		return false;
	}

	curdir = DirectoryTraverse(name, true);
	curdir->children->push_front(new Node(new_dir_name, false, curdir));

	return true;
}
bool GlobalFileTableTree::CreateFile(std::string name){
	std::vector<std::string> path_list = split(name, '/');
	if(path_list.size() == 0){
		std::cout <<"A file must have a name."<<std::endl;
		return false;
	}

	std::string new_file_name = path_list.back();

	Node* curdir = DirectoryTraverse(name, false);
	if(curdir != NULL){
		std::cout << name <<" file already exist."<<std::endl;
		return false;
	}

	curdir = DirectoryTraverse(name, true);
	curdir->children->push_front(new Node(new_file_name, true, curdir));

	return true;
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
		std::cout << name <<" file doesn't exist."<<std::endl;
		return false;
	}

	curdir = curdir->parent;
	std::list<Node*>::iterator iter = curdir->children->begin();
	while(iter != curdir->children->end()){
		if((*iter)->name == remove_dir_name){
			RecursiveRemoveDir(*iter);
			curdir->children->erase(iter);
			return true;
		}
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
			delete (*iter);
			iter++;
		}
		return true;
	}
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

bool GlobalFileTableTree::DeleteFile(std::string name){
	std::vector<std::string> path_list = split(name, '/');
	if(path_list.size() == 0){
		std::cout <<"A file must have a name."<<std::endl;
		return false;
	}
	std::string remove_file_name = path_list.back();

	Node* curdir = DirectoryTraverse(name, false);
	if(curdir == NULL){
		std::cout << name <<" file doesn't exist."<<std::endl;
		return false;
	}

	curdir = curdir->parent;
	std::list<Node*>::iterator iter = curdir->children->begin();
	while(iter != curdir->children->end()){
		if((*iter)->name == remove_file_name){
			curdir->children->erase(iter);
			return true;
		}
	}

	return false;
}
Node* GlobalFileTableTree::GetNode(std::string name){
	return DirectoryTraverse(name, false);
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

}
