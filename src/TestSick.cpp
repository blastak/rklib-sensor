#include <iostream>
#include <opencv2/opencv.hpp>
#include "Sick.h"

using namespace std;
using namespace cv;

void DrawScans(Mat& img,vector<pair<int,double> > vecRTheta)
{
	img = Mat::zeros(500,500,CV_8UC3);

	Point2i center(250,400);
	Point2d xyTemp;
	double scaler = 500;

	for (int i=0; i<(int)vecRTheta.size(); ++i) {
		double deg = (SICK_SCAN_DEG_START/1000.0) + (double)vecRTheta[i].first * (SICK_SCAN_DEG_RESOLUTION/1000.0);
		xyTemp = center;
		xyTemp.x += vecRTheta[i].second * cos(deg/180*CV_PI) * scaler;
		xyTemp.y -= vecRTheta[i].second * sin(deg/180*CV_PI) * scaler;

		circle(img,xyTemp,1,CV_RGB(0,255,0));
	}
}

int main()
{
	CSick objSick;
	if(!objSick.Initialize("192.168.51.100"))
		return -1;

	objSick.StartCapture();

	vector<pair<int, double> > validScans;
	Mat imgLaser = Mat::zeros(500,500,CV_8UC3);
	
	namedWindow("laserview");
	while (1)
	{
		if(objSick.m_bDataAvailable)
		{
			objSick.GetValidDataRTheta(validScans);
			DrawScans(imgLaser,validScans);
			imshow("laserview",imgLaser);
		}
			
		int key = waitKey(1);
		if(key == 27) break;
	}

	objSick.StopCapture();

	objSick.UnInitialize();

	return 0;

}



