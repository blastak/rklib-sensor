#include "Sick.h"

CSick::CSick()
{
	m_bConnect = false;
	m_bRunThread = false;
	m_bDataAvailable = false;
	m_nSETContinuous = -1;
	m_nACKContinuous = -1;

	m_status = SICK_STATUS_UNDEFINED;
}

CSick::~CSick()
{
	if(m_status != SICK_STATUS_MEASUREMENT || m_bRunThread)
		UnInitialize();
}

bool CSick::Initialize( std::string host_ip/*="192.168.51.100"*/ )
{
	if(!ConnectToDevice(host_ip,2111)) {
		return false;
	}

	LOG_OUT("Wait for ready status ...");

	while (m_status != SICK_STATUS_MEASUREMENT) {
		m_status = QueryStatus();
	}

	LOG_OUT("Laser is ready...");

	return true;
}

bool CSick::UnInitialize()
{
	if(m_bRunThread) {
		StopCapture();
	}

	if (m_bConnect)
		DisconnectToDevice();

	return true;
}

bool CSick::StartCapture()
{
	if(m_bRunThread) {
		LOG_OUT("thread is now running..");
		return false;
	}

	if(m_status != SICK_STATUS_MEASUREMENT) {
		LOG_OUT("initialize SICK first..");
		return false;
	}

	m_status = SICK_STATUS_SCANCONTINOUS;

	m_strRemain.clear();
	m_bRunThread = true;
	m_hThread = (HANDLE)_beginthreadex(NULL, 0, &CSick::proc, (void *)this, 0, NULL);

	return ScanContinuous(1);
}

bool CSick::StopCapture()
{
	if(!m_bRunThread) {
		LOG_OUT("thread has already been stopped..");
		return false;
	}

	if(ScanContinuous(0)) {

		m_bRunThread = false;

		long ld = WaitForSingleObject(m_hThread, 1000);
		if(ld == WAIT_OBJECT_0)
			printf("Thread[%s] : 정상 종료\n","SICK");
		else if(ld == WAIT_TIMEOUT)
			printf("Thread[%s] : 종료 Timeout...\n","SICK");

		CloseHandle( m_hThread );

		while (m_status != SICK_STATUS_MEASUREMENT) {
			m_status = QueryStatus();
		}
	}

	return true;
}

bool CSick::GetValidDataRTheta( vector<pair<int,double> > &vecRTheta )
{
	if(!m_bRunThread) {
		LOG_OUT("thread is not running..");
		return false;
	}

	m_bDataAvailable = false;
	vecRTheta = m_vecValidScanRTheta;

	return true;
}


//////////////////////////////////////////////////////////////////////////

bool CSick::ConnectToDevice(std::string host, int port)
{
	if(m_bConnect) {
		LOG_OUT("Sick handle has already been opened...");
		return false;
	}

	WSAStartup(MAKEWORD(2,2),&m_wsadata);				// <------ DLG

	SOCKADDR_IN my_addr;
	my_addr.sin_family = PF_INET;
	my_addr.sin_addr.s_addr = inet_addr(host.c_str());
	my_addr.sin_port = htons(port);

	m_sockDesc = socket(PF_INET, SOCK_STREAM, 0);

	ULONG nonBlock = TRUE;
	ioctlsocket(m_sockDesc, FIONBIO, &nonBlock);

	::connect(m_sockDesc, (SOCKADDR*) &my_addr, sizeof(my_addr));

	fd_set fdset;
	FD_ZERO(&fdset);
	FD_SET(m_sockDesc, &fdset);

	timeval timevalue;
	timevalue.tv_sec = 1;
	timevalue.tv_usec = 500000;

	::select(m_sockDesc+1, NULL, &fdset, NULL, &timevalue);

	if( !FD_ISSET(m_sockDesc, &fdset)) {
		LOG_OUT("connection error...timeout...");

		closesocket(m_sockDesc);
		WSACleanup();

		return false;
	}

	return m_bConnect = true;
}

bool CSick::DisconnectToDevice()
{
	if(!m_bConnect) {
		LOG_OUT("Sick handle has already been closed...");
		return false;
	}

	closesocket(m_sockDesc);
	WSACleanup();

	m_bConnect = false;

	return true;
}

status_t CSick::QueryStatus()
{
	char buf[256+1];
	sprintf_s(buf, "%c%s%c", 0x02, "sRN STlms", 0x03);

	send(m_sockDesc, buf, strlen(buf), 0);

	fd_set fdset;
	timeval timevalue;

	int len = -1;
	while( len < 0 ) {
		FD_ZERO(&fdset);
		FD_SET(m_sockDesc, &fdset);

		timevalue.tv_sec = 0;
		timevalue.tv_usec = 10000; // 10ms

		::select(m_sockDesc+1, NULL, &fdset, NULL, &timevalue);
		if( !FD_ISSET(m_sockDesc, &fdset) ) continue;

		len = recv(m_sockDesc, buf, 256, 0);
	}
	buf[len] = 0;

	int ret;
	sscanf_s((buf + 10), "%d", &ret);

	return (status_t) ret;
}

