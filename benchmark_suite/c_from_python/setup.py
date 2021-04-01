from distutils.core import setup, Extension

setup(name='sleeper', version='1.0', ext_modules=[Extension('sleeper', ['sleep.c'])])
