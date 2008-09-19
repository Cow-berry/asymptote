/*****
 * settings.cc
 * Andy Hammerlindl 2004/05/10
 *
 * Declares a list of global variables that act as settings in the system.
 *****/

#include <iostream>
#include <iomanip>
#include <cstdlib>
#include <cerrno>
#include <sys/stat.h>
#include <cfloat>
#include <locale.h>
#include <unistd.h>
#include <algorithm>

#include "common.h"

#if HAVE_GNU_GETOPT_H
#include <getopt.h>
#else
#include "getopt.h"
#endif

#include "util.h"
#include "settings.h"
#include "interact.h"
#include "locate.h"
#include "lexical.h"
#include "record.h"
#include "env.h"
#include "item.h"
#include "refaccess.h"

#ifdef HAVE_LIBCURSES
extern "C" {

#ifdef HAVE_NCURSES_CURSES_H
#include <ncurses/curses.h>
#include <ncurses/term.h>
#elif HAVE_NCURSES_H
#include <ncurses.h>
#include <term.h>
#elif HAVE_CURSES_H
#include <curses.h>
#include <term.h>
#endif
}
#endif

// Workaround broken curses.h files:
#ifdef clear
#undef clear
#endif

using vm::item;

using trans::itemRefAccess;
using trans::refAccess;
using trans::varEntry;

void doConfig(string filename);

namespace settings {
  
using camp::pair;
  
string asyInstallDir; // Used only by msdos
string defaultXasy="xasy";

#ifdef HAVE_LIBGLUT
const bool haveglut=true;  
#else
const bool haveglut=false;
#endif
  
#ifndef __CYGWIN__
  
bool msdos=false;
const char *HOME="HOME";
const char pathSeparator=':';
string defaultPSViewer="gv";
#ifdef __APPLE__
string defaultPDFViewer="open";
#else  
string defaultPDFViewer="acroread";
#endif  
string defaultGhostscript="gs";
string defaultDisplay="display";
string defaultPython;
const string docdir=ASYMPTOTE_DOCDIR;
void queryRegistry() {}
  
#else  
  
bool msdos=true;
const char *HOME="USERPROFILE";
const char pathSeparator=';';
string defaultPSViewer="gsview32.exe";
string defaultPDFViewer="AcroRd32.exe";
string defaultGhostscript="gswin32c.exe";
string defaultPython="python.exe";
string defaultDisplay="imdisplay";
#undef ASYMPTOTE_SYSDIR
#define ASYMPTOTE_SYSDIR asyInstallDir
const string docdir=".";
  
#include <dirent.h>
  
// Use key to look up an entry in the MSWindows registry, respecting wild cards
string getEntry(const string& key)
{
  string path="/proc/registry/HKEY_LOCAL_MACHINE/SOFTWARE/"+key;
  size_t star;
  string head;
  while((star=path.find("*")) < string::npos) {
    string prefix=path.substr(0,star);
    string suffix=path.substr(star+1);
    size_t slash=suffix.find("/");
    if(slash < string::npos) {
      path=suffix.substr(slash);
      suffix=suffix.substr(0,slash);
    } else {
      path=suffix;
      suffix="";
    }
    string directory=head+stripFile(prefix);
    string file=stripDir(prefix);
    DIR *dir=opendir(directory.c_str());
    if(dir == NULL) return "";
    dirent *p;
    string rsuffix=suffix;
    reverse(rsuffix.begin(),rsuffix.end());
    while((p=readdir(dir)) != NULL) {
      string dname=p->d_name;
      string rdname=dname;
      reverse(rdname.begin(),rdname.end());
      if(dname != "." && dname != ".." && 
	 dname.substr(0,file.size()) == file &&
	 rdname.substr(0,suffix.size()) == rsuffix) {
	head=directory+p->d_name;
	break;
      }
    }
    if(p == NULL) return "";
  }
  std::ifstream fin((head+path).c_str());
  if(fin) {
    string s;
    getline(fin,s);
    size_t end=s.find('\0');
    if(end < string::npos)
      return s.substr(0,end);
  }
  return "";
}
  
void queryRegistry()
{
  string gs=getEntry("GPL Ghostscript/*/GS_DLL");
  if(gs.empty())
    gs=getEntry("AFPL Ghostscript/*/GS_DLL");
  defaultGhostscript=stripFile(gs)+defaultGhostscript;
  defaultPDFViewer=getEntry("Adobe/Acrobat Reader/*/InstallPath/@")+"\\"+
    defaultPDFViewer;
  defaultPSViewer=getEntry("Ghostgum/GSview/*")+"\\gsview\\"+defaultPSViewer;
  defaultPython=getEntry("Python/PythonCore/*/InstallPath/@")+defaultPython;
  asyInstallDir=getEntry("Microsoft/Windows/CurrentVersion/App Paths/Asymptote/Path");
  defaultXasy=asyInstallDir+"\\"+defaultXasy;
}
  
#endif  
  
const char PROGRAM[]=PACKAGE_NAME;
const char VERSION[]=PACKAGE_VERSION;
const char BUGREPORT[]=PACKAGE_BUGREPORT;

// The name of the program (as called).  Used when displaying help info.
char *argv0;

// The verbosity setting, a global variable.
Int verbose;
  
// Colorspace conversion flags (stored in global variables for efficiency). 
bool gray;
bool bw;  
bool rgb;
bool cmyk;
  
// Disable system calls.
bool safe=true;
// Enable writing to (or changing to) other directories
bool globaloption=false;
bool globaloutname=false;
  
bool globalwrite() {return globaloption || !safe;}
  
const string suffix="asy";
const string guisuffix="gui";
  
string initdir;
string historyname;

// Local versions of the argument list.
int argCount = 0;
char **argList = 0;
  
typedef ::option c_option;

types::dummyRecord *settingsModule;

types::record *getSettingsModule() {
  return settingsModule;
}

// The dictionaries of long options and short options.
class option;
typedef mem::map<CONST string, option *> optionsMap_t;
optionsMap_t optionsMap;

typedef mem::map<CONST char, option *> codeMap_t;
codeMap_t codeMap;
  
struct option : public gc {
  string name;
  char code;      // Command line option, i.e. 'V' for -V.
  bool argument;  // If it takes an argument on the command line.  This is set
                  // based on whether argname is empty.
  string argname; // The argument name for printing the description.
  string desc; // One line description of what the option does.
  bool cmdlineonly; // If it is only available on the command line.
  string Default; // A string containing an optional default value.

