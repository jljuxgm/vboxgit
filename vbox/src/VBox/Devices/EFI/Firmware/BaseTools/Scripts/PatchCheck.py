#  Copyright (c) 2020, ARM Ltd. All rights reserved.<BR>
import email.header

                not lines[i].startswith('git-svn-id:') and
                not lines[i].startswith('Reviewed-by') and
                not lines[i].startswith('Acked-by:') and
                not lines[i].startswith('Tested-by:') and
                not lines[i].startswith('Reported-by:') and
                not lines[i].startswith('Suggested-by:') and
                not lines[i].startswith('Signed-off-by:') and
                not lines[i].startswith('Cc:')):
                if self.filename.endswith('.sh') or \
                    self.filename.startswith('BaseTools/BinWrappers/PosixLike/') or \
                    self.filename.startswith('BaseTools/BinPipWrappers/PosixLike/') or \
                    self.filename.startswith('BaseTools/Bin/CYGWIN_NT-5.1-i686/') or \
                    self.filename == 'BaseTools/BuildEnv':
                    # Some linux shell scripts don't end with the ".sh" extension,
                    # they are identified by their path.
                if self.filename == '.gitmodules' or \
                   self.filename == 'BaseTools/Conf/diff.order':
                    # .gitmodules and diff orderfiles are used internally by git
                    # use tabs and LF line endings.  Do not enforce no tabs and
                    # do not enforce CR/LF line endings.
        if self.force_crlf and eol != '\r\n' and (line.find('Subproject commit') == -1):