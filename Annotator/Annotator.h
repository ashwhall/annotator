#pragma once
#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#include <json/json.h>
#include <iostream>
#include <fstream>
#include <stdio.h>
#include <vector>
#include <string>
#include <sstream>
#include <time.h>
#include "spline.h"
#include <openssl\md5.h>
#include <iomanip>

#define NEARNESS_THRESH 30
#define MIN_REGION_SIZE 50
#define CORNER_CIRCLE_RAD 30
#define THIRD_LEVEL_JUMP 100
#define SECOND_LEVEL_JUMP 10
#define FIRST_LEVEL_JUMP 5

#define COLOUR_KEYFRAME Scalar(0, 0, 244)
#define COLOUR_NOT_KEYFRAME Scalar(30, 230, 200)
#define COLOUR_STOP_FRAME Scalar(255, 0, 255) 
#define COLOUR_CROSSHAIR Scalar(255, 255, 255)
#define MAX_ZOOM_FACTOR 8
#define MIN_ZOOM_FACTOR 2
#define ZOOM_SIZE 200
#define AUTOSAVE_INTERVAL 30
#define MESSAGE_DISPLAY_TIME 3


double fontSize = 1;

int zoomFactor = 2;
bool playing = false;
bool zoomActive = false;

bool helpActive = true;

using namespace std;
using namespace cv;

#define KEYFRAME_INDEX 1
#define STOPFRAME_INDEX 2
#define STROKE_INDEX 4
#define BREATH_INDEX 8
#define BREAKOUT_INDEX 16
int* frameIndex;
int frameIndexImgHeight = 50;
double frameIndexRegionSize;
Mat* frameIndexImg;

Mat* overlayMessageImg;
bool messageActive = false;
int messageTimer = 0;

Mat frame;
Mat img;

Mat* zoomImg = NULL;

VideoCapture vid;

string inputFilename, outputFilename;

int thisFrame = 1;
int maxFrames;


struct TrackingPoint* r;
Point handle;

const char* mainWindowHandle = "Video Annotation Tool";


int eventTypeCount = 5;
const char* eventTypes[5] = { "Freestyle", "Backstroke", "Breaststroke", "Butterfly", "Medley" };
string eventType = "";

string fileHash = "";

int selectedSwimmer = -1;

bool wasPlaying = false; // This helps us know when we have just stopped playing


						 // TODO: Add more event types here
int markerTypeCount = 3;
const char* markerTypes[3] = { "Stroke", "Breath", "Breakout" };
int currentMarkerType = 0;






// The mouse handler will change when the user begins to drag a new region into
//  existence or resize an old one.
// normalMouseHandler - default handler that swaps out to other handlers when needed
// newRegMouseHandler - handler for drawing a new region
// moveRegMouseHandler - handler for moving a region
// moveRegCorMouseHandler - handler for moving a corner of a region (resizing)
// markerMouseHandler - handler for adding the markers at the start of the application
// The mouse handlers use the variable 'r' from the top to recall the current region
//  across frames and the variable 'handle' to recall the initially clicked position
void normalMouseHandler(int event, int x, int y, int flags, void* params);
void newRegMouseHandler(int event, int x, int y, int flags, void* params);
void moveRegMouseHandler(int event, int x, int y, int flags, void* params);
void moveRegCorMouseHandler(int event, int x, int y, int flags, void* params);
