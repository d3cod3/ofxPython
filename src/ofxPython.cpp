#include "ofMain.h"

#include "ofxPython.h"

unsigned int ofxPython::instances = 0;

ofxPythonObject make_object_owned(PyObject * obj, bool errcheck);


bool PythonErrorCheck(){
    bool isError = false;
	ofxPythonOperation op;
	PyObject * ptype, * pvalue, * ptraceback;
	PyErr_Fetch(&ptype, &pvalue, &ptraceback);
	if(ptype)
	{
		PyErr_NormalizeException(&ptype, &pvalue, &ptraceback);
		ofxPythonObject optype = make_object_owned(ptype, false);
		ofxPythonObject opvalue = make_object_owned(pvalue, false);
		ofxPythonObject optraceback = make_object_owned(ptraceback, false);
        ofLog(OF_LOG_ERROR,"Python Error: ");
		if(optraceback){
			vector<ofxPythonObject> tb = ofxPython::getObject("format_tb","traceback")(optraceback).asVector();
			for(unsigned int i = 0; i < tb.size(); ++i){
				vector<ofxPythonObject> lines = tb[i].attr("splitlines")().asVector();
				for (unsigned int j=0; j < lines.size(); ++j){
					ofLog() << lines[j].asString();
				}
			}
		}
        ofLog(OF_LOG_ERROR,opvalue.repr());
        isError = true;
	}
    return isError;
}

ofxPythonObject make_object_owned(PyObject * obj, bool errcheck= true)
{
	ofxPythonOperation op;
    ofxPythonObject o;
	if (obj==nullptr)
        ofLog(OF_LOG_WARNING,"Make_object_owned: creating ofxPythonObject with nullptr");
	if(errcheck)
        o.pythonError = PythonErrorCheck();
	o.insert_owned(obj);
	return o;
}

ofxPythonObject make_object_borrowed(PyObject * obj, bool errcheck= true)
{
	ofxPythonOperation op;
    ofxPythonObject o;
	if (obj==nullptr)
        ofLog(OF_LOG_WARNING,"Make_object_borrowed: creating ofxPythonObject with nullptr");
    if(errcheck)
        o.pythonError = PythonErrorCheck();
	o.insert_borrowed(obj);
	return o;
}

//helper to transform a (const) std:string into a char *
class noconststring: vector<char>
{
public:
	noconststring(const string& source):vector<char>(source.c_str(), source.c_str() + source.size() + 1u)
	{}
	operator char *()
	{
		return &(*this).at(0);
	}
};


ofxPython::ofxPython():initialized(false)
{
}

ofxPython::~ofxPython()
{
	if(initialized)
	{
		instances--;
		if(instances == 0)
		{
            {
                ofxPythonOperation op;
                long main_thread_id = ofxPythonOperation::pstate->thread_id;
                for (ofxPythonObject t : ofxPython::getObject("enumerate", "threading")().asVector()){
                    long t_id = t.attr("ident").asInt();
                    if( t_id != main_thread_id){
                        ofxPythonObject exc = locals["__builtins__"]["SystemExit"];
                        int r = PyThreadState_SetAsyncExc(t_id,exc.data->obj);
                        if (r != 1){
                            ofLog() << "Failed attewmpt to shut down thread " << t_id;
                        }
                    }
                }
            }
            PyEval_RestoreThread(ofxPythonOperation::pstate);

			Py_Finalize();
		}
	}
}

int ofxPython::init(){
	if (!initialized){
		initialized = true;
		if(instances == 0){

			Py_Initialize();
            PyEval_InitThreads();

            ofxPythonOperation::pstate = PyEval_SaveThread();
            pythonError = PythonErrorCheck();
            reset();
            addPath(ofToDataPath(""));
		}
		instances++;
	}
	reset();
	return instances;
}

void ofxPython::reset(){
    ofxPythonOperation op;
	locals = make_object_owned(PyDict_New());
    //deal with cyclic references
    ofxPython::getObject("collect", "gc")();
	//insert builtins
	locals["__builtins__"]=make_object_borrowed(PyEval_GetBuiltins());
}

