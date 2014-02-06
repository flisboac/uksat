/* 
 * File:   watched.hpp
 * Author: flisboac
 *
 * Created on 06 February 2014, 03:07
 */

#ifndef WATCHED_HPP
#define	WATCHED_HPP

#include <cstdlib>
#include <ctime>
#include <cmath>
#include <string>
#include <limits>
#include <set>
#include <vector>
#include <sstream>
#include <map>
#include <list>
#include "uksat.hpp"


namespace uksat {

void printidxset(std::set<std::size_t>& vec, std::string& str) {
    const char* sep = "";
    std::stringstream ss;
    ss << "{";
    for (std::set<std::size_t>::iterator it = vec.begin(); it != vec.end(); it++) {
        ss << sep << *it;
        sep = ", ";
    }
    ss << "}";
    str += ss.str();
}

class WatchedDpllSolver : public Solver {
public:

    WatchedDpllSolver(CnfFormula& _cnf, std::clock_t _maxtime = 0)
    : formula(_cnf)
    , clockdeltamax(_maxtime)
    , result(0)
    , nconflicts(0)
    , nsatclauses(0)
    , ncalls(0)
    , clockbegin(0)
    , clockend(0)
    , timeout(false)
    {

    }
    
    
    void clear() {
        result = 0;
        partial.clear();
        nsatclauses = 0;
        decisions.clear();
        propagations.clear();
        clausesats.clear();
        ncalls = 0;
        clockbegin = 0;
        clockend = 0;
        nconflicts = 0;
    }


    bool query() {
        if (!isstarted()) {
            start();
            querystep(); 
        }
        return issatisfied();
    }
   
    
    void querystep() {
        while (intime()) {
            ncalls++;
            //printdecisions(std::cerr);
            propagate();

            if (isconflicting()) {
                if (!backtrack())
                    break;

            } else if (!issatisfied()) {
                //std::cerr << "DECIDE" << std::endl;
                if (!decide())
                    break;
                
            } else if (isfinished()) {
                break;
            }
        }
    }
    
    
    void propagate() {
        if (ncalls == 1) {
            int sat = deduce();
            if (sat) finish(sat);
            
        } else if (nsatclauses == formula.getnclauses()) {
            finish(1);
            
        } else if (isconflicting()) {
            finish(-1);
            
        } else if (!intime()) {
            finish(0);
        }
    }
    
    
    int backtrack() {
        int lastdecision = 0;

        if (intime()) {
            lastdecision = pop();
            
            if (lastdecision) {
                finish(0);
                push(lastdecision, true);
            }
        }
        
        return lastdecision;
    }
    
    
    int decide() {
        int decision = 0;
        
        if (intime()) {
            decision = choosefreevar();
            
            if (decision) {
                //std::cerr << "DECISION var = " << decision << std::endl;
                push(decision, true);

            } else {
                pop(true);
                decision = choosefreevar();

                if (decision) {
                    //std::cerr << "DECISIONPOP var = " << decision << std::endl;
                    push(decision, true);

                } else {
                    decision = pop();

                    if (decision) {
                        //std::cerr << "DECISIONBACK var = " << decision << std::endl;
                        push(decision, true);
                        
                    } else {
                        //std::cerr << "!!!NODECISION!!!" << std::endl;
                    }
                }
            }
        }
        
        return decision;
    }
    
    int deduce() {
        int truth;
        bool propagated;
        int nclausetrue;
        std::pair<int, int> vars;

        do {
            truth = 1;
            propagated = false;
            nclausetrue = 0;

            for (std::size_t clauseidx = 0
                    ; clauseidx < formula.getnclauses() && truth >= 0 && !propagated
                    ; clauseidx++) {
                int clausetruth = checkclause(clauseidx, vars);
                std::pair<int, int>& cwatch = cwatches[clauseidx];

                if (clausetruth <= 0) {
                    truth = clausetruth;
                    if (vars.first  && (!cwatch.first || !cwatch.second)) watch(clauseidx, vars.first);
                    if (vars.second && (!cwatch.first || !cwatch.second)) watch(clauseidx, vars.second);

                } else if (clausetruth > 1) {
                    propagated = true;
                    push(vars.first);
                    
                } else {
                    nclausetrue++;
                }
                
                
                //std::cerr << "CLAUSE idx = " << clauseidx << ", sat = " << clausetruth << ", propagated = " << propagated << std::endl;
            }
            
            if (nclausetrue == formula.getnclauses()) {
                //std::cerr << "FORMULACHECK sat = " << truth << std::endl;
                truth = 1;
            }
        } while (propagated);

        if (truth) {
            //std::cerr << "FORMULA sat = " << truth << std::endl;
            finish(truth);
        }

        return truth;
    }
    
    
    void push(int var, bool decision = false) {
        if (var) {
            if (decision) {
                decisions.push_back(var);

            } else {
                //std::cerr << "PUSHPROPAG var = " << var << std::endl;
                propagations.push_back(std::pair<int, int>(var, currtime()));
            }

            partial.assign(var);
            if (intime()) {
                //std::cerr << "PUSH var = " << var << ", decision = " << decision << std::endl;
                trigger(var);
                
            } else {
                std::cerr << "!!!TIMEOUT!!!" << std::endl;
            }
        }
    }


