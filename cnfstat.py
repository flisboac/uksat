#!/usr/bin/env python


FileExts = ['dimacs', 'cnf']


class CnfClause(object):
	def __init__(self, idx, line = "", lineno = 0, formula = None):
		self.idx = idx
		self.line = line
		self.lineno = lineno
		self.valid = False
		self.formula = formula
		self.load()
	def __str__(self):
		return """{0[idx]}@{0[lineno]}""".format(self.__dict__)
	def __repr__(self):
		return self.__str__()


class CnfFormula(object):
	import re
	ProblemLineRegex = re.compile(r'^\s*[Pp]\s+[Cc][Nn][Ff]\s+(\d+)\s+(\d+)')
	def __init__(self, pathname = "", contents = ""):
		import io
		from os import path
		self.clear()
		self.pathname = pathname
		self.contents = contents
		self.abspath = path.abspath(path.expanduser(path.expandvars(self.pathname)))
		self.basename = path.basename(self.abspath)
		self.dirname = path.dirname(self.abspath)
		self.load()
	def openfile(self):
		if self.contents:
			f = io.StringIO(self.contents)
		else:
			f = open(self.abspath)
		return f
	def clear(self):
		self.pathname = ""
		self.abspath = ""
		self.basename = ""
		self.dirname = ""
		self.valid = False
		self.line = ""
		self.lineno = 0
		self.nvars = 0
		self.nclauses = 0
		self.minclausesize = 0
		self.maxclausesize = 0
	def compare(self, other):
		cmpval = None
		if isinstance(other, self.__class__):
			cmpval = 0
			if self.nclauses == other.nclauses:
				if self.nvars < other.nvars:
					cmpval = -1
				elif self.nvars > other.nvars:
					cmpval = 1
			elif self.nclauses > other.nclauses:
				cmpval = 1
			else:
				cmpval = -1
		return cmpval
	def tostr(self):
		return self.__str__()
	def __str__(self):
		return "{0[abspath]}: nv={0[nvars]}, nc={0[nclauses]}, cs={0[minclausesize]}..{0[maxclausesize]}".format(self.__dict__)
	def __nonzero__(self):
		return self.valid
	def __hash__(self):
		return self.hash(self.abspath)
	def __eq__(self, other):
		cmpval = self.compare(other)
		return cmpval is not None and cmpval == 0
	def __ne__(self, other):
		cmpval = self.compare(other)
		return cmpval is not None and cmpval != 0
	def __le__(self, other):
		cmpval = self.compare(other)
		return cmpval is not None and cmpval <= 0
	def __lt__(self, other):
		cmpval = self.compare(other)
		return cmpval is not None and cmpval < 0
	def __ge__(self, other):
		cmpval = self.compare(other)
		return cmpval is not None and cmpval >= 0
	def __gt__(self, other):
		cmpval = self.compare(other)
		return cmpval is not None and cmpval > 0


class StatCnfClause(CnfClause):
	def __init__(self, idx, line, lineno, formula):
		self.truevars = set()
		self.falsevars = set()
		self.vars = set()
		self.tautology = False
		self.contradiction = False
		CnfClause.__init__(self, idx, line, lineno, formula)
	def load(self):
		self.valid = True
		for var in self.line.split():
			var = int(var)
			normvar = var < 0 and -var or var
			if var > 0:
				self.truevars.add(var)
				if -var in self.falsevars:
					self.tautology = true
			elif var < 0:
				self.falsevars.add(var)
				if -var in self.truevars:
					self.tautology = true
			else:
				break
			self.vars.add(var)
			if normvar < 0 or normvar > self.formula.nvars:
				self.valid = False
		self.size = len(self.vars)
		if len(self.vars) == 0:
			self.contradiction = True


