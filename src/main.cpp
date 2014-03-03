#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <cstdlib>
#include <ctime>
#include <locale>
#include "uksat.hpp"
#include "ezOptionParser.hpp"

// Exit codes

#define RETOK      0
#define RETERR     1
#define RETEARG    2
#define RETSAT     10
#define RETUNSAT   20
#define RETUNDEF   30
#define RETTIMEOUT 40

// For the option parsing
#define EXAMPLETEXT  "Simplest usage: `" uksat_NAME " in.cnf`"
#define OVERVIEWTEXT "A minimalistic SAT WatchedDpllSolver."
#define USAGETEXT     uksat_NAME " [OPTIONS] <INPUT> <OUTPUT>\n" \
	"If <INPUT> equals \"-\", STDIN is used.\n" \
	"If <OUTPUT> equals \"-\", STDOUT is used."

#define HEADERTEXT \
	VERSIONTEXT "\n" \
	OVERVIEWTEXT

#define FOOTERTEXT "\n\n" uksat_NAME " is provided without warranty blabla (and I really need to put a proper license on this).\n"

#define VERSIONTEXT \
	uksat_RELEASENAME

#define HELPTEXT \
	HEADERTEXT "\n" \
	"UK because of UKC. Go google it, you'll understand (maybe?).\n" \
	"Author: Flavio Lisboa <fl81@kent.ac.uk>"

enum EOption {
	  NONE
    , HELP = NONE // -h      Prints the help contents
	, VERSION     // --version Prints version information
	, VERBOSE     // -v      Activates verbose mode
	, DEBUG       // -vv     Activates debug mode
	, RANDSEED    // -r=NUM  Sets the random seed
	, SEQVAR      // -R      Activates sequential variable selection
	, PRINTFML    // -p      Prints the formula
	, WATCHLIT    // -w      Activates watched literals
	, NOWATCHLIT  // -W      Deactivates watched literals
	, NOSOLVE     // -A      Don't try to solve the formula
    , TIMELIMIT   // -t=NUM  Specifies the time limit, in seconds
    , PRINTMAP    // -m      Prints the partial map (always before the formula)
    , SOLFILE     // -s=FILE Provides a solution file
};

struct OptionDescriptor {
	EOption option;
	bool required;
	int numargs;
	char argsep;
	const char* validator;
	const char* shortname;
	const char* longname;
	const char* defaultval;
	const char* helptext;

	const char* getname() const { return longname ? longname : shortname; }
};

const OptionDescriptor descriptors[] = {
// It's EXTREMELY IMPORTANT that the order of declarations corresponds to the one in the enum EOption
/*{ TYPE,       REQUIRED, NUM, ARGSEP, VALIDT, SHORT, LONG,         DEFVAL, HELP }*/
  { HELP,       false,    0,   '\0',   NULL,   "-h",  "--help",     "",     "Show this help content." }
, { VERSION,    false,    0,   '\0',   NULL,   NULL,  "--version",  "",     "Shows version information." }
, { VERBOSE,    false,    0,   '\0',   NULL,   "-v",  "--verbose",  "",     "Sets verbose mode (Verbosity = 1)."}
, { DEBUG,      false,    0,   '\0',   NULL,   "-vv", "--debug",    "",     "Sets debug mode (Verbosity = 2)."}
, { RANDSEED,   false,    1,   '\0',   "d",    "-r",  "--rseed",    "",     "Sets the random seed to NUM."}
, { SEQVAR,     false,    0,   '\0',   NULL,   "-R",  "--no-rand",  "",     "Deactivates random var selection (sequential selection)."}
, { PRINTFML,   false,    0,   '\0',   NULL,   "-p",  "--printfml", "",     "Prints the formula in the output file (after the results)."}
, { WATCHLIT,   false,    0,   '\0',   NULL,   "-w",  "--watch",    "",     "Activates watched literals." }
, { NOWATCHLIT, false,    0,   '\0',   NULL,   "-W",  "--no-watch", "",     "Deactivates watched literals."}
, { NOSOLVE,    false,    0,   '\0',   NULL,   "-A",  "--no-solve", "",     "Don't solve the formula. Useful with `-p`."}
, { TIMELIMIT,  false,    1,   '\0',   NULL,   "-t",  "--maxtime",  "",     "Sets the maximum execution time, in seconds."}
, { PRINTMAP,   false,    0,   '\0',   NULL,   "-m",  "--printmap", "",     "Prints the partial map."}
, { SOLFILE,    false,    1,   '\0',   NULL,   "-s",  "--solfile",  "",     "Specifies a solution file to open."}
, { NONE,       false,    0,   '\0',   NULL,   NULL,  NULL,         NULL,   NULL }
};

