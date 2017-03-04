
#define LOG_TAG "VideoShare"

#include <sys/resource.h>
#include <binder/IPCThreadState.h>
#include <binder/IServiceManager.h>  

#include <utils/Log.h>
#include <utils/Atomic.h>
#include <time.h>
#include <utils/Trace.h>
#include <cutils/properties.h>

#include <videoshare/VideoShare.h>

#include <binder/PermissionCache.h>  //for permission internet
#include <utils/Vector.h>


extern "C" {
	#include "jpeglib.h" 
}
#define COMPRESS_QUALITY 70

namespace android {

	static		Mutex						mCompressLock;
	static		Mutex						mTransmitLock;
	static		Condition					mDataCompress;
	static		Condition					mDataTransmit;

	static		int 						mCurrentIndex;
	static		int 						mSocketFd;

	//added for middleware
	static		int							mSlaveNum;
	static Vector<int> 						mSlaveSockets;
	static		int							mServerSocketFd;
	static		int 						mServerAddrLen;
	static		struct sockaddr_in 			mServerAddress;


//----------------------------------------------------------
const String16 sAccessInternet("android.permission.INTERNET");

//----------------------------------------------------------
//JPEG compress

/* The following declarations and 5 functions are jpeg related  
 * functions used by put_jpeg_grey_memory and put_jpeg_yuv420p_memory 
 */  
typedef struct {  
    struct jpeg_destination_mgr pub;  
    JOCTET *buf;  
    size_t bufsize;  
    size_t jpegsize;  
} mem_destination_mgr;  
    
typedef mem_destination_mgr *mem_dest_ptr;  
    
    
void init_destination(j_compress_ptr cinfo)  
{  
    mem_dest_ptr dest = (mem_dest_ptr) cinfo->dest;  
    dest->pub.next_output_byte = dest->buf;  
    dest->pub.free_in_buffer = dest->bufsize;  
    dest->jpegsize = 0;  
}  
    
boolean empty_output_buffer(j_compress_ptr cinfo)  
{  
    mem_dest_ptr dest = (mem_dest_ptr) cinfo->dest;  
    dest->pub.next_output_byte = dest->buf;  
    dest->pub.free_in_buffer = dest->bufsize;  
    
    return FALSE;  
}  
    
void term_destination(j_compress_ptr cinfo)  
{  
    mem_dest_ptr dest = (mem_dest_ptr) cinfo->dest;  
    dest->jpegsize = dest->bufsize - dest->pub.free_in_buffer;  
}  
    
static void jpeg_mem_dest(j_compress_ptr cinfo, JOCTET* buf, size_t bufsize)  
{  
    mem_dest_ptr dest;  
    
    if (cinfo->dest == NULL) {  
        cinfo->dest = (struct jpeg_destination_mgr *)  
            (*cinfo->mem->alloc_small)((j_common_ptr)cinfo, JPOOL_PERMANENT,  
            sizeof(mem_destination_mgr));  
    }  
    
    dest = (mem_dest_ptr) cinfo->dest;  
    
    dest->pub.init_destination    = init_destination;  
    dest->pub.empty_output_buffer = empty_output_buffer;  
    dest->pub.term_destination    = term_destination;  
    
    dest->buf      = buf;  
    dest->bufsize  = bufsize;  
    dest->jpegsize = 0;  
}  
    
static int jpeg_mem_size(j_compress_ptr cinfo)  
{  
    mem_dest_ptr dest = (mem_dest_ptr) cinfo->dest;  
    return dest->jpegsize;  
}


//----------------------------------------------------------
//SOCKET 

int socket_write(int fd,unsigned char *buffer,int length) 
{ 
	int bytes_left; 
	int written_bytes; 
	unsigned char *ptr; 

	ptr=buffer; 
	bytes_left=length; 
	while(bytes_left>0) 
	{ 
         written_bytes=write(fd,ptr,bytes_left); 
         if(written_bytes<=0)
         {        
			if(errno==EINTR) {
				ALOGE("vaylb-->--------------socket_write EINTR--------------");
				written_bytes=0; 
			}
			else {
				ALOGE("vaylb-->--------------socket_write errno %d--------------", errno);
				return -1; 
			}
         } 
         bytes_left-=written_bytes; 
         ptr+=written_bytes;     
	} 
	return 0; 
} 

//----------------------------------------------------------

long get_time_stamp(){
	struct timeval tv;	  
	gettimeofday(&tv,NULL);    
	return (tv.tv_sec * 1000 + tv.tv_usec / 1000);
}

//----------------------------------------------------------


VideoShare::VideoShare(/*sp<BufferQueue> bufferQueue*/):
	BnVideoShare(),
	mDealer(NULL),
	mShareMemory(NULL),
	mData_u(NULL),
	mData_v(NULL),
	mData_YUV_420p(NULL),
	mMiddleWareFlag(false),
	sendcount(0),
	mSendBuffer(new DataBuffer(1024*512))
{
	ALOGE("vaylb_test-->VideoShare construct.");
}

VideoShare::~VideoShare()
{
	ALOGE("vaylb_test-->VideoShare destruct.");
	free(mJpegAddr);
	mJpegAddr = NULL;
	mJpegSize = 0;
	//added for middle
	if(mMiddleWareFlag){
		Vector<int>::iterator curr = mSlaveSockets.begin();
		Vector<int>::iterator end = mSlaveSockets.end();
		while(curr!=end){
			close(*curr);
			curr++;
		}
	}
	if(mSendBuffer != NULL){
		delete mSendBuffer;
	}
}

void VideoShare::instantiate(sp<VideoShare> videoshare){
	defaultServiceManager()->addService(String16("mediashare"), videoshare);
	ALOGE("vaylb-->VideoShare instantiate");
}

status_t VideoShare::InitParam(const sp<IVideoShare_CallBack>& callback, sp<IMemory>* buffer, size_t framesize)  
{  
	mVsCallback = callback;

	if(mShareMemory != NULL) {
		*buffer = mShareMemory;
		return NO_ERROR;
	}
	
	mDealer = new MemoryDealer(1024*1024*14, "VideoShare");
	//CblkVideo* cblk_video;
	
	size_t size = sizeof(CblkVideo);
	size_t buffersize = framesize*VIDEO_SHARE_FRAME_COUNT; //double buffer
	size += buffersize;

	if(mDealer != NULL && mShareMemory == NULL){
		mShareMemory = mDealer->allocate(size);
		if(mShareMemory != 0){
            mShareCblk = static_cast<CblkVideo*>(mShareMemory->pointer());
		}
	}
	
	if (mShareCblk != NULL) {
		new(mShareCblk) CblkVideo(); //placement new
		mShareCblk->mFrameSize = framesize;
		mShareCblk->mMaxFrameCount = VIDEO_SHARE_FRAME_COUNT;
		for(int i = 0; i < VIDEO_SHARE_FRAME_COUNT; i++) mShareCblk->mMemoryStates[i] = can_write;
		mShareCblk->mBuffer = (char*)mShareCblk + sizeof(CblkVideo);
        memset(mShareCblk->mBuffer,0, buffersize);
	}

	*buffer = mShareMemory;

	mJpegAddr = malloc(framesize);
	memset(mJpegAddr,0,framesize);
	mJpegSize = framesize; 

	mSendAddr = malloc(framesize);
	mSendSize = 0;

	mVideoCompressor = new VideoCompressor(this);
	mVideoTransmitor = new VideoTransmitor(this);
	
	return NO_ERROR;  
}

int VideoShare::video_share_test(int num)  
{  
	ALOGE("vaylb-->[service] video_share_test pid=%d,tid=%d ,got num = %d",getpid(),gettid(),num);  
	if( mVsCallback.get() )  
	{  
		ALOGE("vaylb-->[service] call video_share_test_cb ");  
		mVsCallback->video_share_test_cb(num+1);  
	}  
	return 0;  
} 

status_t VideoShare::setUpSlaveIp(String16 ip){
	if(mSocketFd != NULL) return NO_ERROR;
	
	int res;
	mSocketFd = socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);
	if(mSocketFd == -1){
		ALOGE("vaylb-->create socket error:%d",errno); //errno13 :    Permission denied
		return -1;
	}
	ALOGE("vaylb-->create socket fd = %d",mSocketFd);
	mSlaveAddress.sin_family = AF_INET;
	mSlaveAddress.sin_addr.s_addr = inet_addr((const char*)(ip.string()));
	ALOGE("vaylb-->VideoShare slave ip = %s",(const char*)(ip.string()));
	mSlaveAddress.sin_port = htons(12305);
	mSlaveAddrLen = sizeof(mSlaveAddress);
	res = connect(mSocketFd,(struct sockaddr*)&mSlaveAddress, mSlaveAddrLen);
	if(res == -1){
		ALOGE("vaylb-->connect to slave fd = %d, error:%d",mSocketFd,errno);
		return -1;
	}
	start_threads();
	return NO_ERROR;
}