class StatCnfFormula(CnfFormula):
	import re
	ClauseLineRegex = re.compile(r'^\s*(((-?\d+)(\s+-?\d+)*\s+)?0)\s*$')
	def __init__(self, *args, **kargs):
		self.clauses = list()
		CnfFormula.__init__(self, *args, **kargs)
	def load(self):
		lineno = 0
		clauseidx = 0
		try:
			f = self.openfile()
			for line in f.readlines():
				lineno += 1
				if not self.nvars:
					match = self.__class__.ProblemLineRegex.match(line)
					if match:
						self.lineno = lineno
						self.line = line.strip()
						self.nvars = int(match.group(1))
						self.nclauses = int(match.group(2))
				else:
					match = self.__class__.ClauseLineRegex.match(line)
					if match:
						clause = StatCnfClause(clauseidx, match.group(1).strip(), lineno, self)
						if clause.valid:
							self.clauses.append(clause)
							for var in clause.truevars: self.stats['truevars'].add(var)
							for var in clause.falsevars: self.stats['falsevars'].add(var)
							if clause.size not in self.stats['persize']:
								self.stats['persize'][clause.size] = list()
							if clause.size not in self.stats['sizes']:
								self.stats['sizes'][clause.size] = 1
							else:
								self.stats['sizes'][clause.size] += 1
							self.stats['persize'][clause.size].append(clause)
							if clause.tautology:
								self.stats['tautologies'].append(clause)
							if clause.contradiction:
								self.stats['contradictions'].append(clause)
							if not self.minclausesize or self.minclausesize > clause.size:
								self.minclausesize = clause.size
							if not self.maxclausesize or self.maxclausesize < clause.size:
								self.maxclausesize = clause.size
						else:
							self.clauses.append(clause)
							self.stats['invalids'].append(clause)
			f.close()
			self.stats['purevars'] = self.stats['truevars'] ^ self.stats['falsevars']
			if self.nvars >= 0 and self.nclauses == len(self.clauses) and len(self.stats['invalids']) == 0:
				self.valid = True
			if len(self.stats['tautologies']) == len(self.clauses):
				self.tautology = True
		except IOError:
			self.clear()
	def tostr(self):
		return """{0[abspath]}:
	nvars = {0[nvars]},
	nclauses = {0[nclauses]},
	valid = {0[valid]},
	tautology = {0[tautology]},
	minclausesize = {0[minclausesize]},
	maxclausesize = {0[maxclausesize]},
	clausesizes = {0[stats][sizes]},
	invalids = {0[stats][invalids]},
	tautologies = {0[stats][tautologies]},
	truevars = {0[stats][truevars]},
	falsevars = {0[stats][falsevars]},
	purevars = {0[stats][purevars]}""".format(self.__dict__)
	def clear(self):
		CnfFormula.clear(self)
		self.clauses = list()
		self.stats = {
			'truevars': set(),
			'falsevars': set(),
			'purevars': set(),
			'tautologies': list(),
			'contradictions': list(),
			'invalids': list(),
			'sizes': {},
			'persize': {},
		}

"""
class StatCnfFormula(object):
	import re
	ProblemLineRegex = re.compile(r'^\s*[Pp]\s+[Cc][Nn][Ff]\s+(\d+)\s+(\d+)')
	ClauseLineRegex = re.compile(r'^\s*(((-?\d+)(\s+-?\d+)*\s+)?0)\s*$')
	def __init__(self, pathname = "", contents = ""):
		import io
		from os import path
		self.clear()
		self.pathname = pathname
		self.abspath = path.abspath(path.expanduser(path.expandvars(self.pathname)))
		self.basename = path.basename(self.abspath)
		self.dirname = path.dirname(self.abspath)
		lineno = 0
		clauseidx = 0
		try:
			if contents:
				f = io.StringIO(contents)
			else:
				f = open(self.abspath)
			for line in f.readlines():
				lineno += 1
				if not self.nvars:
					match = self.__class__.ProblemLineRegex.match(line)
					if match:
						self.lineno = lineno
						self.line = line.strip()
						self.nvars = int(match.group(1))
						self.nclauses = int(match.group(2))
				else:
					match = self.__class__.ClauseLineRegex.match(line)
					if match:
						clause = StatCnfClause(clauseidx, match.group(1).strip(), lineno, self.nvars)
						if clause.valid:
							self.clauses.append(clause)
							for var in clause.truevars: self.stats['truevars'].add(var)
							for var in clause.falsevars: self.stats['falsevars'].add(var)
							if clause.size not in self.stats['persize']:
								self.stats['persize'][clause.size] = list()
							if clause.size not in self.stats['sizes']:
								self.stats['sizes'][clause.size] = 1
							else:
								self.stats['sizes'][clause.size] += 1
							self.stats['persize'][clause.size].append(clause)
							if clause.tautology:
								self.stats['tautologies'].append(clause)
							if clause.contradiction:
								self.stats['contradictions'].append(clause)
							if self.stats['minclausesize'] > clause.size:
								self.stats['minclausesize'] = clause.size
							if self.stats['maxclausesize'] < clause.size:
								self.stats['maxclausesize'] = clause.size
						else:
							self.clauses.append(clause)
							self.stats['invalids'].append(clause)
			f.close()
			self.stats['purevars'] = self.stats['truevars'] ^ self.stats['falsevars']
			if self.nvars >= 0 and self.nclauses == len(self.clauses) and len(self.stats['invalids']) == 0:
				self.valid = True
			if len(self.stats['tautologies']) == len(self.clauses):
				self.tautology = True
		except IOError:
			self.clear()
	def clear(self):
		self.pathname = ""
		self.abspath = ""
		self.basename = ""
		self.dirname = ""
		self.valid = False
		self.line = ""
		self.lineno = 0
		self.nvars = 0
		self.nclauses = 0
		self.clauses = list()
		self.tautology = False
		self.stats = {
			'truevars': set(),
			'falsevars': set(),
			'purevars': set(),
			'minclausesize': 0,
			'maxclausesize': 0,
			'tautologies': list(),
			'contradictions': list(),
			'invalids': list(),
			'sizes': {},
			'persize': {},
		}
	def tostr(self):
		return "{0[abspath]}:
	nvars = {0[nvars]},
	nclauses = {0[nclauses]},
	valid = {0[valid]},
	tautology = {0[tautology]},
	minclausesize = {0[stats][minclausesize]},
	maxclausesize = {0[stats][maxclausesize]},
	clausesizes = {0[stats][sizes]},
	invalids = {0[stats][invalids]},
	tautologies = {0[stats][tautologies]},
	truevars = {0[stats][truevars]},
	falsevars = {0[stats][falsevars]},
	purevars = {0[stats][purevars]}".format(self.__dict__)
	def __str__(self):
		return "{0[abspath]}: nv={0[nvars]}, nc={0[nclauses]}, cs={0[stats][minclausesize]}..{0[stats][maxclausesize]}".format(self.__dict__)
	def __nonzero__(self):
		return self.valid
	def __hash__(self):
		return self.hash(self.abspath)
	def __eq__(self, other):
		cmpval = self.compare(other)
		return cmpval is not None and cmpval == 0
	def __ne__(self, other):
		cmpval = self.compare(other)
		return cmpval is not None and cmpval != 0
	def __le__(self, other):
		cmpval = self.compare(other)
		return cmpval is not None and cmpval <= 0
	def __lt__(self, other):
		cmpval = self.compare(other)
		return cmpval is not None and cmpval < 0
	def __ge__(self, other):
		cmpval = self.compare(other)
		return cmpval is not None and cmpval >= 0
	def __gt__(self, other):
		cmpval = self.compare(other)
		return cmpval is not None and cmpval > 0
	def compare(self, other):
		cmpval = None
		if isinstance(other, self.__class__):
			cmpval = 0
			if self.nclauses == other.nclauses:
				if self.nvars < other.nvars:
					cmpval = -1
				elif self.nvars > other.nvars:
					cmpval = 1
			elif self.nclauses > other.nclauses:
				cmpval = 1
			else:
				cmpval = -1
		return cmpval
"""

