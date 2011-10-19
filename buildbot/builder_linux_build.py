from buildbot.process import factory
from buildbot.steps.source import SVN
from buildbot.steps.shell import Compile
from buildbot.steps.shell import Test
from buildbot.steps.shell import ShellCommand

from common import *

import os.path

def generate(settings):
  f1 = factory.BuildFactory()

  # Checkout sources.
  f1.addStep(SVN(svnurl=settings['svnurl'], mode='copy'))

  f1.addStep(ShellCommand(command='svnversion . >REVISION',
      description='getting revision',
      descriptionDone='get revision'))

  # Get valgrind build.
  f1.addStep(ShellCommand(command=['wget', 'http://vm42-m3/b/build/slave/full_valgrind/valgrind_build.tar.gz'],
                          description='getting valgrind build',
                          descriptionDone='get valgrind build'))

  addExtractStep(f1, 'valgrind_build.tar.gz')

  # Build tsan and install it to out/.
  path_flags = ['OFFLINE=',
                'VALGRIND_INST_ROOT=../out',
                'VALGRIND_ROOT=../third_party/valgrind',
                'PIN_ROOT=../../../../third_party/pin']
  f1.addStep(Compile(command=['make', '-C', 'tsan', '-j2'] + path_flags + ['lo', 'ld'],
                     description='building tsan',
                     descriptionDone='build tsan'))

  # Build self-contained tsan binaries.
  f1.addStep(ShellCommand(command=['make', '-C', 'tsan'] + path_flags +
                          ['OS=linux', 'ARCH=amd64', 'DEBUG=1', 'self-contained'],
                          description='packing self-contained tsan (debug)',
                          descriptionDone='pack self-contained tsan (debug)'))

  f1.addStep(ShellCommand(command=['make', '-C', 'tsan'] + path_flags +
                          ['OS=linux', 'ARCH=amd64', 'DEBUG=0', 'self-contained-stripped'],
                          description='packing self-contained tsan',
                          descriptionDone='pack self-contained tsan'))

  # Build 32-bit tsan and install it to out32/.
  path_flags32 = ['OFFLINE=',
                  'OUTDIR=bin32',
                  'VALGRIND_INST_ROOT=../out32',
                  'VALGRIND_ROOT=../third_party/valgrind32',
                  'PIN_ROOT=']
  f1.addStep(Compile(command=['make', '-C', 'tsan', '-j2', 'OFFLINE='] + path_flags32 + ['l32o'],
                     description='building 32-bit tsan',
                     descriptionDone='build 32-bit tsan'))

  f1.addStep(ShellCommand(command=['make', '-C', 'tsan'] + path_flags32 +
                          ['OS=linux', 'ARCH=x86', 'DEBUG=0', 'self-contained-stripped'],
                          description='packing self-contained tsan (32-bit)',
                          descriptionDone='pack self-contained tsan (32-bit)'))

  # Build 64-bit-only tsan and install it to out64/.
  path_flags64 = ['OFFLINE=',
                  'OUTDIR=bin64',
                  'VALGRIND_INST_ROOT=../out64',
                  'VALGRIND_ROOT=../third_party/valgrind64',
                  'PIN_ROOT=']
  f1.addStep(Compile(command=['make', '-C', 'tsan', '-j2', 'OFFLINE='] + path_flags64 + ['l64o'],
                     description='building 64-bit tsan',
                     descriptionDone='build 64-bit tsan'))

  f1.addStep(ShellCommand(command=['make', '-C', 'tsan'] + path_flags64 +
                          ['OS=linux', 'ARCH=amd64', 'DEBUG=0', 'self-contained-stripped'],
                          description='packing self-contained tsan (64-bit)',
                          descriptionDone='pack self-contained tsan (64-bit)'))


  f1.addStep(ShellCommand(command='ln -s tsan/bin/tsan-amd64-linux-self-contained.sh tsan.sh && ' +
                          'ln -s tsan/bin/tsan-amd64-linux-debug-self-contained.sh tsan-debug.sh && ' +
                          'ln -s tsan/bin32/tsan-x86-linux-self-contained.sh tsan32.sh && ' +
                          'ln -s tsan/bin64/tsan-amd64-linux-self-contained.sh tsan64.sh',
                          description='symlinking tsan',
                          descriptionDone='symlink tsan'))

  binaries = {
    'tsan/bin/tsan-amd64-linux-debug-self-contained.sh' : 'tsan-r%s-amd64-linux-debug-self-contained.sh',
    'tsan/bin/tsan-amd64-linux-self-contained.sh' : 'tsan-r%s-amd64-linux-self-contained.sh',
    'tsan/bin32/tsan-x86-linux-self-contained.sh' : 'tsan-r%s-x86-linux-self-contained.sh',
    'tsan/bin64/tsan-amd64-linux-self-contained.sh' : 'tsan-r%s-amd64only-linux-self-contained.sh'}
  addUploadBinariesStep(f1, binaries)

  os = 'linux'
  for bits in [32, 64]:
    for opt in [0, 1]:
      for static in [False, True]:
        addBuildTestStep(f1, os, bits, opt, static)

  addBuildTestStep(f1, os, 32, 1, False, pic=True)


  masks = ['tsan*.sh', 'tsan/bin*/tsan*.sh', 'tsan/bin*/*_test',
    'tsan/bin*/*ts_pin.so', 'tsan/bin*/*ts_pinmt.so', 'tsan/tsan_pin.sh',
    'unittest', 'common.mk', 'REVISION']

  addArchiveStep(f1, '../full_build.tar.gz', masks)


  addUploadBuildTreeStep(f1, '../full_build.tar.gz')

  b1 = {'name': 'buildbot-linux-build',
        'slavename': 'vm42-m3',
        'builddir': 'full_linux_build',
        'factory': f1,
        }

  return [b1]
