/*******************************************************************************
#                                                                              #
#                                                                              #		
#      Copyright 2012 Mark Majeres (slug_)  mark@engine12.com	               #
#                                                                              #
# This program is free software; you can redistribute it and/or modify         #
# it under the terms of the GNU General Public License as published by         #
# the Free Software Foundation; version 2 of the License.                      #
#                                                                              #
# This program is distributed in the hope that it will be useful,              #
# but WITHOUT ANY WARRANTY; without even the implied warranty of               #
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the                #
# GNU General Public License for more details.                                 #
#                                                                              #
# You should have received a copy of the GNU General Public License            #
# along with this program; if not, write to the Free Software                  #
# Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA    #
#                                                                              #
*******************************************************************************/

#include "v4l2JPEGDeviceSource.hh"
#include <sys/ioctl.h>
#include <time.h>


#include <fcntl.h>
#include <pthread.h>
#include <errno.h>


#include "debug.h"
v4l2JPEGDeviceSource* v4l2JPEGDeviceSource::createNew(UsageEnvironment& env) 
{
  return new v4l2JPEGDeviceSource(env);
}

v4l2JPEGDeviceSource::v4l2JPEGDeviceSource(UsageEnvironment& env):JPEGVideoSource(env) 
{
	mFramecountTotal = 0 ;
	mFramecount = 0 ;
	mlast_time_statistics = 0 ;
	mlast_time = 0; // unit:ms

	mfd   = -1 ;
	mSize = -1 ;
	int ret = 0 ;
	mfd = ::open("/mnt/sdcard/test.jpg",O_RDONLY);
	if(mfd < 0){
		DBG("open fail with %d , %s" , errno , strerror(errno) );
		return ;
	}
	mSize = ::lseek(mfd, 0 , SEEK_END);
	mBuffer = (unsigned char*)malloc( mSize);
	::lseek(mfd, 0 , SEEK_SET);
	DBG("open with mfd %d, filesize %d, mBuffer %p ", mfd , mSize , mBuffer);

	ret = read(mfd,mBuffer,mSize);
	if(ret < 0 ){
		DBG("open fail with %d , %s" , errno , strerror(errno) );
		free(mBuffer);
		mBuffer = NULL;
		::close(mfd);
		return ;
	}

	DBG("read with mfd %d, filesize %d, mBuffer %p ", mfd , ret , mBuffer);

	::close(mfd);


	if(server.quality >= 128){
		mQuantizationTables = (unsigned char*)malloc( 8 * (8*8) * 2 ); // 精度8bits*(量化表矩阵8*8)*两个量化表
	}else{
		mQuantizationTables = NULL;
	}
}

v4l2JPEGDeviceSource::~v4l2JPEGDeviceSource() {
  

	if(mBuffer != NULL){
		DBG("~v4l2JPEGDeviceSource free mBuffer ");
		free(mBuffer);
		mBuffer = NULL;
	}

	if(server.quality >= 128){
		DBG("~v4l2JPEGDeviceSource free mQuantizationTables ");
		free(mQuantizationTables);
		mQuantizationTables = NULL;
	}

}