class FastCnfFormula(CnfFormula):
	import re
	ClauseLineRegex = re.compile(r'\s*-?\d')
	def load(self):
		lineno = 0
		clauseidx = 0
		try:
			f = self.openfile()
			for line in f.readlines():
				lineno += 1
				if not self.nvars:
					match = CnfFormula.ProblemLineRegex.match(line)
					if match:
						self.lineno = lineno
						self.line = line.strip()
						self.nvars = int(match.group(1))
						self.nclauses = int(match.group(2))
				elif line and FastCnfFormula.ClauseLineRegex.match(line):
						varlist = line.split()
						clausesize = len(varlist) - 1
						if clausesize > 0:
							if not self.minclausesize or self.minclausesize > clausesize:
								self.minclausesize = clausesize
							if not self.maxclausesize or self.maxclausesize < clausesize:
								self.maxclausesize = clausesize
			f.close()
			if self.nvars >= 0 and self.nclauses >= 0:
				self.valid = True
		except IOError:
			self.clear()


class CnfReader(object):
	FormulaType = StatCnfFormula
	def __init__(self, *pathnames, **options):
		import os, fnmatch
		from os import path
		self.recursive = getattr(options, 'recursive', True)
		self.filter = getattr(options, 'filter', None)
		self.pathnames = pathnames
		self.formulae = list()
		for pathname in self.pathnames:
			pathname = path.abspath(path.expanduser(path.expandvars(pathname)))
			formula = None
			if path.isdir(pathname):
				matches = []
				for root, dirnames, filenames in os.walk(pathname):
					for ext in FileExts:
						for filename in fnmatch.filter(filenames, '*.' + ext):
							matches.append(os.path.join(root, filename))
				for pathname in matches:
					formula = self.__class__.FormulaType(pathname)
					if formula and (not self.filter or self.filter(formula)):
						self.formulae.append(formula)
			else:
				formula = self.__class__.FormulaType(pathname)
				if formula and (not self.filter or self.filter(formula)):
					self.formulae.append(formula)
		self.formulae.sort()


class FastCnfReader(CnfReader):
	FormulaType = FastCnfFormula


if __name__ == '__main__':
	import sys
	reader = FastCnfReader(*sys.argv[1:])
	for formula in reader.formulae:
		print(formula.tostr())
		print()




