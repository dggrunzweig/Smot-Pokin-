#include "RtAudio.h"
#include <math.h>
#include <stdlib.h>
#include <iostream>
#include "chuck_fft.h"
#include "Thread.h"
#include "Stk.h"

using namespace std;

#ifdef __MACOSX_CORE__
#include <GLUT/glut.h>
#include <OpenGL/gl.h>
#include <OpenGL/glu.h>
#else
#include <GL/gl.h>
#include <GL/glu.h>
#include <GL/glut.h>
#endif




//-----------------------------------------------------------------------------
// function prototypes
//-----------------------------------------------------------------------------
void initGfx();
void idleFunc();
void displayFunc();
void reshapeFunc( GLsizei width, GLsizei height );
void keyboardFunc( unsigned char, int, int );
void mouseFunc( int button, int state, int x, int y );

// our datetype
#define SAMPLE double
// corresponding format for RtAudio
#define MY_FORMAT RTAUDIO_FLOAT64
// sample rate
#define MY_SRATE 44100
// number of channels
#define MY_CHANNELS 1
// for convenience
#define MY_PIE 3.14159265358979
#define ZPF 1

// width and height
long g_width = 1024;
long g_height = 720;

// global buffer
float * g_buffer = NULL;
long g_bufferSize;
Mutex g_mutex;

float * g_fftBuff;
float * g_window;
bool g_useWindow = false;
bool g_displayBars = false;
unsigned int g_buffSize = 512;
int g_colorChoice = 0;
bool g_waterFall = true;
int g_windowChoice = 0; //0 = hamming, 1 = blackman, 2 = hanning



//-----------------------------------------------------------------------------
// name: callme()
// desc: audio callback
//-----------------------------------------------------------------------------
int callme( void * outputBuffer, void * inputBuffer, unsigned int numFrames,
            double streamTime, RtAudioStreamStatus status, void * data )
{
    // cast!
    SAMPLE * input = (SAMPLE *)inputBuffer;
    SAMPLE * output = (SAMPLE *)outputBuffer;

	g_mutex.lock();
        
    // zero out the fft buffer (required only if ZPF > 1)
    memset( g_fftBuff, 0, ZPF * numFrames * sizeof(float) );
    
    for(size_t i = 0; i < numFrames; ++i)
    {
        	// assume mono
        g_buffer[i] = input[i];
        // zero output
        output[i] = 0;
        g_fftBuff[i] = input[i];
    }
    
    // apply window to the buffer of audio
    
    apply_window( g_fftBuff, g_window, g_buffSize );        

    // compute the fft
    rfft( g_fftBuff, ZPF * numFrames / 2, FFT_FORWARD );
    
    g_mutex.unlock();
    
    return 0;
}




//-----------------------------------------------------------------------------
// name: main()
// desc: entry point
//-----------------------------------------------------------------------------
int main( int argc, char ** argv )
{
    // instantiate RtAudio object
    RtAudio audio;
    // variables
    unsigned int bufferBytes = 0;
    // frame size
    unsigned int bufferFrames = 512;
    
    // check for audio devices
    if( audio.getDeviceCount() < 1 )
    {
        // nopes
        cout << "no audio devices found!" << endl;
        exit( 1 );
    }

    // initialize GLUT
    glutInit( &argc, argv );
    // init gfx
    initGfx();

    // let RtAudio print messages to stderr.
    audio.showWarnings( true );

    // set input and output parameters
    RtAudio::StreamParameters iParams, oParams;
    iParams.deviceId = audio.getDefaultInputDevice();
    iParams.nChannels = MY_CHANNELS;
    iParams.firstChannel = 0;
    oParams.deviceId = audio.getDefaultOutputDevice();
    oParams.nChannels = MY_CHANNELS;
    oParams.firstChannel = 0;
    
    // create stream options
    RtAudio::StreamOptions options;
    cout<<"1-6: color sets"<<endl;
    cout<<"w : switch between waterfall plot and visualizer"<<endl;
    cout<<"f : enter full screen"<<endl;
    cout<<"x : exit full screen"<<endl;
    cout<<"q : quit"<<endl;
	char input;
    cout<<"Enter Window Type (0 = hamming, 1 = blackman, 2 = hanning): ";
    cin.get(input);
    g_windowChoice = (int)input;

    // go for it
    try {
        // open a stream
        audio.openStream( &oParams, &iParams, MY_FORMAT, MY_SRATE, &bufferFrames, &callme, (void *)&bufferBytes, &options );
		        // allocate the buffer for the fft
        g_fftBuff = new float[g_buffSize * ZPF];
        if ( g_fftBuff == NULL ) {
            cerr << "Something went wrong when creating the fft buffers" << endl;
            exit (1);
        }
        
        // allocate the buffer for the time domain window
        g_window = new float[g_buffSize];
        if ( g_window == NULL ) {
            cerr << "Something went wrong when creating the window" << endl;
            exit (1);
        }


        // create a hanning window
        if (g_windowChoice == 0) hamming( g_window, g_buffSize );
        else if (g_windowChoice == 1) blackman(g_window, g_buffSize);
        else hanning( g_window, g_buffSize);
    }    
    catch( RtError& e )
    {
        // error!
        cout << e.getMessage() << endl;
        exit( 1 );
    }

    // compute
    bufferBytes = bufferFrames * MY_CHANNELS * sizeof(SAMPLE);
    // allocate global buffer
    g_bufferSize = bufferFrames;
    g_buffer = new float[g_bufferSize];
    //memset( g_buffer, 0, sizeof(SAMPLE)*g_bufferSize );

    // go for it
    try {
        // start stream
        audio.startStream();

        // let GLUT handle the current thread from here
        glutMainLoop();
        
        // stop the stream.
        audio.stopStream();
    }
    catch( RtError& e )
    {
        // print error message
        cout << e.getMessage() << endl;
        goto cleanup;
    }
    
cleanup:
    // close if open
    if( audio.isStreamOpen() )
        audio.closeStream();
    
    // done
    return 0;
}




