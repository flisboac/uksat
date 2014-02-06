#!/usr/bin/env python

GnuPlotData = {
	'plotfilename': 'plot.gnu',
	'width': 500,
	'height': 500,
	'ywinx': 50,
	'ywiny': 450,
	'xwinx': 450,
	'xwiny': 50,
	'xmin': 0.0,
	'xmax': 10.0,
	'ymin': 0.0,
	'ymax': 10.0,
	'plotname': "Speed Improvements' Benchmark for Watched Literals",
	'datafilename': "plot.dat",
	'outfilename': "plot.png",
	'xwins': 0,
	'ywins': 0
}


GnuPlotTemplate = """
set terminal pngcairo transparent enhanced font "arial,10" fontscale 1.0 size %(width)d, %(height)d 
set output '%(outfilename)s'
set title "%(plotname)s"
unset key
set xlabel "normal timings (seconds)"
set ylabel "watchedlits timings (seconds)"
set label 1 "watchedlits > normal for %(ywins)d runs" at %(ywinx)d, %(ywiny)d
set label 2 "normal > watchedlits for %(xwins)d runs" at %(xwinx)d, %(xwiny)d
plot [%(xmin)f:%(xmax)f][%(ymin)f:%(ymax)f] x, '%(datafilename)s' using 1:2 ti '%(datafilename)s'
"""


class Runner(object):
	SAT = 1
	UNSAT = 2
	ERROR = 0
	UNDEF = -1
	TIMEOUT = -2
	RESULTSTR = {
		SAT: "T",
		UNSAT: "F",
		UNDEF: "?",
		TIMEOUT: "*",
	}
	ERRORSTR = "!"

	Type = "runner"
	CmdName = 'echo'
	CmdFormat = ['%(cmdname)s', '%(inputpath)s']
	FormatStr = "%(type)s = %(resultstr)s (%(exitcode)d, %(time)gs)"

	def __init__(self, inputpath, **opts):
		from os import path
		self.type = self.__class__.Type
		self.timelimit = opts.get('timelimit', 0)
		self.cmdname = opts.get(self.type + '_cmdname', opts.get('cmdname', self.__class__.CmdName))
		self.inputpath = inputpath
		self.result = Runner.ERROR
		self.time = 0
		self.exitcode = 0
		self.process = None
		try:
			self.cmdformat = self.__class__.CmdFormat
		except:
			self.cmdformat = Runner.CmdFormat
		try:
			self.formatstr = self.__class__.FormatStr
		except:
			self.formatstr = Runner.FormatStr
		self.cmd = self.makecmd()
	#def poll(self):
	#	if not self.process:
	#		self.load()
	#	exitcode = self.process.poll()
	#	if exitcode is not None:
	#		try:
	#			(self.out, self.err) = self.process.communicate()
	#		except ValueError:
	#			#if self.process.stderr: help(self.process.stderr)
	#			self.out = (self.process.stdout and self.process.stdout.readable()) and self.process.stdout.read() or None
	#			self.err = (self.process.stderr and self.process.stderr.readable()) and self.process.stderr.read() or None
	#		self.exitcode = exitcode
	#		#print(self.type, self.inputpath, self.exitcode, self.process.stderr, self.err, self.cmd)
	#		self.parse()
	#	return exitcode
	def poll(self, wait_time = 0.01):
		from subprocess import TimeoutExpired
		exitcode = None
		if not self.process:
			self.load()
		try:
			(self.out, self.err) = self.process.communicate(timeout = wait_time)
			exitcode = self.exitcode = self.process.returncode
			self.parse()
		except TimeoutExpired:
			pass
		return exitcode
	def wait(self):
		print(self.type, self.inputpath)
		(self.out, self.err) = self.process.communicate()
		self.exitcode = self.process.returncode
		self.parse()
		return self.exitcode
	def makecmd(self):
		from copy import copy
		cmd = copy(self.cmdformat)
		for idx in range(len(cmd)): cmd[idx] = cmd[idx] % self.__dict__
		return cmd
	def finish(self, result):
		self.result = result
		self.resultstr = Runner.RESULTSTR.get(result, Runner.ERRORSTR)
		return self.result
	def isgood(self):
		return self.result == Runner.SAT or self.result == Runner.UNSAT
	def __str__(self):
		return self.formatstr % self.__dict__
	def __repr__(self): # This is quite wrong...
		return self.formatstr % self.__dict__


