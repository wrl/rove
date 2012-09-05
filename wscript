#!/usr/bin/env python

import time
import sys

top = "."
out = "build"

# change this stuff

APPNAME = "rove"
VERSION = "0.2"

#
# dep checking functions
#

def check_jack(conf):
	conf.check_cc(
		mandatory=True,
		execute=True,

		lib="jack",
		header_name="jack/jack.h",
		uselib_store="JACK",

		msg="Checking for JACK")

def check_libmonome(conf):
	conf.check_cc(
		mandatory=True,
		execute=True,

		lib="monome",
		header_name="monome.h",
		uselib_store="LIBMONOME",

		msg="Checking for libmonome")

def check_sndfile(conf):
	conf.check_cc(
		mandatory=True,
		execute=True,

		lib="sndfile",
		header_name="sndfile.h",
		uselib_store="SNDFILE",

		msg="Checking for libsndfile")

def check_samplerate(conf):
	conf.check_cc(
		define_name="HAVE_SRC",
		mandatory=False,
		quote=0,

		execute=True,

		lib="samplerate",
		header_name="samplerate.h",
		uselib_store="SAMPLERATE",

		msg="Checking for libsamplerate")

#
# waf stuff
#

def options(opt):
	opt.load("compiler_c")

def configure(conf):
	# just for output prettifying
	# print() (as a function) ddoesn't work on python <2.7
	separator = lambda: sys.stdout.write("\n")

	separator()
	conf.load("compiler_c")
	conf.load("gnu_dirs")

	#
	# conf checks
	#

	separator()

	check_libmonome(conf)
	check_jack(conf)
	check_sndfile(conf)
	check_samplerate(conf)

	separator()

	#
	# setting defines, etc
	#

	conf.env.append_unique("CFLAGS", ["-std=c99", "-Wall", "-Werror", "-D_GNU_SOURCE"])

def build(bld):
	bld.recurse("src")

def dist(dst):
	pats = [".git*", "**/.git*", ".travis.yml"]
	with open(".gitignore") as gitignore:
	    for l in gitignore.readlines():
	        if l[0] == "#":
	            continue

	        pats.append(l.rstrip("\n"))

	dst.excl = " ".join(pats)