//-----------------------------------------------------------------------------
// Name: reshapeFunc( )
// Desc: called when window size changes
//-----------------------------------------------------------------------------
void initGfx()
{
    // double buffer, use rgb color, enable depth buffer
    glutInitDisplayMode( GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH );
    // initialize the window size
    glutInitWindowSize( g_width, g_height );
    // set the window postion
    glutInitWindowPosition( 100, 100 );
    // create the window
    glutCreateWindow( "SmotPokin" );
    
    // set the idle function - called when idle
    glutIdleFunc( idleFunc );
    // set the display function - called when redrawing
    glutDisplayFunc( displayFunc );
    // set the reshape function - called when client area changes
    glutReshapeFunc( reshapeFunc );
    // set the keyboard function - called on keyboard events
    glutKeyboardFunc( keyboardFunc );
    // set the mouse function - called on mouse stuff
    glutMouseFunc( mouseFunc );
    
    // set clear color
    glClearColor( 0, 0, 0, 1 );
    // enable color material
    glEnable( GL_COLOR_MATERIAL );
    // enable depth test
    glEnable( GL_DEPTH_TEST );
}




//-----------------------------------------------------------------------------
// Name: reshapeFunc( )
// Desc: called when window size changes
//-----------------------------------------------------------------------------
void reshapeFunc( GLsizei w, GLsizei h )
{
    // save the new window size
    g_width = w; g_height = h;
    // map the view port to the client area
    glViewport( 0, 0, w, h );
    // set the matrix mode to project
    glMatrixMode( GL_PROJECTION );
    // load the identity matrix
    glLoadIdentity( );
    // create the viewing frustum
    gluPerspective( 45.0, (GLfloat) w / (GLfloat) h, 1.0, 300.0 );
    // set the matrix mode to modelview
    glMatrixMode( GL_MODELVIEW );
    // load the identity matrix
    glLoadIdentity( );
    // position the view point
    gluLookAt( 0.0f, 0.0f, 10.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f );
}




//-----------------------------------------------------------------------------
// Name: keyboardFunc( )
// Desc: key event
//-----------------------------------------------------------------------------
void keyboardFunc( unsigned char key, int x, int y )
{
    switch( key )
    {
        case 'Q':
        case 'q':
            exit(1);
            break;
       	case '1': 
       		g_colorChoice = 0;
        		break;
        case '2':
        		g_colorChoice = 1;
        		break;
        	case '3':
        		g_colorChoice = 2;
        		break;
        case '4':
        		g_colorChoice = 3;
        		break;
        	case '5':
        		g_colorChoice = 4;
        		break;
        case '6':
        		g_colorChoice = 5;
        		break;
        case 'w': 
        		g_waterFall = !g_waterFall;
        		break;
        	case 'f':
        		g_width = glutGet(GLUT_WINDOW_WIDTH);
        		g_height = glutGet(GLUT_WINDOW_HEIGHT);
        		glutFullScreen();
        		break;
        	case 'x':
        		glutReshapeWindow( g_width, g_height );
        		break;
    }
    
    glutPostRedisplay( );
}




//-----------------------------------------------------------------------------
// Name: mouseFunc( )
// Desc: handles mouse stuff
//-----------------------------------------------------------------------------
void mouseFunc( int button, int state, int x, int y )
{
    if( button == GLUT_LEFT_BUTTON )
    {
        // when left mouse button is down
        if( state == GLUT_DOWN )
        {
        }
        else
        {
        }
    }
    else if ( button == GLUT_RIGHT_BUTTON )
    {
        // when right mouse button down
        if( state == GLUT_DOWN )
        {
        }
        else
        {
        }
    }
    else
    {
    }
    
    glutPostRedisplay( );
}




