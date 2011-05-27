#! /usr/bin/env python
# -*- Mode: Python; coding: utf-8; indent-tabs-mode: nil; tab-width: 4 -*-

VERSION = '0.2'
APPNAME = 'rove'

# these variables are mandatory ('/' are converted automatically)
top = '.'
out = 'build'

def options(opt):
    opt.load('compiler_c')

def configure(conf):
    conf.load('compiler_c')
    conf.check_cfg(package='libmonome', mandatory=1, uselib_store='MONOME', args='--cflags --libs')
    conf.check_cfg(package='sndfile', mandatory=1, uselib_store='SNDFILE', args='--cflags --libs')
    conf.check_cfg(package='jack', mandatory=1, uselib_store='JACK', args='--cflags --libs')
    conf.check_cfg(package='samplerate', mandatory=0, uselib_store='SAMPLERATE', args='--cflags --libs')

def build(bld):
    bld.program(
        source = [
            'src/config_parser.c',
            'src/file_loop.c',
            'src/group.c',
            'src/jack.c',
            'src/list.c',
	    'src/monome.c',
	    'src/pattern.c',
	    'src/rove.c',
	    'src/session.c',
	    'src/settings.c',
        ],
        target      = 'rove',
        uselib      = 'JACK MONOME SNDFILE SAMPLERATE',
        includes    = '#src/private',
    )

    bld.install_files(bld.env['DOCDIR'], 'README')