void ofxPython::executeScript(const string& path){
	ofxPythonOperation op;
    pythonError = PythonErrorCheck();
    if(!pythonError){
        executeString(ofBufferFromFile(path).getText());
    }
}

void ofxPython::executeString(const string& script){
	ofxPythonOperation op;
    pythonError = PythonErrorCheck();
    if(!pythonError){
        make_object_owned(PyRun_String(script.c_str(),Py_file_input,locals.data->obj,locals.data->obj));
    }
}

ofxPythonObject ofxPython::executeStatement(const string& script){
	ofxPythonOperation op;
    return make_object_owned(PyRun_String(script.c_str(),Py_single_input,locals.data->obj,locals.data->obj));
}

ofxPythonObject ofxPython::evalString(const string& expression){
	ofxPythonOperation op;
	return  make_object_owned(PyRun_String(expression.c_str(),Py_eval_input,locals.data->obj,locals.data->obj));
}

ofxPythonObject ofxPython::getObject(const string& name, const string& module){
	ofxPythonOperation op;

	ofxPythonObject pmodule = make_object_owned(
		PyImport_Import(ofxPythonObject::fromString(module).data->obj));
	if(pmodule)
		return pmodule.attr(name);
	return ofxPythonObject();
}

ofxPythonObject ofxPython::getObject(const string& name){
	ofxPythonOperation op;
	return locals[name];
}

ofxPythonObject ofxPython::getObjectOrNone(const string& name){
	ofxPythonOperation op;
	return locals.attr("get")(ofxPythonObject::fromString(name));
}

void ofxPython::setObject(const string& name, ofxPythonObject o){
	ofxPythonOperation op;
	locals[name]=o;
}

ofxPythonObject ofxPython::getLocals(){
    return locals;
}

void ofxPython::setVirtualEnv(const string & path){
    ofxPythonOperation op;
    ofxPythonObject venv_file = ofxPythonObject::fromString(path);
    locals["__venv_file"] = venv_file;
    executeString(
                  "import os.path\n"
                  "__venv_file = os.path.join(__venv_file,'bin/activate_this.py')\n"
                  "execfile(__venv_file, dict(__file__=__venv_file))\n"
                  );
}

void ofxPython::addPath(const string & path){
    ofxPythonOperation op;
    locals["__add_path"] = ofxPythonObject::fromString(path);
    executeString(
                  "import sys, os.path\n"
                  "if __add_path not in sys.path:\n"
                  "    sys.path.append(__add_path)\n"
                  );
}

void ofxPythonObject::insert_owned(PyObject * obj){
	data.reset(new ofxPythonObjectManaged(obj));
}

void ofxPythonObject::insert_borrowed(PyObject * obj){
	ofxPythonOperation op;
	Py_XINCREF(obj);
	data.reset(new ofxPythonObjectManaged(obj));
}

ofxPythonObject::ofxPythonObjectManaged::ofxPythonObjectManaged(PyObject * o):obj(o){

}
ofxPythonObject::ofxPythonObjectManaged::~ofxPythonObjectManaged(){
    if(ofxPython::instances > 0){
        ofxPythonOperation op;
        Py_XDECREF(obj);
    }
}

ofxPythonObject ofxPythonObject::method(const string &method_name)
{
	ofxPythonOperation op;
	return make_object_owned(
		PyObject_CallMethod(data->obj,noconststring(method_name),nullptr));
}

ofxPythonObject ofxPythonObject::operator ()()
{
	ofxPythonOperation op;
	return make_object_owned(PyObject_CallObject(data->obj,nullptr));
}

ofxPythonObject ofxPythonObject::operator ()(ofxPythonObject o1)
{
	ofxPythonOperation op;
	if(o1.data->obj == nullptr) o1=ofxPythonObject::_None();
	return make_object_owned(PyObject_CallFunctionObjArgs(data->obj,o1.data->obj,nullptr));
}

