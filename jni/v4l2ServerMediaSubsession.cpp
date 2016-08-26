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


#include "JPEGVideoRTPSink.hh"
#include "v4l2JPEGDeviceSource.hh"

#include "v4l2ServerMediaSubsession.h"
#include "debug.h"

videoServerMediaSubsession* videoServerMediaSubsession::createNew(UsageEnvironment& env, Boolean reuseFirstSource) {
	return new videoServerMediaSubsession(env, reuseFirstSource);
}

videoServerMediaSubsession::videoServerMediaSubsession(UsageEnvironment& env, Boolean reuseFirstSource)
					: OnDemandServerMediaSubsession(env, reuseFirstSource) {
	DBG("videoServerMediaSubsession construct!");
}

videoServerMediaSubsession::~videoServerMediaSubsession() {
	DBG("videoServerMediaSubsession deconstruct!");
}

FramedSource* videoServerMediaSubsession::createNewStreamSource(unsigned /*clientSessionId*/, unsigned& estBitrate) {

	DBG("videoServerMediaSubsession createNewStreamSource!");
	estBitrate = 250000; // kbps   ?? 24829byte/f * 15fps * 8bits = 2979480
	return v4l2JPEGDeviceSource::createNew(envir());
}


RTPSink* videoServerMediaSubsession::createNewRTPSink(Groupsock* rtpGroupsock,
								  unsigned char rtpPayloadTypeIfDynamic,
								  FramedSource* /*inputSource*/) {
	DBG("videoServerMediaSubsession createNewRTPSink!");
	// Fix BUG v4l2JPEGDeviceSource::doGetNextFrame fMax is smaller than payload
	OutPacketBuffer::maxSize = 100000;
	return JPEGVideoRTPSink::createNew(envir(), rtpGroupsock);
}


