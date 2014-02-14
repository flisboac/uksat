#include <uksat.hpp>

uksat::PartialMap::PartialMap() : mapsize(0) {
    
}


uksat::PartialMap::PartialMap(int nvars) : mapsize(0) {
    map.resize(nvars, Entry());
}



int uksat::PartialMap::getnvars() const {
    return map.size();
}


void uksat::PartialMap::setnvars(int nvars) {
    mapsize = 0;
    map.clear();
    map.resize(nvars, Entry());
}


std::set<std::size_t>& uksat::PartialMap::getwatches(int var) {
    int normvar = uksat_NORMALLIT(var);
    int idx = normvar - 1;
    //std::cerr << "GETWATCH normvar = " << normvar << ", var = " << var << ", idx = " << idx << std::endl;
    if (var < 0)
        return map[idx].fclauses;
    else
        return map[idx].tclauses;
}


int uksat::PartialMap::size() const {
    return mapsize;
}


void uksat::PartialMap::clear() {
    mapsize = 0;
    map.clear();
}


void uksat::PartialMap::assign(int var, int truth, int time) {
    int idx = uksat_NORMALLIT(var) - 1;
    int val = map[idx].truth;
    if (val && !truth) mapsize--;
    if (!val && truth) mapsize++;
    map[idx].truthtime = time;
    //std::cerr << "ASSIGN var = " << (idx + 1) << ", truth = " << truth << std::endl;
    map[idx].truth = truth;
}


void uksat::PartialMap::assign(int var, bool truth, int time) {
    assign(var, truth ? 1 : -1, time);
}


void uksat::PartialMap::push(int var, int time) {
    assign(var, var < 0 ? -1 : 1, time);
}


void uksat::PartialMap::unassign(int var) {
    assign(var, 0, 0);
}


bool uksat::PartialMap::isassigned(int var) const {
    int normalizedlit = uksat_NORMALLIT(var);
    return normalizedlit-- ? !!map[normalizedlit].truth : false;
}


bool uksat::PartialMap::istrue(int var) const {
    int normalizedlit = uksat_NORMALLIT(var);
    return normalizedlit-- ? uksat_BOOLVAL(var, map[normalizedlit].truth) : false;
}


bool uksat::PartialMap::isfalse(int var) const {
    int normalizedlit = uksat_NORMALLIT(var);
    return normalizedlit-- ? !uksat_BOOLVAL(var, map[normalizedlit].truth) : false;
}


int
uksat::PartialMap::gettime(int normvar) {
    int normalizedlit = uksat_NORMALLIT(normvar);
    return normalizedlit-- ? map[normalizedlit].truthtime : 0;
}


bool uksat::PartialMap::get(int var) const {
    int normalizedlit = uksat_NORMALLIT(var);
    return normalizedlit-- && map[normalizedlit].truth ? uksat_BOOLVAL(var, map[normalizedlit].truth) : false;
}


int uksat::PartialMap::sat(int var) const {
    int normalizedlit = uksat_NORMALLIT(var);
    return normalizedlit-- && map[normalizedlit].truth ? uksat_NORMALVAL(var, map[normalizedlit].truth) : 0;
    
}

void uksat::PartialMap::copy(std::map<int, bool>& other) const {
    for (int var = 1; var < getnvars(); var++)
        if (isassigned(var)) other[var] = get(var);
}


