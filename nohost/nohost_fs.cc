#include "nohost_fs.h"
#include <string.h>
#include <cassert>

#define ENABLE_LIBFTL

#if 0
#define ENABLE_LIBFTL
#define ENABLE_DIRECT

#ifdef ENABLE_LIBFTL

bdbm_drv_info_t* _bdi = NULL;
int read_cnt = 0;

#define BDBM_ALIGN_UP(addr,size)        (((addr)+((size)-1))&(~((size)-1)))
#define BDBM_ALIGN_DOWN(addr,size)      ((addr)&(~((size)-1)))


//-----------------------------------------------------
#ifdef ENABLE_DIRECT

#include "FlashIndication.h"
#include "FlashRequest.h"
#include "dmaManager.h"

#define BLOCKS_PER_CHIP 32
#define CHIPS_PER_BUS 8 // 8
#define NUM_BUSES 8 // 8

#define FPAGE_SIZE (8192*2)
#define FPAGE_SIZE_VALID (8224)
#define NUM_TAGS 128

#define BLOCKS_PER_CHIP 32
#define CHIPS_PER_BUS 8 // 8
#define NUM_BUSES 8 // 8

#define FPAGE_SIZE (8192*2)
#define FPAGE_SIZE_VALID (8224)
#define NUM_TAGS 128

typedef enum {
	UNINIT,
	ERASED,
	WRITTEN
} FlashStatusT;

typedef struct {
	bool busy;
	int bus;
	int chip;
	int block;
} TagTableEntry;

FlashRequestProxy *device;

pthread_mutex_t flashReqMutex;
pthread_cond_t flashFreeTagCond;

//8k * 128
size_t dstAlloc_sz = FPAGE_SIZE * NUM_TAGS *sizeof(unsigned char);
size_t srcAlloc_sz = FPAGE_SIZE * NUM_TAGS *sizeof(unsigned char);
int dstAlloc;
int srcAlloc;
unsigned int ref_dstAlloc;
unsigned int ref_srcAlloc;
unsigned int* dstBuffer;
unsigned int* srcBuffer;
unsigned int* readBuffers[NUM_TAGS];
unsigned int* writeBuffers[NUM_TAGS];
TagTableEntry readTagTable[NUM_TAGS];
TagTableEntry writeTagTable[NUM_TAGS];
TagTableEntry eraseTagTable[NUM_TAGS];
FlashStatusT flashStatus[NUM_BUSES][CHIPS_PER_BUS][BLOCKS_PER_CHIP];

// for Table 
#define NUM_BLOCKS 4096
#define NUM_SEGMENTS NUM_BLOCKS
#define NUM_CHANNELS 8
#define NUM_CHIPS 8
#define NUM_LOGBLKS (NUM_CHANNELS*NUM_CHIPS)

size_t blkmapAlloc_sz = sizeof(uint16_t) * NUM_SEGMENTS * NUM_LOGBLKS;
int blkmapAlloc;
uint ref_blkmapAlloc;
uint16_t (*blkmap)[NUM_CHANNELS*NUM_CHIPS]; // 4096*64
uint16_t (*blkmgr)[NUM_CHIPS][NUM_BLOCKS];  // 8*8*4096

int tag = 1;

// temp
bdbm_sema_t global_lock;
/***/

void dm_nohost_end_req (bdbm_llm_req_t* r)
{
	bdbm_bug_on (r == NULL);
}

class FlashIndication: public FlashIndicationWrapper {
	public:
		FlashIndication (unsigned int id) : FlashIndicationWrapper (id) { }

		virtual void readDone (unsigned int atag, unsigned int status) {
			//printf ("LOG: readdone: atag=%d status=%d\n", atag, status); fflush (stdout);
			bdbm_sema_unlock (&global_lock);
		}

		virtual void writeDone (unsigned int atag, unsigned int status) {
			//printf ("LOG: writedone: atag=%d status=%d\n", atag, status); fflush (stdout);
			bdbm_sema_unlock (&global_lock);
		}

		virtual void eraseDone (unsigned int atag, unsigned int status) {
			//printf ("LOG: eraseDone, atag=%d, status=%d\n", atag, status); fflush(stdout);
			bdbm_sema_unlock (&global_lock);
		}

