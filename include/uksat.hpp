#ifndef UKSAT_HPP_
#define	UKSAT_HPP_


#include <cstddef>
#include <ctime>
#include <vector>
#include <map>
#include <set>
#include <iostream>

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

namespace uksat {

struct PartialMap;
class CnfFormula;
    
struct Solver {
    virtual ~Solver() {}
	virtual int getsat() const = 0;
    virtual PartialMap& getpartial() = 0;
    virtual CnfFormula& getformula() = 0;
    virtual bool isvalid() const = 0;
	virtual bool issatisfied() const = 0;
	virtual bool isconflicting() const = 0;
	virtual bool isstarted() const = 0;
	virtual bool isfinished() const = 0;
    virtual bool hastimeout() const = 0;
    virtual bool intime() = 0;
    virtual double getelapsedtime() = 0;
    virtual double getmaxtime() const = 0;
    virtual void setmaxtime(double secs) = 0;
	
    virtual void clear() = 0;
	virtual bool query() = 0;
	virtual int apply() = 0;
};

class CnfFormula {
public:
	CnfFormula();
	CnfFormula(unsigned int numvars, std::vector<std::vector<int> >& clist);

	int getnvars() const;
	int getnclauses() const;
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
};


struct PartialMap {
    struct Entry {
        int val;
        std::set<std::size_t> tclauses;
        std::set<std::size_t> fclauses;
        Entry() : val(0) {}
        Entry(const Entry& other) : val(other.val), tclauses(other.tclauses), fclauses(other.fclauses) { }
    };
    
    PartialMap();
    PartialMap(int nvars);
    
    int getnvars() const;
    void setnvars(int nvars);
    std::set<std::size_t>& getwatches(int var);
    int size() const;
    void clear();
    void assign(int var, int truth);
    void assign(int var, bool truth);
    void assign(int var);
    void unassign(int var);
    inline int normalizelit(int var) const;
    bool isassigned(int var) const;
    bool istrue(int var) const;
    bool isfalse(int var) const;
    bool get(int var) const;
    int sat(int var) const;
    void copy(std::map<int, bool>& other) const;

    std::vector<Entry> map;
    std::size_t mapsize;
};

class DpllSolver : public Solver {
public:
    DpllSolver(CnfFormula& _cnf, bool watchinglits = true, std::clock_t _maxtime = 0);
    
public:
	int getsat() const;
    PartialMap& getpartial();
    CnfFormula& getformula();
    bool isvalid() const;
	bool issatisfied() const;
	bool isconflicting() const;
	bool isstarted() const;
	bool isfinished() const;
    bool hastimeout() const;
    bool intime();
    double getelapsedtime();
    bool iswatchinglits() const;
    void setwatchinglits(bool watching = true);
    double getmaxtime() const;
    void setmaxtime(double secs);
	
public:
    void clear();
    void start();
    void finish(int truth = 0, bool restart = false);
	bool query();
	bool querystep();
    void printdecisions(std::ostream& os);

	// Formula checking
	int propagate();
    int propagatewatch();
    int deducesimple();
    int deducewatches();
    int choosefreevar();
	int apply();
	int checkclause(std::size_t clauseidx, std::pair<int, int>& watches);

	// Backtracking
    bool backtrack();
	int decide();
	void push(int var, bool decision = false);
	int pop();
    int currvar();
    int currtime();

	// Watched Literals
    std::pair<int, int> findwatchvars(std::size_t clauseidx, int knownvar = 0);
    void watch(std::size_t clauseidx, int var, int pos = 0);
	void trigger(int var);
    void untrigger(int var);
    bool isvalidtime(int time);
    void storeclause(std::size_t clauseidx);
    void forgetclause(std::size_t clauseidx);
    
private:
    // Options
    CnfFormula& formula;
    std::clock_t clockdeltamax;
    bool watchinglits;
    
    // Results
    int result;
    bool timeout;
    PartialMap partial;
    std::size_t nconflicts;
    
    // Stack
    std::vector<int> decisions;
    std::vector<std::pair<int, int> > propagations;
    
    // Watched Literals
    std::set<int> pushes;
    std::vector<std::pair<int, int> > cwatches; // time when true, pair of watches
    std::size_t nsatclauses;
    std::vector<int> clausesats;
    
    // 
    std::size_t ncalls;
    std::clock_t clockbegin;
    std::clock_t clockend;
};

    
};

#endif	/* UKSAT_HPP */


