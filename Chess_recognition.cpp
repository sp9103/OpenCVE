#include "Chess_recognition.hpp"


Chess_recognition::Chess_recognition(void)
{
}


Chess_recognition::~Chess_recognition(void)
{
	if(MODE == 1){
		CloseHandle(hThread);
		DeleteCriticalSection(&cs);
		DeleteCriticalSection(&vec_cs);
	}
	cvReleaseImage(&img_process);
}

void Chess_recognition::Init(int width, int height, int mode){
	if(mode == 1){
		InitializeCriticalSection(&cs);
		InitializeCriticalSection(&vec_cs);
		hThread = (HANDLE)_beginthreadex(NULL, 0, thread_hough, this, 0, NULL);
	}else if(mode == 2){
		Linefindcount_x = 0, Linefindcount_y = 0;
		line_avg_x1 = 40, line_avg_x2 = 40, line_avg_y1 = 40, line_avg_y2 = 40;
	}

	MODE = mode;
	img_process = cvCreateImage(cvSize(width, height), IPL_DEPTH_8U, 1);
	cvZero(img_process);

	WIDTH = width;	HEIGHT = height;
}

void Chess_recognition::drawLines ( vector<pair<float, float>> lines, IplImage* image){
	for( int i = 0; i < MIN(lines.size(),100); i++ )
    {
		float rho = lines.at(i).first;
        float theta = lines.at(i).second;
        CvPoint pt1, pt2;
        double a = cos(theta), b = sin(theta);
        double x0 = a*rho, y0 = b*rho;								//수직의 시작이 되는점
        pt1.x = cvRound(x0 + 1000*(-b));							//끝까지로 쭉 그려버림
        pt1.y = cvRound(y0 + 1000*(a));
        pt2.x = cvRound(x0 - 1000*(-b));
        pt2.y = cvRound(y0 - 1000*(a));
        cvLine( image, pt1, pt2, CV_RGB(255,0,0), 1, 8 );

		cvDrawCircle(image, cvPoint(x0, y0), 3, cvScalar(255,255), -1);
    }
}

void Chess_recognition::drawPoint( IplImage *src, vector<Chess_point> point){
	char buf[32];
	// display the points in an image 
	for(int i = 0; i < point.size(); i++){
		cvCircle( src, point.at(i).Cordinate, 5, cvScalar(0,255), 2 );
		sprintf(buf, "(%d,%d)",point.at(i).index.x, point.at(i).index.y);
		cvPutText(src, buf, point.at(i).Cordinate, &cvFont(1.0), cvScalar(255,0,0));
	}
}

void Chess_recognition::findIntersections ( vector<pair<float, float>> linesX, vector<pair<float, float>> linesY, vector<Chess_point> *point ){
	char buf[32];
	Chess_point temp_cp;
	point->clear();

	for( int i = 0; i < MIN(linesX.size(),100); i++ )
    {
        for( int j = 0; j < MIN(linesY.size(),100); j++ )
        {                
			float rhoX = linesX.at(i).first;
			float rhoY = linesY.at(j).first;
			float thetaX = linesX.at(i).second, thetaY = linesY.at(j).second;

            double aX = cos(thetaX), bX = sin(thetaX);
            double aY = cos(thetaY), bY = sin(thetaY);

            CvPoint c; // the intersection point of lineX[i] and lineY[j] 
            double Cx = ( rhoX*bY - rhoY*bX ) / ( aX*bY - bX*aY ) ;
            double Cy = ( rhoX - aX*Cx ) / bX ; 
            c.x = cvRound(Cx);
            c.y = cvRound(Cy);

			//save point
			temp_cp.Cordinate = c;
			temp_cp.index = cvPoint(j,i);
			point->push_back(temp_cp);
        }
    }
}

void Chess_recognition::Get_Line(vector<pair<float, float>> *linesX, vector<pair<float, float>> *linesY){
	linesX->clear();
	linesY->clear();
	EnterCriticalSection(&vec_cs);
	copy(vec_LineX.begin(), vec_LineX.end(), back_inserter(*linesX));
	copy(vec_LineY.begin(), vec_LineY.end(), back_inserter(*linesY));
	LeaveCriticalSection(&vec_cs);

	mergeLine(linesX);
	mergeLine(linesY);
}

