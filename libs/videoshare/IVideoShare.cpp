

#define LOG_TAG "IVideoShare"
//#define LOG_NDEBUG 0
#include <utils/Log.h>
#include <stdint.h>
#include <sys/types.h>
#include <binder/Parcel.h>
#include <videoshare/IVideoShare.h>



namespace android {


class BpVideoShare : public BpInterface<IVideoShare>
{
public:
	BpVideoShare(const sp<IBinder>& impl)
        : BpInterface<IVideoShare>(impl)
    {
    }

	virtual int video_share_test(int num) {
        Parcel data, reply;
        data.writeInterfaceToken(IVideoShare::getInterfaceDescriptor());
        data.writeInt32(num);
        remote()->transact(IVideoShare::VS_TEST, data, &reply);
		return (reply.readInt32()); 
    }

	virtual status_t InitParam(const sp<IVideoShare_CallBack>& callback, sp<IMemory>* buffer,size_t framesize)//init parameter  
	{  
		ALOGE("vaylb-->IVideoShare initParam call remote BninitParam");
		Parcel data, reply;  
		data.writeInterfaceToken(IVideoShare::getInterfaceDescriptor());
		data.writeStrongBinder(callback->asBinder()); 
		data.writeInt32(framesize);
		status_t res = remote()->transact(IVideoShare::VS_INITPARAM, data, &reply);
		if (res == NO_ERROR) {
            *buffer = interface_cast<IMemory>(reply.readStrongBinder());
        }
		return res;
	} 
	virtual void signalNewData(int index,int width,int height){
		//ALOGE("vaylb-->IVideoShare signalNewData index = %d",index);
		Parcel data, reply;  
		data.writeInterfaceToken(IVideoShare::getInterfaceDescriptor());
		data.writeInt32(index);
		data.writeInt32(width);
		data.writeInt32(height);
		remote()->transact(IVideoShare::VS_SIGNAL_NEW_DATA, data, &reply);    
	}

	virtual void signalBufferFull(){
		//ALOGE("vaylb-->IVideoShare signalNewData index = %d",index);
		Parcel data, reply;  
		data.writeInterfaceToken(IVideoShare::getInterfaceDescriptor());
		remote()->transact(IVideoShare::VS_SIGNAL_BUFFER_FULL, data, &reply);    
	}
	
	virtual status_t setUpSlaveIp(String16 ip){
		//ALOGE("vaylb-->IVideoShare signalNewData index = %d",index);
		Parcel data, reply;  
		data.writeInterfaceToken(IVideoShare::getInterfaceDescriptor());
		data.writeString16(ip);
		remote()->transact(IVideoShare::VS_SETUP_SLAVE_IP, data, &reply);   
		return (reply.readInt32());  
	}

	virtual void resetSendFlag(){
		//ALOGE("vaylb-->IVideoShare signalNewData index = %d",index);
		Parcel data, reply;  
		data.writeInterfaceToken(IVideoShare::getInterfaceDescriptor());
		remote()->transact(IVideoShare::VS_RESET_SEND_FLAG, data, &reply);
	}
	//added for middleware
	virtual void setUpSlaveNum(int num){
		//ALOGE("vaylb-->IVideoShare signalNewData index = %d",index);
		Parcel data, reply;  
		data.writeInterfaceToken(IVideoShare::getInterfaceDescriptor());
		data.writeInt32(num);
		remote()->transact(IVideoShare::VS_SETUP_SLAVE_NUM, data, &reply);   
	}

};

IMPLEMENT_META_INTERFACE(VideoShare, "android.media.IVideoShare");

// ----------------------------------------------------------------------


class BpVideoShare_CallBack : public BpInterface<IVideoShare_CallBack>
{
public:
	BpVideoShare_CallBack(const sp<IBinder>& impl)
        : BpInterface<IVideoShare_CallBack>(impl)
    {
    }

	virtual void video_share_test_cb(int num) {
		ALOGE("vaylb-->BpVideoShare_CallBack run call BnVideoShare_CallBack");
        Parcel data, reply;
        data.writeInterfaceToken(IVideoShare_CallBack::getInterfaceDescriptor());
        data.writeInt32(num);
        remote()->transact(IVideoShare_CallBack::VS_CALLBACK, data, &reply);
    }

};

IMPLEMENT_META_INTERFACE(VideoShare_CallBack, "android.media.IVideoShare_CallBack");

// ----------------------------------------------------------------------




status_t BnVideoShare::onTransact(
    uint32_t code, const Parcel& data, Parcel* reply, uint32_t flags)
{
    switch (code) {
		case VS_INITPARAM: { 
			ALOGE("vaylb-->BnVideoShare initParam run call VideoShare Server to initParam");
			CHECK_INTERFACE(IVideoShare, data, reply);
			sp<IVideoShare_CallBack> callback = interface_cast<IVideoShare_CallBack>(data.readStrongBinder());  
			size_t framesize = data.readInt32();
			sp<IMemory> buffer;
			status_t res = InitParam(callback,&buffer,framesize);
            reply->writeStrongBinder(buffer->asBinder());  
			return res;  
		} break; 
		case VS_TEST: {
			CHECK_INTERFACE(IVideoShare, data, reply);
			int num = data.readInt32(); 
			reply->writeInt32(video_share_test(num));  
			return NO_ERROR;  
		} break;
		case VS_SIGNAL_NEW_DATA: {
			CHECK_INTERFACE(IVideoShare, data, reply);
			int index = data.readInt32(); 
			int width = data.readInt32();
			int height = data.readInt32();
			signalNewData(index,width,height);
			return NO_ERROR; 
		}break;	
		case VS_SIGNAL_BUFFER_FULL: {
			CHECK_INTERFACE(IVideoShare, data, reply);
			signalBufferFull();
			return NO_ERROR; 
		}break;	
		case VS_SETUP_SLAVE_IP: {
			CHECK_INTERFACE(IVideoShare, data, reply);
			reply->writeInt32(setUpSlaveIp(data.readString16()));
			return NO_ERROR; 
		}break;	
		case VS_RESET_SEND_FLAG: {
			CHECK_INTERFACE(IVideoShare, data, reply);
			resetSendFlag();
			return NO_ERROR; 
		}break;	
		case VS_SETUP_SLAVE_NUM: {
			CHECK_INTERFACE(IVideoShare, data, reply);
			setUpSlaveNum(data.readInt32());
			return NO_ERROR; 
		}break;	
        default:
            return BBinder::onTransact(code, data, reply, flags);
    }
}

// ----------------------------------------------------------------------

status_t BnVideoShare_CallBack::onTransact(
    uint32_t code, const Parcel& data, Parcel* reply, uint32_t flags)
{
    switch (code) {
		case VS_CALLBACK: { 
			ALOGE("vaylb-->BnVideoShare_CallBack run call VideoShare_CallBack");
			CHECK_INTERFACE(IVideoShare_CallBack, data, reply);
			int num = data.readInt32(); 
			video_share_test_cb(num);  
			return NO_ERROR;  
		} break; 
        default:
            return BBinder::onTransact(code, data, reply, flags);
    }
}


}; // namespace android