  option(string name, char code, string argname, string desc,
	 bool cmdlineonly=false, string Default="")
    : name(name), code(code), argument(!argname.empty()), argname(argname),
      desc(desc), cmdlineonly(cmdlineonly), Default(Default) {}

  virtual ~option() {}

  // Builds this option's contribution to the optstring argument of get_opt().
  virtual string optstring() {
    if (code) {
      string base;
      base.push_back(code);
      if(argument) base.push_back(':');
      return base;
    }
    else return "";
  }

  // Sets the contribution to the longopt array.
  virtual void longopt(c_option &o) {
    o.name=name.c_str();
    o.has_arg=argument ? 1 : 0;
    o.flag=0;
    o.val=0;
  }

  // Add to the dictionaries of options.
  virtual void add() {
    optionsMap[name]=this;
    if (code)
      codeMap[code]=this;
  }

  // Set the option from the command-line argument.  Return true if the option
  // was correctly parsed.
  virtual bool getOption() = 0;

  void error(string msg) {
    cerr << endl << argv0 << ": ";
    if (code)
      cerr << "-" << code << " ";
    cerr << "(-" << name << ") " << msg << endl;
  }

  // The "-f,-outformat format" part of the option.
  virtual string describeStart() {
    ostringstream ss;
    if (code)
      ss << "-" << code << ",";
    ss << "-" << name;
    if (argument)
      ss << " " << argname;
    return ss.str();
  }

  // Outputs description of the command for the -help option.
  virtual void describe() {
    // Don't show the option if it has no desciption.
    if (!desc.empty()) {
      const unsigned WIDTH=22;
      string start=describeStart();
      cerr << std::left << std::setw(WIDTH) << start;
      if (start.size() >= WIDTH) {
        cerr << endl;
        cerr << std::left << std::setw(WIDTH) << "";
      }
      cerr << desc;
      if(cmdlineonly) cerr << "; command-line only";
      if(Default != "")
	cerr << " [" << Default << "]";
      cerr << endl;
    }
  }
  
  virtual void reset() {
  }
};

const string noarg;

struct setting : public option {
  types::ty *t;

  setting(string name, char code, string argname, string desc,
          types::ty *t, string Default)
    : option(name, code, argname, desc, false,Default), t(t) {}

  void reset() = 0;

  virtual trans::access *buildAccess() = 0;

  // Add to the dictionaries of options and to the settings module.
  virtual void add() {
    option::add();

    settingsModule->add(name, t, buildAccess());
  }
};

struct itemSetting : public setting {
  item defaultValue;
  item value;

  itemSetting(string name, char code,
              string argname, string desc,
              types::ty *t, item defaultValue, string Default="")
    : setting(name, code, argname, desc, t, Default),
      defaultValue(defaultValue) {reset();}

  void reset() {
    value=defaultValue;
  }