		virtual void debugDumpResp (unsigned int debug0, unsigned int debug1,  unsigned int debug2, unsigned int debug3, unsigned int debug4, unsigned int debug5) {
			//fprintf(stderr, "LOG: DEBUG DUMP: gearSend = %d, gearRec = %d, aurSend = %d, aurRec = %d, readSend=%d, writeSend=%d\n", debug0, debug1, debug2, debug3, debug4, debug5);
		}

		virtual void uploadDone () {
			//fprintf(stderr, "Map Upload(Host->FPGA) done!\n");
		}

		virtual void downloadDone() {
			//fprintf(stderr, "Map Download(FPGA->Host) done!\n");
		}
};

FlashIndication* indication;
uint32_t __dm_nohost_init_device ()
{
	fprintf(stderr, "Initializing DMA...\n");
	device = new FlashRequestProxy(IfcNames_FlashRequestS2H);
	indication = new FlashIndication(IfcNames_FlashIndicationH2S);
    DmaManager *dma = platformInit();

	fprintf(stderr, "Main::allocating memory...\n");
	srcAlloc = portalAlloc(srcAlloc_sz, 0);
	dstAlloc = portalAlloc(dstAlloc_sz, 0);
	srcBuffer = (unsigned int *)portalMmap(srcAlloc, srcAlloc_sz);
	dstBuffer = (unsigned int *)portalMmap(dstAlloc, dstAlloc_sz);

	blkmapAlloc = portalAlloc(blkmapAlloc_sz*2, 0);
	char *tmpPtr = (char*)portalMmap(blkmapAlloc, blkmapAlloc_sz*2);
	blkmap      = (uint16_t(*)[NUM_CHANNELS*NUM_CHIPS]) (tmpPtr);
	blkmgr      = (uint16_t(*)[NUM_CHIPS][NUM_BLOCKS])  (tmpPtr+blkmapAlloc_sz);

	fprintf(stderr, "dstAlloc = %x\n", dstAlloc); 
	fprintf(stderr, "srcAlloc = %x\n", srcAlloc); 
	fprintf(stderr, "blkmapAlloc = %x\n", blkmapAlloc); 
	
	portalCacheFlush(dstAlloc, dstBuffer, dstAlloc_sz, 1);
	portalCacheFlush(srcAlloc, srcBuffer, srcAlloc_sz, 1);
	portalCacheFlush(blkmapAlloc, blkmap, blkmapAlloc_sz*2, 1);

	ref_dstAlloc = dma->reference(dstAlloc);
	ref_srcAlloc = dma->reference(srcAlloc);
	ref_blkmapAlloc = dma->reference(blkmapAlloc);

	device->setDmaWriteRef(ref_dstAlloc);
	device->setDmaReadRef(ref_srcAlloc);
	device->setDmaMapRef(ref_blkmapAlloc);

	for (int t = 0; t < NUM_TAGS; t++) {
		readTagTable[t].busy = false;
		writeTagTable[t].busy = false;
		int byteOffset = t * FPAGE_SIZE;
		readBuffers[t] = dstBuffer + byteOffset/sizeof(unsigned int);
		writeBuffers[t] = srcBuffer + byteOffset/sizeof(unsigned int);
	}
	
	for (int blk=0; blk < BLOCKS_PER_CHIP; blk++) {
		for (int c=0; c < CHIPS_PER_BUS; c++) {
			for (int bus=0; bus< NUM_BUSES; bus++) {
				flashStatus[bus][c][blk] = UNINIT;
			}
		}
	}

	for (int t = 0; t < NUM_TAGS; t++) {
		for ( unsigned int i = 0; i < FPAGE_SIZE/sizeof(unsigned int); i++ ) {
			readBuffers[t][i] = 0xDEADBEEF;
			writeBuffers[t][i] = 0xBEEFDEAD;
		}
	}

#define MainClockPeriod 5

	long actualFrequency=0;
	long requestedFrequency=1e9/MainClockPeriod;
	int status = setClockFrequency(0, requestedFrequency, &actualFrequency);
	fprintf(stderr, "Requested Freq: %5.2f, Actual Freq: %5.2f, status=%d\n"
			,(double)requestedFrequency*1.0e-6
			,(double)actualFrequency*1.0e-6,status);

	device->start(0);
	device->setDebugVals(0,0); //flag, delay

	device->debugDumpReq(0);
	sleep(1);
	device->debugDumpReq(0);
	sleep(1);

	for (int t = 0; t < NUM_TAGS; t++) {
		for ( unsigned int i = 0; i < FPAGE_SIZE/sizeof(unsigned int); i++ ) {
			readBuffers[t][i] = 0xDEADBEEF;
		}
	}

	//device->downloadMap();
	//device->uploadMap();
	
	return 0;
}

