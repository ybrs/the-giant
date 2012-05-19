import os
import glob

from setuptools import setup, find_packages, Extension

SOURCE_FILES = [os.path.join('http-parser', 'http_parser.c')] + \
               glob.glob(os.path.join('thegiant', '*.c'))

thegiant_extension = Extension(
    'thegiant',
    sources       = SOURCE_FILES,
    libraries     = ['ev'],
    include_dirs  = ['http-parser'],
    define_macros = [('WANT_SENDFILE', '1'),
                     ('WANT_SIGINT_HANDLING', '1')
                      ], 
#                     , ('DEBUG', '1')],
    extra_compile_args = ['-std=c99', '-fno-strict-aliasing', '-Wall',
                          '-Wextra', '-Wno-unused', '-g', '-fPIC']
)

setup(
    name         = 'thegiant',
    author       = 'Jonas Haag',
    author_email = 'jonas@lophus.org',
    license      = '2-clause BSD',
    url          = 'https://github.com/jonashaag/bjoern',
    description  = 'A screamingly fast Python WSGI server written in C.',
    version      = '1.2',
    classifiers  = ['Development Status :: 4 - Beta',
                    'License :: OSI Approved :: BSD License',
                    'Programming Language :: C',
                    'Programming Language :: Python',
                    'Topic :: Internet :: WWW/HTTP :: WSGI :: Server'],
    ext_modules  = [thegiant_extension]
)
