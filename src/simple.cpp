#include <iostream>
#include "uksat.hpp"

uksat::LogType::LogType(LogTypeId _type) {
    *this = uksat::LogTypes[_type];
}

uksat::SimpleDpllSolver::SimpleDpllSolver(CnfFormula& _formula) 
: Solver::Solver(_formula)
, ncalls(0)
{
    
}


int uksat::SimpleDpllSolver::currtime() {
    return decisions.size();
}


int uksat::SimpleDpllSolver::currvar() {
    return decisions.empty() ? 0 : *(decisions.rbegin());
}


void
uksat::SimpleDpllSolver::clear() {
    Solver::clear();
    ncalls = 0;
}


bool
uksat::SimpleDpllSolver::query() {
    start();
    while (querystep());
    return issatisfied();
}


bool
uksat::SimpleDpllSolver::querystep() {
    bool keepgoing = true;
    if (isstarted() && intime()) {
        ncalls++;
        uksat_IFLOG_(printdecisions());
        propagate();
        
        if (issatisfied()) {
            keepgoing = false;
            
        } else if (isconflicting()) {
            uksat_LOGMARK_(LOG_BACK_PRE);
            
            if (!backtrack()) {
                uksat_LOGMARK_(LOG_BACK_FAIL);
                keepgoing = false;
            }
            
        } else {
            uksat_LOGMARK_(LOG_DECIDE_PRE);
            
            if (!decide()) {
                uksat_LOGMARK_(LOG_DECIDE_FAIL);
                keepgoing = false;
            }
        }
        
    } else {
        keepgoing = false;
    }
    return keepgoing;
}


void
uksat::SimpleDpllSolver::propagate() {
    int truth = 0;
    bool propagated;
    std::size_t count = 0;
    
    uksat_LOGMARK_(LOG_PROPAG_PRE);
    do {
        truth = 1;
        propagated = false;
        
        for (std::size_t clauseidx = 0
                ; clauseidx < formula.getnclauses() && truth >= 0 && !propagated
                ; clauseidx++) {
            int clausetruth = propagateclause(clauseidx);
            
            if (clausetruth <= 0) {
                truth = clausetruth;
                
            } else if (clausetruth > 1) {
                propagated = true;
            }
        }
        
        uksat_LOG_(LOG_PROPAG_SAT,
            "sat = " << truth
            << ", restarts = " << count
            << ", propagated = " << propagated);
        count++;
    } while (propagated);
    
    if (truth) {
        finish(truth);
    }
}


int
uksat::SimpleDpllSolver::decide() {
    int var = choosefreevar();
    if (var) {
        uksat_LOG_(LOG_DECIDE_FREEVAR, "var = " << var);
        push(var, true);
    } else {
        var = backtrack();
    }
    return var;
}


int
uksat::SimpleDpllSolver::backtrack() {
    int var = pop();
    if (var) {
        uksat_LOG_(LOG_BACK_OK, "var = " << var);
        // NOTE: THAT'S TRICKY! Must finish BEFORE pushing b/c watched literals!
        finish(0);
        push(var, true);
    }
    return var;
}


void
uksat::SimpleDpllSolver::push(int var, bool decision) {
    uksat_LOG_(LOG_STACK_PUSH,
        "var = " << var
        << ", isdecision = " << decision
        << ", currtime = " << currtime()
    );
    
    if (decision) {
        decisions.push_back(var);
    } else {
        propagations.push_back(std::pair<int, int>(var, currtime()));
    }
    partial.assign(var);
}


int
uksat::SimpleDpllSolver::pop() {
    std::vector<int> dummy;
    int invertedvar = pop(dummy);
    return invertedvar;
}


int
uksat::SimpleDpllSolver::pop(std::vector<int>& poppedvars) {
    int invertedvar = 0;
    
    uksat_LOGMARK_(LOG_STACK_POP_PRE);
    
    if (!decisions.empty()) {
        int vartime = currtime();
        int var = 0;//decisions.size();
        
        for (std::vector<int>::reverse_iterator it = decisions.rbegin(); it != decisions.rend(); it++) {
            var = *it;
            uksat_LOG_(LOG_STACK_POPVAR, "isdecision = 1, var = " << var << ", vartime = " << vartime);
            partial.unassign(var);
            poppedvars.push_back(var);
            if (var > 0) break;
            vartime--;
        }

        if (vartime) {
            decisions.resize(vartime - 1);
            int newsize = propagations.size();
            invertedvar = -var;

            for (std::vector<std::pair<int, int> >::reverse_iterator it = propagations.rbegin();
                    it != propagations.rend() && it->second >= vartime;
                    it++, newsize--) {
                uksat_LOG_(LOG_STACK_POPVAR, "isdecision = 0, var = " << it->first << ", vartime = " << vartime);
                partial.unassign(it->first);
                poppedvars.push_back(it->first);
            }

            propagations.resize(newsize);
        }
    }
    
    uksat_LOG_(LOG_STACK_POP,
        "invertedvar = " << invertedvar
        << ", ndecisions = " << decisions.size()
        << ", npropagations = " << propagations.size());
    return invertedvar;
}


int
uksat::SimpleDpllSolver::propagateclause(std::size_t clauseidx) {
    int clausesat = -1;
    int undefvar = 0;
    std::vector<int>::const_iterator iv = formula[clauseidx].begin();
    
    while ((clausesat < 0) && iv != formula[clauseidx].end()) {
        int var = *iv;
        int vartruth = partial.sat(var);
        if (!vartruth) {
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
    
    if (undefvar) {
        uksat_LOG_(LOG_PROPAG_UNIT,
            "clauseidx = " << clauseidx
            << ", deducedvar = " << undefvar);
        clausesat = 2;
        push(undefvar);
    }
    
    uksat_LOG_(LOG_PROPAG_CLAUSE,
        "clauseidx = " << clauseidx
        << ", clausesat = " << clausesat
    );

    return clausesat;
}

void uksat::SimpleDpllSolver::printdecisions() {
    std::ostream& ls = log(LOG_STEP);
    ls << "CALL " << ncalls << ", TIME " << currtime() << ", NPROPAGS " << propagations.size() << ": ";

    const char* sep = "";
    for (std::vector<std::pair<int, int> >::iterator it = propagations.begin(); it != propagations.end(); it++) {
        if (it->second == 0) ls << sep << it->first << "(" << it->second << ")";
        sep = ", ";
    }

    sep = "";
    for (int time = 1; time <= decisions.size(); time++) {
        ls << sep << decisions[time - 1] << "(" << time << ")";
        sep = ", ";
        for (std::vector<std::pair<int, int> >::iterator it = propagations.begin(); it != propagations.end(); it++) {
            if (it->second == time) ls << sep << it->first << "(0)";
        }
    }

    ls << endlog;
}

#if 0
int uksat::SimpleDpllSolver::checkclause(std::size_t clauseidx, std::pair<int, int>& watchvars) {
    int clausesat = -1;
    int undefvar = 0;
    std::vector<int>::const_iterator iv = formula[clauseidx].begin();

    watchvars.first = 0;
    watchvars.second = 0;

    while ((clausesat < 0) && iv != formula[clauseidx].end()) {
        int var = *iv;
        int vartruth = partial.sat(var);

        if (!vartruth) {
            if (undefvar) {
                watchvars.second = watchvars.first; watchvars.first = var;
                clausesat = 0;
                undefvar = 0;

            } else {
                watchvars.second = watchvars.first, watchvars.first = var;
                undefvar = var;
            }

        } else if (vartruth > 0) {
            watchvars.second = watchvars.first; watchvars.first = var;
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
#endif
