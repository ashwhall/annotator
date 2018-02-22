#include "Annotator.h"





void addText(string text, Point location, double fontSize, Scalar colour, int thickness) {
	putText(img, text.c_str(), location, FONT_HERSHEY_DUPLEX, fontSize, colour, thickness, CV_AA);
}
void addText(string text, Point location, Scalar colour, int thickness) {
	addText(text, location, fontSize, colour, thickness);
}
void addText(string text, Point location, int thickness) {
	addText(text, location, fontSize, Scalar(255, 255, 255), thickness);
}
void addText(string text, Point location, double fontSize, int thickness) {
	addText(text, location, fontSize, Scalar(255, 255, 255), thickness);
}
void addText(Mat img, string text, Point location) {
	putText(img, text.c_str(), location, FONT_HERSHEY_DUPLEX, fontSize, cvScalar(255, 255, 255), 2, CV_AA);
}
void addText(Mat img, int num, Point location) {
	ostringstream str;
	str << num;
	addText(img, str.str(), location);
}

// Used for the final menu, ensures that nothing funky happens if they've clicked the big ol' X button
void custom_imshow(Mat img) {
	// Set up a named window for resizing later
	cvNamedWindow(mainWindowHandle, WINDOW_NORMAL);
	cvResizeWindow(mainWindowHandle, 1280, 720);
	imshow(mainWindowHandle, img);
}

// Set a notification message to be displayed (only draw it once to the
// notification Mat for speed)
void addMessage(string message)
{
	if (overlayMessageImg != NULL)
		delete overlayMessageImg;
	int width = (int)(message.length() * 30 + 120);
	overlayMessageImg = new Mat(100, width, CV_8UC3, Scalar(0, 0, 0));
	addText(*overlayMessageImg, message, Point(20, overlayMessageImg->size().height / 2 + 15));
	messageTimer = (int)time(0);
	messageActive = true;
}
// Draw the notification Mat to screen
void drawMessage()
{
	if (messageActive && time(0) - messageTimer < MESSAGE_DISPLAY_TIME) {
		int width = overlayMessageImg->size().width;
		int height= overlayMessageImg->size().height;
		int xLoc = img.size().width / 2 - width / 2;
		int yLoc = img.size().height/ 5 - height / 2;
		Rect roi(xLoc, yLoc, width, height);
		overlayMessageImg->copyTo(img(roi));
	}
	else {
		messageActive = false;
	}
}

// A struct that is used to represent a single bounding box.
// It has two corners, and a frame that it relates to.
// The next reference allows it to be drawn as if it was moving smoothly to the
//  next region, so I can describe only keyframes.
struct TrackingPoint {
	Point pos;
	int frame;
	TrackingPoint* next;
	bool hasNext = false;
	bool stopFrame = false;
	bool visible = true;

	void setPos(int x, int y) {
		pos.x = x;
		pos.y = y;
		frameIndex[frame] |= KEYFRAME_INDEX;
	}
	void setPos(Point newPos) {
		pos = newPos;
		frameIndex[frame] = KEYFRAME_INDEX;
	}

	// Returns an offset between the point based on this point's frame and the current frame.
	Point interpolate(Point from, Point to) {
		float prop = ((float)(thisFrame - frame)) / (next->frame - frame);
		return prop*(to - from);
	}

	Point interpolate() {
		if (hasNext) {
			return interpolate(pos, next->pos);
		}
		return Point(0, 0);
	}

	// Shortening for using interpolated values
	Point getPos() {
		return pos + interpolate();
	}

	// Variations on draw
	void draw(Mat& img) {
		draw(img, Point(0, 0));
	}
	void draw(Mat& img, Point offset) {
		draw(img, offset, offset);
	}
	void draw(Mat& img, Point offset1, Point offset2) {
		Point target_offset_x = Point(CORNER_CIRCLE_RAD, 0);
		Point target_offset_y = Point(0, CORNER_CIRCLE_RAD);
		line(img, getPos() - target_offset_x, getPos() + target_offset_x, COLOUR_CROSSHAIR, 2);
		line(img, getPos() - target_offset_y, getPos() + target_offset_y, COLOUR_CROSSHAIR, 2);

		if (frame == 0 || frame == thisFrame) {
			// If it is the keyframe, then show it in red
			circle(img, pos, CORNER_CIRCLE_RAD, COLOUR_KEYFRAME, 3);
		}
		else {
			// If it is not the keyframe, interpolate the position
			//  and show in lime (purple if stopFrame).
			Point inter2 = getPos() - offset2;
			if (stopFrame) { 
				circle(img, inter2, CORNER_CIRCLE_RAD, COLOUR_STOP_FRAME, 3);
			}
			else
			{
				circle(img, inter2, CORNER_CIRCLE_RAD, COLOUR_NOT_KEYFRAME, 3);
			}

		}
	}
	bool contained(Point click) {
		return norm(click - (pos + interpolate())) <= CORNER_CIRCLE_RAD;
	}
	static TrackingPoint* newInterpolated(TrackingPoint* from) {
		TrackingPoint* r = new TrackingPoint;
		r->frame = thisFrame;
		r->setPos(from->pos + from->interpolate());
		return r;
	}
};