//added for middleware
void VideoShare::setUpSlaveNum(int num){
	ALOGE("vaylb-->VideoShare setUpSlaveNum to %d",num);
	if(mServerSocketFd != NULL) return;
	mMiddleWareFlag = true;

	mServerSocketFd = socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);
	if(mServerSocketFd == -1){
		ALOGE("vaylb-->create socket error:%d",errno); //errno13 :    Permission denied
		return;
	}
	
	mServerAddress.sin_family = AF_INET;
	mServerAddress.sin_addr.s_addr = htonl(INADDR_ANY);
	mServerAddress.sin_port = htons(40004);
	mServerAddrLen = sizeof(mServerAddress);

	status_t res = bind(mServerSocketFd,(struct sockaddr*)&mServerAddress,mServerAddrLen);
	if(res == -1){
		ALOGE("vaylb-->bind socket fd = %d, error:%d",mServerSocketFd,errno);
		close(mServerSocketFd);
		return;
	}
	res = listen(mServerSocketFd,5);
	if(res == -1){
		ALOGE("vaylb-->listen socket error:%d",errno);
		close(mServerSocketFd);
		return;
	}

	do{
		struct sockaddr_in 			clientAddress;
		int addrLen = sizeof(clientAddress);
		ALOGE("vaylb-->---------------waiting for device connect------------------");
		int clientFd = accept(mServerSocketFd,(struct sockaddr*)&clientAddress,&addrLen);
		ALOGE("vaylb->new video connect,fd = %d, ip = %s",clientFd,inet_ntoa(clientAddress.sin_addr));
		mSlaveSockets.push_back(clientFd);
	}while(mSlaveSockets.size() < num);
	
	start_threads();
}


