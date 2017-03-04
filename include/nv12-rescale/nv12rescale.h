#ifndef ANDROID_MEDIA_RESCALE_YUV_NV12_H
#define ANDROID_MEDIA_RESCALE_YUV_NV12_H

#include <utils/threads.h>

namespace android {

class nv12rescale : public virtual RefBase{
public:

            	 nv12rescale();
protected:
          virtual ~nv12rescale();
public:
				void 		nv12_nearest_scale(unsigned char* src, unsigned char*  dst, int srcWidth, int srcHeight, int dstWidth, int dstHeight);
				void 		nv12_bilinear_scale (unsigned char* src, unsigned char* dst, int srcWidth, int srcHeight, int dstWidth,int dstHeight);
				int 		ImageResize(unsigned char * src, unsigned char* dst, int sw,int sh,int dw,int dh);
};

};

#endif // ANDROID_MEDIA_RESCALE_YUV_NV12_H

