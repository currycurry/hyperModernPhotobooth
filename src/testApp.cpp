#include "testApp.h"


//--------------------------------------------------------------
void testApp::setup() {
    
    ofBackground(0,0,0);
	ofSetFrameRate(60);
    
    oneScreen = false;
    
    imgWidth = 640; //size to capture from webcam
    imgHeight = 480;
    drawWidth = ofGetWindowWidth() / 2; //size to draw live on screen
    drawHeight = ofGetWindowHeight();
    snapWidth = drawWidth / 2; //size to draw previous snapshots on second screen
    snapHeight = drawHeight / 2;

	kinect.init();
    //kinect.setRegistration( true );// auto calibrate rgb to depth from of008
	//kinect.init(true);  // shows infrared instead of RGB video image
	//kinect.init(false, false);  // disable infrared/rgb video iamge (faster fps)
	kinect.setVerbose(true);
	kinect.open();

	// start with the live kinect source
	//EffectSource = &kinect;

	colorImg.allocate(kinect.width, kinect.height);
	grayImage.allocate(kinect.width, kinect.height);
	grayThreshNear.allocate(kinect.width, kinect.height);
	grayThreshFar.allocate(kinect.width, kinect.height);
    
    maskedImg.allocate( kinect.width, kinect.height, GL_RGBA );

	nearThreshold = 255;
	farThreshold  = 170;
	bThreshWithOpenCV = true;


	// zero the tilt on startup
	angle = 10;
	kinect.setCameraTiltAngle(angle);
    
    rgbOffsetX = 15;
    rgbOffsetY = 21;
    
    
    numBackgrounds = 33;
    numPartners = 27;
    numSfx = 22;
    
    location.resize( numBackgrounds ); //load background images
    for ( int i = 0; i < numBackgrounds; i ++ ) {
        location[ i ].loadImage("locations/" + ofToString( i ) + ".jpg");
    }
    
    dock.resize( numPartners ); //load scene partners
    for ( int i = 0; i < numPartners; i ++ ) {
        dock[ i ].loadImage("scenePartners/" + ofToString( i ) + ".png");
    }
    
    sfx.resize( numSfx ); //load special effects
    for ( int i = 0; i < numSfx; i ++ ) {
        sfx[ i ].loadImage("sfx/" + ofToString( i ) + ".png");
    }

    locationPicker = 0;
    dockPicker = 0;
    sfxPicker = 0;
    
    dockX = 0;
    dockY = 0;
    dockScaler = 1;
    behind = false;
    
    snapCounter = 0;
    snapShot.allocate( drawWidth, drawHeight, OF_IMAGE_COLOR );
    
    
    lastSnaps.resize( 4 );
    
    for ( int i = 0; i < lastSnaps.size(); i ++ ) {
        lastSnaps[ i ].allocate( imgWidth, imgHeight, OF_IMAGE_COLOR );
        lastSnaps[ i ].setColor( 0, 0, 0 );
        
    }
    
    startTimer = false;
    picTimer = 0;
    picDelay = 100;
    
    //load sound effects
    soundPlayer.loadSound("sounds/instantPhoto.aiff");
    
    //arduino setup
    ard.connect("/dev/tty.usbserial-FTGCSXHP", 57600 ); //change to arduino address
    ofAddListener( ard.EInitialized, this, &testApp::setupArduino );
    
    yellowButton, redButton, blueButton, whiteButton, leftJoy, rightJoy, upJoy, downJoy, lastYellow, lastRed, lastWhite = 0;
    
    bSetupArduino = false;
    
    timeStamp = ofGetUnixTime();
    //uploadPath = " s3://hyper-modern-pics"; //s3 bucket
    //uploadPath = " ec2-user@ec2-54-226-77-105.compute-1.amazonaws.com:./HeyMrDj/public/photobooth-images "; //ec2 approach
    //pathToMeteor = " /Users/curry/Documents/openframeworks_releases/of_007/apps/myApps/hyperModernPhotobooth/bin/data/meteor-dj.pem ";

}

