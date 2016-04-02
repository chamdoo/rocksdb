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
	char* sebuf = new char[1024*256];
	size_t fd, wfd, rfd, size, wsize;
	fd = open("temp", O_RDONLY);
	size = read(fd, buf, sizeof(buf));
	close(fd);

	fd = fs.Open("a.txt", 'w');
	fs.Write(fd, buf, sizeof(buf));
	fs.Write(fd, buf, sizeof(buf));
	fd = fs.Open("b.txt", 'w');
	int i = 0;
	while(i < (512*8)){
		fs.Write(fd, buf, sizeof(buf));
		i++;
	}

	fd = fs.Open("b.txt", 'r');
	fs.SequentialRead(fd, sebuf, 1024*256);


	wfd = open("test", O_CREAT | O_TRUNC | O_WRONLY, 0666);
	wsize = write(wfd, sebuf, 1024*256);
	close(wfd);

	fs.global_file_tree->printAll();




}
