import os
import glob

from setuptools import setup, find_packages, Extension

undef_macros = ['NDEBUG']

SOURCE_FILES = glob.glob(os.path.join('thegiant', '*.c'))

thegiant_extension = Extension(
    'thegiant.server',
    sources       = SOURCE_FILES,
    libraries     = ['ev'],
    # include_dirs  = [],
    define_macros = [('WANT_SENDFILE', '1'),
                     ('WANT_SIGINT_HANDLING', '1')
                      ], 
#                     , ('DEBUG', '1')],
    # assert should assert                     
    undef_macros = undef_macros,                                           
    extra_compile_args = ['-std=c99', '-fno-strict-aliasing', '-Wall',
                          '-Wextra', '-Wno-unused', '-g', '-fPIC']
)

setup(
    name         = 'thegiant',
    author       = 'Aybars Badur',
    author_email = 'aybars.badur@gmail.com',
    license      = '2-clause BSD',
    url          = 'https://github.com/ybrs/the-giant',
    description  = 'A wsgi server that speaks Redis written in C.',
    version      = '1.2',
    classifiers  = ['Development Status :: 4 - Beta',
                    'License :: OSI Approved :: BSD License',
                    'Programming Language :: C',
                    'Programming Language :: Python',
                    'Topic :: Internet :: WWW/HTTP :: WSGI :: Server'],
    install_requires = ['redis'],
    ext_modules  = [thegiant_extension],
    py_modules = ['thegiant']

)
