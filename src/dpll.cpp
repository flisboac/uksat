#include <cstdlib>
#include <ctime>
#include <cmath>
#include <limits>
#include <set>
#include <vector>
#include <map>
#include <list>

#include "uksat.hpp"

uksat::DpllSolver::DpllSolver(CnfFormula& _cnf, bool _watchinglits, std::clock_t _maxtime)
: formula(_cnf)
, clockdeltamax(_maxtime)
, watchinglits(_watchinglits)
, result(0)
, nconflicts(0)
, nsatclauses(0)
, ncalls(0)
, clockbegin(0)
, clockend(0)
, timeout(false)
{

}


/*------------------------------------------------------------------------------
 * [ Getters/Setters ]
 */


bool uksat::DpllSolver::isvalid() const {
    return formula.isvalid();
}


int uksat::DpllSolver::getsat() const {
    return isfinished() ? result : 0;
}


bool uksat::DpllSolver::issatisfied() const {
    return isfinished() && result > 0;
}


bool uksat::DpllSolver::isconflicting() const {
    return isfinished() && result < 0;
}


bool uksat::DpllSolver::isstarted() const {
    return !!clockbegin;
}


bool uksat::DpllSolver::isfinished() const {
    return !!clockend;
}


bool uksat::DpllSolver::hastimeout() const {
    return timeout;
}


bool uksat::DpllSolver::intime() {
    bool stillintime = false;
    
    if (!timeout) {
        if (!clockdeltamax) {
            stillintime = true;

        } else if (clockbegin) {
            std::clock_t clockdelta = 0;

            if (!clockend) {
                clockdelta = std::clock() - clockbegin;

            } else {
                clockdelta = clockend - clockbegin;
            }

            stillintime = clockdelta <= clockdeltamax;
        }
    }
    
//    if (stillintime) {
//        std::cerr << "!!!TIMEOUT!!!" << std::endl;
//    }
    
    return stillintime;
}


uksat::CnfFormula& uksat::DpllSolver::getformula() {
    return formula;
}


uksat::PartialMap& uksat::DpllSolver::getpartial() {
    return partial;
}


bool uksat::DpllSolver::iswatchinglits() const {
    return watchinglits;
}


void uksat::DpllSolver::setwatchinglits(bool watching) {
    if (!isstarted()) {
        watchinglits = watching;
    }
}


void uksat::DpllSolver::setmaxtime(double secs) {
    if (!isstarted() && secs > 0) {
		double ceilsec = std::ceil(secs);
		ceilsec *= CLOCKS_PER_SEC;
		
		if (ceilsec <= std::numeric_limits<clock_t>::max()) {
			clockdeltamax = static_cast<clock_t>(ceilsec);
		}
    }
}


double uksat::DpllSolver::getmaxtime() const {
    return static_cast<double>(clockdeltamax) / static_cast<double>(CLOCKS_PER_SEC);
}


double uksat::DpllSolver::getelapsedtime() {
    double clockspersec = static_cast<double>(CLOCKS_PER_SEC);
	return clockbegin
		? clockend
			? ( static_cast<double>(clockend - clockbegin) / clockspersec )
			: ( static_cast<double>(std::clock() - clockbegin) / clockspersec)
		: 0.0;
}


/*------------------------------------------------------------------------------
 * [ Functions ]
 */


void uksat::DpllSolver::clear() {
    result = 0;
    partial.clear();
    nsatclauses = 0;
    decisions.clear();
    propagations.clear();
    clausesats.clear();
    pushes.clear();
    ncalls = 0;
    clockbegin = 0;
    clockend = 0;
    nconflicts = 0;
}



/*------------------------------------------------------------------------------
 * [ Solving ]
 */


bool uksat::DpllSolver::query() {
    //std::cerr << "!!!START!!!" << std::endl;
    start();
    while (querystep());
    return issatisfied();
}


