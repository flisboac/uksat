
#include "uksat.hpp"

uksat::WatchedDpllSolver::WatchedDpllSolver(CnfFormula& _formula)
: SimpleDpllSolver(_formula)
, nsatclauses(0)
{
    
}


void uksat::WatchedDpllSolver::clear() {
    SimpleDpllSolver::clear();
    nsatclauses = 0;
}


void uksat::WatchedDpllSolver::start() {
    SimpleDpllSolver::start();
    if (isstarted()) {
        nsatclauses = 0;
        cstates.resize(formula.getnclauses());
    }
}


void uksat::WatchedDpllSolver::propagate() {
    if (ncalls == 1) {
        int sat = registerwatches();
        uksat_LOG_(LOG_PROPAG_SAT,
            "sat = " << sat
        );
        if (sat) finish(sat);

    } else if (nsatclauses == formula.getnclauses()) {
        uksat_LOG_(LOG_PROPAG_SAT,
            "sat = 1"
        );
        finish(1);

    } else if (isconflicting()) {
        // NOOOO, SOO WRONG!
        //finish(-1);
        uksat_LOG_(LOG_PROPAG_SAT,
            "sat = -1"
        );

    } else if (!intime()) {
        finish(0);
    }
}


void uksat::WatchedDpllSolver::push(int var, bool decision) {
    SimpleDpllSolver::push(var, decision);
    if (intime()) trigger(var);
}


int uksat::WatchedDpllSolver::pop(std::vector<int>& poppedvars) {
    int invertedvar = SimpleDpllSolver::pop(poppedvars);
    for (std::vector<int>::iterator it = poppedvars.begin(); it != poppedvars.end(); it++) {
        int poppedvar = *it;
        undotrigger(poppedvar);
    }
    return invertedvar;
}


int uksat::WatchedDpllSolver::registerwatches() {
    int truth;
    bool propagated;
    int nclausetrue;
    std::size_t count = 0;

    do {
        truth = 1;
        propagated = false;
        nclausetrue = 0;

        for (std::size_t clauseidx = 0
                ; clauseidx < formula.getnclauses() && truth >= 0 && !propagated
                ; clauseidx++) {
            int clausetruth = propagateclause(clauseidx);

            if (clausetruth > 1) {
                propagated = true;
                
            } else {
                std::pair<int, int>& cwatch = getclausewatches(clauseidx);
                
                if (clausetruth <= 0) {
                    truth = clausetruth;

                } else {
                    nclausetrue++;
                }
                
                if (!cwatch.first || !cwatch.second) {
                    std::pair<int, int> vars = findwatchvars(clauseidx);
                    
                    if (vars.first  && (!cwatch.first || !cwatch.second)) watch(clauseidx, vars.first);
                    if (vars.second && (!cwatch.first || !cwatch.second)) watch(clauseidx, vars.second);
                }
            }
        }
        
        uksat_LOG_(LOG_PROPAG_SAT,
            "sat = " << truth
            << ", restarts = " << count
            << ", propagated = " << propagated);
        
        if (nclausetrue == formula.getnclauses()) {
            truth = 1;
        }
        count++;
    } while (propagated);

    if (truth) {
        finish(truth);
    }

    return truth;
}


std::pair<int, int> uksat::WatchedDpllSolver::findwatchvars(std::size_t clauseidx, int knownvar) {
    std::pair<int, int> vars(0, 0), truevars(0, 0), undefvars(0, 0);
    std::vector<int>::const_iterator iv = formula[clauseidx].begin();
    std::vector<int>::const_iterator end = formula[clauseidx].end();
    
    while (iv != end) {
        int var = *iv;
        
        if (var != knownvar) {
            int sat = partial.sat(var);
            
            if (sat > 0) {
                truevars.second = truevars.first;
                truevars.first = var;
                
            } else if (sat == 0) {
                undefvars.second = undefvars.first;
                undefvars.first = var;
            }
        }
        
        if (undefvars.second || truevars.second) break;
#if 0
        if (knownvar) {
            if (undefvars.first || truevars.first) break;
        } else {
            if (undefvars.second || truevars.second) break;
        }
#endif   
        iv++;
    }
    
    vars = undefvars;
    if (!vars.first) {
        vars = truevars;
        
    } else if (!vars.second) {
        vars.second = truevars.first;
    }
    
    return vars;
}