void Chess_recognition::NMS2 ( IplImage* image, IplImage* image2, int kernel)			//이웃들의 값을 살펴보고 조건에 해당하지 않으면 지움
{																	//조건은 이웃값보다 작음
    float neighbor, neighbor2;
    for ( int y = 0; y < image->height; y++ )
    {
        for ( int x = 0; x < image->width; x++ )
        {
            float intensity = CV_IMAGE_ELEM( image, float, y, x );
            if (intensity > 0) {
                int flag = 0;
                
                for ( int ky = -kernel; ky <= kernel; ky++ ) // in y-direction
                {
                    if ( y+ky < 0 || y+ky >= image->height ) { // border check
                        continue;
                    }
                    for ( int kx = -kernel; kx <= kernel; kx++ ) // in x-direction
                    {
                        if ( x+kx < 0 || x+kx >= image->width ) {  // border check
                            continue;
                        }
                        neighbor = CV_IMAGE_ELEM( image, float, y+ky, x+kx );
                        neighbor2 = CV_IMAGE_ELEM( image2, float, y+ky, x+kx );
                        //                        if ( intensity < neighbor ) {
                        if ( intensity < neighbor || intensity < neighbor2) {
                            CV_IMAGE_ELEM( image, float, y, x ) = 0.0;
                            flag = 1;
                            break;
                        }
                    }
                    if ( 1 == flag ) {
                        break;
                    }
                }
            }
            
            else {
                CV_IMAGE_ELEM( image, float, y, x ) = 0.0;
            }
        }  
    }
}

void Chess_recognition::cast_seq(CvSeq* linesX, CvSeq* linesY){

	vec_LineX.clear();
	vec_LineY.clear();
	for( int i = 0; i < MIN(linesX->total,100); i++ )
    {
        float* line = (float*)cvGetSeqElem(linesX,i);
        float rho = line[0];
        float theta = line[1];

		vec_LineX.push_back(pair<float,float>(rho, theta));
    }

	for( int i = 0; i < MIN(linesY->total,100); i++ )
    {
        float* line = (float*)cvGetSeqElem(linesY,i);
        float rho = line[0];
        float theta = line[1];

		vec_LineY.push_back(pair<float,float>(rho, theta));
    }
}

bool sort_first(pair<float, float> a, pair<float, float> b){
	return a.first < b.first;										//각도로 정렬
}

void Chess_recognition::mergeLine( vector<std::pair<float, float>> *Lines){
	float SUB_UNDER = 0.0, SUB_MIN = 9999;
	vector<std::pair<float, float>> temp;
	pair<int, int> Min_pair;

	//연산 초기화
	for(int i = 0; i < Lines->size(); i++){
		if(Lines->at(i).first < 0){
			Lines->at(i).first = fabs(Lines->at(i).first);
			Lines->at(i).second = Lines->at(i).second - CV_PI;
		}
	}

	copy(Lines->begin(), Lines->end(),  back_inserter(temp));
	sort(temp.begin(), temp.end(), sort_first);

	if(temp.size() > 9){
		for(int i = 0; i < (int)Lines->size()-9; i++){
			SUB_MIN = 9999;		
			for(int j = 1; j < temp.size(); j++){
				float SUB = temp.at(j).first - temp.at(j-1).first;

				if(SUB < SUB_MIN){
					SUB_MIN = SUB;
					Min_pair.first = j-1;
					Min_pair.second = j;
				}
			}
			temp.at(Min_pair.first).first = (temp.at(Min_pair.first).first + temp.at(Min_pair.second).first) / 2.0;
			temp.at(Min_pair.first).second = (temp.at(Min_pair.first).second + temp.at(Min_pair.second).second) / 2.0;

			temp.erase(temp.begin() + Min_pair.second);
		}

		Lines->clear();
		copy(temp.begin(), temp.end(),  back_inserter(*Lines));
	}
}