  trans::access *buildAccess() {
    return new itemRefAccess(&(value));
  }
};

item& Setting(string name) {
  itemSetting *s=dynamic_cast<itemSetting *>(optionsMap[name]);
  if(!s) {
    cerr << "Cannot find setting named '" << name << "'" << endl;
    exit(-1);
  }
  return s->value;
}
  
struct boolSetting : public itemSetting {
  boolSetting(string name, char code, string desc,
              bool defaultValue=false)
    : itemSetting(name, code, noarg, desc,
                  types::primBoolean(), (item)defaultValue,
		  defaultValue ? "true" : "false") {}

  bool getOption() {
    value=(item)true;
    return true;
  }

  option *negation(string name) {
    struct negOption : public option {
      boolSetting &base;

      negOption(boolSetting &base, string name)
        : option(name, 0, noarg, ""), base(base) {}

      bool getOption() {
        base.value=(item)false;
        return true;
      }
    };
    return new negOption(*this, name);
  }

  void add() {
    setting::add();
    negation("no"+name)->add();
    if (code) {
      string nocode="no"; nocode.push_back(code);
      negation(nocode)->add();
    }
  }

  // Set several related boolean options at once.  Used for view and trap which
  // have batch and interactive settings.
  struct multiOption : public option {
    typedef mem::list<boolSetting *> setlist;
    setlist set;
    multiOption(string name, char code, string desc)
      : option(name, code, noarg, desc, true) {}

    void add(boolSetting *s) {
      set.push_back(s);
    }

    void setValue(bool value) {
      for (setlist::iterator s=set.begin(); s!=set.end(); ++s)
        (*s)->value=(item)value;
    }

    bool getOption() {
      setValue(true);
      return true;
    }

    option *negation(string name) {
      struct negOption : public option {
        multiOption &base;

        negOption(multiOption &base, string name)
          : option(name, 0, noarg, ""), base(base) {}

        bool getOption() {
          base.setValue(false);
          return true;
        }
      };
      return new negOption(*this, name);
    }

    void add() {
      option::add();
      negation("no"+name)->add();
      if (code) {
        string nocode="no"; nocode.push_back(code);
        negation(nocode)->add();
      }

      for (multiOption::setlist::iterator s=set.begin(); s!=set.end(); ++s)
        (*s)->add();
    }
  };
};

typedef boolSetting::multiOption multiOption;

struct argumentSetting : public itemSetting {
  argumentSetting(string name, char code,
                  string argname, string desc,
                  types::ty *t, item defaultValue)
    : itemSetting(name, code, argname, desc, t, defaultValue) 
  {
    assert(!argname.empty());
  }
};

struct stringSetting : public argumentSetting {
  stringSetting(string name, char code,
                string argname, string desc,
                string defaultValue)
    : argumentSetting(name, code, argname, desc.empty() ? "" :
		      desc+(defaultValue.empty() ? "" : " ["+defaultValue+"]"),
		      types::primString(), (item)defaultValue) {}

  bool getOption() {
    value=(item)(string)optarg;
    return true;
  }
};

struct stringOutnameSetting : public argumentSetting {
  stringOutnameSetting(string name, char code,
		       string argname, string desc,
		       string defaultValue)
    : argumentSetting(name, code, argname, desc,
		      types::primString(), (item)defaultValue) {}

  bool getOption() {
    value=(item)(string)
      ((globaloutname || globalwrite()) ? optarg : stripDir(optarg));
    return true;
  }
};

struct userSetting : public argumentSetting {
  userSetting(string name, char code,
	      string argname, string desc,
	      string defaultValue)
    : argumentSetting(name, code, argname, desc,
		      types::primString(), (item)defaultValue) {}

  bool getOption() {
    string s=vm::get<string>(value)+string(optarg);
    s.push_back(';');
    value=(item) s;
    return true;
  }
};

string GetEnv(string s, string Default) {
  transform(s.begin(), s.end(), s.begin(), toupper);        
  string t=Getenv(("ASYMPTOTE_"+s).c_str(),msdos);
  return t != "" ? string(t) : Default;
}
  
struct envSetting : public stringSetting {
  envSetting(string name, string Default)
    : stringSetting(name, 0, " ", "", GetEnv(name,Default)) {}
};

template<class T>
struct dataSetting : public argumentSetting {
  string text;
  dataSetting(const char *text, string name, char code,
	      string argname, string desc, types::ty *type,
	      T defaultValue)
    : argumentSetting(name, code, argname, desc,
		      type, (item)defaultValue), text(text) {}