void v4l2JPEGDeviceSource::doGetNextFrame() {




	unsigned char *frame = mBuffer ;
	unsigned int frame_size = mSize  ;




	int sz_Header = setParamsFromHeader(frame,frame_size);
	unsigned int szPayload= frame_size - sz_Header;

	//DBG("frame = %p , frame_size = %d , sz_Header = %d , szPayload = %d" ,
	//		mBuffer , mSize , sz_Header , szPayload );

#define CHK_FRAMERATE_CONTROL
#ifdef CHK_FRAMERATE_CONTROL
#define FRAME_INTERVAL_STATISTICS 100 // unit:frame
#define FRAME_INTERVAL 50  //unit:ms 1000/FRAME_INTERVAL = 20fps


	struct timeval start ;
	gettimeofday( &start, NULL );
	const u_int32_t time_ms = 1000 * start.tv_sec  +   start.tv_usec / 1000; // unit:ms

	if(mlast_time_statistics == 0){ // initial
		DBG("initial statistics");
		mlast_time = mlast_time_statistics  = time_ms ;// unit:ms
	}

	if(mFramecount++ == FRAME_INTERVAL_STATISTICS){
		mFramecount = 0;
		DBG("fps = %d " ,  FRAME_INTERVAL_STATISTICS * 1000 / (time_ms - mlast_time_statistics) );
		mlast_time_statistics = time_ms ;
	}


	if(  (time_ms > mlast_time) && (time_ms -  mlast_time < FRAME_INTERVAL) ){
		usleep( (FRAME_INTERVAL - (time_ms - mlast_time)) * 1000 );
	}

	gettimeofday( &start, NULL );
	mlast_time = 1000 * start.tv_sec  +   start.tv_usec / 1000  ;

	mFramecountTotal++ ;
	DBG("mFramecountTotal = %llu" , mFramecountTotal);
#endif

	gettimeofday(&fPresentationTime, NULL);

	if (szPayload > fMaxSize) {
		DBG("Err : fMaxSize = %d  , szPayload = %d " , fMaxSize  , szPayload   );
		DBG("v4l2JPEGDeviceSource::doGetNextFrame(): "
					"read maximum buffer size: %d bytes."
					"  Frame may be truncated\n", fMaxSize);
		szPayload = fMaxSize;
	}
	memmove(fTo, frame+sz_Header, szPayload);
	fFrameSize = szPayload;

	nextTask() = envir().taskScheduler().scheduleDelayedTask(0, (TaskFunc*)FramedSource::afterGetting, this);
}


u_int8_t v4l2JPEGDeviceSource::type() 
{
	return mColorspace;
}



u_int8_t v4l2JPEGDeviceSource::width() 
{
	//DBG( "v4l2JPEGDeviceSource::width(): %d\n", fLastWidth);
	return fLastWidth;
}

u_int8_t v4l2JPEGDeviceSource::height() 
{
	//DBG( "v4l2JPEGDeviceSource::height(): %d\n", fLastHeight);
	return fLastHeight;
}

// test_quantization 从保存的jpeg文件中获得的  不包含量化表头,e.g 精度
u_int8_t test_quantization[] = {

0x08, 0x06, 0x06,  0x07,  0x06,  0x05,  0x08,  0x07,  0x07, 0x07, 0x09, 0x09, 0x08, 0x0A, 0x0C,
0x14, 0x0D, 0x0C,  0x0B,  0x0B,  0x0C,  0x19,  0x12,  0x13, 0x0F, 0x14, 0x1D, 0x1A, 0x1F, 0x1E, 0x1D,
0x1A, 0x1C, 0x1C,  0x20,  0x24,  0x2E,  0x27,  0x20,  0x22, 0x2C, 0x23, 0x1C, 0x1C, 0x28, 0x37, 0x29,
0x2C, 0x30, 0x31,  0x34,  0x34,  0x34,  0x1F,  0x27,  0x39, 0x3D, 0x38, 0x32, 0x3C, 0x2E, 0x33, 0x34,
0x32,

0x01, 0x09, 0x09 , 0x09 , 0x0C , 0x0B , 0x0C , 0x18  , 0x0D , 0x0D , 0x18 , 0x32 , 0x21 , 0x1C , 0x21,
0x32, 0x32, 0x32 , 0x32 , 0x32 , 0x32 , 0x32 , 0x32  , 0x32 , 0x32 , 0x32 , 0x32 , 0x32 , 0x32 , 0x32 , 0x32,
0x32, 0x32, 0x32 , 0x32 , 0x32 , 0x32 , 0x32 , 0x32  , 0x32 , 0x32 , 0x32 , 0x32 , 0x32 , 0x32 , 0x32 , 0x32,
0x32, 0x32, 0x32 , 0x32 , 0x32 , 0x32 , 0x32 , 0x32  , 0x32 , 0x32 , 0x32 , 0x32 , 0x32 , 0x32 , 0x32 , 0x32,
0x32
};