ofxPythonObject ofxPythonObject::operator ()(ofxPythonObject o1, ofxPythonObject o2)
{
	ofxPythonOperation op;
	if(o1.data->obj == nullptr) o1=ofxPythonObject::_None();
	if(o2.data->obj == nullptr) o2=ofxPythonObject::_None();
	return make_object_owned(PyObject_CallFunctionObjArgs(data->obj,o1.data->obj,o2.data->obj,nullptr));
}

ofxPythonObject ofxPythonObject::operator ()(ofxPythonObject o1, ofxPythonObject o2, ofxPythonObject o3)
{
	ofxPythonOperation op;
	if(o1.data->obj == nullptr) o1=ofxPythonObject::_None();
	if(o2.data->obj == nullptr) o2=ofxPythonObject::_None();
	if(o3.data->obj == nullptr) o3=ofxPythonObject::_None();
	return make_object_owned(PyObject_CallFunctionObjArgs(data->obj,o1.data->obj,o2.data->obj,o3.data->obj,nullptr));
}

ofxPythonObject ofxPythonObject::operator ()(ofxPythonObject o1, ofxPythonObject o2, ofxPythonObject o3, ofxPythonObject o4)
{
    ofxPythonOperation op;
    if(o1.data->obj == nullptr) o1=ofxPythonObject::_None();
    if(o2.data->obj == nullptr) o2=ofxPythonObject::_None();
    if(o3.data->obj == nullptr) o3=ofxPythonObject::_None();
    if(o4.data->obj == nullptr) o4=ofxPythonObject::_None();
    return make_object_owned(PyObject_CallFunctionObjArgs(data->obj,o1.data->obj,o2.data->obj,o3.data->obj,o4.data->obj,nullptr));
}

ofxPythonMappingValue ofxPythonObject::operator [](const string& key)
{
	return ofxPythonMappingValue(*this,key);
}

ofxPythonMappingValue ofxPythonObject::operator [](const char * key)
{
	return ofxPythonMappingValue(*this,string(key));
}

ofxPythonAttrValue ofxPythonObject::attr(const string& attribute)
{
	return ofxPythonAttrValue(*this, attribute);
}

const string ofxPythonObject::repr()
{
	ofxPythonOperation op;
	ofxPythonObject objectsRepresentation = make_object_owned(PyObject_Repr(data->obj));
#ifdef TARGET_LINUX
    string s = string(_PyUnicode_AsString(objectsRepresentation.data->obj));
#else
    string s = string(PyString_AsString(objectsRepresentation.data->obj));
#endif
	PythonErrorCheck();
	return s;
}

const string ofxPythonObject::str()
{
	ofxPythonOperation op;
	ofxPythonObject objectsRepresentation = make_object_owned(PyObject_Str(data->obj));
#ifdef TARGET_LINUX
    string s = string(_PyUnicode_AsString(objectsRepresentation.data->obj));
#else
    string s = string(PyString_AsString(objectsRepresentation.data->obj));
#endif
	PythonErrorCheck();
	return s;
}

ofxPythonObject::operator bool() const
{
	ofxPythonOperation op;
	return data && data->obj && !isNone(); //TODO: check if evaluates to false (0,(,),[])
}

bool ofxPythonObject::isNone() const
{
	ofxPythonOperation op;
	return data && data->obj == Py_None;
}

bool ofxPythonObject::isBool() const
{
	ofxPythonOperation op;
	return data && PyBool_Check(data->obj);
}

bool ofxPythonObject::isInt() const
{
	ofxPythonOperation op;
#ifdef TARGET_LINUX
    return data && (PyLong_Check(data->obj));
#else
    return data && (PyInt_Check(data->obj) || PyLong_Check(data->obj));
#endif

}

bool ofxPythonObject::isFloat() const
{
	ofxPythonOperation op;
	return data && PyFloat_Check(data->obj);
}

bool ofxPythonObject::isString() const
{
	ofxPythonOperation op;
#ifdef TARGET_LINUX
    return data && PyUnicode_Check(data->obj);
#else
    return data && PyString_Check(data->obj);
#endif
}