// A swimmer is the concept of a traceable object. It uses the NEARNESS_THRESH to determine
//  if a new region is a part of an existing swimmer.
// Also keeps track of the frames which the swimmer completes a stroke.
struct Swimmer {
	int laneNumber;
	vector<int> strokeFrames;
	vector<int> breathFrames;
	vector<int> breakoutFrames;
	vector<TrackingPoint*> pastTrackingPoints;
	TrackingPoint* latestRegion(int toFrame) {
		for (vector<TrackingPoint*>::reverse_iterator it = pastTrackingPoints.rbegin(); it != pastTrackingPoints.rend(); ++it) {
			if ((*it)->frame <= toFrame) {
				return *it;
			}
		}
		return NULL;
	}
	// Toggle an event in the given vector of event frames. This is the generalised function
	// which is used for any discrete events (based on the original `toggleStroke` code)
	void toggleEvent(int frame, vector<int>& frameVec) {
		if (frameVec.size() == 0) {
			frameVec.push_back(frame);
			return;
		}

		int toRemove = -1;
		int insertSpot = (int)frameVec.size();
		for (int i = 0; i < frameVec.size(); ++i) {
			// If it is already here, then "toggle off" == "remove"
			if (frameVec[i] == frame) {
				toRemove = i;
				break;
			}
			// Insert in front of the first element bigger.
			if (frameVec[i] > frame) {
				insertSpot = i;
				break;
			}
		}

		if (toRemove != -1) {
			frameVec.erase(frameVec.begin() + toRemove);
		}
		else {
			frameVec.insert(frameVec.begin() + insertSpot, frame);
		}
	}
	void toggleEvent(int frame) {
		switch (currentMarkerType) {
		case 0:
			toggleStroke(frame);
			break;
		case 1:
			toggleBreath(frame);
			break;
		case 2:
			toggleBreakout(frame);
			break;
		default: 
			// Add more event types here...
			break;
		}
	}
	// In the interest of being able to remove a stroke frame if a mistake is made,
	//  it is considered a toggle
	void toggleStroke(int frame) {
		toggleEvent(frame, strokeFrames);
	}
	void toggleBreath(int frame) {
		toggleEvent(frame, breathFrames);
	}
	void toggleBreakout(int frame) {
		toggleEvent(frame, breakoutFrames);
	}

	void addTrackingPoint(TrackingPoint* r) {
		if (pastTrackingPoints.size() == 0) {
			pastTrackingPoints.push_back(r);
			return;
		}

		for (int i = (int)pastTrackingPoints.size() - 1; i >= 0; --i) {
			// Check if this region needs to overwrite a current region.
			if (pastTrackingPoints[i]->frame == r->frame) {
				pastTrackingPoints[i]->pos = r->pos;
				break;
			}
			// Insert in place of the first region that should be earlier than it
			if (pastTrackingPoints[i]->frame < r->frame) {
				if (i + 1 < pastTrackingPoints.size()) {
					r->next = pastTrackingPoints[i + 1];
					r->hasNext = true;
				}
				pastTrackingPoints[i]->next = r;
				pastTrackingPoints[i]->hasNext = true;
				pastTrackingPoints.insert(pastTrackingPoints.begin() + i + 1, r);
				break;
			}
			// Insert before the first tracking point (only occurs when reading file)
			if (i == 0) {
				r->next = pastTrackingPoints[0];
				r->hasNext = true;
				

				pastTrackingPoints.insert(pastTrackingPoints.begin(), r);
			}
		}
	}
	void displayEvents(Mat img) {
		TrackingPoint* r = latestRegion(thisFrame);
		if (!r) {
			return;
		}

		Point bottomRight = r->getPos();
		// Display the smallest frame that's larger than the current frame (next)

		vector<int>* eventFrames = nullptr;
		if (currentMarkerType == 0) {
			eventFrames = &strokeFrames;
		}
		else if (currentMarkerType == 1) {
			eventFrames = &breathFrames;
		}
		else if (currentMarkerType == 2) {
			eventFrames = &breakoutFrames;
		}

		for (int i = 0; i < eventFrames->size(); ++i) {
			if ((*eventFrames)[i] == thisFrame) {
				circle(img, cvPoint(bottomRight.x, bottomRight.y), 100, Scalar(255, 255, 255), 5);
				//addText(img, strokeFrames[i], cvPoint(bottomRight.x - 225, bottomRight.y + 18))
				break;
			}
		}
		/*for (int i = 0; i < strokeFrames.size(); ++i) {
			if (strokeFrames[i] >= thisFrame) {
				addText(img, strokeFrames[i], cvPoint(bottomRight.x - 225, bottomRight.y + 18));
				//addText(img, strokeFrames[i], cvPoint(bottomRight.x - 50, topLeft.y + 18));
				break;
			}
		}
		// Display the largest frame that's smaller than the current frame (prev)
		for (int i = (int)strokeFrames.size() - 1; i >= 0; --i) {
			if (strokeFrames[i] <= thisFrame) {
				//addText(img, strokeFrames[i], cvPoint(topLeft.x + 20, topLeft.y + 18));
				addText(img, strokeFrames[i], cvPoint(bottomRight.x + 100, bottomRight.y + 18));
				break;
			}
		}*/
	}
	void removeTrackingPoint(int atFrame) {
		// Special case if we remove the first point
		if (pastTrackingPoints[0]->frame == atFrame)
		{
			pastTrackingPoints.erase(pastTrackingPoints.begin());
		}
		for (int i = 1; i < pastTrackingPoints.size(); ++i) {
			if (pastTrackingPoints[i]->frame == atFrame) {
				if (i == pastTrackingPoints.size() - 1) {
					pastTrackingPoints[i - 1]->next = NULL;
					pastTrackingPoints[i - 1]->hasNext = false;
				}
				else
					pastTrackingPoints[i - 1]->next = pastTrackingPoints[i + 1];
				pastTrackingPoints.erase(pastTrackingPoints.begin() + i);
				break;
			}
		}
	}
	void stopTracking(int atFrame) {
		TrackingPoint* r = latestRegion(atFrame);
		if (!r) {
			return;
		}

		// If it's not a keyframe, make it a keyframe
		if (r->frame != atFrame) {
			r = TrackingPoint::newInterpolated(r);
			addTrackingPoint(r);
		}
		r->stopFrame = !(r->stopFrame);
	}
};


ostream& operator<<(ostream& os, const TrackingPoint& r) {
	os << "\t\t\"" << r.frame << "\": {" << endl
		<< "\t\t\t\"pos\": { \"x\": " << r.pos.x << ", \"y\": " << r.pos.y << " }";
	if (r.stopFrame) {
		os << "," << endl << "\t\t\t\"stopFrame\": true";
	}
	os << endl << "\t\t}";
	return os;
}