void uksat::WatchedDpllSolver::watch(std::size_t clauseidx, int var, int substvar) {
    std::pair<int, int>& cwatch = getclausewatches(clauseidx);
    std::set<std::size_t>& watchset = getwatchset(var);
    int prevvar = 0;
    
    //uksat_LOGMARK_(LOG_WATCH_PRE);
    
    if (substvar && cwatch.first == substvar) {
        prevvar = cwatch.first;
        partial.getwatches(cwatch.first).erase(clauseidx);
        cwatch.first = var;
        if (var)  {
            watchset.insert(clauseidx);
            uksat_LOG_(LOG_WATCH_DO, "Set"
                << " clauseidx = " << clauseidx
                << ", watchpos = 1"
                << ", newvar = " << var
                << ", oldvar = " << substvar
                << ", othervar = " << cwatch.second
            );
                    
        } else {
            uksat_LOG_(LOG_WATCH_DO, "Remove"
                << " clauseidx = " << clauseidx
                << ", watchpos = 1"
                << ", newvar = " << var
                << ", oldvar = " << substvar
                << ", othervar = " << cwatch.second
            );
        }

    } else if (substvar && cwatch.second == substvar) {
        prevvar = cwatch.second;
        partial.getwatches(cwatch.second).erase(clauseidx);
        cwatch.second = var;
        if (var) {
            watchset.insert(clauseidx);
            uksat_LOG_(LOG_WATCH_DO, "Set"
                << " clauseidx = " << clauseidx
                << ", watchpos = 2"
                << ", newvar = " << var
                << ", oldvar = " << substvar
                << ", othervar = " << cwatch.first
            );
            
        } else {
            uksat_LOG_(LOG_WATCH_DO, "Remove"
                << " clauseidx = " << clauseidx
                << ", watchpos = 2"
                << ", newvar = " << var
                << ", oldvar = " << substvar
                << ", othervar = " << cwatch.first
            );
        }

    } else if (var && !cwatch.first) {
        cwatch.first = var;
        watchset.insert(clauseidx);
        uksat_LOG_(LOG_WATCH_DO, "New"
            << " clauseidx = " << clauseidx
            << ", watchpos = 1"
            << ", newvar = " << var
            << ", oldvar = " << substvar
            << ", othervar = " << cwatch.second
        );

    } else if (var && !cwatch.second) {
        cwatch.second = var;
        watchset.insert(clauseidx);
        uksat_LOG_(LOG_WATCH_DO, "New"
            << " clauseidx = " << clauseidx
            << ", watchpos = 2"
            << ", newvar = " << var
            << ", oldvar = " << substvar
            << ", othervar = " << cwatch.first
        );

    } else {
        // Remove/Shift watches
        std::pair<int, int> oldpair = cwatch;
        if (cwatch.second) partial.getwatches(cwatch.second).erase(clauseidx);
        cwatch.second = cwatch.first;
        cwatch.first = var;
        
        if (var) {
            watchset.insert(clauseidx);
            uksat_LOG_(LOG_WATCH_DO, "Shift"
                << " clauseidx = " << clauseidx
                << ", var = " << var
                << ", substvar = " << substvar
                << ", watches = {" << cwatch.first << ", " << cwatch.second << "}"
                << ", oldwatches = {" << oldpair.first << ", " << oldpair.second << "}"
            );
            
        } else {
            uksat_LOG_(LOG_WATCH_DO, "ShiftRemove"
                << " clauseidx = " << clauseidx
                << ", var = " << var
                << ", substvar = " << substvar
                << ", watches = {" << cwatch.first << ", " << cwatch.second << "}"
                << ", oldwatches = {" << oldpair.first << ", " << oldpair.second << "}"
            );
        }
    }
}


