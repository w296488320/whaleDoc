package com.example.myapplication.utils;

import android.util.Log;



public class CLogUtils {



	/**
	 * Priority constant for the println method; use Log.v.
	 */
	public static final int VERBOSE = 2;

	/**
	 * Priority constant for the println method; use Log.d.
	 */
	public static final int DEBUG = 3;

	/**
	 * Priority constant for the println method; use Log.i.
	 */
	public static final int INFO = 4;

	/**
	 * Priority constant for the println method; use Log.w.
	 */
	public static final int WARN = 5;

	/**
	 * Priority constant for the println method; use Log.e.
	 */
	public static final int ERROR = 6;

	/**
	 * Priority constant for the println method.
	 */
	public static final int ASSERT = 7;

	public static final int LOG_ID_MAIN = 0;


//	public static int Error(String tag, String msg) {
//		return println_native(LOG_ID_MAIN, ERROR, tag, msg);
//	}



	//规定每段显示的长度
	private static int LOG_MAXLENGTH = 5000;

	private static String TAG = "XposedInto";
	private static String NetTAG = "XposedNet";
	public static void e(String msg){
				InfiniteLog(TAG, msg);
	}


	public static void NetLogger(String msg) {
			InfiniteLog(NetTAG, msg);
	}

	public static void e(String TAG, String msg){
			InfiniteLog(TAG, msg);
	}

	/**
	 * log最多 4*1024 长度 这个 方法 可以解决 这个问题
	 * @param TAG
	 * @param msg
	 */
	private static void InfiniteLog(String TAG, String msg) {
		int strLength = msg.length();
		int start = 0;
		int end = LOG_MAXLENGTH;
		for (int i = 0; i < 100; i++) {
			//剩下的文本还是大于规定长度则继续重复截取并输出
			if (strLength > end) {
				Log.e(TAG + i, msg.substring(start, end));
				start = end;
				end = end + LOG_MAXLENGTH;
			} else {
				Log.e(TAG, msg.substring(start, strLength));
				break;
			}
		}
	}
//	private native static int  println_native (int bufID, int priority, String tag, String msg);


}