u_int8_t v4l2JPEGDeviceSource::qFactor()
{
	return server.quality;
}

u_int8_t const* v4l2JPEGDeviceSource::quantizationTables(u_int8_t& precision,
						    u_int16_t& length) {

	if(server.quality >= 128){
		precision = 8  ; 					// Fixed 8bit
		length = 2 * 64 ; 					// Fixed 固定两个量化表 2*(8*8)  8*8是量化表矩阵(数组)
		return mQuantizationTables;
	}
	// Default implementation
	precision = 0;
	length = 0;
	return NULL;
}


//Gets the JPEG size from the array of data passed to the function 
//and returns the size of the JPEG header in bytes
int v4l2JPEGDeviceSource::setParamsFromHeader(unsigned char* data, unsigned int data_size) {
	//Check for valid JPEG image

	unsigned int i=0;
	unsigned int length = 0 ;
	bool foundScan = false;
	mpQuantizationTables = 0;
	//if(data[i] == 0xFF && data[i+1] == 0xD8 && data[i+2] == 0xFF && data[i+3] == 0xDB)
	if(data[0] == 0xFF && data[1] == 0xD8 ) // SOI
    {
		i += 2;
		while( i < data_size && !foundScan )
		{
			if( data[i] != 0xFF ) {
				break;
			}

			switch (data[i+1]) {
				case 0xE0:  // 0xFFE0 APP0
					break;
				case 0xDB:  // 0xFFDB DQT  量化表
					if(mQuantizationTables != NULL ){
						if( mpQuantizationTables == 0 ){
							strncpy((char*)mQuantizationTables , (char*)data+i+5 ,  64);
							mpQuantizationTables = 64 ;
						}else if(mpQuantizationTables == 64 ){
							strncpy((char*)mQuantizationTables + 64  , (char*)data+i+5 ,  64);
							mpQuantizationTables = 128 ;
						}else{
							DBG("QuantizationTables is Too Large");
						}
					}

					break;
				case 0xC0:  // 0xFFC0 SOF Start of Frame
					{
						u_int16_t  height =  (data[i+5] << 8) + data[i+6] ;
						u_int16_t  width =   (data[i+7] << 8) + data[i+8] ;
						//DBG("height = %d , width = %d " , height , width);
						fLastHeight = height / 8 ; // 8 pixel align
						fLastWidth = width / 8 ;
						/*
						 *  see JPEGVideoSource
							virtual u_int8_t width() = 0; // # pixels/8 (or 0 for 2048 pixels)
							virtual u_int8_t height() = 0; // # pixels/8 (or 0 for 2048 pixels)
						 * */
						if( data[i+9] == 0x3 ) {// YCrCb颜色空间 应该为 3
							// 水平/垂直采样因子 1个字节 高4位代表水平采样因子 低4位代表垂直采样因子
							// data[i+10]  ID
							// DBG("colorID = 0x%02x , Y = 0x%02x", data[i+10] , data[i+11]);
							if( data[i+11] == 0x21){ // YUV422
								mColorspace = 0;
							}else if( data[i+11] == 0x22 ){// YUV420
								mColorspace = 1;
							}else{
								DBG("live unsupported format"); // 不是YUV420或者YUV422
							}
						}else{
							DBG("live unsupported format"); // 不是YCrCb颜色空间
						}
					}

					break;
				case 0xC4:	// 0xFFC4 DHT   哈夫曼表
					break;
				case 0xDD:  // 0xFFDD DRI，Define Restart Interval 编码复位间隔
					break;
				case 0xDA:  // 0xFFDA SOS/Start of Scan 一个scan开始
					foundScan = true;
					break;
				default:
					DBG("unkown TagID 0x%02x , please check it" ,data[i+1] );
					break;
			}
			length =  (data[i+2] << 8) + data[i+3] ;
			i += length + 2; // TagId 2个字节  标记段+长度=整个标记端大小
		}

	}
	// else{}  // NOT a valid SOI Start Of Image

	if( foundScan ) {
		return i ;
	}
	return -1 ;
}