void printEventList(ostream& os, vector<int>& eventList, const char* name) {
	os << "\t\"" << name << "\": [";
	cout << name << endl << eventList.size() << endl;
	if (eventList.size() > 0) {
		for (int i = 0; i < eventList.size() - 1; ++i) {
			os << eventList[i] << ", ";
		}
		os << eventList.back();
	}
	os << "]," << endl;
}
ostream& operator<<(ostream& os, Swimmer& s) {
	cout << "_____________" << endl;
	cout << "Writing lane " << s.laneNumber << endl;
	os << "{" << endl << "\t\"laneNumber\": " << s.laneNumber << ", " << endl;
	
	cout << "Writing strokes" << endl;
	printEventList(os, s.strokeFrames, "strokes");
	cout << "Writing breaths" << endl;
	printEventList(os, s.breathFrames, "breaths");
	cout << "Writing breakouts" << endl;
	printEventList(os, s.breakoutFrames, "breakouts");

	cout << "Writing keyframes" << endl;
	os << "\t\"keyFrames\": {" << endl;
	
	if (s.pastTrackingPoints.size() > 0) {
		for (int i = 0; i < s.pastTrackingPoints.size() - 1; ++i) {
			os << *(s.pastTrackingPoints[i]) << "," << endl;
		}
		os << *(s.pastTrackingPoints.back()) << endl;
	}
	os << "\t}" << endl;

	os << "}";
	cout << "_____________" << endl << endl;
	return os;
}

vector<Swimmer*> swimmers;

TrackingPoint* loadTrackingPoint(Json::Value rJson, string sFrame) {
	TrackingPoint* r = new TrackingPoint;
	int frame;
	istringstream(sFrame) >> frame;
	r->frame = frame;
	int x = rJson["pos"]["x"].asInt();
	int y = rJson["pos"]["y"].asInt();
	r->stopFrame = rJson["stopFrame"].asBool();
	r->setPos(x, y);
	return r;
}

Swimmer* loadSwimmer(Json::Value swimJson) {
	Swimmer* s = new Swimmer;

	s->laneNumber = swimJson["laneNumber"].asInt();
	
	int nStrokes = swimJson["strokes"].size();
	for (int i = 0; i < nStrokes; ++i) {
		s->toggleStroke(swimJson["strokes"][i].asInt());
	}

	int nBreaths = swimJson["breaths"].size();
	for (int i = 0; i < nBreaths; ++i) {
		s->toggleBreath(swimJson["breaths"][i].asInt());
	}
	
	int nBreakouts = swimJson["breakouts"].size();
	for (int i = 0; i < nBreakouts; ++i) {
		s->toggleBreakout(swimJson["breakouts"][i].asInt());
	}



	vector<string> keyframeInds = swimJson["keyFrames"].getMemberNames();

	int nKeyFrames = swimJson["keyFrames"].size();
	for (int i = 0; i < nKeyFrames; ++i) {
		Json::Value regionJson = swimJson["keyFrames"][keyframeInds[i]];
		TrackingPoint* r = loadTrackingPoint(regionJson, keyframeInds[i]);
		s->addTrackingPoint(r);
	}

	return s;
}
void showProgress(float percent) {
	int segments = 25;
	printf("\r");
	for (int i = 0; i < segments; ++i) {
		if (percent * segments < i) {
			printf("-");
		}
		else
		{
			printf("=");
		}
	}
	printf("%.2f%% complete", percent*100);
}



// Calculate the MD5 hash of the given file - is written to the JSON file
// and used for video identification (so we don't need to rely on filenames)

void calcBinaryFileMD5() {
	unsigned char digest[16];

	FILE* inFile = fopen(inputFilename.c_str(), "rb");

	ifstream file(inputFilename.c_str(), ios::binary | ios::ate);
	unsigned long totalBytes = (unsigned long)file.tellg();
	cout << "Hashing " << (totalBytes * 0.000001) << "MB file" << endl;
	
	
	int chunkSize = 1024;
	unsigned long requiredIters = totalBytes / chunkSize;
	char* data = new char[chunkSize];
	
	int bytes;
	MD5_CTX mdContext;
	MD5_Init(&mdContext);
	
	int updateFrequency = 5000;
	unsigned long currIter = 0;
	while ((bytes = (int)fread(data, 1, chunkSize, inFile)) != 0) {
		MD5_Update(&mdContext, data, chunkSize);
		currIter++;
		if (currIter % updateFrequency == 0) {
			showProgress((float)currIter / requiredIters);
		}
	}
	showProgress(1);
	cout << endl;

	MD5_Final(digest, &mdContext);

	char mdString[33];
	for (int i = 0; i < 16; i++)
		sprintf(&mdString[i * 2], "%02x", (unsigned int)digest[i]);

	fileHash = string("md5:") + string(mdString);

	delete[] data;
}

void load(const char* filename) {
	ifstream* f;
	bool shouldCalcHash = true;
	// Look for a backup file first
	f = new ifstream(string(filename) + string(".BAK"));
	if(!f->good())
		f = new ifstream(filename);
	if (f->good()) {
		Json::Value past;
		*f >> past;
		
		int i = 0;
		// See if the first object is the event type, instead of a swimmer
		// If so, set it globally and offset the start of the swimmers by 1
		string e_type = past[0]["eventType"].asString();
		if (e_type.length() != 0){
			i = 1;
			eventType = e_type;
		}
		string file_hash = past[0]["fileHash"].asString();
		if (file_hash.length() != 0) {
			i = 1;
			fileHash= file_hash;
			shouldCalcHash = false;
		}
		
		// Read in all swimmers from file, noting which lanes have been represented
		bool foundLanes[10] = { false };
		int nObj = past.size();
		for (; i < nObj; ++i) {
			foundLanes[past[i]["laneNumber"].asInt()] = true;
			Swimmer* s = loadSwimmer(past[i]);
			if(s != NULL)
				swimmers.push_back(s);
		}

		// Create the missing swimmers
		for (i = 0; i < 10; ++i) {
			if (!foundLanes[i]) {
				Swimmer* s = new Swimmer;
				s->laneNumber = i;
				swimmers.push_back(s);
			}
		}
		// Sort the swimmers just for the sake of happiness/neatness
		sort(swimmers.begin(), swimmers.end(),
			[](const Swimmer* a, const Swimmer* b) -> bool
		{
			return a->laneNumber < b->laneNumber;
		});

	}
	else
	{
		// No swimmers or no file - make all empty swimmers
		for (int i = 0; i < 10; ++i) {
			Swimmer* s = new Swimmer;
			s->laneNumber = i;
			swimmers.push_back(s);
		}
	}
	if (shouldCalcHash) {
		cout << "Calculating file hash - will take a moment" << endl;
		calcBinaryFileMD5();
	}
	delete f;
}