bool uksat::DpllSolver::querystep() {
    bool keepgoing = true;
    
    if (isstarted() && intime()) {
        ncalls++;
        //printdecisions(std::cerr);
        propagate();

        if (issatisfied()) {
            keepgoing = false;

        } else if (isconflicting()) {
            //std::cerr << "!!!CONFLICTING!!!" << std::endl;
            if (!backtrack()) {
                keepgoing = false;
                //std::cerr << "!!!NO BACKTRACK!!!" << std::endl;
            }

        } else {
            int nextvar = decide();

            if (!nextvar) {
                //std::cerr << "!!!NO DECIDE!!!" << std::endl;
                finish(watchinglits ? -1 : 0);
                keepgoing = false;

            }
        }
            
    } else {
        keepgoing = false;
    }
    
    return keepgoing;
}


int uksat::DpllSolver::propagate() {
    //return 0;
    int sat = 0;
    
    if (watchinglits) {
        if (ncalls == 1) {
            sat = deducewatches();

        } else if (nsatclauses && nsatclauses == formula.getnclauses()) {
            sat = 1;

        } else if (isconflicting()) {
            pushes.clear();
            sat = -1;
            
        }  else if (!intime()) {
            finish(0);
            
        } else {
            //std::cerr << "INSERTVARS" << std::endl;
//            std::set<int> pushset(pushes);
//            pushes.clear();
//            for (std::set<int>::iterator it = pushset.begin(); it != pushset.end(); it++) {
//                //std::cerr << "INSERT var = " << *it << std::endl;
//                push(*it);
////                //pushset.erase(it);
//            }
        }
        
    } else {
        //std::cerr << "CALLING DEDUCESIMPLE" << std::endl;
        sat = deducesimple();
    }
    
    if (sat) {
        //std::cerr << "FINISH sat = " << sat << std::endl;
        finish(sat);
    }
    
    return sat;
}


int uksat::DpllSolver::deducesimple() {
    int truth;
    std::pair<int, int> vars;
    bool propagated;
    
    //std::cerr << "DEDUCESIMPLE" << std::endl;
    do {
        truth = 1;
        propagated = false;
        
        for (std::size_t clauseidx = 0
                ; clauseidx < formula.getnclauses() && truth >= 0 && !propagated
                ; clauseidx++) {
            int clausetruth = checkclause(clauseidx, vars);
            
            if (clausetruth <= 0) {
                truth = clausetruth;
                
            } else if (clausetruth > 1) {
                propagated = true;
                push(vars.first);
                //std::cerr << "PROPAG var[" << vars.first << "]" << partial.sat(vars.first) << std::endl;
            }
            //std::cerr << "CLAUSESIMPLE idx = " << clauseidx << ", clausesat = " << clausetruth << ", propagated = " << propagated << ", var[" << vars.first << "] = " << partial.sat(vars.first) << std::endl;
        }
        //std::cerr << "FORMULA sat = " << truth << std::endl;
        
    } while (propagated);
    
    if (truth) {
        finish(truth);
    }
    
    return truth;
}

int uksat::DpllSolver::deducewatches() {
    int truth;
    bool propagated;
    std::pair<int, int> vars;
    
    do {
        truth = 1;
        propagated = false;
        
        for (std::size_t clauseidx = 0
                ; clauseidx < formula.getnclauses() && truth >= 0 && !propagated
                ; clauseidx++) {
            int clausetruth = checkclause(clauseidx, vars);
            std::pair<int, int>& cwatch = cwatches[clauseidx];
            
            if (clausetruth <= 0) {
                truth = clausetruth;
                
            } else if (clausetruth > 1) {
                propagated = true;
                //std::cerr << "PROPAG clauseidx = " << clauseidx << ", var = " << vars.first << std::endl;
                push(vars.first);
            }
            
            if (vars.first  && (!cwatch.first || !cwatch.second)) watch(clauseidx, vars.first);
            if (vars.second && (!cwatch.first || !cwatch.second)) watch(clauseidx, vars.second);
            //std::cerr << "CLAUSE idx = " << clauseidx << ", propagated = " << propagated << ", var[" << vars.first << "] = " << partial.get(vars.first) << std::endl;
        }
        //std::cerr << "FORMULA sat = " << truth << ", propagated = " << propagated << std::endl;
    } while (propagated);
    
    if (truth) {
        finish(truth);
    }
    
    return truth;
}