UINT WINAPI Chess_recognition::thread_hough(void *arg){
	Chess_recognition* p = (Chess_recognition*)arg;

	CvSeq *LineX, *LineY;
	double h[] = { -1, -7, -15, 0, 15, 7, 1 };
    
    CvMat DoGx = cvMat( 1, 7, CV_64FC1, h );
    CvMat* DoGy = cvCreateMat( 7, 1, CV_64FC1 );
    cvTranspose( &DoGx, DoGy ); // transpose(&DoGx) -> DoGy

	double minValx, maxValx, minValy, maxValy, minValt, maxValt;
	int kernel = 1;

	IplImage *iplTemp = cvCreateImage(cvSize(p->WIDTH, p->HEIGHT), IPL_DEPTH_32F, 1);                   
    IplImage *iplDoGx = cvCreateImage(cvGetSize(iplTemp), IPL_DEPTH_32F, 1);  
    IplImage *iplDoGy = cvCreateImage(cvGetSize(iplTemp), IPL_DEPTH_32F, 1);  
    IplImage *iplDoGyClone = cvCloneImage(iplDoGy), *iplDoGxClone = cvCloneImage(iplDoGx);
    IplImage *iplEdgeX = cvCreateImage(cvGetSize(iplTemp), 8, 1);
    IplImage *iplEdgeY = cvCreateImage(cvGetSize(iplTemp), 8, 1);

	CvMemStorage* storageX = cvCreateMemStorage(0), *storageY = cvCreateMemStorage(0);

	while(1){
		EnterCriticalSection(&(p->cs));
		cvConvert(p->img_process, iplTemp);
		LeaveCriticalSection(&p->cs);

		cvFilter2D( iplTemp, iplDoGx, &DoGx );								//라인만 축출해내고
		cvFilter2D( iplTemp, iplDoGy, DoGy );
		cvAbs(iplDoGx, iplDoGx);            cvAbs(iplDoGy, iplDoGy);

		cvMinMaxLoc( iplDoGx, &minValx, &maxValx );
		cvMinMaxLoc( iplDoGy, &minValy, &maxValy );
		cvMinMaxLoc( iplTemp, &minValt, &maxValt );
		cvScale( iplDoGx, iplDoGx, 2.0 / maxValx );			//정규화
		cvScale( iplDoGy, iplDoGy, 2.0 / maxValy );
		cvScale( iplTemp, iplTemp, 2.0 / maxValt );

		cvCopy(iplDoGy, iplDoGyClone), cvCopy(iplDoGx, iplDoGxClone);

		//NMS진행후 추가 작업
		p->NMS2 ( iplDoGx, iplDoGyClone, kernel );
		p->NMS2 ( iplDoGy, iplDoGxClone, kernel );

		cvConvert(iplDoGx, iplEdgeY);							//IPL_DEPTH_8U로 다시 재변환
		cvConvert(iplDoGy, iplEdgeX);

		double rho = 1.0; // distance resolution in pixel-related units
        double theta = 1.0; // angle resolution measured in radians
		int threshold = 20;
		if(threshold == 0)	threshold = 1;

		LineX = cvHoughLines2(iplEdgeX, storageX, CV_HOUGH_STANDARD, 1.0*rho, CV_PI/180*theta, threshold, 0, 0);
        LineY = cvHoughLines2(iplEdgeY, storageY, CV_HOUGH_STANDARD, 1.0*rho, CV_PI/180*theta, threshold, 0, 0);

		EnterCriticalSection(&p->vec_cs);
		p->cast_seq(LineX, LineY);
		LeaveCriticalSection(&p->vec_cs);

		Sleep(10);
	}

	cvReleaseMat(&DoGy);

	cvReleaseImage(&iplTemp);
	cvReleaseImage(&iplDoGx);
	cvReleaseImage(&iplDoGy);
	cvReleaseImage(&iplDoGyClone);
	cvReleaseImage(&iplDoGxClone);
	cvReleaseImage(&iplEdgeX);
	cvReleaseImage(&iplEdgeY);

	cvReleaseMemStorage(&storageX);
	cvReleaseMemStorage(&storageY);

	_endthread();

	return 0;
}

void Chess_recognition::Copy_Img(IplImage *src){
	if(MODE == 1)	EnterCriticalSection(&cs);
	if(src->nChannels == 1){
		cvCopy(src, img_process);
	}
	else{
		cvCvtColor(src, img_process, CV_BGR2GRAY);
	}
	if(MODE == 1)	LeaveCriticalSection(&cs);
}

