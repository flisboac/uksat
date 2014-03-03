
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <ctime>
#include <algorithm>

#include "uksat.hpp"

static bool varfreq_less(const std::pair<int, int>& a, const std::pair<int, int>& b) {
    int norm_a = uksat_NORMALLIT(a.first);
    int norm_b = uksat_NORMALLIT(b.first);
    return b.second < a.second || (b.second == a.second && norm_a < norm_b);
}

struct VarFreq_Less {
    bool operator()(const std::pair<int, int>& a, const std::pair<int, int>& b) {
        return varfreq_less(a, b);
    }
};

uksat::CnfFormula::CnfFormula() : nvars(0), nclauses(0) {

}


uksat::CnfFormula::CnfFormula(unsigned int numvars, std::vector<std::vector<int> >& clist) {
	if (numvars > 0) {
		nvars = numvars;
		clauses = clist;
		nclauses = clauses.size();
	}
}


int uksat::CnfFormula::getnvars() const {
	return nvars;
}


int uksat::CnfFormula::getnclauses() const {
	return nclauses;
}


int uksat::CnfFormula::totalfrequency(int normvar) const {
    int normalizedvar = uksat_NORMALLIT(normvar);
    return frequency(normalizedvar) + frequency(-normalizedvar);
}


int uksat::CnfFormula::frequency(int var) const {
    std::map<int, int>::const_iterator var_it = varfrequencies.find(var);
    int total = 0;
    if (var_it != varfrequencies.end())  total = var_it->second;
    return total;
}


const std::vector<int>& uksat::CnfFormula::getvarorder() const {
    return varorder;
}


const std::vector<int>& uksat::CnfFormula::operator[](int clauseidx) const {
	return clauses[clauseidx];
}

bool uksat::CnfFormula::isvalid() const {
	return nvars > 0 && nclauses > 0 && nclauses == clauses.size();
}


bool uksat::CnfFormula::openfile(const char* filename) {
	bool ret = false;

	if (filename) {
		std::ifstream file;
		file.open(filename);

		if (file.is_open()) {
			ret = openfile(file);
			file.close();
		}
	}

	return ret;
}