void VideoShare::signalNewData(int index,int width,int height){
	Mutex::Autolock _l(mCompressLock);
	index_pool.push_back(index);
	mYUV_width = width;
	mYUV_height = height;
	mDataCompress.signal();
}

void VideoShare::signalBufferFull(){
	Mutex::Autolock _l(mCompressLock);
	mDataCompress.signal();
}


void VideoShare::resetSendFlag(){
	stop_threads();
	if(mData_u) { free(mData_u); mData_u = NULL;}
	if(mData_v) { free(mData_v); mData_v = NULL;}
	if(mData_YUV_420p) { free(mData_YUV_420p); mData_YUV_420p = NULL;}
}

void VideoShare::start_threads(){
	if(mVideoCompressor != NULL) mVideoCompressor->threadLoop_run();
	if(mVideoTransmitor != NULL) mVideoTransmitor->threadLoop_run();
}

void VideoShare::stop_threads(){
	if(mVideoCompressor != NULL) mVideoCompressor->threadLoop_exit();
	if(mVideoTransmitor != NULL) mVideoTransmitor->threadLoop_exit();
	signalNewData(0,0,0);
	mVideoCompressor->signalDataTransmitor();
}

void VideoShare::setcallback(void (*fun)(int)){
	callbackfun=fun;
}


//------------------------------------------------------------------------
//                                            VideoCompressor
//------------------------------------------------------------------------
VideoShare::VideoCompressor::VideoCompressor(VideoShare * videoshare)
	:Thread(false /*canCallJava*/),
	mVideoShare(videoshare)
{
	ALOGE("vaylb-->VideoCompressor construct.");
}

VideoShare::VideoCompressor::~VideoCompressor()
{
	ALOGE("vaylb-->VideoCompressor destruct.");
	mVideoShare.clear();
	mVideoShare = NULL;
}

bool VideoShare::VideoCompressor::threadLoop()
{
	ALOGE("vaylb-->VideoCompressor::threadLoop.");
	int compress_size = 0;
	while (!exitPending())
    {	
    	Mutex::Autolock _l(mCompressLock);
		mDataCompress.wait(mCompressLock);
		while(mVideoShare->index_pool.size()>0){
			if(exitPending()) return false;
			if(mVideoShare->mData_u == NULL) mVideoShare->mData_u = (unsigned char*)malloc(mVideoShare->mYUV_width*mVideoShare->mYUV_height/4);
			if(mVideoShare->mData_v == NULL) mVideoShare->mData_v = (unsigned char*)malloc(mVideoShare->mYUV_width*mVideoShare->mYUV_height/4);
			if(mVideoShare->mData_YUV_420p == NULL) mVideoShare->mData_YUV_420p = (unsigned char*)malloc(mVideoShare->mYUV_width*mVideoShare->mYUV_height*1.5);
			
			int index = -1;
			Vector<int>::iterator curr(mVideoShare->index_pool.begin());
			index = *curr;
			mVideoShare->index_pool.erase(curr);
			
			if(mVideoShare->mShareCblk->mMemoryStates[index] == can_read) {
				compress_size = compressToJPEG(index,mVideoShare->mYUV_width,mVideoShare->mYUV_height,(unsigned char *)(mVideoShare->mJpegAddr),&mVideoShare->mJpegSize);
				if(compress_size > 0){
					int datasize = compress_size + sizeof(compress_size);
					while(!exitPending() && mVideoShare->mSendBuffer->getWriteSpace() < datasize) {
						ALOGE("vaylb-->VideoCompressor waiting for DataBuffer can hold %d byte",datasize);
						signalDataTransmitor();
						sleep(1000000); //1ms
					}
					if(exitPending()) return false;
					
					mVideoShare->mSendBuffer->Write((char*)&compress_size,sizeof(compress_size));
					mVideoShare->mSendBuffer->Write((char*)mVideoShare->mJpegAddr,compress_size);
					signalDataTransmitor();
				}
				mVideoShare->mShareCblk->mMemoryStates[index] = can_write;
			}
		}
    }

	ALOGE("vaylb-->VideoCompressor:: threadLoop end.");
    return false;
}

