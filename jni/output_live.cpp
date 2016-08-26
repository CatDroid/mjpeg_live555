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
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <linux/videodev2.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <signal.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <getopt.h>
#include <pthread.h>
#include <fcntl.h>
#include <time.h>
#include <syslog.h>

#include <dirent.h>

#include "debug.h"

#include "liveMedia.hh"
#include "GroupsockHelper.hh"
#include "BasicUsageEnvironment.hh"
#include "UsageEnvironment.hh"

#define OUTPUT_PLUGIN_NAME "Live555 output plugin"

static pthread_t worker;

UsageEnvironment* env;

struct context{
    int port;
    char *credentials;
    bool bMultiCast;
	int quality;
	int tunnel;
	bool bStreamAudio;
	bool bStreamVideo;
} server;

// A structure to hold the state of the current session.
// It is used in the "afterPlaying()" function to clean up the session.
struct sessionState_t {
  FramedSource* audiosource;
  FramedSource* videosource;
  RTPSink* audioSink;
  RTPSink* videoSink;
  RTCPInstance* rtcpAudioInstance;
  RTCPInstance* rtcpVideoInstance;
  Groupsock* rtpGroupsockAudio;
  Groupsock* rtpGroupsockVideo;
  Groupsock* rtcpGroupsockAudio;
  Groupsock* rtcpGroupsockVideo;
  RTSPServer* rtspServer;
} sessionState;

#include "v4l2ServerMediaSubsession.cpp"
#include "v4l2JPEGDeviceSource.cpp"

//#define CONVERT_TO_SPEEX
//#ifdef CONVERT_TO_SPEEX
//#include "speexFromPCMAudioSource.cpp"
//#endif

int output_init();
int output_stop(int id);
int output_run(int id);
int output_cmd(int plugin, unsigned int control_id, unsigned int group, int value);


// To make the second and subsequent client for each stream reuse the same
// input stream as the first client (rather than playing the file from the
// start for each client), change the following "False" to "True":
Boolean reuseFirstSource = true;


/******************************************************************************
Description.: clean up allocated ressources
Input Value.: unused argument
Return Value: -
******************************************************************************/
void worker_cleanup(void *arg)
{
    static unsigned char first_run = 1;

    if(!first_run) {
        DBG("already cleaned up ressources\n");
        return;
    }

    first_run = 0;
    DBG("cleaning up ressources allocated by worker thread\n");
}

void play(); // forward
void listenRTSP();
void afterPlaying(void* clientData); // forward


/******************************************************************************
Description.: this is the main worker thread
              it loops forever, grabs a fresh frame and stores it to file
Input Value.:
Return Value:
******************************************************************************/
void *worker_thread(void *arg)
{
    /* set cleanup handler to cleanup allocated ressources */
    pthread_cleanup_push(worker_cleanup, NULL);

	// Begin by setting up our usage environment:
	TaskScheduler* scheduler = BasicTaskScheduler::createNew();
	env = BasicUsageEnvironment::createNew(*scheduler);
	
	if(server.bMultiCast == true)
		play();
	else
		listenRTSP();

    pthread_cleanup_pop(1);

    return NULL;
}

/*** plugin interface functions ***/
/******************************************************************************
Description.: this function is called first, in order to initialize
              this plugin and pass a parameter string
Input Value.: parameters
Return Value: 0 if everything is OK, non-zero otherwise
******************************************************************************/
int output_init()
{
    server.port = 8554;	
	server.credentials = NULL;
    server.bMultiCast = false;
	server.quality = 90;	// 	>=128 use Customize QuantizationTables
							//  0~99  Standard QuantizationTables

	server.tunnel = -1;
	server.bStreamAudio = false;
	server.bStreamVideo = true;
	
    return 0;
}

/******************************************************************************
Description.: calling this function stops the worker thread
Input Value.: -
Return Value: always 0
******************************************************************************/
int output_stop(int id)
{
    DBG("will cancel worker thread\n");
    //pthread_cancel(worker);
    return 0;
}


/******************************************************************************
Description.: calling this function creates and starts the worker thread
Input Value.: -
Return Value: always 0
******************************************************************************/
int output_run(int id)
{
    DBG("launching worker thread\n");
    pthread_create(&worker, 0, worker_thread, NULL);
    pthread_detach(worker);
    return 0;
}

int output_cmd(int plugin, unsigned int control_id, unsigned int group, int value)
{
    DBG("command (%d, value: %d) for group %d triggered for plugin instance #%02d\n", control_id, value, group, plugin);
    return 0;
}






