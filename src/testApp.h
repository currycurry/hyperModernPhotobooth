#pragma once

#include "ofMain.h"
#include "ofxOpenCv.h"
#include "ofxKinect.h"


class testApp : public ofBaseApp {
	public:
		
        void setup();
		void update();
		void draw();
		void exit();

		void keyPressed  (int key);
		void mouseMoved(int x, int y );
		void mouseDragged(int x, int y, int button);
		void mousePressed(int x, int y, int button);
		void mouseReleased(int x, int y, int button);
		void windowResized(int w, int h);

        bool                fullscreen;
        int                 imgWidth, imgHeight, snapWidth, snapHeight, drawWidth, drawHeight;
        

        //kinect setup
		ofxKinect 			kinect;

		ofxCvColorImage		colorImg;
		ofxCvGrayscaleImage grayImage;			// grayscale depth image
		ofxCvGrayscaleImage grayThreshNear;		// the near thresholded image
		ofxCvGrayscaleImage grayThreshFar;		// the far thresholded image
    
        ofTexture           maskedImg;

		int 				nearThreshold;
		int					farThreshold;
		int					angle;
    
        bool                bThreshWithOpenCV;
    
        int                 rgbOffsetX, rgbOffsetY;
    
        //images for comping
        vector <ofImage>    location;
        vector <ofImage>    dock;
        vector <ofImage>    sfx;
        int                 locationPicker, dockPicker, sfxPicker;
        int                 dockX;
        int                 dockY;
        float               dockScaler;
        bool                behind;
    
        //snapshots
        void                takePic();
        int                 picDelay, picTimer, picStart;
        bool                startTimer;
        ofImage             snapShot;
        int                 snapCounter;
        vector <ofImage>    lastSnaps;
        bool                freezeFrame;
        bool                oneScreen;
    
        int                 numBackgrounds, numPartners, numSfx;
    
        
              
        
        //sound player
        ofSoundPlayer       soundPlayer;
        
        //arduino
        void                setupArduino(const int & version);
        void                digitalPinChanged(const int & pinNum);
        void                updateArduino();
        
        ofArduino           ard;
        bool                bSetupArduino;
        
        int yellowButton, redButton, blueButton, whiteButton, leftJoy, rightJoy, upJoy, downJoy, lastYellow, lastBlue, lastRed, lastWhite;

    
        int                 timeStamp;
        string              uploadPath, pathToMeteor;

		

};
