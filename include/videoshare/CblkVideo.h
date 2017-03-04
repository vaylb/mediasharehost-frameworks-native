#ifndef CBLK_MEMORY_VIDEO_H
#define CBLK_MEMORY_VIDEO_H

#include <stdint.h>
#include <strings.h>
#include <sys/cdefs.h>
#include <sys/types.h>

#define VIDEO_SHARE_FRAME_COUNT 4


#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

enum MemoryState{
	can_read = 0,
	can_write
};

struct cblk_video {
    //volatile int32_t 		mFront;
    //volatile int32_t 		mRear;
	size_t 					mFrameSize;
	int 					mMaxFrameCount;
	MemoryState				mMemoryStates[VIDEO_SHARE_FRAME_COUNT];
	int						mMemoryFilledSize[VIDEO_SHARE_FRAME_COUNT];
	void * 					mBuffer;
};
typedef struct cblk_video CblkVideo;

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif //CBLK_MEMORY_VIDEO_H