class UksatRunner(Runner):
	SatCode = 10
	UnsatCode = 20
	TimeoutCode = 40
	UndefCode = 30
	import re
	Type = "uksat"
	CmdName = 'uksat'
	CmdFormat = ['%(cmdname)s', '-t', '%(timelimit)d', '%(inputpath)s']
	OutputRegex = re.compile(r'(SATISFIABLE|UNSATISFIABLE|TIMEOUT|UNDEFINED)\s+([\w\.\-]+)\s+(-?\d+)')
	def load(self):
		import subprocess
		dir(subprocess)
		self.process = subprocess.Popen(self.cmd, stdout = subprocess.DEVNULL, stderr = subprocess.PIPE)
	def parse(self):
		self.err = self.err.decode().strip()
		match = UksatRunner.OutputRegex.match(self.err)
		self.finish(Runner.ERROR)
		self.sattext = ""
		self.sat = None
		if match:
			self.sattext = match.group(1)
			self.time = float(match.group(2))
			self.sat = int(match.group(3))
			if self.exitcode == UksatRunner.SatCode:#if self.sattext == "SATISFIABLE":
				self.finish(Runner.SAT)
			elif self.exitcode == UksatRunner.UnsatCode:#elif self.sattext == "UNSATISFIABLE":
				self.finish(Runner.UNSAT)
			elif self.exitcode == UksatRunner.TimeoutCode:#elif self.sattext == "TIMEOUT":
				self.finish(Runner.TIMEOUT)
			elif self.exitcode == UksatRunner.UndefCode:#elif self.sattext == "UNDEFINED":
				self.finish(Runner.UNDEF)
		#else:
		#	print(self.inputpath, self.exitcode)



class UksatNowatchRunner(UksatRunner):
	Type = "nowatch"
	CmdName = UksatRunner.CmdName
	CmdFormat = ['%(cmdname)s', '-W', '-t', '%(timelimit)d', '%(inputpath)s']


class MinisatRunner(Runner):
	SatCode = 10
	UnsatCode = 20
	TimeoutCode = 0

	Type = "minisat"
	CmdName = "minisat"
	CmdFormat = ['%(cmdname)s', '-verb=0', '-cpu-lim=%(timelimit)d', '%(inputpath)s']#, '%(discardoutputs)s']

	def __init__(self, *args, **kargs):
		import os
		Runner.__init__(self, *args, **kargs)
		if not self.timelimit:
			self.timelimit = 0#2147483647
		self.cmd = self.makecmd()
	def load(self):
		import subprocess
		self.process = subprocess.Popen(self.cmd, stdout = subprocess.DEVNULL, stderr = subprocess.DEVNULL)
	def parse(self):
		if self.exitcode == MinisatRunner.SatCode:
			self.finish(Runner.SAT)
		elif self.exitcode == MinisatRunner.UnsatCode:
			self.finish(Runner.UNSAT)
		elif self.exitcode == MinisatRunner.TimeoutCode:
			self.finish(Runner.TIMEOUT)
		else:
			self.finish(Runner.ERROR)