void Chess_recognition::Refine_CrossPoint(vector<Chess_point> *point){
	static bool first_check = false;
	static vector<CvPoint> prev_point;
	const float Refine_const = 0.9;

	if(first_check == false && point->size() == 81){
		for(int i = 0; i < 81; i++)
			prev_point.push_back(point->at(i).Cordinate);

		first_check = true;
	}
	else if(first_check == true){
		if(point->size() < 81){
			point->clear();
			for(int i = 0; i < 81; i++){
				Chess_point CP_temp;
				CP_temp.Cordinate = cvPoint(prev_point.at(i).x, prev_point.at(i).y); 
				CP_temp.index = cvPoint(i % 9, i / 9);
				point->push_back(CP_temp);
			}

		}else{
			for(int i = 0; i < 81; i++){
				point->at(i).Cordinate.x = cvRound(Refine_const * (float)prev_point.at(i).x + (1.0 - Refine_const) * (float)point->at(i).Cordinate.x);
				point->at(i).Cordinate.y = cvRound(Refine_const * (float)prev_point.at(i).y + (1.0 - Refine_const) * (float)point->at(i).Cordinate.y);
				//printf("after: %d %d\n", point->at(i).Cordinate.x, point->at(i).Cordinate.y);

				//prev_point.clear();
				prev_point.at(i) = cvPoint(point->at(i).Cordinate.x, point->at(i).Cordinate.y);
			}
		}
	}
}

void Chess_recognition::Set_CalculationDomain(CvCapture *Cam, int *ROI_WIDTH, int *ROI_HEIGHT){
}



///////////////////////////////////////////////////////////////////////////////////////////

void Chess_recognition::Chess_recognition_process(vector<Chess_point> *point){
	GetLinegrayScale(img_process, Linefindcount_x, Linefindcount_y);
	GetgraySidelinesPoint(img_process);

	if(Linefindcount_x > 150 && (in_line_point_x1.size() != 9 || in_line_point_x2.size() != 9)) flag_x = false;
	if(Linefindcount_y > 150 && (in_line_point_y1.size() != 9 || in_line_point_y2.size() != 9)) flag_y = false;

	if(Linefindcount_x < 1 && (in_line_point_x1.size() != 9 || in_line_point_x2.size() != 9)) flag_x = true;
	if(Linefindcount_y < 1 && (in_line_point_y1.size() != 9 || in_line_point_y2.size() != 9)) flag_y = true;

	if(in_line_point_x1.size() == 9 && in_line_point_x2.size() ==  9 && in_line_point_y1.size() == 9 && in_line_point_y2.size() == 9){
		GetSquarePoint(img_process);

		GetInCrossPoint(img_process, point);
	}
	else if(in_line_point_x1.size() != 9 || in_line_point_x2.size() != 9)
	{
		if(flag_x) Linefindcount_x+=5;
		else Linefindcount_x-=5;
		//if(flag_reset) flag_reset = true;
	}
	else if(in_line_point_y1.size() != 9 || in_line_point_y2.size() != 9)
	{
		if(flag_y) Linefindcount_y+=5;
		else Linefindcount_y-=5;
	}

	MemoryClear();
}

