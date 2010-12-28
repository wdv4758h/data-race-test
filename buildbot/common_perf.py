from buildbot.status import builder
from buildbot.steps.shell import Compile
from buildbot.steps.shell import Test
from buildbot.steps.shell import ShellCommand
from common import *
import process_log
import chromium_utils

class ProcessLogShellStep(ShellCommand):

  def __init__(self, log_processor_class, *args, **kwargs):
    self._result_text = []
    self._log_processor = log_processor_class()
    ShellCommand.__init__(self, *args, **kwargs)

  def start(self):
    """Overridden shell.ShellCommand.start method.

    Adds a link for the activity that points to report ULR.
    """
    self._CreateReportLinkIfNeccessary()
    ShellCommand.start(self)

  def _GetRevision(self):
    """Returns the revision number for the build.

    Result is the revision number of the latest change that went in
    while doing gclient sync. If None, will return -1 instead.
    """
    if self.build.getProperty('got_revision'):
      return self.build.getProperty('got_revision')
    return -1

  def commandComplete(self, cmd):
    """Callback implementation that will use log process to parse 'stdio' data.
    """
    self._result_text = self._log_processor.Process(
        self._GetRevision(), self.getLog('stdio').getText())

  def getText(self, cmd, results):
    text_list = self.describe(True)
    if self._result_text:
      self._result_text.insert(0, '<div class="BuildResultInfo">')
      self._result_text.append('</div>')
      text_list = text_list + self._result_text
    return text_list

  def evaluateCommand(self, cmd):
    shell_result = ShellCommand.evaluateCommand(self, cmd)
    log_result = None
    if self._log_processor and 'evaluateCommand' in dir(self._log_processor):
      log_result = self._log_processor.evaluateCommand(cmd)
    if shell_result is builder.FAILURE or log_result is builder.FAILURE:
      return builder.FAILURE
    if shell_result is builder.WARNINGS or log_result is builder.WARNINGS:
      return builder.WARNINGS
    return builder.SUCCESS

  def _CreateReportLinkIfNeccessary(self):
    if self._log_processor.ReportLink():
      self.addURL('results', "%s" % self._log_processor.ReportLink())


def genBenchmarkStep(factory, platform, benchmark, *args, **kwargs):
  base_dir = 'perf/%s/%s' % (platform, benchmark)
  report_link = '%s/report.html?history=1000' % (base_dir,)
  output_dir = 'public_html/%s' % (base_dir,)

  log_processor_class = chromium_utils.InitializePartiallyWithArguments(
      process_log.GraphingLogProcessor,
      report_link=report_link,
      output_dir=output_dir)

  step = chromium_utils.InitializePartiallyWithArguments(
      ProcessLogShellStep, log_processor_class,
      *args, **kwargs)

  return step