/*------------------------------------------------------------------------------
 * [ Solving helpers ]
 */


void uksat::DpllSolver::start() {
    if (formula.isvalid()) {
        clockbegin = std::clock();
        partial.setnvars(formula.getnvars());
        clausesats.resize(formula.getnclauses(), 0);
        cwatches.resize(formula.getnclauses(), std::pair<int, int>(0, 0));
    }
}


void uksat::DpllSolver::finish(int truth, bool restart) {
    if (!truth) {
        result = 0;
        if (restart)
            clockend = 0;
        else
            clockend = std::clock();
        
    } else {
        clockend = std::clock();
        result = truth;
        if (truth < 0)  {
            nconflicts++;
        }
    }
    
    if (!intime()) {
        timeout = true;
    }
}


int uksat::DpllSolver::choosefreevar() {
    int var = 0;
    
    if (partial.size() < formula.getnvars()) {
        for (int v = 1; v <= formula.getnvars(); v++) {
            if (!partial.isassigned(v)) {
                var = v;
                break;
            }
        }
    }
    
    return var;
}


int uksat::DpllSolver::apply() {
    int truth = 1;
    std::pair<int, int> vars;
    
    for (std::size_t clauseidx = 0
            ; clauseidx < formula.getnclauses() && truth >= 0
            ; clauseidx++) {
        int clausetruth = checkclause(clauseidx, vars);
        if (clausetruth > 1) clausetruth = 0;
        if (clausetruth <= 0) {
            truth = clausetruth;
        }
    }
    
    return truth;
}


int uksat::DpllSolver::checkclause(std::size_t clauseidx, std::pair<int, int>& watches) {
    int clausesat = -1;
	int undefvar = 0;
    std::size_t clausesize = formula[clauseidx].size();
	std::vector<int>::const_iterator iv = formula[clauseidx].begin();

    watches.first = 0;
    watches.second = 0;

	while ((clausesat < 0 || (clausesize > 0 && !watches.second)) && iv != formula[clauseidx].end()) {
		int var = *iv;
		
		if (!partial.isassigned(var)) {
			if (undefvar) {
                //std::cout << "Found two undef vars = [" << var << ", " << undefvar << "]" << std::endl;
                watches.second = var; watches.first = undefvar;
                //watches.second = watches.first; watches.first = var;
                clausesat = 0;
                undefvar = 0;

			} else {
                //std::cout << "Found undef var = " << var << std::endl;
                watches.second = watches.first, watches.first = var;
				if (clausesat < 0) undefvar = var;
			}

		} else if (partial.get(var)) {
            //std::cout << "Found true var = " << var << std::endl;
            watches.second = watches.first; watches.first = var;
            clausesat = 1;
            undefvar = 0;
            
		} else {
            //std::cout << "Found false var = " << var << std::endl;
            
        }

		iv++;
	}

	if (undefvar) {
        // Clause is unit. watches.first contains the undef var to propagate
		clausesat = 2;
	}
	
	return clausesat;
}


std::pair<int, int> uksat::DpllSolver::findwatchvars(std::size_t clauseidx, int knownvar) {
    std::pair<int, int> vars(0, 0);
	std::vector<int>::const_iterator iv = formula[clauseidx].begin();

	while (iv != formula[clauseidx].end() && ((knownvar && !vars.first) || !vars.second)) {
		int var = *iv;
		
		if (var != knownvar) {
            int sat = partial.sat(var);
            
			if (!sat || sat > 0) {
                vars.second = vars.first;
                vars.first = var;
            }
        }

		iv++;
	}
    
	return vars;
}

bool uksat::DpllSolver::backtrack() {
    bool success = false;
    int lastdecision = pop();
    
    if (lastdecision) {
        finish(0);
        push(lastdecision, true);
        success = true;
    }
    
    return success;
}