// Draws a floating zoomed-in box at the mouse position
void drawZoomBox() {
	int mouse_x = handle.x, mouse_y = handle.y;

	int topLeft_x = mouse_x - ZOOM_SIZE / 2;
	int topLeft_y = mouse_y - ZOOM_SIZE / 2;
	int botRight_x = mouse_x + ZOOM_SIZE / 2;
	int botRight_y = mouse_y + ZOOM_SIZE / 2;

	int cut_top_x = 0, cut_top_y = 0;
	if (topLeft_x < 0)
		cut_top_x = -topLeft_x;
	if (topLeft_y < 0)
		cut_top_y = -topLeft_y;
	

	int topLeft_x_cut = max(0, topLeft_x);
	int topLeft_y_cut = max(0, topLeft_y);
	int botRight_x_cut = min(img.size().width, botRight_x);
	int botRight_y_cut = min(img.size().height, botRight_y);


	int sourceWid = botRight_x_cut - topLeft_x_cut;
	int sourceHei = botRight_y_cut - topLeft_y_cut;

	if (sourceWid <= 0 || sourceHei <= 0)
		return;
	
	Rect sourceROI(topLeft_x_cut, topLeft_y_cut, sourceWid, sourceHei);
	Mat sourceImg = (*zoomImg)(sourceROI);
	
	Mat image(ZOOM_SIZE, ZOOM_SIZE, CV_8UC3, Scalar(0, 0, 0));
	Rect roi(cut_top_x, cut_top_y, sourceImg.size().width, sourceImg.size().height);
	
	sourceImg.copyTo(image(roi));
	rectangle(image, Rect(0, 0, image.size().width, image.size().height), Scalar(0, 0, 0), 2);
	
	resize(image, image, Size(), zoomFactor, zoomFactor);
		
	topLeft_x = mouse_x - ZOOM_SIZE*zoomFactor/2;
	topLeft_y = mouse_y - ZOOM_SIZE*zoomFactor / 2;
	botRight_x = mouse_x + ZOOM_SIZE*zoomFactor / 2;
	botRight_y = mouse_y + ZOOM_SIZE*zoomFactor / 2;

	cut_top_x = -min(0, topLeft_x);
	cut_top_y = -min(0, topLeft_y);

	int cut_bot_x = -min(0, img.size().width - botRight_x);
	int cut_bot_y = -min(0, img.size().height - botRight_y);

	Mat cutImg;
	roi = Rect(	cut_top_x, 
				cut_top_y, 
				image.size().width - cut_top_x - cut_bot_x, 
				image.size().height - cut_top_y - cut_bot_y);
	cutImg = image(roi).clone();

	int xPos = min(img.size().width, max(0, mouse_x - ZOOM_SIZE*zoomFactor / 2));
	int yPos = min(img.size().height, max(0, mouse_y - ZOOM_SIZE*zoomFactor / 2));

	
	roi = Rect(xPos, yPos, cutImg.size().width, cutImg.size().height);
	cutImg.copyTo(img(roi));
}


const int numHelpStringLines = 24;
char* helpStrings[] = { "Keyboard controls:",
						"    Play/Pause:              <SPACE>",
						"    Frame Skipping:",
						"        Skip -/+ 1 frame:    a / d",
						"        Skip -/+ 5 frames:   s / f",
						"        Skip -/+ 10 frames:  q / e",
						"        Skip -/+ 100 frames: w / r",
						"",
						"    Enable/Disable Zoom:     z",
						"    Zoom -/+:                x / c",
						"",
						"    Select lane:             0-9",
						"    Select all lanes:        `",
						"",
						"    Change event-label:      <TAB>",
						"",
						"    Display help:            h",
						"    Save and exit:           <ESC>",
						"Mouse controls:",
						"    Toggle event-label:      <RIGHT-MOUSE>",
						"    Add keyframe:            <LEFT-MOUSE>",
						"    Move keyframe:           Drag <LEFT-MOUSE>",
						"    Convert to endframe:     <SHIFT>+<LEFT-MOUSE>",
						"    Delete keyframe:         <CTRL>+<LEFT-MOUSE>" };
// Display shortcuts
void displayHelp() {
	int width = img.size().width, height = img.size().height;
	double spacing = height / 35.0;
	rectangle(img, Rect(0, 0, width, height), Scalar(0, 0, 0), -1);
		
	for (int i = 0; i < numHelpStringLines; ++i)
	{
		addText(helpStrings[i], Point(20, (int)(spacing*(i + 1))), 2);
	}

	imshow(mainWindowHandle, img);
}

