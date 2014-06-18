#ifndef UKSAT_HPP_
#define	UKSAT_HPP_


#include <cstddef>
#include <ctime>
#include <vector>
#include <map>
#include <set>
#include <iosfwd>
#include <sstream>

#define uksat_STRQT(s) #s
#define uksat_STRFY(s) uksat_STRQT(s)

#define uksat_NAME "uksat"
#define uksat_MAJORVERSION 0
#define uksat_MINORVERSION 2
#define uksat_PATCHVERSION 0
#define uksat_RELEASETYPE  "beta"

#define uksat_VERSIONNAME \
	uksat_STRFY(uksat_MAJORVERSION) \
    "." uksat_STRFY(uksat_MINORVERSION) \
    "." uksat_STRFY(uksat_PATCHVERSION) \
    "-" uksat_RELEASETYPE
#define uksat_RELEASENAME uksat_NAME " " uksat_VERSIONNAME

#define uksat_NORMALLIT(l)    (((l) < 0) ?   -(l) :   (l))
#define uksat_INVERTLIT(l)    (-(l))
#define uksat_NORMALVAL(l, v) (((l) >= 0) ? (v) : ((v < 0) ? 1 : -1))
#define uksat_BOOLVAL(l, v)   ( uksat_NORMALVAL(l, v) > 0 ? true : false )

#define uksat_IFLOG_(body) if (logstream) body
#define uksat_LOGMARK_(type) \
    uksat_IFLOG_( { if (isloggedtype(type)) log(type) << uksat::endlog; } )
#define uksat_LOG_(type, body) \
    uksat_IFLOG_( \
        if (isloggedtype(type)) \
            log(type) << body << uksat::endlog; \
        )

namespace uksat {


enum LogTypeId {
    // Defaults
    LOG_NONE = 0,
    LOG_END = 0,
    LOG_ALL,
    LOG_TEXT,
    
    // Control
    LOG_FLOW,
        LOG_STEP,
        LOG_TIMEOUT,
        LOG_FINISH,
    
    // Propagation
    LOG_PROPAG,
        LOG_PROPAG_PRE,
        LOG_PROPAG_UNIT,
        LOG_PROPAG_CLAUSE,
        LOG_PROPAG_SAT,
        
    // Stack control
    LOG_STACK,
        LOG_STACK_PUSH,
        LOG_STACK_POP_PRE,
        LOG_STACK_POP,
        LOG_STACK_POPVAR,
        
    // Map/Clause assignments
    LOG_ASSIGN,
        LOG_ASSIGN_CLAUSE,
        
    // Watched Literals
    LOG_WATCH,
        LOG_WATCH_PRE,
        LOG_WATCH_DO,
    
    LOG_TRIGGER,
        LOG_TRIGGER_PRE,
        LOG_TRIGGER_DO,
        LOG_TRIGGER_STRANGE,
        LOG_TRIGGER_UNDO_PRE,
        LOG_TRIGGER_UNDO,
        
    // Backtrack
    LOG_BACKTRACK,
        LOG_BACK_PRE,
        LOG_BACK_OK,
        LOG_BACK_FAIL,
    
    // Decisions
    LOG_DECIDE,
        LOG_DECIDE_PRE,
        LOG_DECIDE_FREEVAR,
        LOG_DECIDE_FAIL,
        
