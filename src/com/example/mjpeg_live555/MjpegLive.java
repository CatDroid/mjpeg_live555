package com.example.mjpeg_live555;

public class MjpegLive {

	
	native public void start();
	
	
	static {
		System.loadLibrary("mjpeg_live");
	}
	
}