bool uksat::CnfFormula::openfile(std::istream& is) {
	bool ret = true;
	nclauses = 0;
	nvars = 0;
	clauses.clear();
    std::map<int, int> vartotals;

	std::string buf;

	if (is.good()) {
		do {
			buf.clear();
			std::getline(is, buf);

			// Ignores empty lines
			if (buf.empty()) continue;
            
            int c = 0;
            size_t idx = 0;
            int var;
            bool error = false;
            std::istringstream ss;

            // Ignores spaces at the start of the line
            while (idx < buf.length()){
                c = buf[idx++];
                if (!std::isspace(c))
                    break;
            }
            if (--idx)
                buf.erase(0, idx);

            // Ignore lines comprised only of spaces
            if (buf.empty()) continue;

            // Checks the type of the line
            if (buf[0] == 'c') {
                // Ignore comments
                continue;

            } else if ( nclauses > 0 && (buf[0] == '-' || isdigit(buf[0])) ) {
                // If we already found a "header", look for clauses
                // until all of them have been read.
                clauses.resize(clauses.size() + 1);
                ss.str(buf);
                
                do {
                    ss >> var;

                    if ((var < 0 && var < -nvars) || (var > 0 && var > nvars)) {
                        // Variable does not exist in the problem space, abort
                        clauses.at(clauses.size() - 1).clear();
                        break;
                                
                    } else if (var) {
                        clauses.at(clauses.size() - 1).push_back(var);
                        std::map<int, int>::iterator it = varfrequencies.find(var);
                        std::map<int, int>::iterator norm_it = vartotals.find(uksat_NORMALLIT(var));
                        
                        if (it != varfrequencies.end()) {
                            it->second++;
                            
                        } else {
                            varfrequencies[var] = 1;
                        }
                        
                        if (norm_it != vartotals.end()) {
                            norm_it->second++;
                            
                        } else {
                            vartotals[uksat_NORMALLIT(var)] = 1;
                        }
                    }
                    
                } while (ss.good() || var != 0);

                if (clauses[clauses.size() - 1].empty()) {
                    // We got an empty clause, abort
                    error = true;
                    //clauses.resize(clauses.size() - 1);
                    
                } else if (clauses.size() == nclauses) {
                    // Got enough clauses, break
                    break;
                }

            } else if (buf[0] == 'p') {
                // Gets the problem line
                if (nclauses == 0) {
                    ss.str(buf);
                    std::string tname;

                    ss >> tname; // The first must be ignored
                    ss >> tname;

                    if (ss.good() && tname.compare("cnf") == 0) {
                        ss >> nvars;

                        if (ss.good()) {
                            ss >> nclauses;

                        } else {
                            error = true;
                        }
                        
                    } else {
                        error = true;
                    }

                } else {
                    // Two problem lines declared in the file, abort
                    error = true;
                }

            //}  else {
                // Ill-formed file, break
                //error = true;
                // Not really... Just ignore this line...
            }

            if (error) {
                nclauses = 0;
                break;
            }

		} while (!is.eof());
        
        // If the problem is incorrectly or partially described, abort
        if (!nclauses || !nvars || clauses.size() != nclauses) {
            nclauses = 0;
            nvars = 0;
            clauses.clear();
            ret = false;
        } else {
            // Ordering variables by frequency
            std::set<std::pair<int, int>, VarFreq_Less> ordvars;
            
            for (std::map<int, int>::iterator it = vartotals.begin(); it != vartotals.end(); ++it) {
                ordvars.insert(*it);
            }
            
            for (std::set<std::pair<int, int>, VarFreq_Less>::iterator it = ordvars.begin(); it != ordvars.end(); ++it) {
                int pushvar = it->first;
                if (frequency(-pushvar) > frequency(pushvar)) {
                    pushvar = -pushvar;
                }
                
                varorder.push_back(pushvar);
#if 0
                if (varorder.empty()) {
                    varorder.push_back(pushvar);
                    
                } else {
                    int lastvar = uksat_NORMALLIT(*varorder.rbegin());
                    if (lastvar != uksat_NORMALLIT(pushvar)) {
                        varorder.push_back(pushvar);
                    }
                }
#endif
            }
        }
        
	} else {
		ret = false;
	}
	
	return ret;
}


bool uksat::CnfFormula::savefile(const char* filename) {
	bool ret = false;

	if (filename) {
		std::ofstream file;
		file.open(filename);

		if (file.is_open()) {
			ret = savefile(file);
			file.close();
		}
	}

	return ret;
}


bool uksat::CnfFormula::savefile(std::ostream& os) {
	bool ret = false;

	if (os.good() && nvars && nclauses) {
		ret = true;

        os << "p " << "cnf" << " " << nvars << " " << nclauses << std::endl;

        for (std::vector<std::vector<int> >::iterator clause = clauses.begin(); clause != clauses.end(); clause++) {
            const char* sep = "";

            for (std::vector<int>::iterator var = clause->begin(); var != clause->end(); var++) {
                os << sep << (*var);
                sep = " ";
            }

            os << sep << "0" << std::endl;
        }

	} else {
		ret = false;
	}

	return ret;
}


bool uksat::CnfFormula::savesolution(const char* filename, Solver& solver) {
    bool ret = false;

	if (filename) {
		std::ofstream file;
		file.open(filename);

		if (file.is_open()) {
			ret = savesolution(file, solver);
			file.close();
		}
	}

	return ret;
}


bool uksat::CnfFormula::savesolution(std::ostream& os, Solver& solver) {
	bool ret = false;

	if (os.good() && nvars && nclauses) {
		ret = true;
        int sat = -1;

        if (solver.isconflicting()) {
            sat = 0;
            os << "c UNSATISFIABLE" << std::endl;

        } else if (solver.issatisfied()) {
            sat = 1;
            os << "c SATISFIABLE" << std::endl;
            
        } else {
            os << "c UNDEFINED" << std::endl;
        }
        
        os << "s cnf " << sat << " " << nvars << " " << nclauses << std::endl;
        
        std::map<int, bool> map;
        solver.getpartial().copy(map);
        for (std::map<int, bool>::const_iterator it = map.begin(); it != map.end(); it++) {
            os << "v " << (it->second ? it->first : -it->first) << std::endl;
        }

	} else {
		ret = false;
	}

	return ret;
}
