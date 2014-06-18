#include <ctime>
#include <cmath>
#include <limits>
#include <iostream>
#include "uksat.hpp"

uksat::Solver::Solver(CnfFormula& _formula)
    : formula(_formula)
    , clockdeltamax(0)
    , nconflicts(0)
    , clockbegin(0)
    , clockend(0)
    , timeout(false)
    , logstream(NULL)
    , logall(false)
{
}


uksat::Solver::~Solver() {
}


// Getters and Setters


int
uksat::Solver::getsat() const {
    return isfinished() ? result : 0;
}


uksat::PartialMap&
uksat::Solver::getpartial() {
    return partial;
}


uksat::CnfFormula&
uksat::Solver::getformula() {
    return formula;
}


bool
uksat::Solver::isvalid() const {
    return formula.isvalid();
}


bool
uksat::Solver::issatisfied() const {
    return isfinished() && result > 0;
}


bool
uksat::Solver::isconflicting() const {
    return isfinished() && result < 0;
}


bool
uksat::Solver::isstarted() const {
    return !!clockbegin;
}


bool
uksat::Solver::isfinished() const {
    return !!clockend;
}


void
uksat::Solver::setlogstream(std::ostream& stream) {
    logstream = &stream;
}


void uksat::Solver::addlogtype(const LogType& type) {
    if (type.id == LOG_ALL) {
        logall = true;
    } else if (type.meta) {
        for (std::size_t idx = 0; idx <= LOG__MAX; idx++) {
            const LogType& subtype = LogTypes[idx];
            if (subtype.parentid == subtype.id) logtypes.insert(subtype.id);
        }
    } else {
        logtypes.insert(type.id);
    }
}


void uksat::Solver::eraselogtype(const LogType& type) {
    if (type.id == LOG_ALL) {
        logall = false;
    } else if (type.meta) {
        std::set<LogTypeId>::iterator it = logtypes.begin();
        while (it != logtypes.end()) {
            const LogType& subtype = LogTypes[*it];
            if (type.id == subtype.parentid) {
                logtypes.erase(it);
                it = logtypes.end();
            }
            it++;
        }
    } else {
        logtypes.erase(type.id);
    }
}


bool uksat::Solver::isloggedtype(const LogType& type) const {
    bool logged = logall;
    if (!logged) {
        if (type.id == LOG_ALL) {
            logged = logall;
        } else if (type.meta) {
            for (std::size_t idx = 0; idx <= LOG__MAX && !logged; idx++) {
                const LogType& subtype = LogTypes[idx];
                if (subtype.parentid == type.id) logged = true;
            }
        } else {
            logged = logtypes.count(type.id) > 0;
        }
    }
    return logged;
}

    
bool
uksat::Solver::hastimeout() const {
    return timeout;
}


double uksat::Solver::getelapsedtime() const {
    double clockspersec = static_cast<double>(CLOCKS_PER_SEC);
    return clockbegin
    ? clockend
        ? ( static_cast<double>(clockend - clockbegin) / clockspersec )
        : ( static_cast<double>(std::clock() - clockbegin) / clockspersec)
    : 0.0;
}


double
uksat::Solver::getmaxtime() const {
    return static_cast<double>(clockdeltamax) / static_cast<double>(CLOCKS_PER_SEC);
}


void
uksat::Solver::setmaxtime(double secs) {
    if (!isstarted() && secs > 0) {
        double ceilsec = std::ceil(secs);
        ceilsec *= CLOCKS_PER_SEC;

        if (ceilsec <= std::numeric_limits<clock_t>::max()) {
            clockdeltamax = static_cast<clock_t>(ceilsec);
        }
    }
}


bool
uksat::Solver::intime() {
    bool stillintime = false;
    if (!timeout) {
        if (!clockdeltamax) {
            stillintime = true;
            
        } else if (clockbegin) {
            std::clock_t clockdelta = 0;
            
            if (!clockend)
                clockdelta = std::clock() - clockbegin;
            else
                clockdelta = clockend - clockbegin;
            
            stillintime = clockdelta <= clockdeltamax;
            
            if (!stillintime) {
                uksat_LOG_(LOG_TIMEOUT, "clockdelta = " << clockdelta << ", clockdeltamax = " << clockdeltamax);
                timeout = true;
            }
        }
    }
    return stillintime;
}


// Actions


void
uksat::Solver::clear() {
    partial.setnvars(formula.getnvars());
    clockbegin = clockend = nconflicts = 0;
    timeout = false;
}


int 
uksat::Solver::apply() {
    int truth = 1;
    for (std::size_t clauseidx = 0
            ; clauseidx < formula.getnclauses() && truth >= 0
            ; clauseidx++) {
        int clausetruth = checkclause(clauseidx);
        if (clausetruth > 1) clausetruth = 0; // Propagations doesn't happen in a simple apply
        if (clausetruth <= 0) {
            truth = clausetruth;
        }
    }
    return truth;
}


void
uksat::Solver::start() {
    if (formula.isvalid()) {
        clockbegin = std::clock();
        partial.setnvars(formula.getnvars());
    }
}


void
uksat::Solver::finish(int truth) {
    clockend = std::clock();
    result = truth;
    if (truth < 0) nconflicts++;
    uksat_LOG_(LOG_FINISH,
        "sat = " << result
        << ", nconflicts = " << nconflicts
        << ", issatisfied = " << issatisfied()
        << ", isconflicting = " << isconflicting());
}


int
uksat::Solver::checkclause(std::size_t clauseidx) {
    int clausesat = -1;
    int undefvar = 0;
    int nunsats = 0;
    
    std::vector<int>::const_iterator iv = formula[clauseidx].begin();
    while ((clausesat <= 0) && iv != formula[clauseidx].end()) {
        int var = *iv;
        int vartruth = partial.sat(var);
        
        if (!vartruth) {
            nunsats++;
            
            if (undefvar) {
                clausesat = 0;
                undefvar = 0;
            } else
                undefvar = var;
            
        } else if (vartruth > 0) {
            clausesat = 1;
            undefvar = 0;
        }
        
        iv++;
    }
    
    if (nunsats == 1 && undefvar) clausesat = 2;
    return clausesat;
}


int
uksat::Solver::choosefreevar() {
    int var = 0;
    
    if (partial.size() < formula.getnvars()) {
        //for (int v = 1; v <= formula.getnvars(); v++) {
        for (std::vector<int>::const_iterator it = formula.getvarorder().begin(); it != formula.getvarorder().end(); ++it) {
            // TODO: In the future, devise a way to denote whether the reverse
            // polarity of a variable was used, so that the algorithm can start
            // with negated values as well.
            int v = *it;
            if (!partial.isassigned(v)) {
                var = v;
                break;
            }
        }
    }
    
    return var;
}


std::ostream&
uksat::Solver::log(LogTypeId ntype) {
    std::ostream* logger = logstream ? logstream : &nullstream;
    //std::cerr << "log() LOGSTREAM: " << std::hex << logger << ", NULLSTREAM: " << std::hex << &nullstream << std::endl;
    const LogType& type = LogTypes[ntype];
    *logger << type;
    return *logger;
}

 std::ostream& operator<<(std::ostream& os, const uksat::LogType& entry) {
    //std::cerr << "operator<<() LOGSTREAM: " << std::hex << &os << ", NULLSTREAM: " << std::hex << &uksat::nullstream << std::endl;
    if (entry.id == uksat::LOG_END) {
        os << std::endl;
        os.flush();
        
    } else if (!entry.meta) {
        const uksat::LogType& type = uksat::LogTypes[entry.id];
        if (type.header != NULL) os << type.header;
    }
    return os;
}
 