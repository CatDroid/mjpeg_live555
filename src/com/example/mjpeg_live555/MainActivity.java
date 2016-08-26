package com.example.mjpeg_live555;

import android.app.Activity;
import android.os.Bundle;
import android.view.Menu;
import android.view.MenuItem;
import android.view.View;
import android.widget.Button;
import android.widget.Toast;

public class MainActivity extends Activity {

	private MjpegLive mMjpegLive = null ; 
	@Override
	protected void onCreate(Bundle savedInstanceState) {
		super.onCreate(savedInstanceState);
		setContentView(R.layout.activity_main);
		
		Button btn = (Button)findViewById(R.id.bStart);
		btn.setOnClickListener( new View.OnClickListener() {
			
			@Override
			public void onClick(View v) {
				if(mMjpegLive == null){
					mMjpegLive = new MjpegLive();
					mMjpegLive.start();	
				}else{
					Toast.makeText(MainActivity.this, "started", Toast.LENGTH_LONG).show();
				}
			}
		});
	}


}