void Chess_recognition::GetLinegrayScale(IplImage *gray_image, int linefindcount_x, int linefindcount_y){

	// 중간 부터 검사

	int image_y = gray_image->height, image_x = gray_image->width;
	int x1, x2, x_mid, y1, y2, y_mid;

	// x축 라인
	y1 = (image_y/5)*2 - linefindcount_x, y2 = (image_y/5)*3 + linefindcount_x, y_mid = (image_y/5)*2;

	line_x1.push_back(setMyGrayPoint(Getgrayscale(gray_image,image_x/2,y1),image_x/2,y1));
	line_x2.push_back(setMyGrayPoint(Getgrayscale(gray_image,image_x/2,y2),image_x/2,y2));
	line_x_mid.push_back(setMyGrayPoint(Getgrayscale(gray_image,image_x/2,y_mid),image_x/2,y_mid));

	for(int x=1; x<= image_x/2; x++){

		line_x1.push_back(setMyGrayPoint(Getgrayscale(gray_image,image_x/2+x,y1),image_x/2+x,y1));
		line_x1.push_back(setMyGrayPoint(Getgrayscale(gray_image,image_x/2-x,y1),image_x/2-x,y1));

		line_x2.push_back(setMyGrayPoint(Getgrayscale(gray_image,image_x/2+x,y2),image_x/2+x,y2));
		line_x2.push_back(setMyGrayPoint(Getgrayscale(gray_image,image_x/2-x,y2),image_x/2-x,y2));

		line_x_mid.push_back(setMyGrayPoint(Getgrayscale(gray_image,image_x/2+x,y_mid),image_x/2+x,y_mid));
		line_x_mid.push_back(setMyGrayPoint(Getgrayscale(gray_image,image_x/2-x,y_mid),image_x/2-x,y_mid));
	}

	// y축 라인

	x1 = (image_x/5)*2 - linefindcount_y, x2 = (image_x/5)*3 + linefindcount_y, x_mid = (image_x/5)*2;

	line_y1.push_back(setMyGrayPoint(Getgrayscale(gray_image,x1,image_y/2),x1,image_y/2));
	line_y2.push_back(setMyGrayPoint(Getgrayscale(gray_image,x2,image_y/2),x2,image_y/2));
	line_y_mid.push_back(setMyGrayPoint(Getgrayscale(gray_image,x_mid,image_y/2),x_mid,image_y/2));

	for(int y=1; y<= image_y/2; y++){

		line_y1.push_back(setMyGrayPoint(Getgrayscale(gray_image,x1,image_y/2+y),x1,image_y/2+y));
		line_y1.push_back(setMyGrayPoint(Getgrayscale(gray_image,x1,image_y/2-y),x1,image_y/2-y));

		line_y2.push_back(setMyGrayPoint(Getgrayscale(gray_image,x2,image_y/2+y),x2,image_y/2+y));
		line_y2.push_back(setMyGrayPoint(Getgrayscale(gray_image,x2,image_y/2-y),x2,image_y/2-y));

		line_y_mid.push_back(setMyGrayPoint(Getgrayscale(gray_image,x_mid,image_y/2+y),x_mid,image_y/2+y));
		line_y_mid.push_back(setMyGrayPoint(Getgrayscale(gray_image,x_mid,image_y/2-y),x_mid,image_y/2-y));
	}
}