void displayInfo() {
	double spacing = img.size().width / 3200.0;
	// Display current frame
	addText(string("Frame: ") + to_string(thisFrame) + string("/") + to_string(maxFrames), 
			Point((int)(20 * spacing), 50), Scalar(0, 127, 255), 3);
	
	// Display zoomsize/amount
	if (zoomActive)
	{
		string zoomString = string("Zoom: ") + to_string(zoomFactor);
		addText(zoomString, Point((int)(600 * spacing), 50), Scalar(0, 127, 255), 3);
	}
	// Where the filename starts in the path - using max means we match UNIX and WIN
	size_t a = inputFilename.find_last_of("\\") + 1;
	size_t b = inputFilename.find_last_of("/") + 1;
	int splitIndex = (int)max(a, b);
	
	string activeSwimmerString = string("   Active swimmer: ") + (selectedSwimmer == -1 ? string("All") : to_string(selectedSwimmer));
	addText(string("File: ") + inputFilename.substr(splitIndex, inputFilename.length() - splitIndex) + 
			activeSwimmerString +
			string("   Marking: ") + string(markerTypes[currentMarkerType]),
			Point((int)(900 * spacing), 50), Scalar(255, 255, 255), 3);

}
void displayFrameIndex() {
	int imgWidth = img.size().width;
	int imgHeight = img.size().height;

	frameIndexRegionSize = (float)imgWidth / maxFrames;
	
	Mat frameIndexImg(frameIndexImgHeight, imgWidth, CV_8UC3, Scalar(0, 0, 0));
	for (int i = 0; i < maxFrames; ++i) {
		if (frameIndex[i])
		{			
			if (frameIndex[i] & KEYFRAME_INDEX)
				line(frameIndexImg, Point((int)(i*frameIndexRegionSize), 0),
						Point((int)(i*frameIndexRegionSize), frameIndexImgHeight), COLOUR_KEYFRAME, 5);
			else if (frameIndex[i] & STOPFRAME_INDEX)
				line(frameIndexImg, Point((int)(i*frameIndexRegionSize), 0),
					Point((int)(i*frameIndexRegionSize), frameIndexImgHeight), COLOUR_STOP_FRAME, 5);
		}
	}
	int currentEventIndex = -1;
	if (currentMarkerType == 0) {
		currentEventIndex = STROKE_INDEX;
	}
	else if (currentMarkerType == 1) {
		currentEventIndex = BREATH_INDEX;
	}
	else if (currentMarkerType == 2) {
		currentEventIndex = BREAKOUT_INDEX;
	}
	for (int i = 0; i < maxFrames; ++i) {
		if (frameIndex[i] & currentEventIndex) {
			circle(frameIndexImg, Point((int)(i*frameIndexRegionSize), 50), 10,
				Scalar(0, 255, 255), -1);
	}

	}
	// Current frame indicator
	int triangleWidth = 12;
	int triangleHeight = (int)(triangleWidth*1.154); // Height = sqrt(3)*side/2
	Point trianglePoints[3];
	trianglePoints[0] = Point((int)(thisFrame * frameIndexRegionSize) - triangleWidth, 0);
	trianglePoints[1] = Point((int)(thisFrame * frameIndexRegionSize) + triangleWidth, 0);
	trianglePoints[2] = Point((int)(thisFrame * frameIndexRegionSize), triangleHeight);
	const Point* ppt[1] = { trianglePoints };
	int npt[] = { 3 };
	fillPoly(frameIndexImg, ppt, npt, 1, Scalar(255, 255, 255));

	Rect roi(0, imgHeight - frameIndexImgHeight, imgWidth, frameIndexImgHeight);
	frameIndexImg.copyTo(img(roi));
}
// Redraws the regions based on the current frame
void redraw() {
	// Copy from the frame again
	frame.copyTo(img);

	if (helpActive) {
		displayHelp();
		return;
	}
	rectangle(img, Rect(0, 0, img.size().width, 60), Scalar(0, 0, 0), -1);

	// For each relevant region, draw it onto the image
	int swimmer_count = (int)swimmers.size();

	for (int i = 0; i < swimmers.size(); i++) {
		if (selectedSwimmer == -1 || swimmers[i]->laneNumber == selectedSwimmer) {
			TrackingPoint* swimmerRegion = swimmers[i]->latestRegion(thisFrame);
			if (swimmerRegion && swimmerRegion->visible) {
				swimmerRegion->draw(img);
				swimmers[i]->displayEvents(img);
			}
		}
	}
	if (!zoomActive && zoomImg != NULL) {
		delete zoomImg;
		zoomImg = NULL;
	}

	if (zoomActive && !playing){
		if (zoomImg == NULL) {
			zoomImg = new Mat();
			frame.copyTo(*zoomImg);
		}

		drawZoomBox();
	}
	displayInfo();

	displayFrameIndex();
	
	drawMessage();
	imshow(mainWindowHandle, img);	
}

Swimmer* existingActiveSwimmer() {
	for (Swimmer* s : swimmers) {
		if (s->laneNumber == selectedSwimmer) {
			return s;
		}
	}
	return NULL;
}
// Determine if the point should be considered a continuation of an existing swimmer
Swimmer* attachedSwimmer(Point cor2) {
	for (vector<Swimmer*>::iterator it = swimmers.begin(); it != swimmers.end(); ++it) {
		TrackingPoint* reg = (*it)->latestRegion(thisFrame);
		// If the swimmer has a region already and it's close enough to this new one
		if (reg && norm(cor2 - (*reg).pos) < NEARNESS_THRESH) {
			return (*it);
		}
	}
	return NULL;
}

// If there is a region at that location, return it, else null
// Overloaded to return the index if we need it
TrackingPoint* existingRegion(Point loc, Swimmer** s, int* index) {
	for (vector<Swimmer*>::iterator it = swimmers.begin(); it != swimmers.end(); ++it) {
		if ((*it)->laneNumber == selectedSwimmer) {
			TrackingPoint* reg = (*it)->latestRegion(thisFrame);
			if (reg && reg->contained(loc)) {
				if (index != NULL)
					*index = (int)(it - swimmers.begin());
				(*s) = (*it);
				return reg;
			}
		}
	}
	return NULL;
}
TrackingPoint* existingRegion(Point loc, Swimmer** s) {
	return existingRegion(loc, s, NULL);
}