int uksat::DpllSolver::decide() {
    int var = choosefreevar();
    
    if (var) {
        //std::cerr << "DECIDENEW var = " << var << std::endl;
        push(var, true);
        
    } else {
        var = pop();
        
        if (var) {
            //std::cerr << "DECIDEBACK var = " << var << std::endl;
            push(var, true);
            
        } else {
            //std::cerr << "!!!NODECIDE!!! " << var << std::endl;
            if (watchinglits) {
                var = choosefreevar();
            }
        }
    }
    
    
    return var;
}


void uksat::DpllSolver::push(int var, bool decision) {
    //std::cerr << "PUSH var = " << var << ", decision = " << decision << std::endl;
    if (var) {
        //std::cerr << "PUSH var = " << var << ", decision = " << decision << ", currtime = " << currtime() << ", currvar = " << currvar() << std::endl;
        
        if (decision) {
            decisions.push_back(var);
            
        } else {
            propagations.push_back(std::pair<int, int>(var, currtime()));
        }
        
        partial.assign(var);
        //std::cerr << "PREVIEW "; printdecisions(std::cerr);
        //printdecisions(std::cerr);
        if (watchinglits) {
            if (intime()) {
                trigger(var);
            } else {
                //std::cerr << "!!!TIMEOUT!!!" << std::endl;
            }
        }
    }
}


int uksat::DpllSolver::pop() {
    int invertedvar = 0;
    
    if (!decisions.empty()) {
        int vartime = decisions.size();
        int var = 0;
        std::list<int> poppedvars;
        
        for (std::vector<int>::reverse_iterator it = decisions.rbegin(); it != decisions.rend(); it++) {
            var = *it;
            partial.unassign(var);
            if (watchinglits) poppedvars.push_back(var);
            if (var > 0) break;
            vartime--;
        }
        
        if (vartime) {
            int newsize = propagations.size();
            invertedvar = -var;
            decisions.resize(vartime - 1);
            
            for (std::vector<std::pair<int, int> >::reverse_iterator it = propagations.rbegin();
                    it != propagations.rend() && it->second >= vartime;
                    it++, newsize--) {
                partial.unassign(it->first);
                if (watchinglits) poppedvars.push_back(it->first);
            }
            
            propagations.resize(newsize);
        }
        
        if (!poppedvars.empty()) {
            for (std::list<int>::iterator it = poppedvars.begin(); it != poppedvars.end(); it++) {
                int poppedvar = *it;
                untrigger(poppedvar);
            }
        }
    }
    
    return invertedvar;
}


int uksat::DpllSolver::currvar() {
    return decisions.empty() ? 0 : *decisions.rbegin();
}


int uksat::DpllSolver::currtime() {
    return decisions.size();
}


void uksat::DpllSolver::storeclause(std::size_t clauseidx) {
    if (!clausesats[clauseidx]) {
        clausesats[clauseidx] = currvar() < 0 ? -currtime() : currtime();
        nsatclauses++;
    }
}


void uksat::DpllSolver::forgetclause(std::size_t clauseidx) {
    if (clausesats[clauseidx]) {
        clausesats[clauseidx] = 0;
        nsatclauses--;
    }
}


void uksat::DpllSolver::watch(std::size_t clauseidx, int var, int pos) {
    std::pair<int, int>& cwatch = cwatches[clauseidx];
    std::set<std::size_t>& watchset = partial.getwatches(var);
    
    if (pos) {
        // Replace watches
        int& watchpos = (pos == 1 ? cwatch.first : cwatch.second);
        
        if (watchpos) {
            //std::cerr << "REMOVEWATCH var = " << cwatch.second << ", clauseidx = " << clauseidx  << ", other = " << cwatch.first << std::endl;
            partial.getwatches(watchpos).erase(clauseidx);
        }
        
        //std::cerr << "WATCH var = " << var << ", clauseidx = " << clauseidx << std::endl;
        watchpos = var;
        watchset.insert(clauseidx);
        
    } else {
        // Remove/Shift watches
        if (cwatch.second) {
            //std::cerr << "REMOVEWATCH var = " << cwatch.second << ", clauseidx = " << clauseidx  << ", other = " << cwatch.first << std::endl;
            partial.getwatches(cwatch.second).erase(clauseidx);
        }

        cwatch.second = cwatch.first;
        cwatch.first = var;
        //std::cerr << "WATCH var = " << var << ", clauseidx = " << clauseidx << std::endl;
        if (var) watchset.insert(clauseidx);
    }
}


