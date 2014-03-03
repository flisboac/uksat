#!/usr/bin/env python

APPNAME = 'uksat'
VERSION = '0.2.0'
MAJOR = 0

top = '.'
out = 'build'
src = 'src'
inc = 'include'
ext = 'ext'
ext_include = ext + '/include'
spec = 'spec'

APPNAME = 'uksat'
LIBNAME = APPNAME + str(MAJOR)
SPECNAME = APPNAME + '_spec'

BUILDS = {
	'all': {
		'all': {
			#'load': 'tool',
			#'load': ['tool1', 'tool2', {'input': 'tool1', 'mandatory': True}]
			'load': 'compiler_cxx',
			'find_program': [
				{'filename': 'diff', 'mandatory': True},
				{'filename': 'minisat', 'mandatory': True},
				{'filename': 'gnuplot', 'mandatory': True},
			],
			'includes': [inc, src, ext_include],
			'cxxflags': [],#['-Wall'],
			'linkflags': [],
		},

		LIBNAME: {
			'features': 'cxx cxxstlib',
			'source': [
                src + '/cnf.cpp',
                src + '/map.cpp',
                src + '/simple.cpp',
                src + '/solver.cpp',
                src + '/watched.cpp',
            ],
		},

		APPNAME: {
			'features': 'cxx cxxprogram',
			'use': LIBNAME,
			'source': [src + '/main.cpp'],
		},

		#SPECNAME: {
		#	'features': 'cxxprogram',
		#	'use': APPNAME,
		#	'source': [spec + '/uksat_spec.cpp'],
		#}
	},

	'debug': {
		LIBNAME: {
			'cxxflags': ['-g', '-O1'],
		},
	},

	'release': {
		'all': {
			'cxxflags': ['-O2', '-s']
		}
	},

	'profile': {
		'all': {
			'cxxflags': ['-O2', '-g']
		},
	}
}


from waflib.Build import BuildContext
from waflib.Task import Task


def options(ctx):
	ctx.load('wafbuild')
	ctx.load('cnfexec')

	ctx.add_option('-O', '--output', action='store', default='-',
		help='Specifies the output file or folder.')
	ctx.add_option('-F', '--folder', action='store', default="formulae",
		help='Specifies the input file, or folder containing formulae for batch tests.')
	ctx.add_option('-R', '--runs', action='store', default=1, type='int',
		help='Specifies how many runs will be done over the inputs while generating stats.')
	ctx.add_option('-C', '--maxclauses', action='store', default=0, type='int',
		help='Defines the maximum number of clauses a formula must have for tests.')
	ctx.add_option('-V', '--maxvars', action='store', default=0, type='int',
		help='Defines the maximum number of variables a formula must have for tests.')
	ctx.add_option('-L', '--timelimit', action='store', default=600, type='int',
		help='Sets up the time limit for solving, in seconds. Defaults to 10 minutes.')
	ctx.add_option('-M', '--cmpmode', action='store', default="uvw",
		help="Specifies how to compare. 'u' activates non-watched; 'v' activates validator; 'w' activates watched. Uppercase letters deactivate.")


def configure(ctx):
	ctx.load('wafbuild')
	ctx.load('cnfexec')


def build(ctx):
	ctx.load('wafbuild')
	ctx.load('cnfexec')


#def distclean(ctx):
#	import shutil
#	pycache_node = ctx.path.find_node('__pycache__')
#	shutil.rmtree(pycache_node.abspath())
#	for node in ctx.path.ant_glob('*.log'):
#		node.delete()
#	ctx.execute()


class BatchContext(BuildContext):
	cmd = 'batch'
	fun = 'batch'