bool ofxPythonObject::isList() const
{
	ofxPythonOperation op;
	return data && PyList_Check(data->obj);
}

bool ofxPythonObject::isTuple() const
{
	ofxPythonOperation op;
	return data && PyTuple_Check(data->obj);
}

bool ofxPythonObject::isDict() const
{
	ofxPythonOperation op;
	return data && PyDict_Check(data->obj);
}

long int ofxPythonObject::asInt() const
{
	ofxPythonOperation op;
#ifdef TARGET_LINUX
    if(PyLong_Check(data->obj)) return PyLong_AsLong(data->obj);
#else
    if (PyInt_Check(data->obj))
        return PyInt_AsLong(data->obj);
    else if(PyLong_Check(data->obj))
        return PyLong_AsLong(data->obj);
#endif
	return 0;
}

bool ofxPythonObject::asBool() const
{
	ofxPythonOperation op;
	if (isBool())
		return data->obj == Py_True;
	return false;
}

double ofxPythonObject::asFloat() const
{
	ofxPythonOperation op;
	if (isFloat())
		return PyFloat_AsDouble(data->obj);
	return 0.0;
}

string ofxPythonObject::asString() const
{
	ofxPythonOperation op;
#ifdef TARGET_LINUX
    if(isString()) return string(_PyUnicode_AsString(data->obj));
#else
    if(isString()) return string(PyString_AsString(data->obj));
#endif
	return string();
}

vector<ofxPythonObject> ofxPythonObject::asVector() const
{
	ofxPythonOperation op;
	std::vector<ofxPythonObject> v;
	if(isList())
	{
		int len = PyList_Size(data->obj);
		for (int i = 0; i<len; ++i)
		{
			v.push_back(make_object_borrowed(PyList_GetItem(data->obj,i)));
		}
	}
	else if(isTuple())
	{
		int len = PyTuple_Size(data->obj);
		for (int i = 0; i<len; ++i)
		{
			v.push_back(make_object_borrowed(PyTuple_GetItem(data->obj,i)));
		}
	}
	return v;
}

std::map<ofxPythonObject,ofxPythonObject> ofxPythonObject::asMap() const
{
	ofxPythonOperation op;
	std::map<ofxPythonObject,ofxPythonObject> m;
	if(isDict())
	{
		PyObject *key, *value;
		Py_ssize_t pos = 0;

		while (PyDict_Next(data->obj, &pos, &key, &value)) {
		    m[make_object_borrowed(key)] = make_object_borrowed(value);
		}
	}
	return m;
}


ofxPythonObject ofxPythonObject::fromInt(long int i)
{
	ofxPythonOperation op;
#ifdef TARGET_LINUX
    return make_object_owned(PyLong_FromLong(i));
#else
    return make_object_owned(PyInt_FromLong(i));
#endif

}

ofxPythonObject ofxPythonObject::fromBool(bool b)
{
	ofxPythonOperation op;
	if(b)
		return make_object_borrowed(Py_True);
	return make_object_borrowed(Py_False);
}

ofxPythonObject ofxPythonObject::_None()
{
	ofxPythonOperation op;
	return make_object_borrowed(Py_None);
}

ofxPythonObject ofxPythonObject::fromFloat(double d)
{
	ofxPythonOperation op;
	return make_object_owned(PyFloat_FromDouble(d));
}

ofxPythonObject ofxPythonObject::fromString(const string& s)
{
	ofxPythonOperation op;
#ifdef TARGET_LINUX
    return make_object_owned(PyUnicode_FromString(s.c_str()));
#else
    return make_object_owned(PyString_FromString(s.c_str()));
#endif

}

ofxPythonTupleMaker ofxPythonObject::makeTuple()
{
	ofxPythonTupleMaker t;
	return t;
}

ofxPythonListMaker ofxPythonObject::makeList()
{
    ofxPythonListMaker t;
    return t;
}