bool CSick::ScanContinuous( int start )
{
	char buf[256+1];
	sprintf_s(buf, "%c%s%d%c", 0x02, "sEN LMDscandata ", start, 0x03);

	m_nSETContinuous = start;

	send(m_sockDesc, buf, strlen(buf), 0);

	while(m_nSETContinuous != m_nACKContinuous);

	m_nSETContinuous = m_nACKContinuous = -1;

	LOG_OUT("ScanContinuous OK");

	return true;
}

unsigned int CALLBACK CSick::proc(void* pParam)
{
	CSick* obj = (CSick*)pParam;

	const int kSizeBuf = 1024;
	char buf[kSizeBuf+1];
	std::string strBuf;

	fd_set fdset;
	timeval timevalue;

	while(obj->m_bRunThread) {
		FD_ZERO(&fdset);
		FD_SET(obj->m_sockDesc, &fdset);

		timevalue.tv_sec = 0;
		timevalue.tv_usec = 10000; // 10ms

		::select(obj->m_sockDesc+1, NULL, &fdset, NULL, &timevalue);
		if( !FD_ISSET(obj->m_sockDesc, &fdset) ) continue;

		int len = recv(obj->m_sockDesc, buf, kSizeBuf, 0);

		if(len > 0) {
			buf[len] = 0;
			strBuf = buf;
			obj->m_strRemain += strBuf;

			int posSTX = obj->m_strRemain.find(SICK_STX);
			int posETX;
			while(posSTX != -1 && obj->m_bRunThread) {
				obj->m_strRemain = obj->m_strRemain.substr(posSTX);

				posETX = obj->m_strRemain.find(SICK_ETX);
				if(posETX == -1) break;

				std::string strNewBuff = obj->m_strRemain.substr(0,posETX+1);

				if(strcmp(strNewBuff.substr(1,3).c_str(), "sSN") == 0) {
					obj->ParseSickData(strNewBuff);
				}
				else if(strcmp(strNewBuff.substr(1,3).c_str(), "sEA") == 0) {
					obj->m_nACKContinuous = strNewBuff[17] - '0';
				}

				obj->m_strRemain = obj->m_strRemain.substr(posETX+1);
				posSTX = obj->m_strRemain.find(SICK_STX);
			}
		}
	}

	return 0;
}