//--------------------------------------------------------------
void testApp::update() {
    
    updateArduino();
    
    ofSetFullscreen( fullscreen );
    if ( fullscreen ) {
        ofHideCursor();
    }

    drawWidth = ofGetWindowWidth() / 2;
    drawHeight = ofGetWindowHeight();
    
    if ( freezeFrame ){
        picTimer = ofGetElapsedTimeMillis() - picStart;
        if ( picTimer > 1000 ) {
            freezeFrame = false;
        }
    }
    
    else if ( !freezeFrame ) {
        kinect.update();
    
        // there is a new frame and we are connected
        if(kinect.isFrameNew()) {

            // load grayscale depth image from the kinect source
            grayImage.setFromPixels(kinect.getDepthPixels(), kinect.width, kinect.height);
            colorImg.setFromPixels(kinect.getPixels(), kinect.width, kinect.height);
            //colorImg.setFromPixels(kinect.getCalibratedRGBPixels(), kinect.width, kinect.height);

            // we do two thresholds - one for the far plane and one for the near plane
            // we then do a cvAnd to get the pixels which are a union of the two thresholds
            if(bThreshWithOpenCV) {
                grayThreshNear = grayImage;
                grayThreshFar = grayImage;
                grayThreshNear.threshold(nearThreshold, true);
                grayThreshFar.threshold(farThreshold);
                cvAnd(grayThreshNear.getCvImage(), grayThreshFar.getCvImage(), grayImage.getCvImage(), NULL);
            } else {

                // or we do it ourselves - show people how they can work with the pixels
                unsigned char * pix = grayImage.getPixels();

                int numPixels = grayImage.getWidth() * grayImage.getHeight();
                for(int i = 0; i < numPixels; i++) {
                    if(pix[i] < nearThreshold && pix[i] > farThreshold) {
                        pix[i] = 255;
                    } else {
                        pix[i] = 0;
                    }
                }
            }
            //grayImage.dilate();

            // update the cv images
            grayImage.flagImageChanged();
            
            
            unsigned char * pixels = new unsigned char[ kinect.width * kinect.height * 4 ];
            unsigned char * colorPixels = colorImg.getPixels();
            unsigned char * alphaPixels = grayImage.getPixels();
            
            for ( int i = 0; i < kinect.width; i++ ){
                for ( int j = 0; j < kinect.height; j++ ){
                    int pos = ( j * kinect.width + i );
                    pixels[ pos * 4   ] = colorPixels[ pos * 3 ];
                    pixels[ pos * 4 + 1 ] = colorPixels[ pos * 3 + 1 ];
                    pixels[ pos * 4 + 2 ] = colorPixels[ pos * 3 + 2 ];
                    if ( pos < rgbOffsetX + kinect.width * rgbOffsetY ) {
                        pixels[ pos * 4 + 3 ] = 0;
                    }
                    else {
                        pixels[ pos * 4 + 3 ] = alphaPixels[ pos - rgbOffsetX - kinect.width * rgbOffsetY ];
                    }
                }
            }

            maskedImg.loadData(pixels, kinect.width, kinect.height, GL_RGBA);
            delete [] pixels;
            
        }
    }
    
	if ( startTimer ) {
        picTimer = ofGetElapsedTimeMillis() - picStart;
        
        if ( picTimer >= picDelay && picTimer != 0 ) {
            takePic();
            startTimer = false;
        }
    }
    

}

//--------------------------------------------------------------
void testApp::draw() {

	//ofSetColor(255);
    location[ locationPicker ].draw( 0, 0, drawWidth, drawHeight ); //draw location
    ofEnableAlphaBlending();
    sfx[ sfxPicker ].draw( 0, 0, drawWidth, drawHeight ); //draw sfx
    ofDisableAlphaBlending();
    
    ofEnableAlphaBlending();
    maskedImg.draw( - 2* rgbOffsetX, -2 * rgbOffsetY, 1024 + 2 * rgbOffsetX,768 + 2 * rgbOffsetY );
    ofDisableAlphaBlending();
    ofEnableAlphaBlending();
    dock[ dockPicker ].draw( dockX, dockY, drawWidth * dockScaler, drawHeight * dockScaler ); //draw dock behind
    ofDisableAlphaBlending();
    
    
    if ( oneScreen == false ){
    
        //display last pics taken on other screen
        lastSnaps[ 3 ].draw( drawWidth, 0, snapWidth, snapHeight );
        lastSnaps[ 2 ].draw( drawWidth + snapWidth, 0, snapWidth, snapHeight );
        lastSnaps[ 1 ].draw( drawWidth, snapHeight, snapWidth, snapHeight );
        lastSnaps[ 0 ].draw( drawWidth + snapWidth, snapHeight, snapWidth, snapHeight );
    }

}