// Grab the current frame
void readFrame() {
	vid.set(CV_CAP_PROP_POS_FRAMES, thisFrame - 1); // Frames are 0 indexed
	vid >> frame;

	if (frame.cols == 0) {
		vid.open(inputFilename);
		vid.set(CV_CAP_PROP_POS_FRAMES, thisFrame - 1); // Frames are 0 indexed
		vid >> frame;
	}

	//addText(frame, thisFrame, cvPoint(30, 30));
}

// Change frames; update number, read frame and redraw
void jumpFrame(int jump) {
	if (thisFrame + jump < 1) {
		thisFrame = 1;
	}
	else if (thisFrame + jump > maxFrames) {
		thisFrame = maxFrames;
	}
	else {
		thisFrame += jump;
	}
	readFrame();
	if (zoomImg != NULL) {
		delete zoomImg;
		zoomImg = new Mat();
		frame.copyTo(*zoomImg);
	}
	redraw();
}

void jumpToFrame(int frameNum) {
	frameNum = max(1, min(maxFrames, frameNum));
	thisFrame = frameNum;
	readFrame();
	if (zoomImg != NULL) {
		delete zoomImg;
		zoomImg = new Mat();
		frame.copyTo(*zoomImg);
	}
	redraw();
}

// Move to the next frame - faster than jumpFrame
void nextFrame() {
	if (thisFrame >= maxFrames) {
		playing = false;
		return;
	}
	if (frame.cols == 0)
		vid.open(inputFilename);
	vid >> frame;
	++thisFrame;
	
	redraw();
}

// When moving or resizing a region, it could have bee only showing
// an interpolation. This function makes a new region at this new frame
TrackingPoint* newKeyFrame(TrackingPoint* candidate, Swimmer* s) {
	TrackingPoint* r;
	if (candidate->frame != thisFrame) {
		r = TrackingPoint::newInterpolated(candidate);
		s->addTrackingPoint(r);
	}
	else {
		r = candidate;
	}
	return r;
}

// Writes data to output file
void printResults(const char* filename) {
	//if (swimmers.size() == 0)
		//return;
	cout << endl;
	cout << "*******************************" << endl;
	cout << "Opening output file for writing" << endl;
	ofstream writeFile;
	writeFile.open(filename);


	cout << "Writing race metadata" << endl;
	writeFile << "[" << endl
			  << "{" << endl;
	writeFile << "\t\"eventType\": \"" << eventType << "\"," << endl;
	writeFile << "\t\"fileHash\": \"" << fileHash << "\"" << endl;

	writeFile << "}," << endl;
	cout << endl;
	if (swimmers.size() > 0) {
		for (int i = 0; i < swimmers.size() - 1; ++i) {
			writeFile << (*swimmers[i]) << "," << endl;
		}
		writeFile << (*swimmers.back()) << endl << "]" << endl;
	}
	else {
		writeFile << "{\n}\n]";
	}
	cout << endl;
	cout << "Closing output file" << endl;
	writeFile.close();
}

void buildFrameIndex() {
	if (frameIndex != NULL)
		delete[] frameIndex;
	frameIndex = new int[maxFrames];
	for (int i = 0; i < maxFrames; i++) {
		frameIndex[i] = 0;
	}

	for (Swimmer* swimmer : swimmers) {
		if (swimmer != NULL) {
			if (selectedSwimmer == -1 || swimmer->laneNumber == selectedSwimmer)
			{
				for (TrackingPoint* trackingPoint : swimmer->pastTrackingPoints) {
					if (trackingPoint->stopFrame)
						frameIndex[trackingPoint->frame] |= STOPFRAME_INDEX;
					else
						frameIndex[trackingPoint->frame] |= KEYFRAME_INDEX;
				}
				vector<int>* eventFrames = nullptr;
				int eventIndex = -1;
				if (currentMarkerType == 0) {
					eventFrames = &swimmer->strokeFrames;
					eventIndex = STROKE_INDEX;
				}
				else if (currentMarkerType == 1) {
					eventFrames = &swimmer->breathFrames;
					eventIndex = BREATH_INDEX;
				}
				else if (currentMarkerType == 2) {
					eventFrames = &swimmer->breakoutFrames;
					eventIndex = BREAKOUT_INDEX;
				}
				for (int strokeFrame : *eventFrames) {
					frameIndex[strokeFrame] |= eventIndex;
				}

			}
		}
	}
}
// Indicate which swimmer we're looking at.
// Setting selectedSwimmer to -1 means we see all
void selectSwimmer(char laneNumber) {
	selectedSwimmer = laneNumber - '0';
	if (laneNumber == '`')
		selectedSwimmer = -1;
	
	buildFrameIndex();
	redraw();
}
void saveBackupFile()
{
	addMessage("Saving backup file");
	redraw();
	string backupFilename = outputFilename + string(".BAK");
	printResults(backupFilename.c_str());
}
void deleteBackupFile()
{
	string backupFilename = outputFilename + string(".BAK");
	remove(backupFilename.c_str());
}
void runMenuLoop()
{
	// Autosave timer
	time_t lastSave = time(0);

	while (!getWindowProperty(mainWindowHandle, 0)) {
		int key = waitKey(30); // read key
		if (key == 27) { // <esc>
			break;
		}
		else if (key == 'h') {
			helpActive = !helpActive;
			redraw();
		}
		// Only respond to key presses if the help screen is hidden
		if (!helpActive) {
			if (playing) {
				nextFrame();
				wasPlaying = true;
			}
			if (wasPlaying) {
				if (zoomImg != NULL) {
					delete zoomImg;
					zoomImg = new Mat();
					frame.copyTo(*zoomImg);
				}
				wasPlaying = false;
			}
			if (key >= '0' && key <= '9' || key == '`') {
				selectSwimmer(key);
			} 
			else if (key == 'w') {
				jumpFrame(-THIRD_LEVEL_JUMP);
			}
			else if (key == 'r') {
				jumpFrame(THIRD_LEVEL_JUMP);
			}
			else if (key == 'q') {
				jumpFrame(-SECOND_LEVEL_JUMP);
			}
			else if (key == 'e') {
				jumpFrame(SECOND_LEVEL_JUMP);
			}
			else if (key == 's') {
				jumpFrame(-FIRST_LEVEL_JUMP);
			}
			else if (key == 'f') {
				jumpFrame(FIRST_LEVEL_JUMP);
			}
			else if (key == 'a') {
				jumpFrame(-1);
			}
			else if (key == 'd') {
				jumpFrame(1);
			}
			else if (key == 32) { // <space>
				playing = !playing;
			}
			else if (key == 'z') {
				zoomActive = !zoomActive;
				redraw();
			}
			else if (key == 'x') {
				zoomFactor = max(MIN_ZOOM_FACTOR, min(MAX_ZOOM_FACTOR, zoomFactor - 1));
				redraw();
			}
			else if (key == 'c') {
				zoomFactor = max(MIN_ZOOM_FACTOR, min(MAX_ZOOM_FACTOR, zoomFactor + 1));
				redraw();
			}
			else if (key == 9) { // <tab>
				currentMarkerType = abs(currentMarkerType + 1) % markerTypeCount;
				buildFrameIndex();
				redraw();
			}
		}
		if (time(0) - lastSave > AUTOSAVE_INTERVAL) {
			saveBackupFile();
			lastSave = time(0);
		}
		if (messageActive && time(0) - messageTimer >= MESSAGE_DISPLAY_TIME)
			redraw();
	}
}