void play() {
	
	// Create 'groupsocks' for RTP and RTCP:
	struct in_addr destinationAddress;
	destinationAddress.s_addr = chooseRandomIPv4SSMAddress(*env);
	const unsigned char ttl = 255;
		
	const unsigned maxCNAMElen = 100;
	unsigned char CNAME[maxCNAMElen+1];
	gethostname((char*)CNAME, maxCNAMElen);
	CNAME[maxCNAMElen] = '\0'; // just in case

	// 广播时候 rtp端口是 8888  rtcp端口是 8888+1
	const unsigned short rtpPortNumVideo = 8888;
	const unsigned short rtcpPortNumVideo = rtpPortNumVideo+1;

	const Port rtpPortVideo(rtpPortNumVideo);
	const Port rtcpPortVideo(rtcpPortNumVideo);

	sessionState.rtpGroupsockVideo 	= new Groupsock(*env, destinationAddress, rtpPortVideo, ttl);
	sessionState.rtcpGroupsockVideo	= new Groupsock(*env, destinationAddress, rtcpPortVideo, ttl);

	sessionState.rtpGroupsockVideo->multicastSendOnly();
	sessionState.rtcpGroupsockVideo->multicastSendOnly();

	unsigned timePerFrame = 1000000/30; // in microseconds (at 30 fps)
	unsigned const averageFrameSizeInBytes = 35000; // estimate
	const unsigned estimatedSessionBandwidthVideo = (8*1000*averageFrameSizeInBytes)/timePerFrame;

	// 创建 一个rtp 和 rtcp
	sessionState.videoSink = JPEGVideoRTPSink::createNew(*env, sessionState.rtpGroupsockVideo);

	sessionState.rtcpVideoInstance = RTCPInstance::createNew(*env, sessionState.rtcpGroupsockVideo,
														  estimatedSessionBandwidthVideo, CNAME,
														  sessionState.videoSink, NULL /* we're a server */,
														  True /* we're a SSM source*/);


	// 创建一个rtsp服务 并创建 一个session 和 一个video sub-session 对应资源
	sessionState.rtspServer = RTSPServer::createNew(*env, server.port);
	if (sessionState.rtspServer == NULL) {
		*env << "Failed to create RTSP server: " << env->getResultMsg() << "\n";
		return;
	}
	ServerMediaSession* sms = ServerMediaSession::createNew(*env, "mjpg_streamer", "mjpg_streamer",
															"Session streamed by v4l2 camera", 
															True/*SSM*/);

	//  PassiveServerMediaSubsession(RTPSink& rtpSink, RTCPInstance* rtcpInstance);
	//
	RTPSink& rtpsink = *sessionState.videoSink;
	RTCPInstance* rtcpinstance =  sessionState.rtcpVideoInstance ;
	sms->addSubsession(
			(ServerMediaSubsession*)PassiveServerMediaSubsession::createNew(
					rtpsink ,rtcpinstance )
	);
	sessionState.rtspServer->addServerMediaSession(sms);

	// 打印这个session的url
	char* url = sessionState.rtspServer->rtspURL(sms);
	DBG("listenRTSP url = %s " , url );
	delete[] url;


	// 创建一个source  作为 服务端 视频数据来源
	sessionState.videosource = v4l2JPEGDeviceSource::createNew(*env);
	if (sessionState.videosource == NULL) {
		*env << "Unable to open camera: "
		 << env->getResultMsg() << "\n";
		return;
	}

	sessionState.videoSink->startPlaying(*sessionState.videosource, afterPlaying, sessionState.videoSink);

  
	env->taskScheduler().doEventLoop( );
}


void afterPlaying(void* /*clientData*/) {

  // One of the sinks has ended playing.
  // Check whether any of the sources have a pending read.  If so,
  // wait until its sink ends playing also:
  if (sessionState.audiosource->isCurrentlyAwaitingData()
      || sessionState.videosource->isCurrentlyAwaitingData()) return;

  // Now that both sinks have ended, close both input sources,
  // and start playing again:

		
	*env << "...done streaming\n";
	
	sessionState.audioSink->stopPlaying();
	sessionState.videoSink->stopPlaying();
  
	// End by closing the media:
	Medium::close(sessionState.rtspServer);
	Medium::close(sessionState.audioSink);
	Medium::close(sessionState.videoSink);
	delete sessionState.rtpGroupsockAudio;
	delete sessionState.rtpGroupsockVideo;
	Medium::close(sessionState.audiosource);
	Medium::close(sessionState.videosource);
	Medium::close(sessionState.rtcpAudioInstance);
	Medium::close(sessionState.rtcpVideoInstance);
	delete sessionState.rtcpGroupsockAudio;
	delete sessionState.rtcpGroupsockVideo;

}


void listenRTSP() {

	// 创建RTSP服务
	UserAuthenticationDatabase* authDB = NULL;
	RTSPServer* rtspServer = RTSPServer::createNew(*env, server.port, authDB , 0);
	if (rtspServer == NULL) {
		DBG("Failed to create RTSP server: %s" , env->getResultMsg() );
		return;
	}

	
	// 创建一个session 并创建一个sub_session关联 这样 服务器就有一个url资源(DESCIRBE返回 )
	ServerMediaSession* sms = ServerMediaSession::createNew(*env, "mjpg_streamer", "mjpg_streamer",
															"Session streamed by v4l2 camera");
	Boolean add_done = sms->addSubsession((ServerMediaSubsession*)videoServerMediaSubsession::createNew(*env, reuseFirstSource));



	rtspServer->addServerMediaSession(sms);

	// 打印这个session的url
	char* url = rtspServer->rtspURL(sms);
	DBG("listenRTSP url = %s , add_done = %d " , url ,  add_done  );
	delete[] url;

	
	env->taskScheduler().doEventLoop(); // does not return

}



#ifdef __cplusplus
extern "C" {
#endif
/*
 * Class:     com_example_mjpeg_live555_MjpegLive
 * Method:    start
 * Signature: ()V
 */
#include <jni.h>

JNIEXPORT void JNICALL Java_com_example_mjpeg_1live555_MjpegLive_start
  (JNIEnv * env , jobject jobj ){
	DBG("mjpeg live start");
	output_init();
	output_run(0);
}

#ifdef __cplusplus
}
#endif