//--------------------------------------------------------------
void testApp::exit() {
	kinect.setCameraTiltAngle(0); // zero the tilt on exit
	kinect.close();
	
}

//--------------------------------------------------------------
void testApp::takePic() {
    
    freezeFrame = true;
    snapShot.grabScreen( 0, 0, drawWidth, drawHeight );
    snapShot.saveImage( "photos/dockSnap-" + ofToString(snapCounter) + ".jpg" );
    
    for ( int i = 0; i < lastSnaps.size() - 1; i ++ ) {
        lastSnaps[ i ] = lastSnaps[ i + 1 ];
    }
    
    lastSnaps[ lastSnaps.size() - 1 ].loadImage( "photos/dockSnap-" + ofToString( snapCounter ) + ".jpg");
    
    ofFile file( ofToDataPath( "photos/dockSnap-" + ofToString( snapCounter - lastSnaps.size()) + ".jpg"));
    timeStamp = ofGetUnixTime();
    if ( snapCounter >= lastSnaps.size()) {
        cout << snapCounter << endl;
        file.renameTo( "photos/upload/dockSnap-" + ofToString( timeStamp ) + ".jpg" );
        string pathToFile = file.getAbsolutePath();
        //string fileCMD = "/usr/local/bin/s3cmd put " + pathToFile + uploadPath;
        //string fileCMD = "/usr/bin/scp -r -i" + pathToMeteor + pathToFile + uploadPath;
        //string fileCMD = "/usr/local/bin/sshpass -p 'K1ngk@p!' scp -r " + pathToFile +  " root@50.56.126.196:/var/www/vhosts/goodbyemark.wknyc.com/photobooth/images";
        //const char * fileCMDChar = fileCMD.c_str();
        //system( fileCMDChar );
        //cout << fileCMD << endl;
    }

    snapCounter ++;
    picStart = ofGetElapsedTimeMillis();
    picTimer = 0;

    
    
}

//--------------------------------------------------------------
void testApp::setupArduino(const int & version) {
	
	// remove listener because we don't need it anymore
	ofRemoveListener(ard.EInitialized, this, &testApp::setupArduino);
    
    bSetupArduino = true;
    
    ard.sendDigitalPinMode( 2, ARD_INPUT );
    ard.sendDigitalPinMode( 3, ARD_INPUT );
    ard.sendDigitalPinMode( 5, ARD_INPUT );
    ard.sendDigitalPinMode( 7, ARD_INPUT );
    ard.sendDigitalPinMode( 8, ARD_INPUT );
    ard.sendDigitalPinMode( 10, ARD_INPUT );
    ard.sendDigitalPinMode( 11, ARD_INPUT );
    ard.sendDigitalPinMode( 12, ARD_INPUT );
    
    // Listen for changes on the digital and analog pins
    ofAddListener(ard.EDigitalPinChanged, this, &testApp::digitalPinChanged);
    
}

//--------------------------------------------------------------
void testApp::updateArduino(){
    
	// update the arduino, get any data or messages.
	ard.update();
    
    if (bSetupArduino) {
        //listen for messages
    }
    
}

//--------------------------------------------------------------
void testApp::digitalPinChanged(const int & pinNum) {
    // do something with the digital input. here we're simply going to print the pin number and
    // value to the screen each time it changes
    //  buttonState = "digital pin: " + ofToString(pinNum) + " = " + ofToString(ard.getDigital(pinNum));
    yellowButton = ard.getDigital( 7 );
    blueButton = ard.getDigital( 5 );
    redButton = ard.getDigital( 3 );
    whiteButton = ard.getDigital( 2 );
    
    leftJoy = ard.getDigital( 8 );
    rightJoy = ard.getDigital( 10 );
    upJoy = ard.getDigital( 11 );
    downJoy = ard.getDigital( 12 );
    
    if ( lastYellow != yellowButton  && yellowButton == 1 ) {
        dockPicker ++;
        if ( dockPicker > dock.size() - 1 ) {
            dockPicker = 0;
        }
        dockX = 0;
        dockY = 0;
    }
    
    if ( lastBlue != blueButton && blueButton == 1 ) {
        locationPicker ++;
        if ( locationPicker > location.size() - 1 ) {
            locationPicker = 0;
        }
    }
    
    if ( lastRed != redButton && redButton == 1 ) {
        sfxPicker ++;
        if ( sfxPicker > sfx.size() - 1 ) {
            sfxPicker = 0;
        }
    }
    
    if ( leftJoy == 1 ) {
        dockX -= 10;
    }
    
    if ( rightJoy == 1 ) {
        dockX += 10;
    }
    
    if ( upJoy == 1 ) {
        dockY += 10;
    }
    
    if ( downJoy == 1 ) {
        dockY -= 10;
    }
    
    if ( lastWhite != whiteButton && whiteButton == 1 ) {
        soundPlayer.play();
        picStart = ofGetElapsedTimeMillis();
        startTimer = true;
    }
    
    lastYellow = yellowButton;
    lastBlue = blueButton;
    lastRed = redButton;
    lastWhite = whiteButton;
    
}


