#test
from openframeworks import *

class myApp(object):
	color = (255,255,255)
	def __init__(self):
		self.x = 100.0
		self.y = 100.0
		self.d = 10
	
	def update(self):
		self.x = ofRandom(100)
		self.y = ofRandom(100)

	def draw(self):
		ofBackground(0,0,0)
		color = ofColor(*self.color)
		ofSetColor(color)
		ofDrawCircle(self.x,self.y,self.d)
