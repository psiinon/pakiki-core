#include <iostream>
#include <Python.h>
#include <frameobject.h>
#include <stdio.h>
#include <string>

using std::cout;
using std::endl;
using std::string;
using std::wcout;

#ifdef _WIN32
#include <direct.h>
#else
#include <fcntl.h>
#endif

wchar_t *GetWC(const char *c) {
  const size_t cSize = strlen(c)+1;
  wchar_t* wc = new wchar_t[cSize];
  mbstowcs (wc, c, cSize);

  return wc;
}

bool errorOccurred() {
  if (PyErr_Occurred()) {
    /* if we want to get the exception string */
    PyObject *errtype, *errvalue, *errtraceback;
    PyErr_Fetch(&errtype, &errvalue, &errtraceback);
    PyErr_NormalizeException(&errtype, &errvalue, &errtraceback);
    if(errvalue != nullptr) {
      PyObject *s = PyObject_Str(errvalue);
      Py_ssize_t size;
      const char* exception = PyUnicode_AsUTF8AndSize(s, &size);
      printf("PROXIMITY_PYTHON_INTERPRETER_ERROR\n");
      printf("%s\n", exception);
      Py_DECREF(s);
    }

    PyTracebackObject* traceback = (PyTracebackObject*)errtraceback;
    if(traceback) {
      do {
        // TODO: Here we can also print all of the details of the python frame (traceback->tb_frame)
        PyObject *s = PyObject_Str(traceback->tb_frame->f_code->co_filename);
        Py_ssize_t size;
        const char* filename = PyUnicode_AsUTF8AndSize(s, &size);

        printf("%s:%d\n", filename, traceback->tb_lineno);
        traceback = traceback->tb_next;
        Py_DECREF(s);

      } while(traceback != nullptr);
    }

    Py_XDECREF(errvalue);
    Py_XDECREF(errtype);
    Py_XDECREF(traceback);

    return true;
  }
  else {
    return false;
  }
}

void incrementFilename(char* filename) {
  if(strcmp(filename, "") == 0) {
    strcpy(filename, "proximity_common.py");
  }
  else if(strcmp(filename, "proximity_common.py") == 0) {
    strcpy(filename, "script.py");
  }
  else {
    strcpy(filename, "function_call.py");
  }
}

char* concatenateDir(const char* path) {
  char* currDir = (char*)malloc(102400);
  currDir = getcwd(currDir, 102400);
  if(currDir == nullptr) {
    fprintf(stderr, "%d", errno);
  }

  size_t currDirLen = strlen(currDir);

  if(currDirLen > (102400 - strlen(path) - 3))
    return nullptr;

  memcpy(currDir + currDirLen, path, strlen(path) + 1);

  return currDir;
}

char* getDir() {
#ifdef _WIN32
  char* dir = concatenateDir("\\python39");
#elif defined(__linux__)
  char* dir = concatenateDir("/python39");
#else

  char* dir = concatenateDir("/python39");
  int fd = open(dir, O_RDONLY);
  if(fd == -1) {
    free(dir);
    dir = concatenateDir("/Razor.app/Contents/MacOS/python39");
    fd = open(dir, O_RDONLY);

    if(fd == -1)
      return nullptr;
  }

  close(fd);
#endif
  return dir;
}

bool runDiscreteCode(string code, char* filename, PyObject* py_dict) {
  PyObject* compiledObject = Py_CompileString(code.c_str(), filename, Py_file_input);

  if(errorOccurred()) {
    return false;
  }

  PyEval_EvalCode(compiledObject, py_dict, py_dict);
  if(errorOccurred()) {
    return false;
  }

  return true;
}

bool runPythonScript() {
  PyThreadState* globalThreadState = PyThreadState_Get();
  PyThreadState* threadState = Py_NewInterpreter();
  if(threadState == nullptr) {
    return false;
  }

  PyThreadState_Swap(threadState);

  //cout << "Created thread and swapped to it" << endl;
  //cout << "RAZOR_PYTHON_INTERPRETER_READY" << endl;

  string pythonCode;
  string line;
  std::getline(std::cin, line, '\n');

  PyObject *py_main = PyImport_AddModule("__main__");
  PyObject *py_dict = PyModule_GetDict(py_main);

  bool errorThrown = false;

  char filename[256];
  filename[0] = 0;
  incrementFilename(filename);
  bool endInterpreter = false;

  //cout << "Starting main interpreter loop" << endl;

  while(line != "PROXIMITY_PYTHON_INTERPRETER_END_OF_SCRIPT" && line != "PROXIMITY_PYTHON_INTERPRETER_END_INTERPRETER") {
    if(line == "PROXIMITY_PYTHON_INTERPRETER_END_OF_BLOCK") {
      if(pythonCode != "") {
        if(runDiscreteCode(pythonCode, filename, py_dict) == false) {
          errorThrown = true;
          break;
        }
      }
      pythonCode = "";
      incrementFilename(filename);
      cout << "PROXIMITY_PYTHON_INTERPRETER_READY" << endl;
    }
    else {
      pythonCode += line + "\n";
    }
    std::getline(std::cin, line, '\n');
  }

  if(line == "PROXIMITY_PYTHON_INTERPRETER_END_INTERPRETER") {
    endInterpreter = true;
  }

  if(!errorThrown) {
    runDiscreteCode(pythonCode, filename, py_dict);
  }

  cout << endl << "PROXIMITY_PYTHON_INTERPRETER_SCRIPT_FINISHED" << endl;

  Py_EndInterpreter(PyThreadState_Get());
  PyThreadState_Swap(globalThreadState);

  return endInterpreter;
}

int main(int /*argc*/, char *argv[]) {
  wchar_t *program = Py_DecodeLocale(argv[0], nullptr);
  if (program == nullptr) {
    fprintf(stderr, "Fatal error: cannot decode argv[0]\n");
    exit(1);
  }
  Py_SetProgramName(program);  /* optional but recommended */
  char* currDir = getDir();

  Py_SetPythonHome(GetWC(currDir));

  Py_UnbufferedStdioFlag = 1;

  Py_Initialize();

  //while(!runPythonScript());
  runPythonScript();

  Py_Finalize();
  PyMem_RawFree(program);

  return 0;
}
