from buildbot.process import factory
from buildbot.steps.source import SVN
from buildbot.steps.shell import Compile
from buildbot.steps.shell import Test
from buildbot.steps.shell import ShellCommand
from buildbot.steps.transfer import FileUpload
from buildbot.steps.transfer import FileDownload
from buildbot.process.properties import WithProperties

import os.path


def unitTestBinary(osname, bits, opt, static, extra_build_suffix=None,
    test_base_name='racecheck_unittest'):
  if bits == 64:
    arch = 'amd64'
  else:
    arch = 'x86'
  name = os.path.join('unittest', 'bin', '%s-%s-%s-O%d' % (test_base_name, osname, arch, opt))
  if static:
    name += '-static'
  if extra_build_suffix:
    name += extra_build_suffix
  if osname == 'windows':
    name += '.exe'
  return name


def getTestDesc(osname, bits, opt, static, extra_build_suffix=None):
  desc = []
  desc.append(osname)
  desc.append(str(bits))
  desc.append('O%d' % opt)
  if static:
    desc.append('static')
  if extra_build_suffix:
    desc.append(extra_build_suffix)
  return '(' + ','.join(desc) + ')'


def findExtraBuildSuffix(extra_args):
  if not extra_args:
    return None
  for arg in extra_args:
    if arg.startswith('EXTRA_BUILD_SUFFIX='):
      return arg.split('=', 1)[1]
  return None


def addBuildTestStep(factory, osname, bits, opt, static, pic=False, more_args=None):
  """Adds a step for building a unit test binary."""
  command = ['make', '-C', 'unittest', 'all']
  command.append('OS=%s' % osname)

  if bits == 64:
    command.append('ARCH=amd64')
  else:
    command.append('ARCH=x86')

  command.append('OPT=%d' % opt)

  command.append('STATIC=%d' % static)

  extra_cflags = []
  extra_cxxflags = []
  extra_build_suffix = []

  if pic:
    extra_cflags.append('-fPIC')
    extra_cxxflags.append('-fPIC')
    extra_build_suffix.append('-PIC')

  if extra_cflags:
    command.append('EXTRA_CFLAGS=%s' % ' '.join(extra_cflags))

  if extra_cxxflags:
    command.append('EXTRA_CXXFLAGS=%s' % ' '.join(extra_cxxflags))

  if extra_build_suffix:
    command.append('EXTRA_BUILD_SUFFIX=%s' % ''.join(extra_build_suffix))

  desc_common = getTestDesc(osname, bits, opt, static,
      extra_build_suffix=''.join(extra_build_suffix))
  

  if more_args:
    command.extend(more_args)

  print command
  factory.addStep(Compile(command = command,
                          description = 'building unittests ' + desc_common,
                          descriptionDone = 'build unittests ' + desc_common))
  return desc_common


def addTestStep(factory, debug, threaded, mode, test_binary, test_desc,
                frontend_binary=None, extra_args=[], frontend='valgrind',
                pin_root=None, timeout=1800, test_base_name='racecheck_unittest',
                append_command=None, step_generator=Test, extra_test_args=[],
                prefix=None):
  """Adds a step for running unit tests with tsan."""
  args = []
  env = {}
  desc = []

  if frontend == 'valgrind':
    if debug:
      frontend_binary = frontend_binary or './tsan-debug.sh'
    else:
      frontend_binary = frontend_binary or './tsan.sh'
    env['VALGRIND_EXTRACT_DIR'] = '.'
  elif frontend == 'pin':
    if not frontend_binary:
      frontend_binary = 'tsan/tsan_pin.sh'
    if debug:
      args.extend(['--dbg'])
    else:
      args.extend(['--opt'])
    if threaded:
      args.extend(['--mt'])
  elif frontend == 'pin-win':
    if not frontend_binary:
      if debug:
        frontend_binary = 'out\\tsan-x86-windows\\tsan-debug.bat'
      else:
        frontend_binary = 'out\\tsan-x86-windows\\tsan.bat'
      if threaded:
        frontend_binary = 'out\\tsan-x86-windows\\tsan_mt.bat'


  if debug:
    desc.append('debug')
  # if frontend == 'valgrind':
  #   tool_arg = '--tool=tsan'
  #   if debug:
  #     tool_arg += '-debug'
  #     desc.append('debug')
  #   args.append(tool_arg)

  if frontend == 'pin':
    if pin_root:
      env['PIN_ROOT'] = pin_root
    env['TS_ROOT'] = 'tsan'

  if mode == 'phb':
    args.extend(['--pure-happens-before=yes'])
  elif mode == 'hybrid':
    args.extend(['--pure-happens-before=no'])

  args.append('--suppressions=' + os.path.join('unittest', 'racecheck_unittest.supp'))
  args.append('--ignore=' + os.path.join('unittest', 'racecheck_unittest.ignore'))

  desc.append(mode)
  desc_common = 'tsan-' + frontend
  if threaded:
    desc_common += '-MT'
  desc_common += ' (' + ','.join(desc) + ')'

  if frontend == 'pin-win':
    args.append('--')

  command = []
  if prefix:
    command += prefix
  # if timeout:
  #   command += ['alarm', '-l', str(timeout)]
  command += [frontend_binary] + extra_args + args + [test_binary] + extra_test_args
  if append_command:
    command = ' '.join(command + [append_command])
  print command

  factory.addStep(step_generator(command = command, env = env,
                       description = 'testing ' + desc_common + ' on ' + test_base_name + ' ' + test_desc,
                       descriptionDone = 'test ' + desc_common + ' on ' + test_base_name + ' ' + test_desc))


