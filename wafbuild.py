#!/usr/bin/env python

#from waflib.Configure import conf
#from waflib.TaskGen   import feature


def options(ctx):
	ctx.add_option('-B', '--build', action='store', default="release",
		help='Specifies which build to run.')
	ctx.add_option('--list-builds', action='store_true',
		help='Lists all available builds and their targets (NOT IMPLEMENTED YET).')
	target = _get_all_all_target(ctx)
	tools = _get_tools(ctx, {'all': target})
	#help(ctx.load)
	for tool in tools:
		#print(tool)
		ctx.load(tool['tool'], **tool)


def configure(ctx):
	targets = _get_build_targets(ctx, include_all = False)
	tools = _get_tools(ctx, targets)
	for targetname in targets:
		print(targetname, targets[targetname])
		print()
	programs = _get_programs(ctx, targets)
	#help(ctx.load)
	for tool in tools:
		#print(tool)
		ctx.load(tool['tool'])
	for program in programs:
		ctx.find_program(**program)
	ctx.env.build = ctx.options.build


def build(ctx):
	targets = _get_build_targets(ctx)
	for targetname in targets:
		print(targetname, targets[targetname])
		print()
		ctx(**targets[targetname])


def _get_list(ctx, targets, key, defaultkey):
	values = {}
	for targetname in targets:
		target = targets[targetname]
		#print(target)
		valuelist = target.get(key, [])
		if type(valuelist) is list or type(valuelist) is tuple:
			for value in valuelist:
				if type(value) is dict:
					values[value[defaultkey]] = value
					#values.append(value)
				else:
					values[value] = {defaultkey: value}
					#values.append({defaultkey: value})
		else:
			values[valuelist] = {defaultkey: valuelist}
			#values.append({defaultkey: valuelist})
	return list(values.values())


def _get_tools(ctx, targets):
	return _get_list(ctx, targets, 'load', defaultkey = 'tool')


def _get_programs(ctx, targets):
	return _get_list(ctx, targets, 'find_program', defaultkey = 'filename')


def _get_all_all_target(ctx):
	targets = _get_build_targets(ctx, 'all', include_all = True)
	all_target = targets['all'] or {}
	return all_target


def _get_build_targets(ctx, buildname = None, include_all = False):
	from waflib import Context
	if not buildname:
		try:
			buildname = ctx.env.build
			if not buildname: buildname = ctx.options.build
		except:
			buildname = ctx.options.build

	try:
		builds = Context.g_module.BUILDS
	except:
		builds = {}

	try:
		allbuilddata = builds['all']
	except:
		allbuilddata = {}

	# It's mandatory to have the build declared.
	try:
		targetbuilddata = builds[buildname]
	except:
		raise Exception("Build '" + buildname + "' is not declared.")

	targetnames = set()
	targets = {}
	for targetname in allbuilddata: targetnames.add(targetname)
	for targetname in targetbuilddata: targetnames.add(targetname)
	for targetname in targetnames:
		#target = _get_build_target(ctx, targetname, buildname)
		#print(targetname, target)
		if include_all or targetname != 'all':
			targets[targetname] = _get_build_target(ctx, targetname, buildname)
			#print(targets[targetname])
	return targets


def _get_build_target(ctx, targetname, buildname = None):
	from copy import copy
	from waflib import Context
	if not buildname:
		try:
			buildname = ctx.env.build
			if not buildname: buildname = ctx.options.build
		except:
			buildname = ctx.options.build

	try:
		builds = Context.g_module.BUILDS
	except:
		raise Exception("BUILDS dictionary is not declared.")

	try:
		allbuilddata = builds['all']
	except:
		allbuilddata = {}

	try:
		allalldata = allbuilddata['all']
	except:
		allalldata = {}

	try:
		alldata = allbuilddata[targetname]
	except:
		alldata = {}

	# It's mandatory to have the build declared.
	try:
		targetbuilddata = builds[buildname]
	except:
		targetbuilddata = {}

	try:
		targetalldata = targetbuilddata['all']
	except:
		targetalldata = {}

	try:
		targetdata = targetbuilddata[targetname]
	except:
		targetdata = {}

	#if not allbuilddata and not targetbuilddata:
	#	raise Exception("Build '" + buildname + "' is not declared.")

	data = copy(allalldata)
	for key in alldata: data[key] = alldata[key]
	for key in targetalldata: data[key] = targetalldata[key]
	for key in targetdata: data[key] = targetdata[key]

	if not data:
		raise Exception("No target '" + targetname + "' for build '" + buildname + "'.")
	else:
		if 'target' not in data:
			data['target'] = targetname
	
	return data
	