void uksat::DpllSolver::trigger(int var) {
    int invvar = uksat_INVERTLIT(var);
    std::set<std::size_t>& watchset = partial.getwatches(var);
    std::set<std::size_t>& invwatchset = partial.getwatches(invvar);
    
    //std::cerr << "TRIGGER var = " << var << std::endl;
    
    // To avoid problems with invalid iterators in loops, all pushes,
    // removed clauses and watch pairs detected in
    // this call will be done at once, at the end of the algorithm.
    std::list<std::pair<std::size_t, std::pair<int, int> > > watchpairs;
    std::list<std::pair<int, std::size_t> > pushes;
    std::set<int> pushset;
    
    // First, iterate over all watches on the inverted var ("false" case)
    for (std::set<std::size_t>::iterator it = invwatchset.begin(); it != invwatchset.end() && !isconflicting(); it++) {
        std::size_t clauseidx = *it;
        
        // Take the two var watches registered for the clause currently being notified
        std::pair<int, int>& cwatch = cwatches[clauseidx];
        
        // watchpos will receive a new var (because the var referred by watchedpos it is now false).
        // otherpos will be checked for propagation or contradiction.
        int& watchpos = (invvar == cwatch.first ? cwatch.first : cwatch.second);
        int& otherpos = (invvar == cwatch.first ? cwatch.second : cwatch.first);
        int watchposidx = (invvar == cwatch.first ? 1 : 2);
        
        if (invvar != cwatch.first && invvar != cwatch.second) {
            // This does NOT make any sense... Delete as soon as the debugging
            // time is over.
            //std::cerr << "!!!INVVAR NOT IN WATCHES!!! invvar = " << invvar << ", clauseidx = " << clauseidx << std::endl
        }
        
        // Check the truth value of othervar
        int vartruth = partial.sat(otherpos);

        if (!vartruth) {
            // The second watched variable is now unit because it is not
            // assigned. The clause is now true for the time currtime() and
            // below.
            // The watch for this second variable is kept.
            int normvar = uksat_NORMALLIT(otherpos);
            if (!pushset.count(normvar)) {
                // As the pushes happens only after the loop is over, only
                // add the variable to the push queue if it's not already there.
                //std::cerr << "TRIGGERUNITPUSH clauseidx = " << clauseidx << ", var = " << otherpos << ", triggervar = " << var << ", time = " << currtime() << ", nsatclauses = " << nsatclauses << std::endl;
                //pushes.push_back(std::pair<int, std::size_t>(otherpos, clauseidx));
                //pushset.insert(normvar);
                storeclause(clauseidx);
                push(otherpos);
                
                
            } else {
                // The `storeclause` happens later.
                //std::cerr << "TRIGGERUNIT clauseidx = " << clauseidx << ", var = " << otherpos << ", triggervar = " << var << ", time = " << currtime() << ", nsatclauses = " << nsatclauses << std::endl;
                pushes.push_back(std::pair<int, std::size_t>(0, clauseidx));
            }
            //push(cwatch.second);
            

        //} else if (vartruth > 0) {
            // The variable was already true. Consequently, the clause was
            // already true before.
            // The clause SHOULD NOT be `storeclause`'d here, because its truth comes
            // from an assignment done in a previous time. Storing it here
            // could make future iterations to incorrectly `forgetclause` it
            // because it may be on a time ahead of the current in case
            // a backtrack happens.

        //} else {
            // The other variable is false.
            // If the second variable is also false, this means that the
            // clause has no more free/true variables, and is therefore false.
            // Either way, a new variable should be looked up and watched.
            //forgetclause(clauseidx);
        }
        
        // A new watch must be put in watchpos, with a variable different
        // from otherpos.
        std::pair<int, int> vars = findwatchvars(clauseidx, otherpos);

        if (vars.first) {
            // Watch this new variable
            watchpairs.push_back(std::pair<std::size_t, std::pair<int, int> >(
                clauseidx, std::pair<int, int>(vars.first, watchposidx))
            );

        } else if (vartruth < 0) {
            // There's no other free/true variable. Two possibilities:
            // 1. The clause has only two variables, and the other one
            //    is false;
            // 2. The clause has more than two variables, and all of them
            //    are false.
            // Either way, the clause is now false, because there's no more
            // variables eligible to watch. The clause can be `forgetclause`'d.
            // Note that the watches must be kept, so that when backtracking,
            // the clause can be re-checked.
            //std::cerr << "TRIGGERUNSAT var = " << var << ", clauseidx = " << *it << ", time = " << currtime() << std::endl;
            forgetclause(clauseidx);
            finish(-1);
        }
    }
    
    // Post-adding new watches, in the order they were included
    for (std::list<std::pair<std::size_t, std::pair<int, int> > >::iterator it = watchpairs.begin();
            it != watchpairs.end();
            it++) {
        watch(it->first, it->second.first, it->second.second);
    }
    
    // Post-pushing the deduced variables
    for (std::list<std::pair<int, std::size_t> >::iterator it = pushes.begin();
            it != pushes.end();
            it++) {
        storeclause(it->second);
        if (it->first) push(it->first);
    }
    
    // Checking the "true" watches
    for (std::set<std::size_t>::iterator it = watchset.begin(); it != watchset.end(); it++) {
        storeclause(*it);
        //std::cerr << "TRIGGERSTORECLAUSE var = " << var << ", clauseidx = " << *it << ", time = " << currtime() << std::endl;
    }
}