def addClobberStep(factory):
  factory.addStep(ShellCommand(command='rm -rf -- *',
                               description='clobbering build dir',
                               descriptionDone='clobber build dir'))

def addArchiveStep(factory, archive_path, paths=['.']):
  factory.addStep(ShellCommand(
      command='tar czvf '+ archive_path + '.tmp '+ ' '.join(paths) + 
      ' && mv ' + archive_path + '.tmp ' + archive_path +
      ' && chmod 644 ' + archive_path,
      description='packing build tree',
      descriptionDone='pack build tree'))

def addUploadBuildTreeStep(factory, archive_path):
  factory.addStep(FileUpload(slavesrc=archive_path, masterdest='build/linux_build.tgz', mode=0644))


def addExtractStep(factory, archive_path):
  factory.addStep(ShellCommand(
      command=['tar', 'xzvf', archive_path],
      description='extract build tree',
      descriptionDone='extract build tree'))


class GetRevisionStep(ShellCommand):

  def __init__(self, *args, **kwargs):
    kwargs['command'] = 'cat REVISION'
    kwargs['description'] = 'getting revision'
    kwargs['descriptionDone'] = 'get revision'
    ShellCommand.__init__(self, *args, **kwargs)

  def commandComplete(self, cmd):
    revision = self.getLog('stdio').getText().rstrip();
    self.build.setProperty('got_revision', revision, 'Build');


# Gets full build tree from another builder, unpacks it and gets its svn revision
def addSetupTreeForTestsStep(factory):
  addClobberStep(factory)
  factory.addStep(FileDownload(slavedest='full_build.tar.gz', mastersrc='build/linux_build.tgz', mode=0644))
  factory.addStep(FileDownload(slavedest='memcheck64.sh', mastersrc='public_html/binaries/memcheck-latest-amd64-linux-self-contained.sh', mode=0755))
  factory.addStep(FileDownload(slavedest='memcheck32.sh', mastersrc='public_html/binaries/memcheck-latest-x86-linux-self-contained.sh', mode=0755))
  addExtractStep(factory, 'full_build.tar.gz')
  factory.addStep(GetRevisionStep());


def addUploadBinariesStep(factory, binaries):
  for (local_name, remote_name) in binaries.items():
    dst = WithProperties('public_html/binaries/' + remote_name, 'got_revision')
    factory.addStep(FileUpload(slavesrc=local_name, masterdest=dst, mode=0755))


# Run GTest-based tests from tsan/ directory.
def addTsanTestsStep(factory, archosd_list):
  pathsep = '/'
  if archosd_list[0].find('windows') >= 0:
    pathsep = '\\'
  for archosd in archosd_list:
    command = pathsep.join(['tsan', 'bin', '%s-suppressions_test' % archosd])
    factory.addStep(Test(command=command,
                         description='testing suppressions (%s)' % archosd,
                         descriptionDone='test suppressions (%s)' % archosd))
  for archosd in archosd_list:
    command = pathsep.join(['tsan', 'bin', '%s-thread_sanitizer_test' % archosd])
    factory.addStep(Test(command=command,
                    description='testing thread_sanitizer (%s)' % archosd,
                    descriptionDone='test thread_sanitizer (%s)' % archosd))


__all__ = ['unitTestBinary', 'addBuildTestStep', 'addTestStep',
           'addClobberStep', 'addArchiveStep', 'addUploadBuildTreeStep',
           'addExtractStep',
           'addSetupTreeForTestsStep', 'getTestDesc', 'addUploadBinariesStep',
           'addTsanTestsStep']