    int pop(bool nodecision = false) {
        int invertedvar = 0;

        if (!decisions.empty()) {
            int vartime = currtime();
            int var = 0;
            std::list<int> poppedvars;
            
            if (!nodecision) {
                vartime = decisions.size();
                
                for (std::vector<int>::reverse_iterator it = decisions.rbegin(); it != decisions.rend(); it++) {
                    var = *it;
                    partial.unassign(var);
                    poppedvars.push_back(var);
                    //std::cerr << "POPDECISION var = " << var << std::endl;
                    if (var > 0) break;
                    vartime--;
                }
                
                if (vartime) decisions.resize(vartime - 1);
            }

            if (vartime) {
                int newsize = propagations.size();
                invertedvar = -var;

                for (std::vector<std::pair<int, int> >::reverse_iterator it = propagations.rbegin();
                        it != propagations.rend() && it->second >= vartime;
                        it++, newsize--) {
                    partial.unassign(it->first);
                    //std::cerr << "POP var = " << var << std::endl;
                    poppedvars.push_back(it->first);
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
    
    
    void watch(std::size_t clauseidx, int var, int substvar = 0) {
        std::pair<int, int>& cwatch = cwatches[clauseidx];
        std::pair<int, int> oldcwatch = cwatch;
        std::set<std::size_t>& watchset = partial.getwatches(var);
        int prevvar = 0;
        
        if (substvar && cwatch.first == substvar) {
            prevvar = cwatch.first;
            partial.getwatches(cwatch.first).erase(clauseidx);
            cwatch.first = var;
            if (var) watchset.insert(clauseidx);
            
        } else if (substvar && cwatch.second == substvar) {
            prevvar = cwatch.second;
            partial.getwatches(cwatch.second).erase(clauseidx);
            cwatch.second = var;
            if (var) watchset.insert(clauseidx);
            
        } else if (var && !cwatch.first) {
            // Insert watch in available slot
            cwatch.first = var;
            watchset.insert(clauseidx);
            
        } else if (var && !cwatch.second) {
            // Insert watch in available slot
            cwatch.second = var;
            watchset.insert(clauseidx);
            
        //} else {
            // Remove/Shift watches
        //    if (cwatch.second) partial.getwatches(cwatch.second).erase(clauseidx);
        //    cwatch.second = cwatch.first;
        //    cwatch.first = var;
        //    if (var) watchset.insert(clauseidx);
        }
        
        //std::cerr << "WATCH clauseidx = " << clauseidx << ", var = " << var << ", prevvar = " << prevvar << ", substvar = " << substvar << ", oldcwatch = {" << oldcwatch.first << ", " << oldcwatch.second << "}, cwatches = {" << cwatch.first << ", " << cwatch.second << "}" << ", included = " << partial.getwatches(var).count(clauseidx) << std::endl;
        
    }


    void trigger(int var) {
        int invvar = uksat_INVERTLIT(var);
        std::set<std::size_t>& watchset = partial.getwatches(var);
        std::set<std::size_t>& invwatchset = partial.getwatches(invvar);
        
        // Checking the "true" watches
        for (std::set<std::size_t>::iterator it = watchset.begin(); it != watchset.end(); it++) {
            std::size_t clauseidx = *it;
            if (!isvalidclause(clauseidx)) {
                //std::cerr << "TRIGGERSTORE clauseidx = " << clauseidx << ", nsatclauses = " << nsatclauses << " + 1" << std::endl;
                storeclause(clauseidx);
            }
        }
        
        // First, iterate over all watches on the inverted var ("false" case)
        std::set<std::size_t>::iterator it = invwatchset.begin();
        while (it != invwatchset.end() && !isconflicting()) {
            std::size_t clauseidx = *it;
            
            //if (isvalidclause(clauseidx)) {
            //    it++;
            //    continue;
            //};
            
            // Take the two var watches registered for the clause currently being notified
            std::pair<int, int>& cwatch = cwatches[clauseidx];

            // watchpos will receive a new var (because the var referred by watchedpos it is now false).
            // otherpos will be checked for propagation or contradiction.
            int* pwatchpos;// = (var == cwatches[clauseidx].second ? &(cwatches[clauseidx].second) : &(cwatches[clauseidx].first));
            int* potherpos;// = (var == cwatches[clauseidx].first ? &(cwatches[clauseidx].second) : &(cwatches[clauseidx].first));
            
            if (invvar == cwatches[clauseidx].first) {
                pwatchpos = &(cwatches[clauseidx].first);
                potherpos = &(cwatches[clauseidx].second);
                
            } else {
                pwatchpos = &(cwatches[clauseidx].second);
                potherpos = &(cwatches[clauseidx].first);
            }
            
            if (*pwatchpos != invvar) {
                std::string str;
                //printidxset(invwatchset, str);
                //std::cerr << "!!!WHAT?!!! clauseidx = " << clauseidx << ", triggervar = " << var << ", invvar = " << invvar << ", watchpos = " << *pwatchpos << ", otherpos = " << *potherpos << ", watches = {" << cwatches[clauseidx].first << ", " << cwatches[clauseidx].second << "}" << ", watchset = " << str.c_str() << std::endl;
            }
            // Check the truth value of othervar
            std::pair<int, int> vars = findwatchvars(clauseidx, *potherpos);

            //std::cerr << "TRIGGERINV clauseidx = " << clauseidx << ", triggervar = " << var << ", vartruth = " << vartruth << ", watches = {" << cwatch.first << ", " << cwatch.second << "}" << std::endl;
            
            if (vars.first && vars.first != *pwatchpos) {
                //std::cerr << "TRIGGERWATCH clauseidx = " << clauseidx << ", var = " << vars.first << ", substvar = " << *pwatchpos << ", triggervar = " << var << std::endl;
                watch(clauseidx, vars.first, *pwatchpos);
                
            } else {
                int vartruth = partial.sat(*potherpos);
                
                if (vartruth < 0) {
                    //std::cerr << "TRIGGERUNSAT clauseidx = " << clauseidx << ", othervar = " << *potherpos << ", triggervar = " << var << std::endl;
                    forgetclause(clauseidx);
                    finish(-1);

                } else if (vartruth > 0) {
                    // NOP
                    //if (!isvalidclause(clauseidx))
                        //std::cerr << "!!!POSITIVEERROR!!!" << std::endl;
                    
                } else {
                    //std::cerr << "TRIGGERPROPAG clauseidx = " << clauseidx << ", var = " << *potherpos << ", triggervar = " << var << std::endl;
                    storeclause(clauseidx);
                    push(*potherpos);
                    it = invwatchset.begin();
                    continue;
                }
            }
            
            it++;
        }
    }
    
    
    void untrigger(int var) {
        // TODO rewrite this function to only forget clauses that have no true-assigned
        // variable
        int invvar = uksat_INVERTLIT(var);
        std::set<std::size_t>& watchset = partial.getwatches(var);
        std::set<std::size_t>& invwatchset = partial.getwatches(invvar);
        
        //std::cerr << "UNWATCH var = " << var << std::endl;
        
        for (std::set<std::size_t>::iterator it = watchset.begin(); it != watchset.end(); it++) {
            std::size_t clauseidx = *it;
            if (!isvalidtime(clauseidx)) forgetclause(clauseidx);
        }

        for (std::set<std::size_t>::iterator it = invwatchset.begin(); it != invwatchset.end(); it++) {
            std::size_t clauseidx = *it;
            if (!isvalidtime(clauseidx)) forgetclause(clauseidx);
        }
    }


    int currvar() {
        return decisions.empty() ? 0 : *decisions.rbegin();
    }


    int currtime() {
        return decisions.size();
    }

    
    bool isvalidclause(std::size_t clauseidx) {
        std::pair<bool, int>& pair = clausesats[clauseidx];
        if (pair.first)
            return isvalidtime(pair.second);
        return false;
    }
    
    
    void storeclause(std::size_t clauseidx) {
        std::pair<bool, int>& pair = clausesats[clauseidx];
        if (!pair.first) {
            pair.first = true;
            pair.second = currvar() < 0 ? -currtime() : currtime();
            nsatclauses++;
        }
    }


    void forgetclause(std::size_t clauseidx) {
        std::pair<bool, int>& pair = clausesats[clauseidx];
        if (pair.first) {
            pair.first = false;
            pair.second = 0;
            nsatclauses--;
        }
    }

    
    bool isvalidtime(int ntime) {
        int time = currtime();
        int normtime = ntime < 0 ? -ntime : ntime;
        bool samesign = (normtime < 0 && time < 0) || (normtime > 0 && time > 0);
        bool result = time >= normtime && samesign;
        //std::cerr << "VALIDTIME time = " << currtime() << ", ntime = " << ntime << ", valid = " << result << std::endl;
        return result;
    }


    void printdecisions(std::ostream& os) {
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

    
    std::pair<int, int> findwatchvars(std::size_t clauseidx, int knownvar) {
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


    int checkclause(std::size_t clauseidx, std::pair<int, int>& freevars) {
        int clausesat = -1;
        int undefvar = 0;
        std::vector<int>::const_iterator iv = formula[clauseidx].begin();

        freevars.first = 0;
        freevars.second = 0;

        while ((clausesat < 0) && iv != formula[clauseidx].end()) {
            int var = *iv;
            int vartruth = partial.sat(var);
            
            if (!vartruth) {
                if (undefvar) {
                    freevars.second = freevars.first; freevars.first = var;
                    clausesat = 0;
                    undefvar = 0;

                } else {
                    freevars.second = freevars.first, freevars.first = var;
                    undefvar = var;
                }

            } else if (vartruth > 0) {
                freevars.second = freevars.first; freevars.first = var;
                clausesat = 1;
                undefvar = 0;
            }

            iv++;
        }

        if (undefvar) {
            // Clause is unit
            clausesat = 2;
            push(undefvar);
        }

        return clausesat;
    }
    
    
    int choosefreevar() {
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

    
    int apply() {
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


    void start() {
        if (formula.isvalid()) {
            clockbegin = std::clock();
            partial.setnvars(formula.getnvars());
            clausesats.resize(formula.getnclauses(), std::pair<bool, int>(false, 0));
            cwatches.resize(formula.getnclauses(), std::pair<int, int>(0, 0));
        }
    }

    void finish(int truth = 0) {
        clockend = std::clock();
        
        if (!truth) {
            result = 0;

        } else {
            clockend = std::clock();
            result = truth;
            if (truth < 0) nconflicts++;
        }

        if (!intime()) timeout = true;
    }
    
    
    bool intime() {
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
                
                if (!stillintime) {
                    std::cerr << "!!!NOT IN TIME!!!" << std::endl;
                    timeout = true;
                }
            }
        }
        
        return stillintime;
    }
    
    bool isvalid() const {
        return formula.isvalid();
    }


    int getsat() const {
        return isfinished() ? result : 0;
    }


    bool issatisfied() const {
        return isfinished() && result > 0;
    }


    bool isconflicting() const {
        return isfinished() && result < 0;
    }


    bool isstarted() const {
        return !!clockbegin;
    }


    bool isfinished() const {
        return !!clockend;
    }


    bool hastimeout() const {
        return timeout;
    }


    uksat::CnfFormula& getformula() {
        return formula;
    }


    uksat::PartialMap& getpartial() {
        return partial;
    }


    void setmaxtime(double secs) {
        if (!isstarted() && secs > 0) {
            double ceilsec = std::ceil(secs);
            ceilsec *= CLOCKS_PER_SEC;

            if (ceilsec <= std::numeric_limits<clock_t>::max()) {
                clockdeltamax = static_cast<clock_t>(ceilsec);
            }
        }
    }


    double getmaxtime() const {
        return static_cast<double>(clockdeltamax) / static_cast<double>(CLOCKS_PER_SEC);
    }


    double getelapsedtime() {
        double clockspersec = static_cast<double>(CLOCKS_PER_SEC);
        return clockbegin
            ? clockend
                ? ( static_cast<double>(clockend - clockbegin) / clockspersec )
                : ( static_cast<double>(std::clock() - clockbegin) / clockspersec)
            : 0.0;
    }

private:
    // Options
    CnfFormula& formula;
    std::clock_t clockdeltamax;

    // Results
    int result;
    bool timeout;
    PartialMap partial;
    std::size_t nconflicts;
    
    // Stack
    std::vector<int> decisions;
    std::vector<std::pair<int, int> > propagations;
    
    // Watched Literals
    std::vector<std::pair<int, int> > cwatches; // time when true, pair of watches
    std::size_t nsatclauses;
    std::vector<std::pair<bool, int> > clausesats;
    
    // 
    std::size_t ncalls;
    std::clock_t clockbegin;
    std::clock_t clockend;    
};

};

#endif	/* WATCHED_HPP */