// Option state

struct ArgState {
	ez::ezOptionParser optparser;
	int exitcode;
    std::string resultstr;
	int verboselvl;
	bool choosingseq;
	double rseed;
	bool printingfml;
	bool watchinglits;
	bool solvingfml;
    bool printingmap;
    int maxtime;
	std::string inputname;
	std::string outputname;
	std::string solname;

	ArgState()
		: exitcode(RETOK)
		, verboselvl(NONE) // 0: Silent, 1: Verbose (Info), 2: Debug (Trace operations)
		, choosingseq(false)
		, rseed(0)
		, printingfml(false)
		, watchinglits(true)
		, solvingfml(true)
        , printingmap(false)
        , maxtime(0)
	{}

	int isset(EOption option) { return optparser.isSet(descriptors[option].getname()); }
	ez::OptionGroup* get(EOption option) { return optparser.get(descriptors[option].getname()); }
	bool isdebug() { return verboselvl == DEBUG; }
	bool isverbose() { return isdebug() || verboselvl == VERBOSE; }
};

static int evalresult(ArgState& arg, uksat::Solver& solver);
static void printhelp(ArgState& arg);
static void printversion(ArgState& arg);
static void setupopts(ArgState& arg);
static bool checkargs(ArgState& arg);
static void printsummary(ArgState& arg, uksat::CnfFormula& cnf, uksat::Solver& partial);
static void printresults(ArgState& arg, uksat::CnfFormula& cnf, uksat::Solver& partial);

int main(int argc, const char** argv) {
	ArgState arg;
	setupopts(arg);
	arg.optparser.parse(argc, argv);
    
    std::set_terminate(__gnu_cxx::__verbose_terminate_handler);
    
	if ( arg.isset(HELP) ) {
		printhelp(arg);
		return RETOK;

	} else if ( arg.isset(VERSION) ) {
		printversion(arg);
		return RETOK;
	}

	if (checkargs(arg)) {
		bool keepgoing = true;
        int sat = 0;
		uksat::CnfFormula cnf;
        uksat::SimpleDpllSolver simplesolver(cnf);
        uksat::WatchedDpllSolver watchedsolver(cnf);
        uksat::Solver& solver = arg.watchinglits ? watchedsolver : simplesolver;
        
		// Setting configuration
        if (arg.maxtime) solver.setmaxtime(static_cast<double>(arg.maxtime));
        if (arg.isverbose()) solver.setlogstream(std::cerr);
        if (arg.isdebug()) solver.addlogtype(uksat::LOG_ALL);
        
		// Input and output
		std::istream* is = NULL;
		std::ostream* os = NULL;
        std::istream* ss = NULL;
		std::stringstream ibuf;
		std::stringstream sbuf;
		std::ifstream ifile;
		std::ofstream ofile;
        std::ifstream sfile;

		// Opening input file
		if (arg.inputname.compare("-") == 0) {
			std::string buf;
			is = &ibuf;
			while (std::getline(std::cin, buf)) {
				ibuf << buf << std::endl;
			}

		} else {
			is = &ifile;
			ifile.open(arg.inputname.c_str());

			if (!ifile.is_open()) {
				std::cerr << "ERROR: Could not open input file \"" << arg.inputname << "\"." << std::endl;
				arg.exitcode = RETERR;
				keepgoing = false;
			}
		}

		// Opening output file
		if (arg.outputname.compare("-") == 0) {
			os = &std::cout;

		} else if (!arg.outputname.empty()) {
			os = &ofile;
			ofile.open(arg.outputname.c_str());

			if (!ofile.is_open()) {
				std::cerr << "ERROR: Could not open output file \"" << arg.outputname << "\"." << std::endl;
				arg.exitcode = RETERR;
				keepgoing = false;
			}
		}
        
        // Opening solution file
        if (keepgoing && !arg.solname.empty()) {
            if (arg.inputname.compare("-") == 0) {
                std::string buf;
                is = &sbuf;
                while (std::getline(std::cin, buf)) {
                    sbuf << buf << std::endl;
                }
                
            } else {
                sfile.open(arg.solname.c_str());

                if (!sfile.is_open()) {
                    std::cerr << "ERROR: Could not open solution file \"" << arg.solname << "\"." << std::endl;
                    arg.exitcode = RETERR;
                    keepgoing = false;
                }
            }
		}
        
		// Loading formula
		if (keepgoing && !cnf.openfile(*is)) {
			std::cerr << "ERROR: Could not load the formula, or the formula is invalid." << std::endl;
			keepgoing = false;
		}

		// Writing summary of current run and solving formula
		if (keepgoing) {
			printsummary(arg, cnf, solver);
			if (arg.solvingfml) {
                solver.query();
                sat = solver.issatisfied() ? 1 : (solver.isconflicting() ? -1 : 0);
                
            } else {
                sat = solver.apply();
            }
		}

		// Writing results to the output file, if asked to
		if (keepgoing && (arg.printingmap || arg.printingfml) && os) {
			const std::time_put<char>& tmput = std::use_facet <std::time_put<char> > (os->getloc());
			std::time_t timestamp;
		    std::time ( &timestamp );
		    std::tm * now = std::localtime ( &timestamp );
		    std::string pattern ("c Solved with " uksat_RELEASENAME ", date: %I:%M%p\n");
		    tmput.put(*os, *os, ' ', now, pattern.data(), pattern.data() + pattern.length());
            
            if (arg.printingmap) {
                cnf.savesolution(*os, solver);
			}
            
			if (arg.printingfml) {
				cnf.savefile(*os);
			}
		}

		// Printing summary and setting final result
		if (keepgoing) {
            evalresult(arg, solver);
			printresults(arg, cnf, solver);
		}

		// Closing files, if needed
		if (ifile.is_open()) ifile.close();
		if (ofile.is_open()) ofile.close();
		if (sfile.is_open()) sfile.close();
	}

	return arg.exitcode;
}