def batch(ctx):
	import cnfexec
	from cnfexec import RunnerBatch, UksatRunner, UksatNowatchRunner, MinisatRunner
	from cnfexec import GnuPlotData
	from os import path
	datafilename = path.abspath(GnuPlotData['datafilename'])
	dirpath = ctx.options.folder
	_copy_uksat(ctx)
	f = open(datafilename, 'w')
	try:
		options = {
			'timelimit': ctx.options.timelimit,
			'maxrunners': ctx.options.jobs,
			'minisat_cmdname': ctx.env.MINISAT,
			'uksat_cmdname': ctx.uksat_node.abspath(),
			'optrunner': ctx.options.cmpmode.count('w') and UksatRunner or None,
			'chkrunner': ctx.options.cmpmode.count('u') and UksatNowatchRunner or None,
			'valrunner': ctx.options.cmpmode.count('v') and MinisatRunner or None,
			'uksat_cmdname': 'build/uksat',
			'nowatch_cmdname': 'build/uksat',
			'onfinishedrunner': lambda runner: _printrunner(ctx, runner),
		}
		if ctx.options.maxclauses:
			options['filter'] = lambda formula: formula.nclauses <= ctx.options.maxclauses
		if ctx.options.maxvars:
			if 'filter' in options:
				prevfilter = options['filter']
				options['filter'] = lambda formula: prevfilter(formula) and formula.nvars <= ctx.options.maxvars
			else:
				options['filter'] = lambda formula: formula.nvars <= ctx.options.maxvars
		print("* Looking for formulae...")
		runnerbatch = RunnerBatch(dirpath, **options)
		print("* Found %d formulae. Starting batch tests..." % len(runnerbatch.formulae))
		print()
		runnerbatch.datafile = f
		runnerbatch.run()
		print("* Writing result (plot) files...")
		runnerbatch.writeplotfiles()
		print("* Finished.")
	except KeyboardInterrupt:
		print("* Interrupted!")
	finally:
		ctx.uksat_node.delete()
		f.close()


class GraphContext(BuildContext):
	cmd = 'graph'
	fun = 'graph'
def graph(ctx):
	from cnfexec import GnuPlotData
	from os import path
	import subprocess
	plotfilename = path.abspath(GnuPlotData['plotfilename'])
	if not path.exists(plotfilename):
		ctx.fatal("Please run `waf batch` first.")
	gnu_proc = subprocess.Popen([ctx.env.GNUPLOT, plotfilename])
	gnu_proc.wait()
	if gnu_proc.returncode:
		ctx.fatal("ERROR: Could not create graphic!")


def parselog_full(ctx):
	import re
	import os, sys
	good_pat = re.compile(r"WIN|PASS")
	win_pat = re.compile(r"WIN")
	pass_pat = re.compile(r"PASS")
	filename_pat = re.compile(r"^\[(\d+)/(\d+) nc:(\d+), nv:(\d+), cs:(\d+)..(\d+)\]\s+(.*)$")
	results_pat = re.compile(r"^\s*([\w,\s]+)\s+delta = (.*?), \[uksat = ([TF*!?]) \((\d+), (.*?)s\), nowatch = ([TF*!?]) \((\d+), (.*?)s\), minisat = ([TF*!?]) \((\d+), (.*?)s\)\]\s*$")
	filename_match = None
	runlist = []
	with open(ctx.options.folder) as inputf:
		for line in inputf.readlines():
			if filename_match:
				results_match = results_pat.match(line)
				if results_match:
					rundata = {
						'formulaidx': int(filename_match.group(1)),
						'totalformulae': int(filename_match.group(2)),
						'nclauses': int(filename_match.group(3)),
						'nvars': int(filename_match.group(4)),
						'minclausesize': int(filename_match.group(5)),
						'maxclausesize': int(filename_match.group(6)),
						'filename': filename_match.group(7),

						'resultstr': results_match.group(1),
						'deltatime': float(results_match.group(2)),
						'uksat': {
							'resultstr': results_match.group(3),
							'exitcode': int(results_match.group(4)),
							'time': float(results_match.group(5)),
						},
						'nowatch': {
							'resultstr': results_match.group(6),
							'exitcode': int(results_match.group(7)),
							'time': float(results_match.group(8)),
						},
						'minisat': {
							'resultstr': results_match.group(9),
							'exitcode': int(results_match.group(10)),
							'time': float(results_match.group(11)),
						},
					}
					runlist.append(rundata)
					filename_match = None
				else:
					ctx.fatal("Something went wrong, check me!\nfilename_line: \"%s\"\nline:\"%s\"" % (filename_line, line))
			else:
				filename_match = filename_pat.match(line)
				if filename_match: 
					filename_line = line
				else:
					filename_line = None			
	is_std = not ctx.options.output or ctx.options.output == "-"
	if is_std:
		outputf = sys.stdout
	else:
		outputf = open(ctx.options.output, "w")	
	plot_data = {
		'nwins': 0,
		'npasses': 0,
		'pmin': 0,
		'pmax': 0,
	}
	try:
		for rundata in runlist:
			if good_pat.search(rundata['resultstr']):
				outputf.write("%f; %f; %f; %d; %d; %s\n" % (
					rundata['nowatch']['time'],
					rundata['uksat']['time'],
					rundata['deltatime'],
					rundata['uksat']['exitcode'],
					rundata['nowatch']['exitcode'],
					rundata['filename'])
				)
				if not plot_data['pmin'] or plot_data['pmin'] > rundata['uksat']['time']:
					plot_data['pmin'] = rundata['uksat']['time']
				if not plot_data['pmin'] or plot_data['pmin'] > rundata['nowatch']['time']:
					plot_data['pmin'] = rundata['nowatch']['time']
				if not plot_data['pmax'] or plot_data['pmax'] < rundata['uksat']['time']:
					plot_data['pmax'] = rundata['uksat']['time']
				if not plot_data['pmax'] or plot_data['pmax'] < rundata['nowatch']['time']:
					plot_data['pmax'] = rundata['nowatch']['time']
			if win_pat.search(rundata['resultstr']):
				plot_data['nwins'] += 1
			elif pass_pat.search(rundata['resultstr']):
				plot_data['npasses'] += 1
	finally:
		if not is_std:
			outputf.close()
	if ctx.options.cmpmode:
		with open(ctx.options.cmpmode, "w") as plotf:
			base_contents = """
set title "Speed Improvements' Benchmark for Watched Literals"
unset key
set xlabel "normal timings (seconds)"
set ylabel "watchedlits timings (seconds)"
set label 1 "watchedlits > normal for %(nwins)d runs" at 50, 450
set label 2 "normal > watchedlits for %(npasses)d runs" at 450, 50
set datafile sep ';'
plot [%(pmin)f:%(pmax)f][%(pmin)f:%(pmax)f] x, 'plot.dat' using 1:2 ti 'plot.dat'
""" % plot_data
			svg_contents = """
#set terminal canvas  solid butt size 1000,1000 fsize 10 lw 1 fontscale 1 mousing
#set output 'plot.html'
set terminal svg enhanced font "arial,10" size 1000, 1000 
set output 'plot.svg'
""" + base_contents
			html_contents = """
#set terminal svg enhanced font "arial,10" size 1000, 1000 
#set output 'plot.svg'
set terminal canvas  solid butt size 1000,1000 fsize 10 lw 1 fontscale 1 mousing
set output 'plot.html'
""" + base_contents
			plotf.write(html_contents)