  bool getOption() {
    try {
      value=(item)lexical::cast<T>(optarg);
    } catch (lexical::bad_cast&) {
      error("option requires " + text + " as an argument");
      return false;
    }
    return true;
  }
};

template<class T>
string String(T x)
{
  ostringstream buf;
  buf << x; 
  return buf.str();
}
  
template<class T>
string description(string desc, T defaultValue) 
{
  return desc.empty() ? "" : desc+" ["+String(defaultValue)+"]";
}

struct IntSetting : public dataSetting<Int> {
  IntSetting(string name, char code,
	     string argname, string desc, Int defaultValue=0)
    : dataSetting<Int>("an int", name, code, argname,
		       description(desc,defaultValue),
		       types::primInt(), defaultValue) {}
};
  
struct realSetting : public dataSetting<double> {
  realSetting(string name, char code,
	      string argname, string desc, double defaultValue=0.0)
    : dataSetting<double>("a real", name, code, argname,
			  description(desc,defaultValue),
			  types::primReal(), defaultValue) {}
};
  
struct pairSetting : public dataSetting<pair> {
  pairSetting(string name, char code,
	      string argname, string desc, pair defaultValue=0.0)
    : dataSetting<pair>("a pair", name, code, argname,
			  description(desc,defaultValue),
			types::primPair(), defaultValue) {}
};
  
// For setting the alignment of a figure on the page.
struct alignSetting : public argumentSetting {
  alignSetting(string name, char code,
	       string argname, string desc,
	       Int defaultValue=(Int) CENTER)
    : argumentSetting(name, code, argname, desc,
		      types::primInt(), (item)defaultValue) {}

  bool getOption() {
    string str=optarg;
    if (str == "C")
      value=(Int) CENTER;
    else if (str == "T")
      value=(Int) TOP;
    else if (str == "B")
      value=(Int) BOTTOM;
    else if (str == "Z") {
      value=(Int) ZERO;
    } else {
      error("invalid argument for option");
      return false;
    }
    return true;
  }
};

template<class T>
string stringCast(T x)
{
  ostringstream buf;
  buf.precision(DBL_DIG);
  buf.setf(std::ios::boolalpha);
  buf << x;
  return string(buf.str());
}

template <class T>
struct refSetting : public setting {
  T *ref;
  T defaultValue;
  string text;

  refSetting(string name, char code, string argname,
             string desc, types::ty *t, T *ref, T defaultValue,
	     const char *text="")
    : setting(name, code, argname, desc, t, stringCast(defaultValue)),
      ref(ref), defaultValue(defaultValue), text(text) {
    reset();
  }

  virtual bool getOption() {
    try {
      *ref=lexical::cast<T>(optarg);
    } catch (lexical::bad_cast&) {
      error("option requires " + text + " as an argument");
      return false;
    }
    return true;
  }
  
  virtual void reset() {
    *ref=defaultValue;
  }

  trans::access *buildAccess() {
    return new refAccess<T>(ref);
  }
};

struct boolrefSetting : public refSetting<bool> {
  boolrefSetting(string name, char code, string desc, bool *ref,
		 bool Default=false)
    : refSetting<bool>(name, code, noarg, desc,
		       types::primBoolean(), ref, Default) {}
  bool getOption() {
    *ref=true;
    return true;
  }
  
  option *negation(string name) {
    struct negOption : public option {
      boolrefSetting &base;

      negOption(boolrefSetting &base, string name)
        : option(name, 0, noarg, ""), base(base) {}

      bool getOption() {
        *(base.ref)=false;
        return true;
      }
    };
    return new negOption(*this, name);
  }
  
  void add() {
    setting::add();
    negation("no"+name)->add();
    if (code) {
      string nocode="no"; nocode.push_back(code);
      negation(nocode)->add();
    }
  }
};

struct boolintrefSetting : public boolrefSetting {
  boolintrefSetting(string name, char code, string desc, int *ref,
		    bool Default=false)
    : boolrefSetting(name, code, desc, (bool *) ref, Default) {}
};

struct incrementSetting : public refSetting<Int> {
  incrementSetting(string name, char code, string desc, Int *ref)
    : refSetting<Int>(name, code, noarg, desc,
		      types::primInt(), ref, 0) {}

  bool getOption() {
    // Increment the value.
    ++(*ref);
    return true;
  }
  
  option *negation(string name) {
    struct negOption : public option {
      incrementSetting &base;

      negOption(incrementSetting &base, string name)
        : option(name, 0, noarg, ""), base(base) {}

      bool getOption() {
        if(*base.ref) --(*base.ref);
        return true;
      }
    };
    return new negOption(*this, name);
  }
  
  void add() {
    setting::add();
    negation("no"+name)->add();
    if (code) {
      string nocode="no"; nocode.push_back(code);
      negation(nocode)->add();
    }
  }
};

struct incrementOption : public option {
  Int *ref;
  Int level;
  
  incrementOption(string name, char code, string desc, Int *ref,
		  Int level=1)
    : option(name, code, noarg, desc, true), ref(ref), level(level) {}