//-----------------------------------------------------------------------------
// Name: idleFunc( )
// Desc: callback from GLUT
//-----------------------------------------------------------------------------
void idleFunc( )
{
    // render the scene
    glutPostRedisplay( );
}



//-----------------------------------------------------------------------------
// Name: displayFunc( )
// Desc: callback function invoked to draw the client area
//-----------------------------------------------------------------------------
void displayFunc( )
{
	static float rot = 0;
	static float amax = 0;
	static float threshold = 0;
	static int shape = 0;
	static int count = 0;
	static int direction = 1;
	

	// cast to use the complex data type defined in chuck_fft
    complex * fft = (complex *)g_fftBuff;


    // clear the color and depth buffers
    glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );
    
    glEnable( GL_LIGHT0 );
    glEnable( GL_LIGHTING );
    
    // line width
    if (g_waterFall == false){
    		g_mutex.lock();
		glPolygonMode( GL_FRONT_AND_BACK, GL_LINE );
		glLineWidth( .4  );
		float sum = 0;
		float trebles = 0, trebleCount = 0;
		float mids = 0, midCount = 0;
		float bass = 0, bassCount = 0;

	
		for (int i = 0; i < g_bufferSize; i++){    
			sum += fabs(g_buffer[i]);
		}
		float avg = (sum / g_bufferSize);
		for (int i = 0; i < g_buffSize/2; i++){   
			float frequency =  i* 20000/(2*g_buffSize);
			
				if (frequency <= 250) {
					bass+= 2 * ZPF * cmp_abs( fft[i] );
					bassCount++;
				} else if (frequency > 250 && frequency < 2500) {
					mids += 2*ZPF*cmp_abs(fft[i]);
					midCount++;
				}
				else {
					trebles += 2 * ZPF * cmp_abs( fft[i] );
					trebleCount++;
				}
			
		}
		bass /= bassCount;
		if (bass == NAN) bass = 0;
		mids /= midCount;
		trebles /= trebleCount;
		//cout<<"Bass: "<<500*bass<<", Mids: "<<10000*mids<<", Trebles: "<<100000*trebles<<endl;
		

		if (avg > amax) {
				amax = avg;
			threshold = .65*amax;
		}
		
		glPushMatrix();
	
		for (GLfloat z = 0; z < 25; z+=1){
			GLfloat trebColor = 50000*trebles;
			GLfloat midColor = 5000*mids;
			GLfloat bassColor = .25+500*bass;
			if (g_colorChoice == 0) glColor3f( trebColor, midColor, bassColor );
			else if (g_colorChoice == 1) glColor3f( midColor, trebColor, bassColor );
			else if (g_colorChoice == 2) glColor3f( bassColor, midColor, trebColor );
			else if (g_colorChoice == 3) glColor3f( trebColor, bassColor, midColor);
			else if (g_colorChoice == 4) glColor3f( midColor, bassColor, trebColor);
			else if (g_colorChoice == 5) glColor3f( bassColor, trebColor, midColor);
		  	glRotatef(rot, 0, 0, 1);
		  	glTranslatef( 0, 0, -z );
			rot+=direction*avg/(100*amax);
			if (rot == 360) {rot = 0;}	
	 		if (shape == 2) {
	 			glRectf(-7, -7, 7, 7);
	 		}
	 		else if (shape == 1){
	 			glBegin(GL_TRIANGLES);
		    		glVertex3f(0, 0, 0);
		    		glVertex3f(6, 0, 0);
		    		glVertex3f(0, 6, 0);
		    		glEnd();
		    	}
	 		else if (shape == 0){
				glBegin(GL_LINES);
	 			glVertex2f(0, 0); glVertex2f(0, 1000*avg*z);
	 			glEnd();
	 		}
		
		
		}
		glPopMatrix();
		if (avg>threshold) {
			count++;
			shape++;
			shape%=3;
			if (count == 10) direction = -direction;
			count %= 10;
    		}
    		g_mutex.unlock();
	} else {

		GLfloat x=0;
		// find the x increment
		GLfloat xinc = (float)20/g_buffSize;
		g_mutex.lock();
		glPushMatrix();
		for (int i = 0; i < g_buffSize / 2; i ++){
			
				float r = (float)i/(g_buffSize/2);
		 		float g = .5;
		 		float b = (float)(g_buffSize/2 - i)/(g_buffSize/2);
		 		glColor3f( r,g,b);
		 		float y = 50 * ZPF * cmp_abs(fft[i]);
		 		glRectf(-5+x, -2, -4.98+x, -1.9+y); 
		 		glRectf(-5+x, 2, -4.98+x, 1.9-y); 
		 		x+=xinc;
		 	
	 	}
	 	glPopMatrix();
	 	g_mutex.unlock();
		
	}
    //pop
    
    // flush!
    glFlush( );
    // swap the double buffer
    glutSwapBuffers( );
  
}