int VideoShare::VideoCompressor::compressToJPEG(int buffer_id,int width,int height,unsigned char * dest_image,unsigned long* dest_size){
	
	unsigned char * image = (unsigned char *)mVideoShare->mShareCblk->mBuffer+buffer_id*mVideoShare->mShareCblk->mFrameSize;
	int size_w_h = width*height;
	
	unsigned char * uv_start = image+size_w_h;
	for(int i = 0;i < size_w_h/2;i+=2){ //u v plane
		mVideoShare->mData_u[i/2] = uv_start[i];
		mVideoShare->mData_v[i/2] = uv_start[i+1];
	}
	
	memcpy(mVideoShare->mData_YUV_420p,image,size_w_h);
	memcpy(mVideoShare->mData_YUV_420p+size_w_h,mVideoShare->mData_u,size_w_h/4);
	memcpy(mVideoShare->mData_YUV_420p+size_w_h+size_w_h/4,mVideoShare->mData_v,size_w_h/4);
	
	int jpeg_image_size = 0; 
	int i,j;

	JSAMPROW y[16],cb[16],cr[16]; // y[2][5] = color sample of row 2 and pixel column 5; (one plane)
	JSAMPARRAY data[3]; // t[0][2][5] = color sample 0 of row 2 and column 5
	struct jpeg_compress_struct cinfo;
	struct jpeg_error_mgr jerr;
	data[0] = y;
	data[1] = cb;
	data[2] = cr;
	 
	cinfo.err = jpeg_std_error(&jerr);	// errors get written to stderr
	jpeg_create_compress (&cinfo);
	cinfo.image_width = width;
	cinfo.image_height = height;
	cinfo.input_components = 3;
	jpeg_set_defaults (&cinfo);
	jpeg_set_colorspace(&cinfo, JCS_YCbCr);
	cinfo.raw_data_in = TRUE; // supply downsampled data
	cinfo.comp_info[0].h_samp_factor = 2;
	cinfo.comp_info[0].v_samp_factor = 2;
	cinfo.comp_info[1].h_samp_factor = 1;
	cinfo.comp_info[1].v_samp_factor = 1;
	cinfo.comp_info[2].h_samp_factor = 1;
	cinfo.comp_info[2].v_samp_factor = 1;
	 
	jpeg_set_quality (&cinfo, COMPRESS_QUALITY, TRUE);
	cinfo.dct_method = JDCT_FASTEST;
	
	int mem_size = size_w_h*2;
	jpeg_mem_dest(&cinfo, dest_image, mem_size);
	
	jpeg_start_compress (&cinfo, TRUE);
	 
	for (j=0;j < height;j+=16)
	{
		for (i=0;i < 16;i++)
		{
			y[i] = mVideoShare->mData_YUV_420p + width*(i+j);
			if (i%2 == 0)
			{
				cb[i/2] = mVideoShare->mData_YUV_420p + size_w_h + width/2*((i+j)/2);
				cr[i/2] = mVideoShare->mData_YUV_420p + size_w_h + size_w_h/4 + width/2*((i+j)/2);
			}
		}
		jpeg_write_raw_data (&cinfo, data, 16);
	}
	
	jpeg_finish_compress (&cinfo);
	jpeg_image_size = jpeg_mem_size(&cinfo);
	*dest_size = jpeg_image_size;
	jpeg_destroy_compress (&cinfo);
	return jpeg_image_size;
}