uint32_t dm_nohost_make_req (int type, int lpa, int bsize, uint8_t* buf)
{
	uint32_t punit_id, ret, i;

	//bdbm_msg ("dm_nohost_make_req - begin");
	bdbm_sema_lock (&global_lock);
	//bdbm_msg ("dm_nohost_make_req - ok");

	/* submit reqs to the device */
	switch (type) {
	case REQTYPE_WRITE:
	case REQTYPE_RMW_WRITE:
	case REQTYPE_GC_WRITE:
	case REQTYPE_META_WRITE:
		//printf ("LOG: device->writePage, tag=%d lpa=%d\n", tag, lpa); fflush(stdout);
		memcpy (writeBuffers[tag], buf, bsize);
		device->writePage (tag, lpa, tag * FPAGE_SIZE);
		bdbm_sema_lock (&global_lock);
		break;

	case REQTYPE_READ:
	case REQTYPE_READ_DUMMY:
	case REQTYPE_RMW_READ:
	case REQTYPE_GC_READ:
	case REQTYPE_META_READ:
		//printf ("LOG: device->readPage, tag=%d lap=%d\n", tag, lpa); fflush(stdout);
		device->readPage (tag, lpa, tag * FPAGE_SIZE);
		bdbm_sema_lock (&global_lock);
		memcpy (buf, readBuffers[tag], bsize);
		break;

	case REQTYPE_GC_ERASE:
		//printf ("LOG: device->eraseBlock, tag=%d lpa=%d\n", tag, lpa); fflush(stdout);
		device->eraseBlock (tag, lpa);
		bdbm_sema_lock (&global_lock);
		break;

	default:
		break;
	}

	bdbm_sema_unlock (&global_lock);

	return 0;
}
#endif
//-----------------------------------------------------


#ifdef ENABLE_DIRECT

static int __libftl_write (uint64_t boffset, uint64_t bsize, uint8_t* data)
{
	//bdbm_msg ("__libftl_write - begin");
	dm_nohost_make_req (REQTYPE_WRITE, boffset, bsize, data);
	//bdbm_msg ("__libftl_write - end");
	return 0;
}

#else

static void libftl_write_done (void* req)
{
	bdbm_blkio_req_t* blkio_req = (bdbm_blkio_req_t*)req;
	bdbm_sema_unlock ((bdbm_sema_t*)blkio_req->user);
}

static int __libftl_write (uint64_t boffset, uint64_t bsize, uint8_t* data)
{
	uint32_t j = 0;
	bdbm_blkio_req_t* blkio_req = 
		(bdbm_blkio_req_t*)bdbm_malloc (sizeof (bdbm_blkio_req_t));

	/* build blkio req */
	blkio_req->bi_rw = REQTYPE_WRITE;
	blkio_req->bi_offset = (boffset + 511) / 512;
	blkio_req->bi_size = (bsize + 511) / 512;
	blkio_req->bi_bvec_cnt = (bsize + 4095) / 4096;
	blkio_req->cb_done = libftl_write_done;
	blkio_req->user = (bdbm_sema_t*)bdbm_malloc (sizeof (bdbm_sema_t));
	bdbm_sema_init ((bdbm_sema_t*)blkio_req->user);
	for (j = 0; j < blkio_req->bi_bvec_cnt; j++) {
		uint32_t wsize = 4096;
		blkio_req->bi_bvec_ptr[j] = (uint8_t*)bdbm_malloc (4096);
		if (bsize < (j + 1) * 4096) wsize = bsize % 4096;
		bdbm_memcpy (blkio_req->bi_bvec_ptr[j], data + (j * 4096), wsize);
	}

	/* send req to ftl */
	bdbm_sema_lock ((bdbm_sema_t*)blkio_req->user);
	_bdi->ptr_host_inf->make_req (_bdi, blkio_req);
	bdbm_sema_lock ((bdbm_sema_t*)blkio_req->user);

	/* copy read data to buffer */
	for (j = 0; j < blkio_req->bi_bvec_cnt; j++)
		bdbm_free (blkio_req->bi_bvec_ptr[j]);
	bdbm_free (blkio_req->user);

	return bsize;
}