    LOG__MAX = LOG_DECIDE_FAIL
};


struct LogType {
    LogType()
        : id(LOG_END), meta(true), parentid(LOG_END), name(NULL), header(NULL) {}
    LogType(LogTypeId _type);
    LogType(LogTypeId _type, bool _outputted, LogTypeId _parent, const char* _name, const char* _header)
        : id(_type), meta(_outputted), parentid(_parent), name(_name), header(_header) {}
    LogType(const LogType& other)
        : id(other.id), meta(other.meta), parentid(other.parentid), name(other.name), header(other.header) {}
    bool operator==(const LogType& other) { return id == other.id; }
    bool operator!=(const LogType& other) { return id != other.id; }
    bool operator==(LogTypeId other) { return id == other; }
    bool operator!=(LogTypeId other) { return id != other; }
    LogTypeId id;
    LogTypeId parentid;
    const char* name;
    const char* header;
    bool meta;
};


static const LogType LogTypes[] = {
    LogType(LOG_END, true, LOG_NONE, NULL, NULL),
    LogType(LOG_ALL, true, LOG_NONE, "all", NULL),
    LogType(LOG_TEXT, false, LOG_NONE, NULL, NULL),
    
    LogType(LOG_FLOW, true, LOG_NONE, "flow", NULL),
        LogType(LOG_STEP, false, LOG_FLOW, "step", "::: STEP "),
        LogType(LOG_TIMEOUT, false, LOG_FLOW, "timeout", "!!! TIMEOUT "),
        LogType(LOG_FINISH, false, LOG_FLOW, "finish", "!!! FINISH "),
    
    LogType(LOG_PROPAG, true, LOG_NONE, "propag", NULL),
        LogType(LOG_PROPAG_PRE, false, LOG_PROPAG, "propagpre", "::: PROPAG "),
        LogType(LOG_PROPAG_UNIT, false, LOG_PROPAG, "propagunit", "    PROPAG.CLAUSEUNIT "),
        LogType(LOG_PROPAG_CLAUSE, false, LOG_PROPAG, "propagclause", "    PROPAG.CLAUSE "),
        LogType(LOG_PROPAG_SAT, false, LOG_PROPAG, "propagsat", "    PROPAG.FORMULASAT "),
    
    LogType(LOG_STACK, true, LOG_NONE, "stack", NULL),
        LogType(LOG_STACK_PUSH, false, LOG_STACK, "push", "    PUSH "),
        LogType(LOG_STACK_POP, false, LOG_STACK, "poppre", "    POP "),
        LogType(LOG_STACK_POP, false, LOG_STACK, "pop", "    POP "),
        LogType(LOG_STACK_POPVAR, false, LOG_STACK, "popvar", "    POP.VAR "),
    
    LogType(LOG_ASSIGN, true, LOG_NONE, "assign", NULL),
        LogType(LOG_ASSIGN_CLAUSE, false, LOG_ASSIGN, "assignclause", "    ASSIGN.CLAUSE "),
    
    LogType(LOG_WATCH, true, LOG_NONE, "watch", NULL),
        LogType(LOG_WATCH_PRE, false, LOG_WATCH, "watchpre", "... WATCH "),
        LogType(LOG_WATCH_DO, false, LOG_WATCH, "watchdo", "    WATCH.DO "),
    
    LogType(LOG_TRIGGER, true, LOG_NONE, "trigger", NULL),
        LogType(LOG_TRIGGER_PRE, false, LOG_TRIGGER, "triggerpre", "... TRIGGER "),
        LogType(LOG_TRIGGER_DO, false, LOG_TRIGGER, "triggered", "    TRIGGER.DO "),
        LogType(LOG_TRIGGER_STRANGE, false, LOG_TRIGGER, "triggerhuh", "    TRIGGER.HUH? "),
        LogType(LOG_TRIGGER_UNDO_PRE, false, LOG_TRIGGER, "untriggerpre", "... UNDOTRIGGER "),
        LogType(LOG_TRIGGER_UNDO, false, LOG_TRIGGER, "untrigger", "    UNDOTRIGGER "),
    
    LogType(LOG_BACKTRACK, true, LOG_NONE, "back", NULL),
        LogType(LOG_BACK_PRE, false, LOG_BACKTRACK, "backpre", "::: BACKTRACK "),
        LogType(LOG_BACK_OK, false, LOG_BACKTRACK, "backing", "    BACKTRACK.OK "),
        LogType(LOG_BACK_FAIL, false, LOG_BACKTRACK, "backfail", "!!! BACKTRACK.FAIL "),
    
    LogType(LOG_DECIDE, true, LOG_NONE, "decide", NULL),
        LogType(LOG_DECIDE_PRE, false, LOG_DECIDE, "decidepre", "::: DECIDE "),
        LogType(LOG_DECIDE_FREEVAR, false, LOG_DECIDE, "decidefree", "    DECIDE.FREEVAR "),
        LogType(LOG_DECIDE_FAIL, false, LOG_DECIDE, "decidefail", "!!!  DECIDE.FAIL "),
    
};


static const LogType& endlog = LogTypes[LOG_END];

static std::map<const char*, const LogType*>& maplogtypes(std::map<const char*, const LogType*>& map) {
    for (std::size_t idx = 0; idx < LOG__MAX; idx++) {
        const LogType& type = LogTypes[idx];
        if (type.name != NULL) map[type.name] = &type;
    }
    return map;
}


struct NullStream : public std::stringstream {
    NullStream() { setstate(std::ios_base::badbit); }
};
static NullStream nullstream;

struct PartialMap;
class CnfFormula;

class Solver;

class CnfFormula {
public:
	CnfFormula();
	CnfFormula(unsigned int numvars, std::vector<std::vector<int> >& clist);

	int getnvars() const;
	int getnclauses() const;
    int totalfrequency(int normvar) const;
    int frequency(int var) const;
    const std::vector<int>& getvarorder() const;
	const std::vector<int>& operator[](int clauseidx) const;

	bool isvalid() const;
	bool openfile(const char* filename);
	bool openfile(std::istream& is);
	bool savefile(const char* filename);
	bool savefile(std::ostream& os);
    bool savesolution(const char* filename, Solver& solver);
    bool savesolution(std::ostream& os, Solver& solver);
private:
	int nvars;
	int nclauses;
	std::vector<std::vector<int> > clauses;
    std::map<int, int> varfrequencies;
    std::vector<int> varorder;
};


struct PartialMap {
    struct Entry {
        int truth;
        int truthtime;
        std::set<std::size_t> tclauses;
        std::set<std::size_t> fclauses;
        Entry() : truth(0), truthtime(0) {}
        Entry(const Entry& other) : truth(other.truth), truthtime(other.truthtime), tclauses(other.tclauses), fclauses(other.fclauses) { }
    };
    