void uksat::WatchedDpllSolver::trigger(int var) {
    int invvar = uksat_INVERTLIT(var);
    std::set<std::size_t>& watchset = getwatchset(var);
    std::set<std::size_t>& invwatchset = getinvwatchset(var);
    std::size_t nloops = 0;
    
    uksat_LOG_(LOG_TRIGGER_PRE,
        "var = " << var
        << ", watchsize = " << watchset.size()
        << ", invwatchsize = " << invwatchset.size()
    );
    
    // Checking the "true" watches
    for (std::set<std::size_t>::iterator it = watchset.begin(); it != watchset.end(); it++) {
        std::size_t clauseidx = *it;
        if (!isvalidclausesat(clauseidx)) {
            uksat_LOG_(LOG_PROPAG_CLAUSE,
                " clauseidx = " << clauseidx
                << ", clausesat = 1"
                << ", var = " << var
                << ", currtime = " << currtime()
            );
            setclausesat(clauseidx);
        }
    }

    // Iterate over all watches on the inverted var ("false" case)
    std::set<std::size_t>::iterator it = invwatchset.begin();
    while (it != invwatchset.end()) {
        std::size_t clauseidx = *it;
        nloops++;

        // Take the two var watches registered for the clause currently being notified
        std::pair<int, int>& cwatch = getclausewatches(clauseidx);

        // watchpos will receive a new var (because the var referred by watchedpos it is now false).
        // otherpos will be checked for propagation or contradiction.
        int* pwatchpos;
        int* potherpos;
        int posidx = 1;

        if (invvar == cwatch.first) {
            pwatchpos = &(cwatch.first);
            potherpos = &(cwatch.second);
            
        } else {
            pwatchpos = &(cwatch.second);
            potherpos = &(cwatch.first);
            posidx = 2;
        }

        if (*pwatchpos != invvar) {
            // Something is wrong...
            uksat_LOG_(LOG_TRIGGER_STRANGE, "NoValidTriggerPos"
                << " clauseidx = " << clauseidx
                << ", triggervar = " << var
                << ", invtriggervar = " << invvar
                << ", watchidx = " << posidx
                << ", watchvar = " << *pwatchpos
                << ", watches = {" << cwatch.first << ", " << cwatch.second << "}"
            );
            throw std::exception();
        }
        
        // Check the truth value of othervar
        std::pair<int, int> vars = findwatchvars(clauseidx, *potherpos);
        
        if (vars.first && vars.first != *pwatchpos) {
            uksat_LOG_(LOG_TRIGGER_DO, "Watch"
                << " clauseidx = " << clauseidx
                << ", watchidx = " << posidx
                << ", newvar = " << vars.first
                << ", oldvar = " << *pwatchpos
                << ", othervar = " << *potherpos
            );
            watch(clauseidx, vars.first, *pwatchpos);
            int newvartruth = partial.sat(vars.first);
            
            if (newvartruth > 0) {
                if (!isvalidclausesat(clauseidx)) {
                    int assigntime = partial.gettime(*potherpos);
                    uksat_LOG_(LOG_PROPAG_CLAUSE, 
                        " clauseidx = " << clauseidx
                        << ", clausesat = 1"
                        << ", triggervar = " << var
                        << ", invtriggervar = " << invvar
                        << ", watchidx = " << posidx
                        << ", watchvar = " << *pwatchpos
                        << ", assigntime = " << assigntime
                        << ", watches = {" << cwatch.first << ", " << cwatch.second << "}"
                    );
                    setclausesat(clauseidx, assigntime);
                }
                
            } else if (newvartruth < 0) {
                // Something is wrong...
                uksat_LOG_(LOG_TRIGGER_STRANGE, "NewWatchVarNotWatchable"
                    << " clauseidx = " << clauseidx
                    << ", triggervar = " << var
                    << ", invtriggervar = " << invvar
                    << ", watchidx = " << posidx
                    << ", watchvar = " << *pwatchpos
                    << ", watches = {" << cwatch.first << ", " << cwatch.second << "}"
                );
                throw std::exception();
                
            } else {
                if (!vars.second) {
                    int secondvartruth = partial.sat(*potherpos);
                    
                    if (secondvartruth < 0) {
                        // The only other variable found was an unassigned one. This
                        // means that this variable is the only one eligible for
                        // watching. In other words, all other variables are false,
                        // and therefore, can be propagated.
                        uksat_LOG_(LOG_PROPAG_UNIT,
                            "clauseidx = " << clauseidx
                            << ", deducedvar = " << vars.first
                            << ", watchidx = " << posidx
                            << ", watchvar = " << *pwatchpos
                        );
                        eraseclausesat(clauseidx); // The assignment will be done on the next push
                        push(vars.first);
                    }
                }
            }
            it = invwatchset.begin();
            continue;
            
        } else {
            int vartruth = partial.sat(*potherpos);
            
            if (vartruth < 0) {
                uksat_LOG_(LOG_PROPAG_CLAUSE,
                    "sat = -1"
                    << ", clauseidx = " << clauseidx
                    << ", watchidx = " << posidx
                    << ", watchvar = " << *pwatchpos
                    << ", othervar = " << *potherpos
                );
                uksat_LOG_(LOG_PROPAG_SAT,
                    "sat = -1"
                    << ", nloops = " << nloops
                    << ", propagated = 0"
                    << ", clauseidx = " << clauseidx
                    << ", watchidx = " << posidx
                    << ", watchvar = " << *pwatchpos
                    << ", othervar = " << *potherpos
                );
                eraseclausesat(clauseidx);
                finish(-1);
                
            } else if (vartruth > 0) {                
                if (!isvalidclausesat(clauseidx)) {
                    // Should NOT happen!
                    int assigntime = partial.gettime(*potherpos);
                    uksat_LOG_(LOG_TRIGGER_STRANGE, "ClauseShouldBeSat"
                        "clauseidx = " << clauseidx
                        << ", triggervar = " << var
                        << ", invtriggervar = " << invvar
                        << ", watchidx = " << posidx
                        << ", watchvar = " << *pwatchpos
                        << ", assigntime = " << assigntime
                        << ", watches = {" << cwatch.first << ", " << cwatch.second << "}"
                    );
                    setclausesat(clauseidx, assigntime);
                }
                
            } else {
                uksat_LOG_(LOG_PROPAG_UNIT,
                    "clauseidx = " << clauseidx
                    << ", deducedvar = " << *potherpos
                    << ", watchidx = " << posidx
                    << ", watchvar = " << *pwatchpos
                );
                eraseclausesat(clauseidx); // The assignment will be done on the next push
                push(*potherpos);
                it = invwatchset.begin();
                continue;
            }
        }

        it++;
    }
}