void Chess_recognition::GetgraySidelinesPoint(IplImage *chess_image){

	int line_count_x1, line_count_x2, line_count_x_mid, line_count_y1, line_count_y2, line_count_y_mid;
	int jump_count_p1, jump_count_m1, jump_count_p2, jump_count_m2, jump_count_p3, jump_count_m3;

	line_count_x1 = line_count_x2 = line_count_x_mid = line_count_y1 = line_count_y2 = line_count_y_mid = 0;
	jump_count_p1 = jump_count_p2 = jump_count_p3 = jump_count_m1 = jump_count_m2 = jump_count_m3 = 0;

	line_point_x1.x1 = line_point_x1.x2 = line_point_x2.x1 = line_point_x2.x2 = line_point_x_mid.x1 = line_point_x_mid.x2 = chess_image->width/2;
	line_point_y1.y1 = line_point_y1.y2 = line_point_y2.y1 = line_point_y2.y2 = line_point_y_mid.y1 = line_point_y_mid.y1 = chess_image->height/2;

	for(int i=0;i<line_x1.size()-10;i++){
		if((i%2 == 1) && (jump_count_p1 > 0)){
			jump_count_p1--;
		}
		else if((i%2 == 0) && (jump_count_m1 > 0)){
			jump_count_m1--;
		}
		else{
			if(abs(line_x1[i].grayscale - line_x1[i+2].grayscale) >= 30){
				int flag = true;
				for(int j=1;j<=3;j++){
					if(abs(line_x1[i].grayscale - line_x1[i+(j*2)].grayscale) < 30){
						flag = false;
						break;
					}
				}
				if(flag){
					if(line_count_x1 < 9){
						if(line_point_x1.x1 > line_x1[i].x){
							line_point_x1.x1 = line_x1[i].x;
							line_point_x1.y1 = line_x1[i].y;
						}

						if(line_point_x1.x2 < line_x1[i].x){
							line_point_x1.x2 = line_x1[i].x;
							line_point_x1.y2 = line_x1[i].y;
						}

						in_line_point_x1.push_back(setMyPoint(line_x1[i].x, line_x1[i].y));

						line_count_x1++;
						cvCircle(chess_image, cvPoint(line_x1[i].x, line_x1[i].y),5,cvScalar(255,255,255));
					}
					if(i%2 == 1) jump_count_p1 = 40;
					else jump_count_m1 = 40;	
				}
			}
		}


		if((i%2 == 1) && (jump_count_p2 > 0)){
			jump_count_p2--;
		}
		else if((i%2 == 0) && (jump_count_m2 > 0)){
			jump_count_m2--;
		}
		else{
			if(abs(line_x2[i].grayscale - line_x2[i+2].grayscale) >= 30){
				int flag = true;
				for(int j=1;j<=3;j++){
					if(abs(line_x2[i].grayscale - line_x2[i+(j*2)].grayscale) < 30){
						flag = false;
						break;
					}
				}
				if(flag){
					if(line_count_x2 < 9){
						if(line_point_x2.x1 > line_x2[i].x){
							line_point_x2.x1 = line_x2[i].x;
							line_point_x2.y1 = line_x2[i].y;
						}

						if(line_point_x2.x2 < line_x2[i].x){
							line_point_x2.x2 = line_x2[i].x;
							line_point_x2.y2 = line_x2[i].y;
						}

						in_line_point_x2.push_back(setMyPoint(line_x2[i].x, line_x2[i].y));

						line_count_x2++;
						cvCircle(chess_image, cvPoint(line_x2[i].x, line_x2[i].y),5,cvScalar(255,255,255));
					}
					if(i%2 == 1) jump_count_p2 = 40;
					else jump_count_m2 = 40;
				}
			}
		}

		if(line_count_x1 == line_count_x2 /*== line_count_x_mid*/ == 9)
			break;
	}

	jump_count_p1 = jump_count_p2 = jump_count_p3 = jump_count_m1 = jump_count_m2 = jump_count_m3 = 0;

	for(int i=0;i<line_y1.size()-10;i++){
		if((i%2 == 1) && (jump_count_p1 > 0)){
			jump_count_p1--;
		}
		else if((i%2 == 0) && (jump_count_m1 > 0)){
			jump_count_m1--;
		}
		else{
			if(abs(line_y1[i].grayscale - line_y1[i+2].grayscale) >= 30){
				int flag = true;
				for(int j=1;j<=3;j++){
					if(abs(line_y1[i].grayscale - line_y1[i+j*2].grayscale) < 30){
						flag = false;
						break;
					}
				}
				if(flag){
					if(line_count_y1 < 9){
						if(line_point_y1.y1 > line_y1[i].y){
							line_point_y1.x1 = line_y1[i].x;
							line_point_y1.y1 = line_y1[i].y;
						}

						if(line_point_y1.y2 < line_y1[i].y){
							line_point_y1.x2 = line_y1[i].x;
							line_point_y1.y2 = line_y1[i].y;
						}

						in_line_point_y1.push_back(setMyPoint(line_y1[i].x, line_y1[i].y));

						line_count_y1++;
						cvCircle(chess_image, cvPoint(line_y1[i].x, line_y1[i].y),5,cvScalar(255,255,255));
					}
					if(i%2 == 1) jump_count_p1 = 30;
					else jump_count_m1 = 30;
				}
			}
		}

		if((i%2 == 1) && (jump_count_p2 > 0)){
			jump_count_p2--;
		}
		else if((i%2 == 0) && (jump_count_m2 > 0)){
			jump_count_m2--;
		}
		else{
			if(abs(line_y2[i].grayscale - line_y2[i+2].grayscale) >= 30){
				int flag = true;
				for(int j=1;j<=3;j++){
					if(abs(line_y2[i].grayscale - line_y2[i+j*2].grayscale) < 30){
						flag = false;
						break;
					}
				}
				if(flag){
					if(line_count_y2 < 9){
						if(line_point_y2.y1 > line_y2[i].y){
							line_point_y2.x1 = line_y2[i].x;
							line_point_y2.y1 = line_y2[i].y;
						}

						if(line_point_y2.y2 < line_y2[i].y){
							line_point_y2.x2 = line_y2[i].x;
							line_point_y2.y2 = line_y2[i].y;
						}

						in_line_point_y2.push_back(setMyPoint(line_y2[i].x, line_y2[i].y));

						line_count_y2++;
						cvCircle(chess_image, cvPoint(line_y2[i].x, line_y2[i].y),5,cvScalar(255,255,255));
					}

					if(i%2 == 1) jump_count_p2 = 30;
					else jump_count_m2 = 30;
				}
			}
		}

		if(line_count_y1 == line_count_y2 /*== line_count_y_mid*/ == 9)
			break;
	}
}