    // Constructors
    PartialMap();
    PartialMap(const PartialMap& other);
    explicit PartialMap(int nvars);
    
    // Getters / Setters
    int getnvars() const;
    void setnvars(int nvars);
    std::set<std::size_t>& getwatches(int var);
    int size() const;

    // Operations
    void clear();
    void assign(int var, int truth, int time = 0);
    void assign(int var, bool truth, int time = 0);
    void push(int var, int time = 0);
    void unassign(int var);
    int  normalizelit(int var) const;
    bool isassigned(int var) const;
    bool istrue(int var) const;
    bool isfalse(int var) const;
    int gettime(int normvar);
    bool get(int var) const;
    int  sat(int var) const;
    void copy(std::map<int, bool>& other) const;

    // Fields
    std::vector<Entry> map;
    std::size_t mapsize;
};


class Solver {
public:
	Solver(CnfFormula& _formula);
	virtual ~Solver();

	// Getters and Setters
	virtual int getsat() const;
    virtual PartialMap& getpartial();
    virtual CnfFormula& getformula();
    virtual void setlogstream(std::ostream& stream);
    virtual void addlogtype(const LogType& type);
    virtual void eraselogtype(const LogType& type);
    virtual bool isloggedtype(const LogType& type) const;
    virtual bool isvalid() const;
	virtual bool issatisfied() const;
	virtual bool isconflicting() const;
	virtual bool isstarted() const;
	virtual bool isfinished() const;
    virtual bool hastimeout() const;
    virtual double getelapsedtime() const;
    virtual double getmaxtime() const;
    virtual void setmaxtime(double secs);
    virtual bool intime();
	
	// Actions
    virtual void clear();
	virtual int  apply();
	virtual int  checkclause(std::size_t clauseidx);

	// The query function
	virtual bool query() = 0;

protected:
    // Helpers
    virtual void start();
    virtual void finish(int truth = 0);
    virtual int choosefreevar();
    virtual std::ostream& log(LogTypeId type = LOG_TEXT);

protected:
	// Input and Options
	CnfFormula& formula;
	std::clock_t clockdeltamax;
    std::ostream* logstream;
    bool logall;
    std::set<LogTypeId> logtypes;

	// Results
	int result;
	PartialMap partial;
    std::size_t nconflicts;

    // Execution and Time Control
	std::clock_t clockbegin;
	std::clock_t clockend;
	bool timeout;
};


class SimpleDpllSolver : public Solver {
public:
    SimpleDpllSolver(CnfFormula& _formula);
	virtual bool query();
    
protected:
    virtual int currtime();
    virtual int currvar();
    
    virtual void clear();
	virtual bool querystep();
	virtual void propagate();
	virtual int  decide();
	virtual int  backtrack();
	virtual void push(int var, bool decision = false);
	virtual void push(const std::pair<int, bool>& decision);
    virtual std::pair<int, bool> pop();
	virtual std::pair<int, bool> pop(std::vector<int>& poppedvars);
    virtual int propagateclause(std::size_t clauseidx);
    virtual void printdecisions();

protected:
    std::size_t ncalls;
    
    // Stack
	std::vector<std::pair<int, bool> > decisions;
	std::vector<std::pair<int, int> > propagations;
};


class WatchedDpllSolver: public SimpleDpllSolver {
public:
	struct ClauseState {
        bool satisfied;
        int sattime;
        std::pair<int, int> watches;
        ClauseState() : satisfied(false), sattime(0) {}
        ClauseState(const ClauseState& other) : satisfied(other.satisfied), sattime(other.sattime), watches(other.watches) {}
    };
    WatchedDpllSolver(CnfFormula& _formula);
    
protected:
    virtual void clear();
    virtual void start();
	virtual void propagate();
    
	virtual void push(int var, bool decision = false);
	virtual void push(const std::pair<int, bool>& decision);
	virtual std::pair<int, bool> pop(std::vector<int>& poppedvars);
    
    virtual int registerwatches();
    virtual int findwatchvar(std::size_t clauseidx, int knownvar = 0);
    virtual std::pair<int, int> findwatchvars(std::size_t clauseidx, int knownvar = 0);
    
    virtual void watch(std::size_t clauseidx, int var, int substvar = 0);
    virtual void trigger(int var);
    virtual void undotrigger(int var);
    
    virtual bool isvalidtime(int assigntime);
    virtual bool isvalidclausesat(std::size_t clauseidx);
    virtual void setclausesat(std::size_t clauseidx, int time = 0);
    virtual void eraseclausesat(std::size_t clauseidx);
    virtual std::pair<int, int>& getclausewatches(std::size_t clauseidx);
    virtual std::set<std::size_t>& getwatchset(int var);
    virtual std::set<std::size_t>& getinvwatchset(int var);
    
protected:
    // Watches
    std::vector<ClauseState> cstates;
    std::size_t nsatclauses;
};

};


std::ostream& operator<<(std::ostream& os, const uksat::LogType& entry);


#endif	/* UKSAT_HPP */