class RunnerComparer(object):
	WIN = 2
	PASS = 1
	ERROR = 0
	FAIL = -1
	TIMEOUT = -2
	def __init__(self, inputname, **opts):
		from os import path
		self.options = opts
		self.cmpidx = 0
		self.cmptotals = 0
		self.formula = None
		self.timelimit = opts.get('timelimit', 0)
		self.inputname = inputname
		self.inputpath = path.abspath(path.expanduser(path.expandvars(self.inputname)))
		optrunner = opts.get('optrunner', None)
		chkrunner = opts.get('chkrunner', None)
		valrunner = opts.get('valrunner', None)
		self.optrunner = optrunner and optrunner(self.inputpath, **opts) or None
		self.chkrunner = chkrunner and chkrunner(self.inputpath, **opts) or None
		self.valrunner = valrunner and valrunner(self.inputpath, **opts) or None
		self.runners = []
		if self.optrunner: self.runners.append(self.optrunner)
		if self.chkrunner: self.runners.append(self.chkrunner)
		if self.valrunner: self.runners.append(self.valrunner)
		self.result = RunnerComparer.ERROR
		self.chkresult = RunnerComparer.ERROR
		self.valresult = RunnerComparer.ERROR
		self.deltatime = 0
	def load(self):
		for runner in self.runners:
			runner.load()
	def poll(self):
		finished = None
		processes = set(self.runners)
		finishedprocs = set()
		for proc in processes:
			if proc.poll() is not None:
				finishedprocs.add(proc)
		processes -= finishedprocs
		if not processes:
			finished = True
			self.parse()
		return finished
	def wait(self):
		processes = set(self.runners)
		while processes:
			import time
			finishedprocs = set()
			for proc in processes:
				if proc.poll() is not None:
					finishedprocs.add(proc)
			processes -= finishedprocs
			if processes:
				time.sleep(0.1)
		self.parse()
	def parse(self):
		# CHECKING CONSISTENCY
		sep = ""
		if self.optrunner and self.chkrunner:
			sep = ", "
			self.deltatime = self.chkrunner.time - self.optrunner.time
			self.runner = self.optrunner

			# if the check runner has timed out, the optimized runner has partially won
			if self.chkrunner.result == Runner.TIMEOUT:
				self.consistent = True
				# if the optimized runner also has timed out, it's a draw
				if self.optrunner.result == Runner.TIMEOUT:
					self.result = RunnerComparer.TIMEOUT
					self.resultstr = "CONSISTENT"
				else:
					self.resultstr = "FAST"
					self.result = RunnerComparer.WIN
			# If the check runner couldn't solve the formula, it's also a
			# partial win for the optimized runner
			# Either way, something is wrong; the checker should not yield undefined.
			elif self.chkrunner.result == Runner.UNDEF:
				if self.optrunner.result == Runner.UNDEF:
					self.result = RunnerComparer.FAIL
					self.consistent = True
					self.resultstr = "CONSISTENT"
				elif self.optrunner.isgood():
					self.result = RunnerComparer.PASS
					self.consistent = False
					self.resultstr = "ODD"
				else:
					self.result = RunnerComparer.FAIL
					self.consistent = False
					self.resultstr = "INCONSISTENT"
			# In this case, both yielded the same results
			elif self.chkrunner.result == self.optrunner.result:
				self.consistent = True
				if self.deltatime >= 0:
					self.result = RunnerComparer.WIN
					self.resultstr = "FASTER"
				else:
					self.result = RunnerComparer.PASS
					self.resultstr = "SLOWER"
			# Optimized version is too slow
			elif self.optrunner == Runner.TIMEOUT:
				self.consistent = False
				self.result = RunnerComparer.TIMEOUT
				self.resultstr = "SLOW"
			# Different results
			else:
				self.consistent = False
				self.result = RunnerComparer.FAIL
				self.resultstr = "INCONSISTENT"
		elif self.optrunner or self.chkrunner:
			#sep = ", "
			self.deltatime = 0
			self.runner = self.optrunner or self.chkrunner
			self.consistent = True
			self.resultstr = "" #self.resultstr = "INCONSISTENT"
			if self.runner == Runner.TIMEOUT:
				self.result = RunnerComparer.TIMEOUT
			elif self.runner.isgood():
				self.result = RunnerComparer.WIN
			else:
				self.result = RunnerComparer.FAIL
		else:
			self.deltatime = 0
			self.runner = None
			self.consistent = True
			self.resultstr = ""

		# CHECKING CORRECTNESS
		if self.runner:
			if self.valrunner:
				if self.valrunner.isgood():
					if self.consistent:
						#print(self.inputpath, self.valrunner.result, self.runner.result, self.result)
						if self.valrunner.result == self.runner.result:
							if self.iswin():
								self.resultstr = "      WIN" + sep + self.resultstr
							elif self.ispass():
								self.resultstr = "     PASS" + sep + self.resultstr
							else:
								self.resultstr = "     FAIL" + sep + self.resultstr
						else:
							self.resultstr = "     FAIL" + sep + self.resultstr
					else:
						if self.valrunner.result == self.runner.result and self.isgood():
							self.resultstr = "     PASS" + sep + self.resultstr
						else:
							self.resultstr = "     FAIL" + sep + self.resultstr
				else:
					if self.consistent and self.iswin():
						self.resultstr = " SELF WIN" + sep + self.resultstr
					elif (self.consistent and self.isgood()) or (not self.consistent and self.isgood()):
						self.resultstr = "SELF PASS" + sep + self.resultstr
					else:
						self.resultstr = "SELF FAIL" + sep + self.resultstr
			else:
				if self.iswin():
					self.resultstr = " SELF WIN" + sep + self.resultstr
				elif self.ispass():
					self.resultstr = "SELF PASS" + sep + self.resultstr
				else:
					self.resultstr = "SELF FAIL" + sep + self.resultstr
		else:
			if self.valrunner:
				if self.valrunner.isgood():
					self.result = RunnerComparer.WIN
					self.resultstr = "      WIN"
				elif self.valrunner.result == Runner.TIMEOUT:
					self.result = RunnerComparer.TIMEOUT
					self.resultstr = "     FAIL, TIMEOUT"
				else:
					self.result = RunnerComparer.FAIL
					self.resultstr = "     FAIL"
			else:
				self.result = RunnerComparer.ERROR
				self.reultstr = "    ERROR"
	def iserror(self):
		return not self.result
	def iswin(self):
		return self.result == self.__class__.WIN
	def ispass(self):
		return self.iswin() or self.result == self.__class__.PASS
	def isgood(self):
		return self.result > 0
	def isfail(self):
		return self.result < 0
	def isbad(self):
		return self.result <= 0
	def __str__(self):
		return "%(resultstr)s delta = %(deltatime)f, %(runners)s" % self.__dict__


