#pragma once

#include <iostream>
#include <vector>
#include <process.h>
#include <WinSock2.h>								// <----- stdafx.h
#pragma comment(lib,"ws2_32.lib")					// <----- stdafx.h
#include <Windows.h>

using namespace std;

#ifdef _DEBUG
#define LOG_OUT(fmt) printf( "[%s:%d] %s\n",__FUNCTION__,__LINE__,fmt);
#else
#define LOG_OUT
#endif

typedef enum {
	SICK_STATUS_UNDEFINED		= 0,
	SICK_STATUS_INITIALIZATION	= 1,
	SICK_STATUS_CONFIGURATION	= 2,
	SICK_STATUS_IDLE			= 3,
	SICK_STATUS_ROTATED			= 4,
	SICK_STATUS_PREPARATION		= 5,
	SICK_STATUS_READY			= 6,
	SICK_STATUS_MEASUREMENT		= 7,
	SICK_STATUS_SCANCONTINOUS	= 8
} status_t;

enum {
	SICK_SCAN_NUM_MAX = 381,
	SICK_SCAN_DIST_MIN = 50,		// 0.05 m
	SICK_SCAN_DIST_MAX = 50000,		// 50 m
	SICK_SCAN_DEG_CENTER = 190000,	// 190 deg
	SICK_SCAN_DEG_START = -5000,	// -5 deg
	SICK_SCAN_DEG_RESOLUTION = 500	// 0.5 deg
};

class CSick
{
public:
	CSick();
	~CSick();
	bool Initialize(std::string host_ip="192.168.51.100");
	bool UnInitialize();
	bool StartCapture();
	bool StopCapture();
	bool GetValidDataRTheta( vector<pair<int,double> > &vecRTheta );
	static unsigned int CALLBACK proc(void* arg);

	bool m_bDataAvailable;

private:
	bool ConnectToDevice(std::string host, int port);
	bool DisconnectToDevice();
	inline bool IsConnected() {return m_bConnect;}
	status_t QueryStatus();
	bool ScanContinuous(int start);
	void ParseSickData(std::string& strBuf);
	void ConvertRawToRTheta();

	enum {
		SICK_STX = 0x02,
		SICK_ETX = 0x03
	};

	// ป๓ลย
	bool m_bConnect;
	status_t m_status;

	// data
	vector<double> m_vecRawScans;
	vector<pair<int,double> > m_vecValidScanRTheta;

	// communication
	SOCKET m_sockDesc;
	WSADATA m_wsadata;
	std::string m_strRemain;
	int m_nSETContinuous;
	int m_nACKContinuous;

	// thread
	HANDLE m_hThread;
	volatile bool m_bRunThread;
};