#endif

static int libftl_write (uint64_t boffset, uint64_t bsize, uint8_t* data)
{
	int64_t left = bsize;
	int64_t ofs = 0;
	uint8_t* ptr_data = data;
	//int max_length = 256*4096;
	int max_length = 8192;

	while (left > 0) {
		int cur_length;

		if (max_length < left) {
			//printf ("max_length: %d, left: %d\n", max_length, left);
			cur_length = max_length;
		} else
			cur_length = left;

		__libftl_write (boffset+ofs, cur_length, ptr_data);

		left -= cur_length;
		ofs += cur_length;
		ptr_data += cur_length;
	}

	return bsize;
}


#ifdef ENABLE_DIRECT

static int __libftl_read (uint64_t boffset, uint64_t bsize, uint8_t* data)
{
	//bdbm_msg ("__libftl_read - begin (%llu %llu)", boffset, bsize);
	dm_nohost_make_req (REQTYPE_READ, boffset, bsize, data);
	//bdbm_msg ("__libftl_read - end");
	return 0;
}

#else

static void libftl_read_done (void* req)
{
	bdbm_blkio_req_t* blkio_req = (bdbm_blkio_req_t*)req;
	bdbm_sema_unlock ((bdbm_sema_t*)blkio_req->user);
}

static int __libftl_read (uint64_t boffset, uint64_t bsize, uint8_t* data)
{
	uint32_t j = 0;
	uint64_t ret = 0;
	bdbm_blkio_req_t* blkio_req = 
		(bdbm_blkio_req_t*)bdbm_malloc (sizeof (bdbm_blkio_req_t));

	/* build blkio req */
	blkio_req->bi_rw = REQTYPE_READ;
	blkio_req->bi_offset = (boffset + 511) / 512;
	blkio_req->bi_size = (bsize + 511) / 512;
	blkio_req->bi_bvec_cnt = (bsize + 4095) / 4096;
	blkio_req->cb_done = libftl_read_done;
	blkio_req->user = (bdbm_sema_t*)bdbm_malloc (sizeof (bdbm_sema_t));
	blkio_req->user2 = data;
	bdbm_sema_init ((bdbm_sema_t*)blkio_req->user);
	for (j = 0; j < blkio_req->bi_bvec_cnt; j++)
		blkio_req->bi_bvec_ptr[j] = (uint8_t*)bdbm_malloc (4096);

	/* send req to ftl */
	bdbm_sema_lock ((bdbm_sema_t*)blkio_req->user);
	_bdi->ptr_host_inf->make_req (_bdi, blkio_req);
	bdbm_sema_lock ((bdbm_sema_t*)blkio_req->user);

	/* copy read data to buffer */
	for (j = 0; j < blkio_req->bi_bvec_cnt; j++) {
		uint32_t wsize = 4096;
		if (bsize < (j + 1) * 4096) wsize = bsize % 4096;
		//printf ("READ ==> bsize = %llu, wsize = %llu, j = %u\n", bsize, wsize, j);
		bdbm_memcpy (data + (j * 4096), blkio_req->bi_bvec_ptr[j], wsize);
		bdbm_free (blkio_req->bi_bvec_ptr[j]);
	}
	bdbm_free (blkio_req->user);

	return bsize;
}
#endif

static int libftl_read (uint64_t boffset, uint64_t bsize, uint8_t* data)
{
	int64_t left = bsize;
	int64_t ofs = 0;
	uint8_t* ptr_data = data;
	int max_length = 8192;

	while (left > 0) {
		int cur_length;

		if (max_length < left) {
			//printf ("max_length: %d, left: %d\n", max_length, left);
			cur_length = max_length;
		} else
			cur_length = left;

		__libftl_read (boffset+ofs, cur_length, ptr_data);

		left -= cur_length;
		ofs += cur_length;
		ptr_data += cur_length;
	}

	return bsize;
}

