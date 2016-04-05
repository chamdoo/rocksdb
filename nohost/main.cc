#include <iostream>
#include <stdio.h>

#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include "nohost_fs.h"

char buf[1024*2];

int main() {


	rocksdb::NoHostFs fs = rocksdb::NoHostFs(1024*4);
	char* sebuf = new char[1024*1024*8];
	size_t fd, wfd, size, wsize;
	fd = open("nohost_global_file_table.cc", O_RDONLY);
	size = read(fd, buf, sizeof(buf));
	close(fd);

	fd = fs.Open("a.txt", 'w');
	fs.Write(fd, buf, sizeof(buf));
	printf("eof: %d\n", fs.IsEof(fd));

	fs.Close(fd);
	fd = fs.Open("b.txt", 'w');
	int i = 0;
	while(i < (512*8)){
		fs.Write(fd, buf, sizeof(buf));
		i++;
	}
	fs.Close(fd);

	fd = fs.Open("b.txt", 'r');
	fs.SequentialRead(fd, sebuf, 1024*1024*8);

	printf("eof: %d\n", fs.IsEof(fd));
	printf("lseek: %d\n", fs.Lseek(fd, 0));
	printf("eof: %d\n", fs.IsEof(fd));
	fs.Close(fd);

	fs.Rename("b.txt", "fdf.txt");
	std::cout << fs.Access("b.txt") << std::endl;
	std::cout << fs.Access("fdf.txt") << std::endl;
	int dirfd = fs.Open("sdf", 'd');
	rocksdb::Node* no;

	do{
		no = fs.ReadDir(dirfd);
		if(no != NULL) printf("name: %s\n", no->name.c_str());
	}while(no != NULL);



	wfd = open("test", O_CREAT | O_TRUNC | O_WRONLY, 0666);
	wsize = write(wfd, sebuf, 512);
	close(wfd);
	fs.Close(fd);

	fs.CreateDir("/name0");
	fs.CreateDir("/name1");
	fs.CreateDir("/name2");
	fs.CreateDir("/name3");
	fs.CreateDir("/name0/dir1");
	fs.CreateDir("/name1/dir2");
	fs.CreateDir("/name2/dir3");
	fs.CreateDir("/name3/dir4");
	fs.CreateFile("/name0/dir1/file1");
	fs.CreateFile("/name1/dir2/file2");
	fs.CreateFile("/name2/dir3/file3");
	fs.CreateFile("/name3/dir4/file3");
	fs.CreateFile("/name3/dir4/fil43");
	fs.CreateFile("/name3/dir4/file433");
	fs.CreateFile("/name3/dir4/file4342343");
	fs.CreateDir("/name3/dir4/dir5");
	fs.CreateDir("/name3/dir4/dir7");
	fs.CreateFile("/name3/dir4/dir7/234");
	fs.Rename("/name2", "HOON10");
	fs.Link("name3/src", "/name3/dir4/file3");


	fs.DeleteFile("/name3/dir4/file4342343");
	fs.DeleteFile("/name3/dir4/sdf");
	perror("delete");
	//fs.DeleteDir("/name3/dir4");

	printf("dirExists: %d\n", fs.DirExists("/name3/dir4/dir7"));
	printf("dirExists: %d\n", fs.DirExists("/name3/dir4/dir7233"));
	printf("modifiedTime: %ld\n", fs.GetFileModificationTime("fdf.txt"));




	fs.global_file_tree->printAll();




}
