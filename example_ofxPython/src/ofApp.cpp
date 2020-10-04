#include "ofApp.h"

//--------------------------------------------------------------
void ofApp::setup(){
    // init python
    python.init();

    // load testing scripts
    ofFile scriptfile("testing.py");

    python.reset();
    python.addPath(scriptfile.getEnclosingDirectory());
    python.executeScript(scriptfile.getAbsolutePath());

    // inject script path inside the script for references ( loading files )
    string temppath = scriptfile.getEnclosingDirectory().substr(0,scriptfile.getEnclosingDirectory().size()-1);

#ifdef TARGET_WIN32
    std::replace(temppath.begin(),temppath.end(),'\\','/');
#endif

    string tempstring = "SCRIPT_PATH = '"+temppath+"'\n";
    python.executeString(tempstring);

    ///////////////////////////////////////////
    // PYTHON SETUP
    klass = python.getObject("ofxPythonApp");
    if(klass){
        ofLog(OF_LOG_NOTICE,"[verbose] python script: %s loaded & running!",scriptfile.getAbsolutePath().c_str());
        // run python script setup:
        script = klass();
    }else{
        script = ofxPythonObject::_None();
    }
    ///////////////////////////////////////////

}

//--------------------------------------------------------------
void ofApp::update(){

    ///////////////////////////////////////////
    // PYTHON UPDATE
    if(script){
        updatePython = script.attr("update");
        if(updatePython && !script.isPythonError() && !updatePython.isPythonError()){
            // run python script update:
            updatePython();
        }
    }
    ///////////////////////////////////////////

}

//--------------------------------------------------------------
void ofApp::draw(){

}

//--------------------------------------------------------------
void ofApp::keyPressed(int key){

}

//--------------------------------------------------------------
void ofApp::keyReleased(int key){

}

//--------------------------------------------------------------
void ofApp::mouseMoved(int x, int y ){

}

//--------------------------------------------------------------
void ofApp::mouseDragged(int x, int y, int button){

}

//--------------------------------------------------------------
void ofApp::mousePressed(int x, int y, int button){

}

//--------------------------------------------------------------
void ofApp::mouseReleased(int x, int y, int button){

}

//--------------------------------------------------------------
void ofApp::mouseEntered(int x, int y){

}

//--------------------------------------------------------------
void ofApp::mouseExited(int x, int y){

}

//--------------------------------------------------------------
void ofApp::windowResized(int w, int h){

}

//--------------------------------------------------------------
void ofApp::gotMessage(ofMessage msg){

}

//--------------------------------------------------------------
void ofApp::dragEvent(ofDragInfo dragInfo){

}