//--------------------------------------------------------------
void testApp::keyPressed (int key) {
	switch (key) {

		case '>':
		case '.':
			farThreshold ++;
			if (farThreshold > 255) farThreshold = 255;
			break;

		case '<':
		case ',':
			farThreshold --;
			if (farThreshold < 0) farThreshold = 0;
			break;

		case '+':
		case '=':
			nearThreshold ++;
			if (nearThreshold > 255) nearThreshold = 255;
			break;

		case '-':
			nearThreshold --;
			if (nearThreshold < 0) nearThreshold = 0;
			break;

		case 'w':
			kinect.enableDepthNearValueWhite(!kinect.isDepthNearValueWhite());
			break;

		case 'o':
			kinect.setCameraTiltAngle(angle);	// go back to prev tilt
			kinect.open();
			break;

		case 'c':
			kinect.setCameraTiltAngle(0);		// zero the tilt
			kinect.close();
			break;

		/*case OF_KEY_UP:
			angle++;
			if(angle>30) angle=30;
			kinect.setCameraTiltAngle(angle);
			break;

		case OF_KEY_DOWN:
			angle--;
			if(angle<-30) angle=-30;
			kinect.setCameraTiltAngle(angle);
			break;*/
    
        case 'l':
            rgbOffsetX ++;
            cout << "rgbOffsetX: " << rgbOffsetX << ", rgbOffsetY: " << rgbOffsetY;
            break;

        case 'j':
            rgbOffsetX --;
            cout << "rgbOffsetX: " << rgbOffsetX << ", rgbOffsetY: " << rgbOffsetY;
            break;
    
        case 'k':
            rgbOffsetY ++;
            cout << "rgbOffsetX: " << rgbOffsetX << ", rgbOffsetY: " << rgbOffsetY;
            break;
            
        case 'i':
            rgbOffsetY --;
            cout << "rgbOffsetX: " << rgbOffsetX << ", rgbOffsetY: " << rgbOffsetY;
            break;
            
        case OF_KEY_UP:
            dockY -= 10;
            break;
            
        case OF_KEY_DOWN:
            dockY += 10;
            break;
            
        case OF_KEY_LEFT:
            dockX -= 10;
            break;
            
        case OF_KEY_RIGHT:
            dockX += 10;
            break;
            
        case 'h':
            dockScaler += .1;
            break;
            
        case 'g':
            dockScaler -= .1;
            break;
            
        case '1':
            locationPicker ++;
            if ( locationPicker > location.size() - 1 ) {
                locationPicker = 0;
            }
            break;
            
        case '2':
            dockPicker ++;
            if ( dockPicker > dock.size() - 1 ) {
                dockPicker = 0;
            }
            dockX = 0;
            dockY = 0;
            break;
            
        case '3':
            sfxPicker ++;
            if ( sfxPicker > sfx.size() - 1 ) {
                sfxPicker = 0;
            }
            break;
            
            
        case 'f':
        case 'F':
            fullscreen = !fullscreen;
            break;
            
        case 'b':
            behind = !behind;
            break;
            
        case ' ':
            soundPlayer.play();
            picStart = ofGetElapsedTimeMillis();
            startTimer = true;
            break;


        }
    
}

//--------------------------------------------------------------
void testApp::mouseMoved(int x, int y) {
	
}

//--------------------------------------------------------------
void testApp::mouseDragged(int x, int y, int button)
{}

//--------------------------------------------------------------
void testApp::mousePressed(int x, int y, int button)
{}

//--------------------------------------------------------------
void testApp::mouseReleased(int x, int y, int button)
{}

//--------------------------------------------------------------
void testApp::windowResized(int w, int h)
{}