namespace rocksdb {

#ifdef ENABLE_DIRECT

int libftl_trim (uint64_t boffset, uint64_t bsize)
{
	//bdbm_msg ("__libftl_trim - begin (%llu)", boffset);
	boffset = boffset / (2^14*2) * (2^14*2);
	//bdbm_msg ("__libftl_trim - begin (%llu)", boffset);
	dm_nohost_make_req (REQTYPE_GC_ERASE, boffset, bsize, NULL);
	//bdbm_msg ("__libftl_trim - end");
	return 0;
}

#else

int libftl_trim (uint64_t boffset, uint64_t bsize)
{
	uint32_t j = 0;
	bdbm_blkio_req_t* blkio_req = 
		(bdbm_blkio_req_t*)bdbm_malloc (sizeof (bdbm_blkio_req_t));

	if (boffset % (16*1024*1024) != 0) {
		printf ("what???\n");
		exit (-1);
	}
	bsize = (bsize / (16*1024*1024) + 1) * (16*1024*1024);

	//printf ("offset=%u size=%u\n", boffset, bsize);

	/* build blkio req */
	blkio_req->bi_rw = REQTYPE_TRIM;
	blkio_req->bi_offset = (boffset + 511) / 512;
	blkio_req->bi_size = (bsize + 511) / 512;
	blkio_req->bi_bvec_cnt = (bsize + 4095) / 4096;
	blkio_req->cb_done = libftl_write_done;
	blkio_req->user = (bdbm_sema_t*)bdbm_malloc (sizeof (bdbm_sema_t));
	bdbm_sema_init ((bdbm_sema_t*)blkio_req->user);
	/*
	for (j = 0; j < blkio_req->bi_bvec_cnt; j++) {
		uint32_t wsize = 4096;
		blkio_req->bi_bvec_ptr[j] = (uint8_t*)bdbm_malloc (4096);
		if (bsize < (j + 1) * 4096) wsize = bsize % 4096;
	}
	*/

	/* send req to ftl */
	bdbm_sema_lock ((bdbm_sema_t*)blkio_req->user);
	_bdi->ptr_host_inf->make_req (_bdi, blkio_req);
	bdbm_sema_lock ((bdbm_sema_t*)blkio_req->user);

	/* copy read data to buffer */
	/*
	for (j = 0; j < blkio_req->bi_bvec_cnt; j++)
		bdbm_free (blkio_req->bi_bvec_ptr[j]);
	*/
	bdbm_free (blkio_req->user);

	return bsize;

}

#endif

#endif

#endif

#include "libmemio.h"

memio_t* mio = NULL;

namespace rocksdb {

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

NoHostFs::NoHostFs(size_t assign_size){
	global_file_tree = new GlobalFileTableTree(assign_size);
	open_file_table = new std::vector<OpenFileEntry*>();

#if 0
#ifdef ENABLE_LIBFTL
#ifndef ENABLE_DIRECT
	if ((_bdi = bdbm_drv_create ()) == NULL) {
		bdbm_error ("[kmain] bdbm_drv_create () failed");
		return;
	}
	if (bdbm_dm_init (_bdi) != 0) {
		bdbm_error ("[kmain] bdbm_dm_init () failed");
		return;
	}
	bdbm_drv_setup (_bdi, &_userio_inf, bdbm_dm_get_inf (_bdi));
	bdbm_drv_run (_bdi);
#else
	__dm_nohost_init_device ();
	bdbm_sema_init (&global_lock);
#endif
#endif // ENABLE_LIBFTL
#endif

#ifdef ENABLE_LIBFTL
	if ((mio = memio_open ()) == NULL) {
		fprintf (stderr, "oops! memio_open() failed!\n");
		exit (-1);
	}
#endif

	flash_fd = open("flash.db", O_CREAT | O_RDWR | O_TRUNC, 0666);
	this->page_size = assign_size;
}

NoHostFs::~NoHostFs(){
	delete global_file_tree;
	for(size_t i =0; i < open_file_table->size(); i++){
		if(open_file_table->at(i) != NULL)
			delete open_file_table->at(i);
	}
	close(flash_fd);
	//unlink("flash.db");

#if 0
#ifdef ENABLE_LIBFTL
	//printf ("# of reads: %d\n", read_cnt);
	bdbm_drv_close (_bdi);
	bdbm_dm_exit (_bdi);
	bdbm_drv_destroy (_bdi);
#endif
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
size_t NoHostFs::GetFileSize(std::string name){
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
			wsizet = pwrite(flash_fd, entry->node->file_buf->buffer, page_unit , offset);
			if(wsizet < 0){ std::cout << "write error\n"; return wsizet; }
#ifdef ENABLE_LIBFTL
			printf ("pwrite-1: offset = %lld, page_unit = %lld\n", offset, page_unit);
			//libftl_write(offset, page_unit, (uint8_t*)entry->node->file_buf->buffer);
			memio_write (mio, offset/8192, page_unit, (uint8_t*)entry->node->file_buf->buffer);
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
			wsizet= pwrite(flash_fd, entry->node->file_buf->buffer, page_unit , offset);
			if(wsizet < 0){ std::cout << "write error\n"; return wsizet; }
#ifdef ENABLE_LIBFTL
			printf ("pwrite-2: offset = %lld, page_unit = %lld\n", offset, page_unit);
			//libftl_write(offset, page_unit, (uint8_t*)entry->node->file_buf->buffer);
			memio_write (mio, offset/8192, page_unit, (uint8_t*)entry->node->file_buf->buffer);
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

		wsizet = pwrite(flash_fd, buf, ((dsize/page_unit)*page_unit) , offset);
		if(wsizet < 0){ std::cout << "write error\n"; return wsizet; }
#ifdef ENABLE_LIBFTL
		if (((dsize/page_unit)*page_unit) != 0) {
			printf ("pwrite-3: offset = %lld, page_unit = %lld\n", offset, ((dsize/page_unit)*page_unit));
			//libftl_write(offset, (dsize/page_unit)*page_unit, (uint8_t*)entry->node->file_buf->buffer);
			memio_write (mio, offset/8192, (dsize/page_unit)*page_unit, (uint8_t*)entry->node->file_buf->buffer);
			memio_wait (mio);
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
	ssize_t rsize;
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
		rsize = pread(flash_fd, unit_buffer_i, page_num*page_unit, start_page*page_unit);
		if(rsize < 0){ std::cout << "read error\n"; return rsize; }
#ifdef ENABLE_LIBFTL
		//libftl_read (start_page*page_unit, page_num*page_unit, (uint8_t*)unit_buffer_i);
		printf ("pread-1: offset = %lld, page_unit = %lld\n", start_page*page_unit, page_num*page_unit);
		memio_read (mio, start_page*page_unit/8192, page_num*page_unit, (uint8_t*)unit_buffer_i);
		memio_wait (mio);
#endif
		unit_buffer_i += (offset % page_unit);
		memcpy(buf, unit_buffer_i, dsize);
		delete [] unit_buffer;
		if(!ispread) entry->r_offset += (off_t)dsize;
		rsize = dsize;
	}
	else{
		if(offset >= buf_start + buf_offset){
			tbuf += (offset - (buf_start + buf_offset));
			memcpy(buf, tbuf, dsize);
			if(!ispread) entry->r_offset += (off_t)dsize;
			rsize = (off_t)dsize;
		}
		else{
			unit_buffer = new char[(buf_start + buf_offset) - (start_page*page_unit)];
			unit_buffer_i = unit_buffer;
			rsize = pread(flash_fd, unit_buffer_i,((buf_start + buf_offset) - (start_page*page_unit)), start_page*page_unit);
			if(rsize < 0){ std::cout << "read error\n"; return rsize; }
#ifdef ENABLE_LIBFTL
			//libftl_read (start_page*page_unit, ((buf_start + buf_offset) - (start_page*page_unit)), (uint8_t*)unit_buffer_i);
			printf ("pread-2: offset = %lld, page_unit = %lld\n", start_page*page_unit, ((buf_start + buf_offset) - (start_page*page_unit)));
			memio_read (mio, start_page*page_unit/8192, ((buf_start + buf_offset) - (start_page*page_unit)), (uint8_t*)unit_buffer_i);
			memio_wait (mio);
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