ofxPythonObject ofxPythonObject::makeDict(){
    ofxPythonOperation op;
    return make_object_owned(PyDict_New());
}

ofxPythonMappingValue::ofxPythonMappingValue(ofxPythonObject o, const string& k)
:object(o), key(k){}

ofxPythonMappingValue::operator ofxPythonObject()
{
	ofxPythonOperation op;
	return make_object_owned(
		PyMapping_GetItemString(object.data->obj, noconststring(key)));
}

ofxPythonMappingValue& ofxPythonMappingValue::operator =(ofxPythonObject o)
{
	ofxPythonOperation op;
	PyMapping_SetItemString(object.data->obj, noconststring(key) , o.data->obj);
	PythonErrorCheck();
	return *this;
}

ofxPythonAttrValue::ofxPythonAttrValue(ofxPythonObject o, const string& attr)
:object(o),attribute(attr){}

ofxPythonAttrValue::operator ofxPythonObject()
{
	ofxPythonOperation op;
	return make_object_owned(
		PyObject_GetAttrString(object.data->obj,attribute.c_str()));
}

ofxPythonAttrValue & ofxPythonAttrValue::operator =(ofxPythonObject o)
{
	ofxPythonOperation op;
	PyObject_SetAttrString(object.data->obj, attribute.c_str(), o.data->obj);
	PythonErrorCheck();
	return *this;
}

ofxPythonMappingValue & ofxPythonMappingValue::operator =(ofxPythonMappingValue & o)
{
	return *this = (ofxPythonObject) o;
}

ofxPythonAttrValue & ofxPythonAttrValue::operator =(ofxPythonAttrValue & o)
{
	return *this = (ofxPythonObject) o;
}

ofxPythonObject ofxPythonObjectLike::method(const string &method_name)
{
	ofxPythonOperation op;
	ofxPythonObject PO = *this;
	return PO.method(method_name);
}
ofxPythonObject ofxPythonObjectLike::operator ()()
{
	ofxPythonOperation op;
	ofxPythonObject PO = *this;
	return PO();
}
ofxPythonObject ofxPythonObjectLike::operator ()(ofxPythonObject o)
{
	ofxPythonOperation op;
	ofxPythonObject PO = *this;
	return PO(o);
}
ofxPythonObject ofxPythonObjectLike::operator ()(ofxPythonObject o1, ofxPythonObject o2)
{
	ofxPythonOperation op;
	ofxPythonObject PO = *this;
	return PO(o1,o2);
}
ofxPythonObject ofxPythonObjectLike::operator ()(ofxPythonObject o1, ofxPythonObject o2, ofxPythonObject o3)
{
	ofxPythonOperation op;
	ofxPythonObject PO = *this;
	return PO(o1,o2,o3);
}
ofxPythonObject ofxPythonObjectLike::operator ()(ofxPythonObject o1, ofxPythonObject o2, ofxPythonObject o3, ofxPythonObject o4)
{
    ofxPythonOperation op;
    ofxPythonObject PO = *this;
    return PO(o1,o2,o3,o4);
}
ofxPythonAttrValue ofxPythonObjectLike::attr(const string& attribute)
{
	ofxPythonOperation op;
	ofxPythonObject PO = *this;
	return PO.attr(attribute);
}
ofxPythonMappingValue ofxPythonObjectLike::operator [](const string& key)
{
	ofxPythonOperation op;
	ofxPythonObject PO = *this;
	return PO[key];
}
ofxPythonMappingValue ofxPythonObjectLike::operator [](const char * key)
{
	ofxPythonOperation op;
	ofxPythonObject PO = *this;
	return PO[key];
}
bool ofxPythonObjectLike::isNone()
{
	ofxPythonOperation op;
	ofxPythonObject PO = *this;
	return PO.isNone();
}
bool ofxPythonObjectLike::isBool()
{
	ofxPythonOperation op;
	ofxPythonObject PO = *this;
	return PO.isBool();
}
bool ofxPythonObjectLike::isInt()
{
	ofxPythonOperation op;
	ofxPythonObject PO = *this;
	return PO.isInt();
}
bool ofxPythonObjectLike::isFloat()
{
	ofxPythonOperation op;
	ofxPythonObject PO = *this;
	return PO.isFloat();
}
bool ofxPythonObjectLike::isString()
{
	ofxPythonOperation op;
	ofxPythonObject PO = *this;
	return PO.isString();
}
bool ofxPythonObjectLike::isList()
{
	ofxPythonOperation op;
	ofxPythonObject PO = *this;
	return PO.isList();
}
bool ofxPythonObjectLike::isDict()
{
	ofxPythonOperation op;
	ofxPythonObject PO = *this;
	return PO.isDict();
}
bool ofxPythonObjectLike::isTuple()
{
	ofxPythonOperation op;
	ofxPythonObject PO = *this;
	return PO.isTuple();
}
bool ofxPythonObjectLike::asBool()
{
	ofxPythonOperation op;
	ofxPythonObject PO = *this;
	return PO.asBool();
}
long int ofxPythonObjectLike::asInt()
{
	ofxPythonOperation op;
	ofxPythonObject PO = *this;
	return PO.asInt();
}
double ofxPythonObjectLike::asFloat()
{
	ofxPythonOperation op;
	ofxPythonObject PO = *this;
	return PO.asFloat();
}
string ofxPythonObjectLike::asString()
{
	ofxPythonOperation op;
	ofxPythonObject PO = *this;
	return PO.asString();
}
vector<ofxPythonObject> ofxPythonObjectLike::asVector()
{
	ofxPythonOperation op;
	ofxPythonObject PO = *this;
	return PO.asVector();
}