void VideoShare::VideoCompressor::signalDataTransmitor(){
	Mutex::Autolock _l(mTransmitLock);
	mDataTransmit.signal();
}

void VideoShare::VideoCompressor::sleep(long sleepNs){
	const struct timespec req = {0, sleepNs};
	nanosleep(&req, NULL);
}

void VideoShare::VideoCompressor::threadLoop_run(){
	run("VideoCompressor", PRIORITY_URGENT_DISPLAY);
}

void VideoShare::VideoCompressor::threadLoop_exit(){
	ALOGE("vaylb-->VideoCompressor exit");
	this->requestExit();
	//this->requestExitAndWait();
}


//------------------------------------------------------------------------
//                                            VideoTransmitor
//------------------------------------------------------------------------
VideoShare::VideoTransmitor::VideoTransmitor(VideoShare * videoshare)
	:Thread(false /*canCallJava*/),
	mVideoShare(videoshare)
{
	ALOGE("vaylb-->VideoTransmitor construct.");
}

VideoShare::VideoTransmitor::~VideoTransmitor()
{
	ALOGE("vaylb-->VideoTransmitor destruct.");
	mVideoShare.clear();
	mVideoShare = NULL;
}

bool VideoShare::VideoTransmitor::threadLoop()
{
	ALOGE("vaylb-->VideoTransmitor::threadLoop.");
	int compress_size = 0;
	while (!exitPending())
    {
    	Mutex::Autolock _l(mTransmitLock);
		mDataTransmit.wait(mTransmitLock);
		if(exitPending()) return false;
		int datasize = 0;
		while(mVideoShare->mSendBuffer->getReadSpace() > sizeof(datasize)){
			mVideoShare->mSendBuffer->Read((char*)&datasize,sizeof(datasize));
			if(datasize>0){
				while(mVideoShare->mSendBuffer->getReadSpace() < datasize){
					ALOGE("vaylb-->VideoTransmitor waiting for DataBuffer can read %d byte",datasize);
					sleep(1000000); //1ms
				}
				mVideoShare->mSendBuffer->Read((char*)mVideoShare->mSendAddr,datasize);
				if(sendData((unsigned char *)mVideoShare->mSendAddr,datasize)){
					mVideoShare->sendcount++;
				}else{
					ALOGE("vaylb-->VideoTransmitor send data error!");
				}
			}else{
				ALOGE("get frame size error");
			}
		}
    }

	ALOGE("vaylb-->VideoTransmitor:: threadLoop end.");
    return false;
}

bool VideoShare::VideoTransmitor::sendData(unsigned char * addr,int size){
	if(!mVideoShare->mMiddleWareFlag){
		int send = htonl(size);
		int res = write(mSocketFd,(const void*)&send,sizeof(size));
		if(res == -1)
		{
			ALOGE("vaylb-->-------------VideoShare write socket error %d, fd = %d, size %d-------------",errno,mSocketFd, size);
			return false;
		}

		if(socket_write(mSocketFd,addr,size) == -1){
			ALOGE("vaylb-->-------------socket write error-------------");
		}
	}else{
		Vector<int>::iterator curr = mSlaveSockets.begin();
		Vector<int>::iterator end = mSlaveSockets.end();
		int send = htonl(size+sizeof(int));
		while(curr!=end){
			int res = write(*curr,(const void*)&send,sizeof(size));
			if(res == -1)
			{
				ALOGE("vaylb-->VideoShare write socket error %d, fd = %d",errno,mSocketFd);
				return false;
			}
			socket_write(*curr,addr,size);
			curr++;
		}
	}
	return true;
}

void VideoShare::VideoTransmitor::threadLoop_run(){
	run("VideoTransmitor", PRIORITY_URGENT_DISPLAY);
}

void VideoShare::VideoTransmitor::threadLoop_exit(){
	ALOGE("vaylb-->VideoTransmitor exit");
	this->requestExit();
	//this->requestExitAndWait();
}

void VideoShare::VideoTransmitor::sleep(long sleepNs){
	const struct timespec req = {0, sleepNs};
	nanosleep(&req, NULL);
}



//--------------------------------------------------------------------------
//                            VideoShare_CallBack                      
//--------------------------------------------------------------------------

VideoShare_CallBack::VideoShare_CallBack()
{
	ALOGE("vaylb-->VideoShare_CallBack construct");
}

VideoShare_CallBack::~VideoShare_CallBack(){

}

void VideoShare_CallBack::video_share_test_cb(int num){
	ALOGE("vaylb-->VideoShare_CallBack callback num = %d",num);
	//mBufferQueue->test_friend(num);
}

}; // namespace android