void Chess_recognition::GetSquarePoint(IplImage *chess_image){

	SetMyLinePoint(line_point_x1.x1, line_point_x1.y1, line_point_x2.x1, line_point_x2.y1, &line_square_left);
	SetMyLinePoint(line_point_y1.x1, line_point_y1.y1, line_point_y2.x1, line_point_y2.y1, &line_square_top);
	SetMyLinePoint(line_point_x1.x2, line_point_x1.y2, line_point_x2.x2, line_point_x2.y2, &line_square_right);
	SetMyLinePoint(line_point_y1.x2, line_point_y1.y2, line_point_y2.x2, line_point_y2.y2, &line_square_bottom);

	MyPoint t_square_lt, t_square_lb, t_square_rt, t_square_rb;

	if(GetCrossPoint(line_square_left, line_square_top, &t_square_lt) && GetCrossPoint(line_square_left, line_square_bottom, &t_square_lb)
		&& GetCrossPoint(line_square_right, line_square_top, &t_square_rt) && GetCrossPoint(line_square_right, line_square_bottom, &t_square_rb)){
			main_square.LeftTop = t_square_lt;
			main_square.LeftBottom = t_square_lb;
			main_square.RightTop = t_square_rt;
			main_square.RightBottom = t_square_rb;

			cvCircle(chess_image, cvPoint(main_square.LeftTop.x, main_square.LeftTop.y),5,cvScalar(0, 0, 0));
			cvCircle(chess_image, cvPoint(main_square.LeftBottom.x, main_square.LeftBottom.y),5,cvScalar(0, 0, 0));
			cvCircle(chess_image, cvPoint(main_square.RightTop.x, main_square.RightTop.y),5,cvScalar(0, 0, 0));
			cvCircle(chess_image, cvPoint(main_square.RightBottom.x, main_square.RightBottom.y),5,cvScalar(0, 0, 0));
	}

	else
		printf("Get Cross Point error!\n");
}