  bool getOption() {
    // Increment the value.
    (*ref) += level;
    return true;
  }
};

void addOption(option *o) {
  o->add();
}

void version()
{
  cerr << PROGRAM << " version " << VERSION << SVN_REVISION
       << " [(C) 2004 Andy Hammerlindl, John C. Bowman, Tom Prince]" 
       << endl;
}

void usage(const char *program)
{
  version();
  cerr << "\t\t\t" << "http://asymptote.sourceforge.net/"
       << endl
       << "Usage: " << program << " [options] [file ...]"
       << endl;
}

void reportSyntax() {
  cerr << endl;
  usage(argv0);
  cerr << endl << "Type '" << argv0
       << " -h' for a description of options." << endl;
  exit(1);
}

void displayOptions()
{
  cerr << endl;
  cerr << "Options (negate by replacing - with -no): " 
       << endl << endl;
  for (optionsMap_t::iterator opt=optionsMap.begin();
       opt!=optionsMap.end();
       ++opt)
    opt->second->describe();
}

struct helpOption : public option {
  helpOption(string name, char code, string desc)
    : option(name, code, noarg, desc, true) {}

  bool getOption() {
    usage(argv0);
    displayOptions();
    cerr << endl;
    exit(0);

    // Unreachable code.
    return true;
  }
};

struct versionOption : public option {
  versionOption(string name, char code, string desc)
    : option(name, code, noarg, desc, true) {}

  bool getOption() {
    version();
    exit(0);

    // Unreachable code.
    return true;
  }
};

// For security reasons, these options aren't fields of the settings module.
struct boolOption : public option {
  bool *variable;
  bool value;

  boolOption(string name, char code, string desc,
	     bool *variable, bool value, bool Default)
    : option(name, code, noarg, desc, true, Default ? "true" : "false"),
      variable(variable), value(value) {}

  bool getOption() {
    *variable=value;
    return true;
  }
};

struct stringOption : public option {
  char **variable;
  stringOption(string name, char code, string argname,
	       string desc, char **variable)
    : option(name, code, argname, desc, true), variable(variable) {}