class RunnerBatch(object):
	def __init__(self, *pathnames, **options):
		import cnfstat
		self.pathnames = pathnames
		self.options = options
		self.reader = cnfstat.FastCnfReader(*pathnames, **options)
		self.formulae = list(self.reader.formulae) # kinda implicit, but formulae sorting happens here
		self.formulae.sort()
		self.results = {
			'timelimits': [0, 0],
			'wins': [],
			'passes': [],
			'fails': [],
		}
		self.maxrunners = options.get('maxrunners', 1)
	def run(self):
		totalformulae = len(self.formulae)
		formulaidx = 0
		runners = []
		formulae = list(self.formulae)
		while formulae or runners:
			import time
			while formulae and len(runners) < self.maxrunners:
				formula = formulae.pop(0)
				formulaidx += 1
				comparer = RunnerComparer(formula.abspath, **self.options)
				comparer.cmpidx = formulaidx
				comparer.cmptotals = totalformulae
				comparer.formula = formula
				runners.append(comparer)
				comparer.load()
			if runners:
				finished_runners = []
				for runner in runners:
					#if runner.poll():
					#	finished_runners.append(runner)
					runner.wait()
					finished_runners.append(runner)
				for finished_runner in finished_runners:
					runners.remove(finished_runner)
					self.onfinishedrunner(finished_runner)
			time.sleep(0.1)
	def onfinishedrunner(self, runner):
		if 'onfinishedrunner' in self.options:
			return self.options['onfinishedrunner'](runner)


if __name__ == '__main__':
	pass