void uksat::DpllSolver::untrigger(int var) {
    // TODO rewrite this function to only forget clauses that have no true-assigned
    // variable
    int invvar = uksat_INVERTLIT(var);
    std::set<std::size_t>& watchset = partial.getwatches(var);
    std::set<std::size_t>& invwatchset = partial.getwatches(invvar);
    
    for (std::set<std::size_t>::iterator it = watchset.begin(); it != watchset.end(); it++) {
        std::size_t clauseidx = *it;
        if (!isvalidtime(clauseidx)) forgetclause(clauseidx);
    }
    
    for (std::set<std::size_t>::iterator it = invwatchset.begin(); it != invwatchset.end(); it++) {
        std::size_t clauseidx = *it;
        if (!isvalidtime(clauseidx)) forgetclause(clauseidx);
    }
}


bool uksat::DpllSolver::isvalidtime(int ntime) {
    int time = currtime();
    int normtime = ntime < 0 ? -ntime : ntime;
    bool samesign = (normtime < 0 && time < 0) || (normtime > 0 && time > 0);
    return time >= normtime && samesign;
}


void uksat::DpllSolver::printdecisions(std::ostream& os) {
    os << "CALL " << ncalls << ", TIME " << currtime() << ", NSATCLAUSES " << nsatclauses << ", NPROPAGS " << propagations.size() << ": ";
    
    const char* sep = "";
    for (std::vector<std::pair<int, int> >::iterator it = propagations.begin(); it != propagations.end(); it++) {
        if (it->second == 0) os << sep << it->first << "(" << it->second << ")";
        sep = ", ";
    }
    
    sep = "";
    for (int time = 1; time <= decisions.size(); time++) {
        os << sep << decisions[time - 1] << "(" << time << ")";
        sep = ", ";
        for (std::vector<std::pair<int, int> >::iterator it = propagations.begin(); it != propagations.end(); it++) {
            if (it->second == time) os << sep << it->first << "(0)";
        }
    }
    
    os << std::endl;
}