  bool getOption() {
    *variable=optarg;
    return true;
  }
};

string build_optstring() {
  string s;
  for (codeMap_t::iterator p=codeMap.begin(); p !=codeMap.end(); ++p)
    s +=p->second->optstring();

  return s;
}

c_option *build_longopts() {
  size_t n=optionsMap.size();

  c_option *longopts=new(UseGC) c_option[n];

  Int i=0;
  for (optionsMap_t::iterator p=optionsMap.begin();
       p !=optionsMap.end();
       ++p, ++i)
    p->second->longopt(longopts[i]);

  return longopts;
}

void resetOptions()
{
  for(optionsMap_t::iterator opt=optionsMap.begin(); opt != optionsMap.end();
      ++opt)
    if(opt->first != "config" && opt->first != "dir")
      opt->second->reset();
}
  
void getOptions(int argc, char *argv[])
{
  globaloutname=true;
  bool syntax=false;
  optind=0;

  string optstring=build_optstring();
  //cerr << "optstring: " << optstring << endl;
  c_option *longopts=build_longopts();
  int long_index = 0;

  errno=0;
  for(;;) {
    int c = getopt_long_only(argc,argv,
                             optstring.c_str(), longopts, &long_index);
    if (c == -1)
      break;

    if (c == 0) {
      const char *name=longopts[long_index].name;
      //cerr << "long option: " << name << endl;
      if (!optionsMap[name]->getOption())
        syntax=true;
    }
    else if (codeMap.find((char)c) != codeMap.end()) {
      //cerr << "char option: " << (char)c << endl;
      if (!codeMap[(char)c]->getOption())
        syntax=true;
    }
    else {
      syntax=true;
    }

    errno=0;
  }
  
  if (syntax)
    reportSyntax();
  globaloutname=false;
}

#ifdef USEGC
void no_GCwarn(char *, GC_word)
{
}
#endif

void initSettings() {
  queryRegistry();

  settingsModule=new types::dummyRecord(symbol::trans("settings"));
  
  multiOption *view=new multiOption("View", 'V', "View output");
  view->add(new boolSetting("batchView", 0, "View output in batch mode",
			    msdos));
  view->add(new boolSetting("multipleView", 0,
			    "View output from multiple batch-mode files",
			    false));
  view->add(new boolSetting("interactiveView", 0,
			    "View output in interactive mode", true));
  addOption(view);
  addOption(new stringSetting("xformat", 0, "format", 
			      "GUI deconstruction format","png"));
  addOption(new stringSetting("outformat", 'f', "format",
			      "Convert each output file to specified format",
			      ""));
  addOption(new boolSetting("prc", 0,
                            "Embed 3D PRC graphics in PDF output", true));
  addOption(new IntSetting("render", 0, "n",
			   "Render 3D graphics using n pixels per bp (-1=auto)",
			   haveglut ? -1 : 0));
  addOption(new boolSetting("twosided", 0,
                            "Use two-sided 3D lighting model for rendering",
			    true));
  addOption(new pairSetting("position", 0, "pair", 
			    "Initial 3D rendering screen position"));
  addOption(new boolSetting("thick", 0,
                            "Render thick 3D lines", true));
  addOption(new boolSetting("fitscreen", 0,
                            "Fit rendered image to screen", true));
  addOption(new IntSetting("maxviewport", 0, "n",
			   "Maximum viewport width/height",2048));
  addOption(new boolSetting("psimage", 0,
                            "Output ps image of 3D PRC graphics", false));
  addOption(new stringOutnameSetting("outname", 'o', "name",
				     "Alternative output name for first file",
				     ""));
  addOption(new boolSetting("interactiveWrite", 0,
                            "Write expressions entered at the prompt to stdout",
                            true));
  addOption(new helpOption("help", 'h', "Show summary of options"));
  addOption(new versionOption("version", 0, "Show version"));

  addOption(new pairSetting("offset", 'O', "pair", "PostScript offset"));
  addOption(new alignSetting("align", 'a', "C|B|T|Z",
			     "Center, Bottom, Top, or Zero page alignment [Center]"));
  
  addOption(new boolSetting("debug", 'd', "Enable debugging messages"));
  addOption(new incrementSetting("verbose", 'v',
				 "Increase verbosity level", &verbose));
  // Resolve ambiguity with --version
  addOption(new incrementOption("vv", 0,"", &verbose,2));
  addOption(new incrementOption("novv", 0,"", &verbose,-2));
  
  addOption(new boolSetting("keep", 'k', "Keep intermediate files"));
  addOption(new boolSetting("keepaux", 0,
			    "Keep intermediate LaTeX .aux files"));
  addOption(new stringSetting("tex", 0,"engine",
			      "TeX engine (\"latex|pdflatex|tex|pdftex|none\")",
			      "latex"));
  addOption(new boolSetting("twice", 0,
			    "Run LaTeX twice (to resolve references)"));
  addOption(new boolSetting("inlinetex", 0, "Generate inline tex code"));
  addOption(new boolSetting("parseonly", 'p', "Parse file"));
  addOption(new boolSetting("translate", 's',
			    "Show translated virtual machine code"));
  addOption(new boolSetting("tabcompletion", 0,
                            "Interactive prompt auto-completion", true));
  addOption(new boolSetting("listvariables", 'l',
			    "List available global functions and variables"));
  addOption(new boolSetting("where", 0,
			    "Show where listed variables are declared"));
  
  multiOption *mask=new multiOption("mask", 'm',
				    "Mask fpu exceptions");
  mask->add(new boolSetting("batchMask", 0,
			    "Mask fpu exceptions in batch mode", false));
  mask->add(new boolSetting("interactiveMask", 0,
			    "Mask fpu exceptions in interactive mode", true));
  addOption(mask);

  addOption(new boolrefSetting("bw", 0,
			       "Convert all colors to black and white",&bw));
  addOption(new boolrefSetting("gray", 0, "Convert all colors to grayscale",
			       &gray));
  addOption(new boolrefSetting("rgb", 0, "Convert cmyk colors to rgb",&rgb));
  addOption(new boolrefSetting("cmyk", 0, "Convert rgb colors to cmyk",&cmyk));

  addOption(new boolOption("safe", 0,
			   "Disable system call", &safe, true, true));
  addOption(new boolOption("unsafe", 0,
			   "Enable system call (=> global)", &safe, false,
			   false));
  addOption(new boolOption("globalwrite", 0,
			   "Allow write to other directory",
			   &globaloption, true, false));
  addOption(new boolOption("noglobalwrite", 0,
			   "", &globaloption, false, true));
  addOption(new stringOption("cd", 0, "directory", "Set current directory",
			     &startpath));
  
#ifdef USEGC  
  addOption(new boolintrefSetting("compact", 0,
				  "Conserve memory at the expense of speed",
				  &GC_dont_expand));
  addOption(new refSetting<GC_word>("divisor", 0, "n",
			      "Free space divisor for garbage collection",
				    types::primInt(),&GC_free_space_divisor,2,
				    "an int"));
#endif  
  
  addOption(new stringSetting("prompt", 0,"string","Prompt","> "));
  addOption(new stringSetting("prompt2", 0,"string",
                              "Continuation prompt for multiline input ",
			      ".."));
  addOption(new boolSetting("multiline", 0,
                            "Input code over multiple lines at the prompt"));

  addOption(new boolSetting("wait", 0,
			    "Wait for child processes to finish before exiting"));
  // Be interactive even in a pipe
  addOption(new boolSetting("interactive", 0, ""));
			    
  addOption(new boolSetting("localhistory", 0,
			    "Use a local interactive history file"));
  addOption(new IntSetting("historylines", 0, "n",
			   "Retain n lines of history",1000));
  addOption(new IntSetting("scroll", 0, "n",
			   "Scroll standard output n lines at a time",0));
  addOption(new IntSetting("level", 0, "n", "Postscript level",3));
  addOption(new boolSetting("autoplain", 0,
			    "Enable automatic importing of plain",
			    true));
  addOption(new boolSetting("autorotate", 0,
			    "Enable automatic PDF page rotation",
			    false));
  addOption(new boolSetting("pdfreload", 0,
                            "Automatically reload document in pdfviewer",
			    false));
  addOption(new IntSetting("pdfreloaddelay", 0, "usec",
			   "Delay before attempting initial pdf reload"
			   ,750000));
  addOption(new stringSetting("autoimport", 0, "string",
			      "Module to automatically import", ""));
  addOption(new userSetting("command", 'c', "string",
			    "Command to autoexecute", ""));
  addOption(new userSetting("user", 'u', "string",
			    "General purpose user string", ""));
  
  addOption(new realSetting("paperwidth", 0, "bp", ""));
  addOption(new realSetting("paperheight", 0, "bp", ""));
  
  addOption(new stringSetting("dvipsOptions", 0, "string", "", ""));
  addOption(new stringSetting("gsOptions", 0, "string", "", ""));
  addOption(new stringSetting("psviewerOptions", 0, "string", "", ""));
  addOption(new stringSetting("pdfviewerOptions", 0, "string", "", ""));
  addOption(new stringSetting("pdfreloadOptions", 0, "string", "", ""));
  addOption(new stringSetting("glOptions", 0, "string", "", ""));
  
  addOption(new envSetting("config","config."+suffix));
  addOption(new envSetting("pdfviewer", defaultPDFViewer));
  addOption(new envSetting("psviewer", defaultPSViewer));
  addOption(new envSetting("gs", defaultGhostscript));
  addOption(new envSetting("texpath", ""));
  addOption(new envSetting("texcommand", ""));
  addOption(new envSetting("texdvicommand", ""));
  addOption(new envSetting("dvips", "dvips"));
  addOption(new envSetting("convert", "convert"));
  addOption(new envSetting("display", defaultDisplay));
  addOption(new envSetting("animate", "animate"));
  addOption(new envSetting("python", defaultPython));
  addOption(new envSetting("xasy", defaultXasy));
  addOption(new envSetting("papertype", "letter"));
  addOption(new envSetting("dir", ""));
}

// Access the arguments once options have been parsed.
int numArgs() { return argCount; }
char *getArg(int n) { return argList[n]; }

void setInteractive() {
  if(numArgs() == 0 && !getSetting<bool>("listvariables") && 
     getSetting<string>("command").empty() &&
     (isatty(STDIN_FILENO) || getSetting<bool>("interactive")))
    interact::interactive=true;
  
  historyname=getSetting<bool>("localhistory") ? "."+suffix+"_history" 
    : (initdir+"/history");
}

bool view() {
  if (interact::interactive)
    return getSetting<bool>("interactiveView");
  else
    return getSetting<bool>("batchView") && 
      (numArgs() == 1 || getSetting<bool>("multipleView"));
}

bool trap() {
  if (interact::interactive)
    return !getSetting<bool>("interactiveMask");
  else
    return !getSetting<bool>("batchMask");
}

string outname() {
  string name=getSetting<string>("outname");
  return name.empty() ? "out" : name;
}

void initDir() {
  initdir=Getenv(HOME,msdos)+"/."+suffix;
  mkdir(initdir.c_str(),0777);
}
  
void setPath() {
  searchPath.clear();
  searchPath.push_back(".");
  string asydir=getSetting<string>("dir");
  if(asydir != "") {
    size_t p,i=0;
    while((p=asydir.find(pathSeparator,i)) < string::npos) {
      if(p > i) searchPath.push_back(asydir.substr(i,p-i));
      i=p+1;
    }
    if(i < asydir.length()) searchPath.push_back(asydir.substr(i));
  }
  searchPath.push_back(initdir);
#ifdef ASYMPTOTE_SYSDIR
  searchPath.push_back(ASYMPTOTE_SYSDIR);
#endif
}

void SetPageDimensions() {
  string paperType=getSetting<string>("papertype");

  if(paperType == "" &&
     getSetting<double>("paperwidth") != 0.0 &&
     getSetting<double>("paperheight") != 0.0) return;
  
  if(paperType == "letter") {
    Setting("paperwidth")=8.5*inches;
    Setting("paperheight")=11.0*inches;
  } else {
    Setting("paperwidth")=21.0*cm;
    Setting("paperheight")=29.7*cm;
    
    if(paperType != "a4") {
      cerr << "Unknown paper size \'" << paperType << "\'; assuming a4." 
	   << endl;
      Setting("papertype")=string("a4");
    }
  }
}

bool pdf(const string& texengine) {
  return texengine == "pdflatex" || texengine == "pdftex";
}

bool latex(const string& texengine) {
  return texengine == "latex" || texengine == "pdflatex";
}

string nativeformat() {
  return pdf(getSetting<string>("tex")) ? "pdf" : "eps";
}

string defaultformat() {
  string format=getSetting<string>("outformat");
  return (format == "") ? nativeformat() : format;
}

// TeX special command to set up currentmatrix for typesetting labels.
const char *beginlabel(const string& texengine) {
  if(pdf(texengine))
    return "\\special{pdf:q #5 0 0 cm}"
      "\\wd\\ASYbox 0pt\\dp\\ASYbox 0pt\\ht\\ASYbox 0pt";
  else 
    return "\\special{ps:gsave currentpoint currentpoint translate [#5 0 0] "
      "concat neg exch neg exch translate}";
}

// TeX special command to restore currentmatrix after typesetting labels.
const char *endlabel(const string& texengine) {
  if(pdf(texengine))
    return "\\special{pdf:Q}";
  else
    return "\\special{ps:currentpoint grestore moveto}";
}

// TeX macro to typeset raw postscript code
const char *rawpostscript(const string& texengine) {
  if(pdf(texengine))
    return "\\def\\ASYraw#1{#1}";
  else
    return "\\def\\ASYraw#1{\n"
      "currentpoint currentpoint translate matrix currentmatrix\n"
      "100 12 div -100 12 div scale\n"
      "#1\n"
      "setmatrix neg exch neg exch translate}";
}

// TeX macro to begin picture
const char *beginpicture(const string& texengine) {
  if(latex(texengine))
    return "\\begin{picture}";
  else
    return "\\picture";
}

// TeX macro to end picture
const char *endpicture(const string& texengine) {
  if(latex(texengine))
    return "\\end{picture}%";
  else
    return "\\endpicture%";
}

// Begin TeX special command.
const char *beginspecial(const string& texengine) {
  if(pdf(texengine))
    return "\\special{pdf:";
  return "\\special{ps:";
}

// End TeX special command.
const char *endspecial() {
  return "}%";
}

bool fataltex[]={false,true};
const char *pdftexerrors[]={"! "," ==> Fatal error",NULL};
const char *texerrors[]={"! ",NULL};

// Messages that signify a TeX error.
const char **texabort(const string& texengine)
{
  return settings::pdf(texengine) ? pdftexerrors : texerrors;
}

string texcommand(bool ps)
{
  string command;
  if(ps) {
    command=getSetting<string>("texdvicommand");
    if(command == "")
      command=latex(getSetting<string>("tex")) ? "latex" : "tex";
  } else
    command=getSetting<string>("texcommand");
  return command.empty() ? getSetting<string>("tex") : command;
}
  
string texprogram(bool ps)
{
  string path=getSetting<string>("texpath");
  string engine=texcommand(ps);
  return (path == "") ? engine : (string) (path+"/"+engine);
}

Int getScroll() 
{
  Int scroll=settings::getSetting<Int>("scroll");
#ifdef HAVE_LIBCURSES  
  if(scroll < 0) {
    char *terminal=getenv("TERM");
    if(terminal) {
      setupterm(terminal,1,NULL);
      scroll=lines > 2 ? lines-1 : 1;
    }
  }
#endif
  return scroll;
}

void setOptions(int argc, char *argv[])
{
  argv0=argv[0];

  cout.precision(DBL_DIG);
  
  // Make configuration and history directory
  initDir();
  
  // Build settings module.
  initSettings();
  
  // Read command-line options initially to obtain CONFIG and DIR.
  getOptions(argc,argv);
  
  resetOptions();
  
  // Read user configuration file.
  setPath();
  doConfig(getSetting<string>("config"));
  
  // Read command-line options again to override configuration file defaults.
  getOptions(argc,argv);
  
#ifdef USEGC
  if(verbose == 0 && !getSetting<bool>("debug")) GC_set_warn_proc(no_GCwarn);
#endif  

  if(setlocale (LC_ALL, "") == NULL && getSetting<bool>("debug"))
    perror("setlocale");
  
  // Set variables for the file arguments.
  argCount = argc - optind;
  argList = argv + optind;

  // Recompute search path.
  setPath();
  
  if(getSetting<double>("paperwidth") != 0.0 && 
     getSetting<double>("paperheight") != 0.0)
    Setting("papertype")=string("");
  
  SetPageDimensions();
  
  setInteractive();
}

}