void setupopts(ArgState& arg) {
	arg.optparser.overview = HELPTEXT;
	arg.optparser.syntax = USAGETEXT;
	arg.optparser.example = EXAMPLETEXT;
	arg.optparser.footer = FOOTERTEXT;

	const OptionDescriptor* desc = descriptors;
	while (desc->option || (desc->shortname || desc->longname)) {
		ez::ezOptionValidator* validator = desc->validator ? new ez::ezOptionValidator(desc->validator) : NULL;

		if (desc->shortname && desc->longname) {
			arg.optparser.add(
				desc->defaultval
				, desc->required
				, desc->numargs
				, desc->argsep
				, desc->helptext
				, desc->shortname
				, desc->longname
				, validator
			);
		} else {
			arg.optparser.add(
				desc->defaultval
				, desc->required
				, desc->numargs
				, desc->argsep
				, desc->helptext
				, desc->shortname ? desc->shortname : desc->longname
				, validator
			);
		}

		desc++;
	}
}

bool checkargs(ArgState& arg) {
	bool ret = true;
	std::vector<std::string> badOptions;
	std::vector<std::string> badArgs;

	if (arg.optparser.lastArgs.size() < 1) {
		std::cerr << "ERROR: Input file not given." << std::endl;
		std::cerr << "Check `" << uksat_NAME << " -h` for help." << std::endl;
		ret = false;
	}

	if (ret && !arg.optparser.gotRequired(badOptions)) {
		for(int i=0; i < badOptions.size(); ++i) {
			std::cerr << "ERROR: Missing required option " << badOptions[i] << ".\n";
		}
		std::cerr << "Check `" << uksat_NAME << " -h` for help." << std::endl;
		ret = false;
	}

	if (ret && !arg.optparser.gotExpected(badOptions)) {
		for(int i=0; i < badOptions.size(); ++i) {
			std::cerr << "ERROR: Wrong arguments for option " << badOptions[i] << ".\n";
		}
		std::cerr << "Check `" << uksat_NAME << " -h` for help." << std::endl;
		ret = false;
	}

	if (ret && !arg.optparser.gotValid(badOptions, badArgs)) {
		for(int i=0; i < badOptions.size(); ++i) {
			std::cerr << "ERROR: Got invalid argument \"" << badArgs[i] << "\" for option " << badOptions[i] << "." << std::endl;
		}
		std::cerr << "Check `" << uksat_NAME << " -h` for help." << std::endl;
		ret = false;
	}

	if (ret) {
		if (arg.isset(VERBOSE)) {
			arg.verboselvl = VERBOSE;
		}

		if (arg.isset(DEBUG)) {
			arg.verboselvl = DEBUG;
		}

		if (arg.isset(RANDSEED)) {
			double val;
			arg.get(RANDSEED)->getDouble(val);
			arg.choosingseq = false;
			arg.rseed = val;
		}

		if (arg.isset(SEQVAR)) {
			arg.rseed = 0;
			arg.choosingseq = true;
		}

		if (arg.isset(PRINTFML)) {
			arg.printingfml = true;
		}

		if (arg.isset(WATCHLIT)) {
			arg.watchinglits = true;
		}

		if (arg.isset(NOWATCHLIT)) {
			arg.watchinglits = false;
		}

		if (arg.isset(NOSOLVE)) {
			arg.solvingfml = false;
		}

		arg.inputname = *arg.optparser.lastArgs[0];
		if (arg.optparser.lastArgs.size() > 1) {
			arg.outputname = *arg.optparser.lastArgs[1];
		}
        
        if (arg.isset(TIMELIMIT)) {
            int maxtime;
            arg.get(TIMELIMIT)->getInt(maxtime);
            arg.maxtime = maxtime;
        }
        
		if (arg.isset(PRINTMAP)) {
			arg.printingmap = true;
		}
        
        if (arg.isset(SOLFILE)) {
            std::string solfilename;
            arg.get(TIMELIMIT)->getString(solfilename);
            arg.solname = solfilename;
        }

	} else {
		arg.exitcode = RETEARG;
	}

	return ret;
}