void Chess_recognition::GetInCrossPoint(IplImage *chess_image, vector<Chess_point> *point){

	point->clear();

	// in_line_point 오름차순 정렬
	for(int i=0;i<in_line_point_x1.size();i++){
		for(int j=i+1;j<in_line_point_x1.size();j++){
			if(in_line_point_x1[i].x > in_line_point_x1[j].x){
				MyPoint t_point = in_line_point_x1[i];
				in_line_point_x1[i] = in_line_point_x1[j];
				in_line_point_x1[j] = t_point;
			}
			//MyPointSwap(&in_line_point_x1[i], &in_line_point_x1[j]);
			if(in_line_point_x2[i].x > in_line_point_x2[j].x){
				MyPoint t_point = in_line_point_x2[i];
				in_line_point_x2[i] = in_line_point_x2[j];
				in_line_point_x2[j] = t_point;
			}
			//MyPointSwap(&in_line_point_x2[i], &in_line_point_x2[j]);
			if(in_line_point_y1[i].y > in_line_point_y1[j].y){
				MyPoint t_point = in_line_point_y1[i];
				in_line_point_y1[i] = in_line_point_y1[j];
				in_line_point_y1[j] = t_point;
			}
			//MyPointSwap(&in_line_point_y1[i], &in_line_point_y1[j]);
			if(in_line_point_y2[i].y > in_line_point_y2[j].y){
				MyPoint t_point = in_line_point_y2[i];
				in_line_point_y2[i] = in_line_point_y2[j];
				in_line_point_y2[j] = t_point;
			}
			//MyPointSwap(&in_line_point_y2[i], &in_line_point_y2[j]);
		}
	}

	MyLinePoint t_in_line_point_x, t_in_line_point_y;
	MyPoint t_in_point;

	for(int i=0;i<in_line_point_x1.size();i++){
		for(int j=0;j<in_line_point_x1.size();j++){

			SetMyLinePoint(in_line_point_x1[i].x, in_line_point_x1[i].y, in_line_point_x2[i].x, in_line_point_x2[i].y, &t_in_line_point_x);
			SetMyLinePoint(in_line_point_y1[j].x, in_line_point_y1[j].y, in_line_point_y2[j].x, in_line_point_y2[j].y, &t_in_line_point_y);

			GetCrossPoint(t_in_line_point_x, t_in_line_point_y, &t_in_point);
			//cvCircle(chess_image, cvPoint(t_in_point.x, t_in_point.y),5,cvScalar(0, 0, 0));

			Chess_point temp;
			temp.Cordinate = cvPoint(t_in_point.x, t_in_point.y);
			temp.index = cvPoint(i,j);
			point->push_back(temp);
		}
	}
}

void Chess_recognition::SetMyLinePoint(int x1, int y1, int x2, int y2, MyLinePoint *setLinePoint){
	setLinePoint->x1 = x1;
	setLinePoint->x2 = x2;
	setLinePoint->y1 = y1;
	setLinePoint->y2 = y2;
}

int Chess_recognition::Getgrayscale(IplImage *gray_image, int x, int y){
	int index = x + y*gray_image->widthStep ;
	unsigned char value = gray_image->imageData[index];

	return (int)value;
}

Chess_recognition::MyGrayPoint Chess_recognition::setMyGrayPoint(int grayscale, int x, int y){
	MyGrayPoint t_graypoint;

	t_graypoint.grayscale = grayscale;
	t_graypoint.x = x;
	t_graypoint.y = y;

	return t_graypoint;
}

Chess_recognition::MyPoint Chess_recognition::setMyPoint(int x, int y){
	MyPoint t_point;
	t_point.x = x;
	t_point.y = y;

	return t_point;
}

bool Chess_recognition::GetCrossPoint(MyLinePoint line1, MyLinePoint line2, MyPoint *out)
{
	float x12 = line1.x1 - line1.x2;
	float x34 = line2.x1 - line2.x2;
	float y12 = line1.y1 - line1.y2;
	float y34 = line2.y1 - line2.y2;

	float c = x12 * y34 - y12 * x34;

	if (fabs(c) < 0.01)
	{
		// No intersection
		return false;
	}
	else
	{
		// Intersection
		float a = line1.x1 * line1.y2 - line1.y1 * line1.x2;
		float b = line2.x1 * line2.y2 - line2.y1 * line2.x2;

		float x = (a * x34 - b * x12) / c;
		float y = (a * y34 - b * y12) / c;

		out->x = (int)x;
		out->y = (int)y;

		return true;
	}
}

void Chess_recognition::MemoryClear(){
	line_x1.clear(), line_x2.clear(), line_x_mid.clear(), line_y1.clear(), line_y2.clear(), line_y_mid.clear();

	in_line_point_x1.clear(), in_line_point_x2.clear(), in_line_point_y1.clear(), in_line_point_y2.clear();
}

void Chess_recognition::Chess_recog_wrapper(IplImage *src, vector<Chess_point> *point){
	vector<std::pair<float, float>> CH_LineX, CH_LineY;

	if(MODE == 1){
		Get_Line(&CH_LineX, &CH_LineY);
		drawLines(CH_LineX, src);
		drawLines(CH_LineY, src);
		findIntersections(CH_LineX, CH_LineY, point);
		Refine_CrossPoint(point);
		drawPoint(src, *point);
	}
	else if(MODE == 2){
		Chess_recognition_process(point);
		Refine_CrossPoint(point);
		drawPoint(src, *point);
	}
}