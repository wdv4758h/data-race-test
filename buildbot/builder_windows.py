from buildbot.process import factory
from buildbot.steps.source import SVN
from buildbot.steps.shell import Compile
from buildbot.steps.shell import Test
from buildbot.steps.shell import ShellCommand
from buildbot.steps.transfer import FileUpload
from common import *


def generate(settings):
  f1 = factory.BuildFactory()

  # Checkout sources.
  f1.addStep(SVN(svnurl=settings['svnurl'], mode='copy'))

  # Build tsan + pin.
  f1.addStep(Compile(command=['make', '-C', 'tsan', '-j4',
                              'VALGRIND_ROOT=', 'PIN_ROOT=c:/pin',
                              'w32o'],
                     description='building tsan with pin',
                     descriptionDone='build tsan with pin'))

  f1.addStep(Compile(command=['make', '-C', 'tsan', '-j4',
                              'VALGRIND_ROOT=', 'PIN_ROOT=c:/pin',
                              'w32d'],
                     description='building tsan-debug with pin',
                     descriptionDone='build tsan-debug with pin'))

  f1.addStep(Compile(command=['make', '-C', 'tsan',
                              'VALGRIND_ROOT=', 'PIN_ROOT=c:/pin',
                              'w32-sfx'],
                     description='packing sfx binary',
                     descriptionDone='pack sfx binary'))

  f1.addStep(ShellCommand(command="bash -c 'mkdir -p out; cd out; ../tsan/tsan-x86-windows-sfx.exe'",
                     description='extracting sfx',
                     descriptionDone='extract sfx'))

  # Run thread_sanitizer and suppressions tests.
  addTsanTestsStep(f1, ['x86-windows-debug'])

  # Run tests.
  test_binaries = {} # (os, bits, opt, static, name) -> (binary, desc)
  os = 'windows'
  #                  test binary | tsan + run parameters
  #             bits, opt, static,   tsan-debug,   mode
  variants = [
    # ((  32,   1, False),(        True, 'fast')),
    ((  32,   1, False),(        True, 'hybrid')),
    ((  32,   1, False),(        True,  'phb')),
    # ((  32,   0, False),(        True, 'slow')),
    ((  32,   0, False),(       False,  'phb'))
    ]
  for (test_variant, run_variant) in variants:
    (tsan_debug, mode) = run_variant
    if not test_binaries.has_key(test_variant):
      (bits, opt, static) = test_variant
      test_desc = addBuildTestStep(f1, os, bits, opt, static)
      test_binaries[test_variant] = test_desc
    test_binary = unitTestBinary(os, bits, opt, static)
    addTestStep(f1, tsan_debug, mode, test_binary, test_desc, frontend='pin-win',
                pin_root='c:/pin', timeout=None, extra_args=['--error_exitcode=1'])
    addTestStep(f1, tsan_debug, mode, test_binary, test_desc + ' RV 1st pass', frontend='pin-win',
                pin_root='c:/pin', timeout=None,
                extra_args=['--show-expected-races', '--error_exitcode=1'],
                extra_test_args=['--gtest_filter="RaceVerifierTests.*"'],
                append_command='2>&1 | tee raceverifier.log')
    addTestStep(f1, tsan_debug, mode, test_binary, test_desc + ' RV 2nd pass', frontend='pin-win',
                pin_root='c:/pin', timeout=None,
                extra_args=['--error_exitcode=1', '--race-verifier=raceverifier.log'],
                extra_test_args=['--gtest_filter="RaceVerifierTests.*"'],
                append_command='2>&1')


  binaries = {
    'tsan\\tsan-x86-windows-sfx.exe' : 'tsan-r%s-x86-windows-sfx.exe'}
  addUploadBinariesStep(f1, binaries)

  b1 = {'name': 'buildbot-winxp',
        'slavename': 'vm10-m3',
        'builddir': 'full_winxp',
        'factory': f1,
        }

  b2 = {'name': 'buildbot-vista',
        'slavename': 'vm50-m3',
        'builddir': 'full_vista',
        'factory': f1,
        }

  b3 = {'name': 'buildbot-win7',
        'slavename': 'vm51-m3',
        'builddir': 'full_win7',
        'factory': f1,
        }

  return [b1, b2, b3]
