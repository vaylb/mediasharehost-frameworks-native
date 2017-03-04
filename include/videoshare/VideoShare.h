

#ifndef ANDROID_MEDIA_VIDEO_SHARE_H
#define ANDROID_MEDIA_VIDEO_SHARE_H

#include <utils/threads.h>
#include <utils/Debug.h>
#include <utils/Thread.h>
#include <pthread.h>

//vaylb for socket
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <videoshare/IVideoShare.h>
//for IMemory
#include <binder/MemoryDealer.h>
#include <binder/IMemory.h>
#include <videoshare/CblkVideo.h>
//for databuffer
#include <videoshare/DataBuffer.h>




/* Venus NV12:
	 * YUV 4:2:0 image with a plane of 8 bit Y samples followed
	 * by an interleaved U/V plane containing 8 bit 2x2 subsampled
	 * colour difference samples.
	 *
	 * <-------- Y/UV_Stride -------->
	 * <------- Width ------->
	 * Y Y Y Y Y Y Y Y Y Y Y Y X X X X  ^          ^
	 * Y Y Y Y Y Y Y Y Y Y Y Y X X X X  |           |
	 * Y Y Y Y Y Y Y Y Y Y Y Y X X X X  Height    |
	 * Y Y Y Y Y Y Y Y Y Y Y Y X X X X  |          Y_Scanlines
	 * Y Y Y Y Y Y Y Y Y Y Y Y X X X X  |           |
	 * Y Y Y Y Y Y Y Y Y Y Y Y X X X X  |           |
	 * Y Y Y Y Y Y Y Y Y Y Y Y X X X X  |           |
	 * Y Y Y Y Y Y Y Y Y Y Y Y X X X X  V           |
	 * X X X X X X X X X X X X X X X X              |
	 * X X X X X X X X X X X X X X X X              |
	 * X X X X X X X X X X X X X X X X              |
	 * X X X X X X X X X X X X X X X X              V
	 * U V U V U V U V U V U V X X X X  ^
	 * U V U V U V U V U V U V X X X X  |
	 * U V U V U V U V U V U V X X X X  |
	 * U V U V U V U V U V U V X X X X  UV_Scanlines
	 * X X X X X X X X X X X X X X X X  |
	 * X X X X X X X X X X X X X X X X  V
	 * X X X X X X X X X X X X X X X X  --> Buffer size alignment
	 *
	 * Y_Stride : Width aligned to 128
	 * UV_Stride : Width aligned to 128
	 * Y_Scanlines: Height aligned to 32
	 * UV_Scanlines: Height/2 aligned to 16
	 * Total size = align((Y_Stride * Y_Scanlines
	 *          + UV_Stride * UV_Scanlines + 4096), 4096)
	 */

namespace android {

class VideoShare : public BnVideoShare
{
public:

            	VideoShare();
	static void instantiate(sp<VideoShare> videoshare);  
	
	virtual status_t InitParam(const sp<IVideoShare_CallBack>& callback, sp<IMemory>* buffer,size_t framesize) ;//init parameter
	virtual int video_share_test(int num);

protected:
          virtual ~VideoShare();
public:
				status_t 	setUpSlaveIp(String16 ip);
				void		signalNewData(int index,int width, int height);
				void		signalBufferFull();
				void		resetSendFlag();
				
				void 		start_threads();
				void 		stop_threads();
				//added for middleware
				void 	setUpSlaveNum(int num);
				void setcallback(void (*fun)(int));
				void  (*callbackfun)(int);

public:
	sp<IVideoShare_CallBack>	mVsCallback;
	
	int 						mSlaveAddrLen;
	struct sockaddr_in 			mSlaveAddress;
	
	sp<MemoryDealer> 			mDealer;
	sp<IMemory>					mShareMemory;
	CblkVideo*					mShareCblk;
	void*						mShareAddr;
	
	void*						mJpegAddr; //compress YUV to JPEG then sort here
	unsigned long				mJpegSize;
	
	void*						mSendAddr; //sort the data which are going to send
	unsigned long				mSendSize;
	//bool						mSendUsing;
	//for temp use to format yuv_420_sp to yuv_420_p
	unsigned char*				mData_u;  //u plane data width*height/4
	unsigned char*				mData_v;  //v plane data width*height/4
	unsigned char*				mData_YUV_420p;
	int							mYUV_width;
	int							mYUV_height;
	Vector<int> 				index_pool;

	//added for mark middleware
	bool						mMiddleWareFlag;

	//bool 						mflag;
	int							sendcount;
	DataBuffer*					mSendBuffer;


	class VideoCompressor: public Thread {

	public:
								VideoCompressor(VideoShare* videoshare);
    	virtual 				~VideoCompressor();

    	virtual     bool        threadLoop();
					int			compressToJPEG(int buffer_id,int width, int height,unsigned char* dest_image,unsigned long* dest_size);
					void        threadLoop_exit();
					void        threadLoop_run();
					void		signalDataTransmitor();
					void		sleep(long time);

	private:
		sp<VideoShare>		mVideoShare;
	};  // class VideoCompressor

	class VideoTransmitor : public Thread {

	public:
			VideoTransmitor(VideoShare* host);
    		virtual ~VideoTransmitor();
		    virtual     bool        threadLoop();
			bool		sendData(unsigned char * addr,int size);
			void        threadLoop_exit();
			void        threadLoop_run();
			void		sleep(long time);

	private:
		sp<VideoShare>		mVideoShare;
	};  // class VideoTransmitor

	private:
		sp<VideoCompressor>		mVideoCompressor;
		sp<VideoTransmitor>		mVideoTransmitor;
	
};

//----------------------------------------------------------------------------------------

class VideoShare_CallBack: public BnVideoShare_CallBack 
{  
public:  
	virtual void video_share_test_cb(int num);//call from service   

				VideoShare_CallBack();  
	virtual 	~VideoShare_CallBack();  
	//sp<BufferQueue> mBufferQueue;
};  



}; // namespace android

#endif // ANDROID_MEDIA_VIDEO_SHARE_H
