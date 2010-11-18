from buildbot.process import factory
from buildbot.steps.source import SVN
from buildbot.steps.shell import Compile
from buildbot.steps.shell import Test
from buildbot.steps.shell import ShellCommand
from common import *

def generate(settings):
  f1 = factory.BuildFactory()

  addSetupTreeForTestsStep(f1)

  # Run thread_sanitizer and suppressions tests.
  addTsanTestsStep(f1, ['amd64-linux-debug', 'x86-linux-debug'])

  # Run output tests.
  f1.addStep(ShellCommand(command='make -C unittest OS=linux ARCH=amd64 TSAN="../tsan.sh" run_output_tests',
                          description="running output tests 64",
                          descriptionDone="output tests 64"))
  f1.addStep(ShellCommand(command='make -C unittest OS=linux ARCH=x86 TSAN="../tsan.sh" run_output_tests',
                          description="running output tests 32",
                          descriptionDone="output tests 32"))
                          
  # Run unit tests.
  test_binaries = {} # (bits, opt, static) -> (binary, desc)
  os = 'linux'
  #                  test binary | tsan + run parameters
  #             bits, opt, static,   tsan-debug,   mode
  variants = [
((  64,   0, False, None),(        False, 'hybrid')),
((  32,   1, False, '-PIC'),(        False, 'hybrid'))
]
  for (test_variant, run_variant) in variants:
    (tsan_debug, mode) = run_variant
    (bits, opt, static, build_extra) = test_variant
    test_desc = getTestDesc(os, bits, opt, static,
        extra_build_suffix=build_extra)
    test_binary = unitTestBinary(os, bits, opt, static,
        extra_build_suffix=build_extra)
    addTestStep(f1, tsan_debug, mode, test_binary, test_desc, extra_args=["--error_exitcode=1"])


  b1 = {'name': 'buildbot-linux-small',
        'slavename': 'bot6name',
        'builddir': 'full_linux_small',
        'factory': f1,
        }

  return [b1]