std::map<ofxPythonObject,ofxPythonObject> ofxPythonObjectLike::asMap()
{
	ofxPythonOperation op;
	ofxPythonObject PO = *this;
	return PO.asMap();
}

ofxPythonObjectLike::operator bool()
{
	ofxPythonOperation op;
	ofxPythonObject PO = *this;
	return (bool)PO;
}
const string ofxPythonObjectLike::repr()
{
	ofxPythonOperation op;
	ofxPythonObject PO = *this;
	return PO.repr();
}

const string ofxPythonObjectLike::str()
{
	ofxPythonOperation op;
	ofxPythonObject PO = *this;
	return PO.str();
}

ofxPythonTupleMaker& ofxPythonTupleMaker::operator<<(ofxPythonObject o)
{
	contents.push_back(o);
	return *this;
}

ofxPythonTupleMaker::operator ofxPythonObject()
{
	ofxPythonOperation op;
	ofxPythonObject tuple = make_object_owned(
		PyTuple_New(contents.size()));
	for (unsigned int i = 0; i < contents.size(); ++i)
	{
		PyTuple_SetItem(tuple.data->obj,i,contents[i].data->obj);
		Py_XINCREF(contents[i].data->obj); //finding this one was tricky!
	}
    tuple.pythonError = PythonErrorCheck();
	return tuple;
}

ofxPythonListMaker& ofxPythonListMaker::operator<<(ofxPythonObject o)
{
    contents.push_back(o);
    return *this;
}

ofxPythonListMaker::operator ofxPythonObject()
{
    ofxPythonOperation op;
    ofxPythonObject list = make_object_owned(
        PyList_New(contents.size()));
    for (unsigned int i = 0; i < contents.size(); ++i)
    {
        PyList_SetItem(list.data->obj,i,contents[i].data->obj);
        Py_XINCREF(contents[i].data->obj); //finding this one was tricky!
    }
    list.pythonError = PythonErrorCheck();
    return list;
}

PyThreadState * ofxPythonOperation::pstate = nullptr;
unsigned int ofxPythonOperation::instances = 0;

ofxPythonOperation::ofxPythonOperation(){
    if(instances == 0){
        PyEval_RestoreThread(pstate);
    }
    instances++;
}

ofxPythonOperation::~ofxPythonOperation(){
    instances--;
    if(instances ==0){
        pstate = PyEval_SaveThread();
    }
}