void CSick::ParseSickData( std::string& strBuf )
{
	vector<double> rawScans;

	char* buf = strdup(strBuf.c_str());

	char* tok = strtok(buf, " "); //Type of command
	tok = strtok(NULL, " "); //Command
	tok = strtok(NULL, " "); //VersionNumber
	tok = strtok(NULL, " "); //DeviceNumber
	tok = strtok(NULL, " "); //Serial number
	tok = strtok(NULL, " "); //DeviceStatus
	tok = strtok(NULL, " "); //MessageCounter
	tok = strtok(NULL, " "); //ScanCounter
	tok = strtok(NULL, " "); //PowerUpDuration
	tok = strtok(NULL, " "); //TransmissionDuration
	tok = strtok(NULL, " "); //InputStatus
	tok = strtok(NULL, " "); //OutputStatus
	tok = strtok(NULL, " "); //ReservedByteA
	tok = strtok(NULL, " "); //ScanningFrequency
	tok = strtok(NULL, " "); //MeasurementFrequency
	tok = strtok(NULL, " ");
	tok = strtok(NULL, " ");
	tok = strtok(NULL, " ");
	tok = strtok(NULL, " "); //NumberEncoders
	int NumberEncoders;
	sscanf(tok, "%d", &NumberEncoders);
	for (int i = 0; i < NumberEncoders; i++) {
		tok = strtok(NULL, " "); //EncoderPosition
		tok = strtok(NULL, " "); //EncoderSpeed
	}

	tok = strtok(NULL, " "); //NumberChannels16Bit
	int NumberChannels16Bit;
	sscanf(tok, "%d", &NumberChannels16Bit);
	for (int i = 0; i < NumberChannels16Bit; i++) {
		int type = -1; // 0 DIST1 1 DIST2 2 RSSI1 3 RSSI2
		char content[6];
		tok = strtok(NULL, " "); //MeasuredDataContent
		sscanf(tok, "%s", content);
		if (!strcmp(content, "DIST1")) {
			type = 0;
		} else if (!strcmp(content, "DIST2")) {
			type = 1;
		} else if (!strcmp(content, "RSSI1")) {
			type = 2;
		} else if (!strcmp(content, "RSSI2")) {
			type = 3;
		}
		tok = strtok(NULL, " "); //ScalingFactor
		tok = strtok(NULL, " "); //ScalingOffset
		tok = strtok(NULL, " "); //Starting angle
		tok = strtok(NULL, " "); //Angular step width
		tok = strtok(NULL, " "); //NumberData
		int NumberData;
		sscanf(tok, "%X", &NumberData);

		if (type == 0) {
			//data.dist_len1 = NumberData;
		} else if (type == 1) {
			//data.dist_len2 = NumberData;
		} else if (type == 2) {
			//data.rssi_len1 = NumberData;
		} else if (type == 3) {
			//data.rssi_len2 = NumberData;
		}

		for (int i = 0; i < NumberData; i++) {
			int dat;
			tok = strtok(NULL, " "); //data
			sscanf(tok, "%X", &dat);

			if (type == 0) {
				rawScans.push_back(dat);
			} else if (type == 1) {
				//data.dist2[i] = dat;
			} else if (type == 2) {
				//data.rssi1[i] = dat;
			} else if (type == 3) {
				//data.rssi2[i] = dat;
			}
		}
	}

	tok = strtok(NULL, " "); //NumberChannels8Bit
	int NumberChannels8Bit;
	sscanf(tok, "%d", &NumberChannels8Bit);
	for (int i = 0; i < NumberChannels8Bit; i++) {
		int type = -1;
		char content[6];
		tok = strtok(NULL, " "); //MeasuredDataContent
		sscanf(tok, "%s", content);
		if (!strcmp(content, "DIST1")) {
			type = 0;
		} else if (!strcmp(content, "DIST2")) {
			type = 1;
		} else if (!strcmp(content, "RSSI1")) {
			type = 2;
		} else if (!strcmp(content, "RSSI2")) {
			type = 3;
		}
		tok = strtok(NULL, " "); //ScalingFactor
		tok = strtok(NULL, " "); //ScalingOffset
		tok = strtok(NULL, " "); //Starting angle
		tok = strtok(NULL, " "); //Angular step width
		tok = strtok(NULL, " "); //NumberData
		int NumberData;
		sscanf(tok, "%X", &NumberData);

		if (type == 0) {
			//data.dist_len1 = NumberData;
		} else if (type == 1) {
			//data.dist_len2 = NumberData;
		} else if (type == 2) {
			//data.rssi_len1 = NumberData;
		} else if (type == 3) {
			//data.rssi_len2 = NumberData;
		}

		for (int i = 0; i < NumberData; i++) {
			int dat;
			tok = strtok(NULL, " "); //data
			sscanf(tok, "%X", &dat);

			if (type == 0) {
// 				if(dat >= SICK_SCAN_DIST_MIN && dat <= SICK_SCAN_DIST_MAX) {
// 					pair<int, double> pairTemp;
// 					pairTemp.first = i;
// 					pairTemp.second = dat/1000.0; // meter to mm
// 					m_aScanData.push_back(pairTemp);
// 				}
			} else if (type == 1) {
				//data.dist2[i] = dat;
			} else if (type == 2) {
				//data.rssi1[i] = dat;
			} else if (type == 3) {
				//data.rssi2[i] = dat;
			}
		}
	}

	int flag;
	tok = strtok(NULL, " "); // Position
	sscanf(tok, "%d", &flag);
	if (flag)
		for(int i = 0; i < 7; i++) tok = strtok(NULL, " ");
	tok = strtok(NULL, " "); // Name
	sscanf(tok, "%d", &flag);
	if (flag) tok = strtok(NULL, " ");
	tok = strtok(NULL, " "); // Comment
	sscanf(tok, "%d", &flag);
	if (flag) tok = strtok(NULL, " ");
	tok = strtok(NULL, " "); // Time
	sscanf(tok, "%d", &flag);
	struct tm lms_time;
	lms_time.tm_isdst = -1;
	if (flag) {
		tok = strtok(NULL, " ");
		sscanf(tok, "%X", &lms_time.tm_year);
		tok = strtok(NULL, " ");
		sscanf(tok, "%X", &lms_time.tm_mon);
		tok = strtok(NULL, " ");
		sscanf(tok, "%X", &lms_time.tm_mday);
		tok = strtok(NULL, " ");
		sscanf(tok, "%X", &lms_time.tm_hour);
		tok = strtok(NULL, " ");
		sscanf(tok, "%X", &lms_time.tm_min);
		tok = strtok(NULL, " ");
		sscanf(tok, "%X", &lms_time.tm_sec);
		tok = strtok(NULL, " ");
		//sscanf(tok, "%X", &data.timestamp.tv_usec);
		lms_time.tm_year -= 1900;
		lms_time.tm_mon--;
		//data.timestamp.tv_sec = mktime(&lms_time);
	}

	m_bDataAvailable = false;
	m_vecRawScans = rawScans;
	ConvertRawToRTheta();
	m_bDataAvailable = true;

	free(buf);
}

void CSick::ConvertRawToRTheta()
{
	m_vecValidScanRTheta.clear();

	pair<int,double> pairRTheta;

	for (int i=0; i<(int)m_vecRawScans.size(); ++i) {
		int dat = m_vecRawScans[i];
		if(dat >= SICK_SCAN_DIST_MIN && dat <= SICK_SCAN_DIST_MAX) {
			pairRTheta.first = i;
			pairRTheta.second = dat/1000.0; // mm to meter
			m_vecValidScanRTheta.push_back(pairRTheta);
		}
	}
}