// Draws the event type options to the screen
void redrawEventChoices(int index) {
	int width = img.size().width, height = img.size().height;
	double spacing = height / 35.0;
	rectangle(img, Rect(0, 0, width, height), Scalar(0, 0, 0), -1);

	addText("You must enter the event type before exiting.", Point(20, (int)(spacing)), 2);
	for (int i = 0; i < eventTypeCount; ++i) {
		if (i == index) {
			addText(to_string(i+1) + string(": ") + string(eventTypes[i]), Point(100, (int)(spacing*(i + 2))), Scalar(127, 255, 0), 2);
		}
		else
		{
			addText(to_string(i+1) + string(": ") + string(eventTypes[i]), Point(100, (int)(spacing*(i + 2))), 2);
		}
		
	}
	addText("Press the corresponding number, then <ENTER> to save.",
		Point(20, (int)(spacing*(eventTypeCount + 2))), 2);
	addText("<ESC> to cancel and return to annotating.",
		Point(20, (int)(spacing*(eventTypeCount + 3))), 2);

	custom_imshow(img);
}

// Prompts the user to choose the event type - return val indicates if they wish to actually exit 
bool setEventType() {
	// Highlight the currently selected event_type
	int index = -1;
	if (eventType.length() > 0) {
		for(int i = 0; i < eventTypeCount; ++i) {
			if (strcmp(eventTypes[i], eventType.c_str()) == 0) {
				index = i+1;
			}
		}
	}
	redrawEventChoices(index-1);

	// Loop, taking input and updating the options
	while (true) {
		int key = waitKey(30); // read key
		if(key == 27) { // <esc>
			return false;
		}
		if (key == 13) // <enter>
		{
			if (index >= 1 && index <= eventTypeCount) 
				break;
		}
		else if (key != 255) {
			index = key - 48;
			redrawEventChoices(index-1);
		}
	}
	eventType = string(eventTypes[index-1]);
	
	return true;
}

int main(int argc, char* argv[]) {
	
	// If they provide only one arg, create a matching output file with .json extension
	if (argc == 2) {
		inputFilename = argv[1];
		int ext_index = (int)inputFilename.find_last_of(".");

		outputFilename = inputFilename;
		outputFilename.replace(ext_index, outputFilename.length(), ".json");
		
		cout << "input file: " << inputFilename << endl
			<< "output file: " << outputFilename << endl;
	}
	else if (argc == 3) {
		inputFilename = argv[1];
		outputFilename = argv[2];
	}
	else {
		cout << "Usage: ./Annotator <video_file> <json_file>" << endl;
		cout << "                    OR " << endl;
		cout << "       ./Annotator <video_file>" << endl;
		for (int i = 0; i < numHelpStringLines; ++i)
		{
			cout << helpStrings[i] << endl;
		}
		return 0;
		//inputFilename = "D:\\swimming\\footage\\red drive\\CLIP0000085_000.mov";
		//outputFilename = "D:\\swimming\\footage\\red drive\\CLIP0000085_000.json";
	}
	
	vid.open(inputFilename);
	maxFrames = (int)vid.get(CV_CAP_PROP_FRAME_COUNT);
	maxFrames -= 3; // For some unknown reason, the frame count is always out by three...
	cout << "Frame count: " << maxFrames << endl;

	readFrame();
	fontSize = (frame.size().height / 1100.0);
	//font_size = 1.5;
	buildFrameIndex();
	
	
	load(outputFilename.c_str());

	// Set up a named window for resizing later
	cvNamedWindow(mainWindowHandle, WINDOW_NORMAL);
	cvResizeWindow(mainWindowHandle, 1280, 720);

	// Jump zero frames == read current frame
	jumpFrame(0);

	printf("Press ESC to exit\n");


	// The loop gives them a chance to cancel quitting
	bool actually_quit = false;
	do
	{
		redraw();
		cvSetMouseCallback(mainWindowHandle, normalMouseHandler, NULL);
		runMenuLoop();
		cvSetMouseCallback(mainWindowHandle, NULL, NULL);
		actually_quit = setEventType();
	} while (!actually_quit);

	printResults(outputFilename.c_str());
	cout << "Deleting backup file" << endl << endl;
	deleteBackupFile();
	cout << "Complete. Program exiting normally" << endl;
	return 0;
}