void uksat::WatchedDpllSolver::undotrigger(int var) {
    int invvar = uksat_INVERTLIT(var);
    std::set<std::size_t>& watchset = getwatchset(var);
    std::set<std::size_t>& invwatchset = getinvwatchset(var);
    
    uksat_LOGMARK_(LOG_TRIGGER_UNDO_PRE);
    
    for (std::set<std::size_t>::iterator it = watchset.begin(); it != watchset.end(); it++) {
        std::size_t clauseidx = *it;
        if (!isvalidtime(clauseidx)) {
            uksat_LOG_(LOG_TRIGGER_UNDO, "WatchSet"
                << " var = " << var
                << ", clauseidx = " << clauseidx
                << ", isclausesat = " << cstates[clauseidx].satisfied
                << ", currtime = " << currtime()
                << ", clausetime = " << cstates[clauseidx].sattime
            );
            eraseclausesat(clauseidx);
        }
    }
    for (std::set<std::size_t>::iterator it = invwatchset.begin(); it != invwatchset.end(); it++) {
        std::size_t clauseidx = *it;
        if (!isvalidtime(clauseidx)) {
            uksat_LOG_(LOG_TRIGGER_UNDO, "InvWatchSet"
                << " var = " << var
                << ", clauseidx = " << clauseidx
                << ", isclausesat = " << cstates[clauseidx].satisfied
                << ", currtime = " << currtime()
                << ", clausetime = " << cstates[clauseidx].sattime
            );
            eraseclausesat(clauseidx);
        }
    }
}


bool uksat::WatchedDpllSolver::isvalidtime(int assigntime) {
    int time = currtime();
    int normtime = assigntime < 0 ? -assigntime : assigntime;
    bool samesign = (normtime < 0 && time < 0) || (normtime > 0 && time > 0);
    bool result = time >= normtime && samesign;
    return result;
}


bool uksat::WatchedDpllSolver::isvalidclausesat(std::size_t clauseidx) {
    ClauseState& cstate = cstates[clauseidx];
    if (cstate.satisfied)
        return isvalidtime(cstate.sattime);
    return false;
}


void uksat::WatchedDpllSolver::setclausesat(std::size_t clauseidx, int time) {
    ClauseState& cstate = cstates[clauseidx];
    
    if (!cstate.satisfied) {
        cstate.satisfied = true;
        if (!time) time = (currvar() < 0 ? -currtime() : currtime());
        cstate.sattime = time;
        nsatclauses++;
        
    } else if (time) {
        cstate.satisfied = true;
        cstate.sattime = time;
    }
    
    uksat_LOG_(LOG_ASSIGN_CLAUSE,
        "sat = 1"
        << ", clauseidx = " << clauseidx
        << ", time = " << time
        << ", nsatclauses = " << nsatclauses
    );
}


void uksat::WatchedDpllSolver::eraseclausesat(std::size_t clauseidx) {
    ClauseState& cstate = cstates[clauseidx];
    if (cstate.satisfied) {
        cstate.satisfied = false;
        cstate.sattime = 0;
        nsatclauses--;
        uksat_LOG_(LOG_ASSIGN_CLAUSE,
            "sat = 0"
            << ", clauseidx = " << clauseidx
            << ", time = " << time
            << ", nsatclauses = " << nsatclauses
        );
    }
}


std::pair<int, int>& uksat::WatchedDpllSolver::getclausewatches(std::size_t clauseidx) {
    return cstates[clauseidx].watches;
}


std::set<std::size_t>& uksat::WatchedDpllSolver::getwatchset(int var) {
    return partial.getwatches(var);
}


std::set<std::size_t>& uksat::WatchedDpllSolver::getinvwatchset(int var) {
    return partial.getwatches(uksat_INVERTLIT(var));
}