int evalresult(ArgState& arg, uksat::Solver& solver) {
    int ret = RETUNDEF;
    const char* str = "UNDEFINED";
    
    if (solver.hastimeout()) {
        ret = RETTIMEOUT;
        str = "TIMEOUT";
        
    } else if (solver.issatisfied()) {
        ret = RETSAT;
        str = "SATISFIABLE";
        
    } else if (solver.isconflicting()) {
        ret = RETUNSAT;
        str = "UNSATISFIABLE";
    }
    
    arg.exitcode = ret;
    arg.resultstr = str;
    
    return ret;
}

void printhelp(ArgState& arg) {
	std::string txt;
	arg.optparser.getUsage(txt);
	std::cerr << txt;
}

void printversion(ArgState& arg) {
	std::cerr << HELPTEXT << FOOTERTEXT << std::endl;
}

void printsummary(ArgState& arg, uksat::CnfFormula& cnf, uksat::Solver& partial) {
	if (!arg.isverbose()) return;

	std::cerr << uksat_RELEASENAME << std::endl << "OPTIONS:" << std::endl;
	std::cerr << "\tinputname: '" << (arg.inputname.compare("-") == 0 ? "<stdin>" : arg.inputname.c_str()) << "'" << std::endl;
	std::cerr << "\toutputname: '" << (arg.outputname.compare("-") == 0 ? "<stdout>" : arg.outputname.c_str()) << "'" << std::endl;
	std::cerr << "\tverbosity: '" << (arg.isdebug() ? "debug" : (arg.isverbose() ? "verbose" : "silent")) << "'" << std::endl;
	std::cerr << "\tchoosingseq: " << (arg.choosingseq ? "true" : "false") << std::endl;
	std::cerr << "\trseed: " << arg.rseed << std::endl;
	std::cerr << "\tprintingfml: " << (arg.printingfml ? "true" : "false") << std::endl;
	std::cerr << "\twatchinglits: " << (arg.watchinglits ? "true" : "false") << std::endl;
	std::cerr << "\tsolvingfml: " << (arg.solvingfml ? "true" : "false") << std::endl;
	std::cerr << "FORMULA:" << std::endl;
	std::cerr << "\tnumclauses: " << cnf.getnclauses() << std::endl;
	std::cerr << "\tnumvars: " << cnf.getnvars() << std::endl;
    std::cerr << "\tVariable Ordering: ";
    const char* sep = "";
    for (std::vector<int>::const_iterator it = cnf.getvarorder().begin(); it != cnf.getvarorder().end(); ++it) {
        int var = *it;
        int normvar = uksat_NORMALLIT(*it);
        int normfreq = cnf.frequency(normvar);
        int invfreq = cnf.frequency(-normvar);
        std::cerr << sep << var << " (" << (normfreq + invfreq) << ": " << normvar << " = " << normfreq << ", " << -normvar << " = " << invfreq << ")";
        sep = ", ";
    }
    std::cerr << std::endl << std::endl;
}

void printresults(ArgState& arg, uksat::CnfFormula& cnf, uksat::Solver& solver) {
	if (!arg.solvingfml) return;

	if (arg.isverbose()) {
		std::cerr << arg.resultstr << " " << solver.getelapsedtime()
                << " " << solver.apply() << std::endl; //partial.clockbegin << " " << partial.clockend << " " << CLOCKS_PER_SEC  << std::endl;
	} else {
		std::cerr << arg.resultstr << " " << solver.getelapsedtime()
				<< " " << solver.apply() << std::endl; //partial.clockbegin << " " << partial.clockend << " " << CLOCKS_PER_SEC << std::endl;
	}
}

