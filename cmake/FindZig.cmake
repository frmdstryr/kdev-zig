#=============================================================================
# 2017  Emma Gospodinova <emma.gospodinova@gmail.com>
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
#
# 1. Redistributions of source code must retain the copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
# 3. The name of the author may not be used to endorse or promote products
#    derived from this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
# IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
# OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
# IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
# INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
# NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
# THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#=============================================================================

include(FindPackageHandleStandardArgs)

find_program(ZIG_EXECUTABLE zig
    HINTS $ENV{HOME}/.local/bin
          $ENV{HOME}/projects/zig/build/bin
)

find_package_handle_standard_args(Zig DEFAULT_MSG ZIG_EXECUTABLE)

mark_as_advanced(ZIG_EXECUTABLE)

execute_process(COMMAND ${ZIG_EXECUTABLE} version
    OUTPUT_VARIABLE ZIG_VERSION_OUTPUT
    OUTPUT_STRIP_TRAILING_WHITESPACE)

string(REGEX MATCH "([A-Za-z0-9_-]*)\n" ZIG_TARGET_TRIPLE "${ZIG_VERSION_OUTPUT}")
string(REGEX MATCH "([A-Za-z0-9_-]*)\n" ZIG_RELEASE "${ZIG_VERSION_OUTPUT}")

mark_as_advanced(ZIG_TARGET_TRIPLE)
mark_as_advanced(ZIG_RELEASE)