// Debugging tool: prints memory locations of things in a swimmer
void printSwimmer(Swimmer* s) {
	cout << "Swimmer: ==================" << endl;
	for (vector<TrackingPoint*>::iterator it = (s->pastTrackingPoints).begin(); it != (s->pastTrackingPoints).end(); ++it) {
		cout << "Mem:       " << &(**it) << endl;
		cout << "Key Frame: " << (*it)->frame << endl;
		cout << (*it)->pos << endl;
		if ((*it)->next) {
			cout << "Mem(next):      " << (*it)->next << endl;
			cout << "Next key frame: " << (*it)->next->frame << endl;
			cout << (*it)->next->pos << endl;
		}
		cout << endl << endl;
	}
}

bool trackBarClick() {
	int x = handle.x, y = handle.y;

	// Check we clicked in the region
	if (y > img.size().height - frameIndexImgHeight) {
		int frame = (int)(x / frameIndexRegionSize);
		jumpToFrame(frame);

		return true;
	}
	return false;
}
void normalMouseHandler(int event, int x, int y, int flags, void* params) {
	// Dont' respond to the mouse if the help screen is visible or the video's playing
	if (helpActive)
		return;
	handle = Point(x, y);
	if (zoomActive && !playing) {
		redraw();
	}

	// if it's not a left-click, then we don't care
	/*if (event != CV_EVENT_LBUTTONDOWN) {
		return;
	}*/

	// Delete keyframe
	if (!playing && flags == (EVENT_FLAG_CTRLKEY + EVENT_FLAG_LBUTTON)) {
		Swimmer* s;
		int swimmerIndex;
		TrackingPoint* r = existingRegion(handle, &s, &swimmerIndex);
		
		// Entirely delete swimmer if we're removing their only tracking point
		if (r && s && s->pastTrackingPoints.size() <= 1) {
		
			swimmers.erase(swimmers.begin() + swimmerIndex);
			buildFrameIndex();
			redraw();

		}
		if (r && r->frame == thisFrame) {
			s->removeTrackingPoint(thisFrame);
			buildFrameIndex();
			redraw();
		}
	}
	else if (!playing && event == CV_EVENT_RBUTTONDOWN) {
		for (vector<Swimmer*>::iterator it = swimmers.begin(); it != swimmers.end(); ++it) {
			if ((*it)->laneNumber == selectedSwimmer) {
				(*it)->toggleEvent(thisFrame);
				buildFrameIndex();
				redraw();
			}
		}
	}
	else if (!playing && flags == (EVENT_FLAG_SHIFTKEY + EVENT_FLAG_LBUTTON)) {
		Swimmer* s = NULL;
		existingRegion(handle, &s);
		if (s) {
			s->stopTracking(thisFrame);
			buildFrameIndex();
			redraw();
		}
	}
	/*else if (!playing && flags == (EVENT_FLAG_CTRLKEY + EVENT_FLAG_ALTKEY + EVENT_FLAG_LBUTTON)) {
		Swimmer* s;
		int swimmerIndex;

		// Entirely delete swimmer if we're removing their only tracking point
		if (existingRegion(handle, &s, &swimmerIndex)) {
			swimmers.erase(swimmers.begin() + swimmerIndex);
			buildFrameIndex();
			redraw();
		}
	}*/
	else if (event == CV_EVENT_LBUTTONDOWN) {
		TrackingPoint* existing = NULL;
		Swimmer* s = NULL;

		if (trackBarClick() || selectedSwimmer == -1)
		{ 
		}
		else if (!playing && !!(existing = existingRegion(handle, &s))) {
			r = newKeyFrame(existing, s);
			r->visible = false;
			cvSetMouseCallback(mainWindowHandle, moveRegMouseHandler, NULL);
		}
		else if(!playing) {
			r = new TrackingPoint;
			(*r).pos = handle;
			r->visible = false;
			cvSetMouseCallback(mainWindowHandle, newRegMouseHandler, NULL);
		}
	}
}

// Corner 1 was registered when changing to this mode.
// Corner 2 is the continually updated other corner
void newRegMouseHandler(int event, int x, int y, int flags, void* params) {
	// Clean display
	handle = Point(x, y);
	redraw();

	// Take the new position, add it to the image and show it
	r->pos = Point(x, y);
	//r->draw(img);
	//imshow(mainWindowHandle, img);
	//redraw();
	// When the user lets go, record the actual frame
	if (event == CV_EVENT_LBUTTONUP) {
		r->visible = true;
		r->frame = thisFrame;
		// If the region is large enough to store, find a swimmer
		//  (or make a new one), and add the region to it.
		//Swimmer* s = attachedSwimmer(r->pos);
		Swimmer* s = existingActiveSwimmer();
		if (!s) {
			s = new Swimmer;
			s->laneNumber = selectedSwimmer;
			swimmers.push_back(s);
		}
		s->addTrackingPoint(r);
		buildFrameIndex();
		// Ensure clean display
		redraw();
		// Set back to default mouse mode.
		cvSetMouseCallback(mainWindowHandle, normalMouseHandler, NULL);
	}
}

void moveRegMouseHandler(int event, int x, int y, int flags, void* params) {
	handle = Point(x, y);
	redraw();

	// The offset is the difference from the original starting position
	Point offset = handle - Point(x, y);
	//r->draw(img, offset);
	//imshow(mainWindowHandle, img);
	//redraw();
	// When we're done moving it, actually update the corner positions
	if (event == CV_EVENT_LBUTTONUP) {
		r->visible = true;
		r->pos = Point(x, y);
		redraw();
		// And back to default mouse mode.
		cvSetMouseCallback(mainWindowHandle, normalMouseHandler, NULL);
	}
}

void moveRegCorMouseHandler(int event, int x, int y, int flags, void* params) {
	handle = Point(x, y);
	redraw();

	// The offset is the difference from the original starting position
	// Only use this offset on corner 2
	Point offset = handle - Point(x, y);
	r->draw(img, Point(0, 0), offset);
	imshow(mainWindowHandle, img);

	if (event == CV_EVENT_LBUTTONUP) {
		r->pos = Point(x, y);
		redraw();
		cvSetMouseCallback(mainWindowHandle, normalMouseHandler, NULL);
	}
}