def _printrunner(ctx, runner):
	print("[%d/%d nc:%d, nv:%d, cs:%d..%d] %s" % (
		runner.cmpidx,
		runner.cmptotals,
		runner.formula.nvars,
		runner.formula.nclauses,
		runner.formula.minclausesize,
		runner.formula.maxclausesize,
		runner.formula.abspath)
	)
	print(runner)
	print()


def _copy_uksat(ctx):
	import uuid, shutil
	uksat_id = uuid.uuid1()
	ctx.uksat_node = ctx.bldnode.make_node(APPNAME + '-' + str(uksat_id))
	uksat_node = ctx.bldnode.find_node(APPNAME)
	shutil.copyfile(uksat_node.abspath(), ctx.uksat_node.abspath())


def _graph(ctx):
	"""Generates statistical graphs comparing the use of watched literals and non-usage.
	"""
	import os, subprocess
	
	results = _iterate_formulae(ctx, compare = False)
	gnudata = GNUPLOT_DATA
	#gnudata['plotfilename'] = 
	#gnudata['width'] = 
	#gnudata['height'] = 
	#gnudata['ywinx'] = 
	#gnudata['ywiny'] = 
	#gnudata['xwinx'] = 
	#gnudata['xwiny'] = 
	gnudata['xmin'] = results['timelimits'][0]
	gnudata['xmax'] = results['timelimits'][1] 
	gnudata['ymin'] = gnudata['xmin']
	gnudata['ymax'] = gnudata['xmax']
	#gnudata['plotname'] = 
	#gnudata['datafilename'] = 
	#gnudata['outfilename'] = 
	gnudata['xwins'] = len(results['passes'])
	gnudata['ywins'] = len(results['wins'])
	
	with open(gnudata['datafilename'], "w") as file:
		file.write("# Format is: nowatch_time uksat_time\n")
		file.write("# WINS\n")
		for data in results['wins']: file.write("%f %f\n" % (data['nowatch_time'], data['uksat_time']))
		file.write("# PASSES\n")
		for data in results['passes']: file.write("%f %f\n" % (data['nowatch_time'], data['uksat_time']))
	
	with open(gnudata['plotfilename'], "w") as file:
		file.write(GNUPLOT_TPL % gnudata)
	
	gnu_proc = subprocess.Popen([ctx.env.GNUPLOT, gnudata['plotfilename']])
	gnu_proc.wait()
	if gnu_proc.returncode:
		ctx.fatal("ERROR: Could not create graphic!")
	
