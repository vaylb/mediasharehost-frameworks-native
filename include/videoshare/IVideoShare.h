
#ifndef ANDROID_GUI_IVIDEOSHARE_H
#define ANDROID_GUI_IVIDEOSHARE_H

#include <stdint.h>
#include <sys/types.h>

#include <utils/RefBase.h>
#include <utils/Errors.h>
#include <binder/IInterface.h>
#include <binder/IMemory.h>
#include <utils/LinearTransform.h>
#include <utils/String8.h> 

namespace android {

// ----------------------------------------------------------------------------

class IVideoShare_CallBack : public IInterface
{
public:  
	enum {  
		VS_CALLBACK = IBinder::FIRST_CALL_TRANSACTION,    
	};
public:
    DECLARE_META_INTERFACE(VideoShare_CallBack);
	//virtual status_t		open(int* count, sp<IMemory>* buffer1, sp<IMemory>* buffer2) = 0;
	//virtual void        setstartflag(int flag) = 0;
	//virtual void        write_I() = 0;
	//vaylb added  for offload
	//virtual ssize_t        write_I_offload() = 0;
	//virtual bool        checkstandby() = 0;
	//virtual bool        checkexit() = 0;
	//virtual void        standby_I() = 0;
	//virtual void        time_delay_host(long time) = 0;

	//add pzhao for check track become ready
	//virtual bool     	needcheckwrited()=0;
	virtual void     	video_share_test_cb(int num)=0;
};

// ----------------------------------------------------------------------------

class IVideoShare : public IInterface
{
public:
	enum {  
		VS_TEST = IBinder::FIRST_CALL_TRANSACTION,   
		VS_INITPARAM,
		VS_SIGNAL_NEW_DATA,
		VS_SIGNAL_BUFFER_FULL,
		VS_SETUP_SLAVE_IP,
		VS_RESET_SEND_FLAG,
		VS_SETUP_SLAVE_NUM
	};
public:
    DECLARE_META_INTERFACE(VideoShare);
	//virtual status_t		open(int* count, sp<IMemory>* buffer1, sp<IMemory>* buffer2) = 0;
	//virtual void        setstartflag(int flag) = 0;
	//virtual void        write_I() = 0;
	//vaylb added  for offload
	//virtual ssize_t        write_I_offload() = 0;
	//virtual bool        checkstandby() = 0;
	//virtual bool        checkexit() = 0;
	//virtual void        standby_I() = 0;
	//virtual void        time_delay_host(long time) = 0;

	//add pzhao for check track become ready
	//virtual bool     	needcheckwrited()=0;
	virtual int     		video_share_test(int num)=0;
	virtual status_t 		InitParam(const sp<IVideoShare_CallBack>& callback, sp<IMemory>* buffer,size_t framesize) = 0;//init parameter 
	virtual void			signalNewData(int index,int width,int height) = 0;
	virtual void			signalBufferFull() = 0;
	virtual status_t		setUpSlaveIp(String16 ip) = 0;
	virtual void			resetSendFlag() = 0;
	virtual void			setUpSlaveNum(int num) = 0;
};

// ----------------------------------------------------------------------------

class BnVideoShare_CallBack: public BnInterface<IVideoShare_CallBack>
{
public:
    virtual status_t    onTransact( uint32_t code,
                                    const Parcel& data,
                                    Parcel* reply,
                                    uint32_t flags = 0);
};

// ----------------------------------------------------------------------------
class BnVideoShare: public BnInterface<IVideoShare>
{
public:
    virtual status_t    onTransact( uint32_t code,
                                    const Parcel& data,
                                    Parcel* reply,
                                    uint32_t flags = 0);
};

// ----------------------------------------------------------------------------

}; // namespace android

#endif // ANDROID_GUI_IVIDEOSHARE_H