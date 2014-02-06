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


void uksat::PartialMap::assign(int var, int truth) {
    int idx = uksat_NORMALLIT(var) - 1;
    int val = map[idx].val;
    if (val && !truth) mapsize--;
    if (!val && truth) mapsize++;
    //std::cerr << "ASSIGN var = " << (idx + 1) << ", truth = " << truth << std::endl;
    map[idx].val = truth;
}


void uksat::PartialMap::assign(int var, bool truth) {
    assign(var, truth ? 1 : -1);
}


void uksat::PartialMap::assign(int var) {
    assign(var, var < 0 ? -1 : 1);
}


void uksat::PartialMap::unassign(int var) {
    assign(var, 0);
}


bool uksat::PartialMap::isassigned(int var) const {
    int normalizedlit = uksat_NORMALLIT(var);
    return normalizedlit-- ? !!map[normalizedlit].val : false;
}


bool uksat::PartialMap::istrue(int var) const {
    int normalizedlit = uksat_NORMALLIT(var);
    return normalizedlit-- ? uksat_BOOLVAL(var, map[normalizedlit].val) : false;
}


bool uksat::PartialMap::isfalse(int var) const {
    int normalizedlit = uksat_NORMALLIT(var);
    return normalizedlit-- ? !uksat_BOOLVAL(var, map[normalizedlit].val) : false;
}


bool uksat::PartialMap::get(int var) const {
    int normalizedlit = uksat_NORMALLIT(var);
    return normalizedlit-- && map[normalizedlit].val ? uksat_BOOLVAL(var, map[normalizedlit].val) : false;
}


int uksat::PartialMap::sat(int var) const {
    int normalizedlit = uksat_NORMALLIT(var);
    return normalizedlit-- && map[normalizedlit].val ? uksat_NORMALVAL(var, map[normalizedlit].val) : 0;
    
}

void uksat::PartialMap::copy(std::map<int, bool>& other) const {
    for (int var = 1; var < getnvars(); var++)
        if (isassigned(var)) other[var] = get(var);
}


