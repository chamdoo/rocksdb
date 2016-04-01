#include <iostream>
#include <stdio.h>

#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include "nohost_fs.h"

char buf[1024*8];

int main() {
	rocksdb::NoHostFs fs = rocksdb::NoHostFs(1024*1024*16);
	int fd, wfd, rfd, size, wsize;
	fd = open("nohost_global_file_table.cc", O_RDONLY);
	size = read(fd, buf, 1024*8);
	close(fd);

	fd = fs.Open("a.txt", 'w');
	fs.Write(fd, buf, sizeof(buf));
	fd = fs.Open("b.txt", 'w');
	int i = 0;
	while(i < (256*128) + 1){ fs.Write(fd, buf, sizeof(buf)); i++; }



	rfd = fs.Open("b.txt", 'r');
	wfd = open("test", O_CREAT | O_TRUNC | O_WRONLY, 0666);
	i = 0;
	while(i <  10){
		size = fs.Read(rfd, buf, sizeof(buf));
		wsize = write(wfd, buf, sizeof(buf));
		i++;
	}
	close(wfd);

	fs.global_file_tree->printAll();




}
