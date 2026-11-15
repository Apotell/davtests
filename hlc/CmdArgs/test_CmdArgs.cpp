/*
 Copyright 2020 Apotell

 Licensed under the Apache License, Version 2.0 (the "License");
 you may not use this file except in compliance with the License.
 You may obtain a copy of the License at

 http://www.apache.org/licenses/LICENSE-2.0

 Unless required by applicable law or agreed to in writing, software
 distributed under the License is distributed on an "AS IS" BASIS,
 WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 See the License for the specific language governing permissions and
 limitations under the License.
*/

// Command-line tests: the events command-line arguments raise.
//
// Each test suite below reads the command file of one subdirectory, <Name>/<Name>.hlc, which runs
// "-wd ../../../tests/CmdArgs top.sv" followed by the argument under test (quoted in the comment above
// each suite). Every suite passes -nocompile: only the command line is parsed, and the argument may
// make the parse fail. The arguments whose event is raised only while compiling (CacheRejected,
// GlobalTimescale, UndefinedConfig) keep their own test binaries, next to the cache data they use.

#include <hlc/Events/EventType.h>
#include <hlc/Tests/Test.h>

#include <gtest/gtest.h>

namespace hlc {

// -cache afile.txt/cache: a cache directory under a file.
class CmdArgsCacheNotCreatableTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-nocompile", "-f", "CacheNotCreatable/CacheNotCreatable.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

TEST_F(CmdArgsCacheNotCreatableTest, ReportsCmdPpCannotCreateCacheDir) {
  EXPECT_NE(findEvent(EventType::CmdPpCannotCreateCacheDir), nullptr)
      << "CmdPpCannotCreateCacheDir expected for: a cache directory under a file";
}

// -f .: -f names a directory: it exists but cannot be read.
class CmdArgsCannotOpenForReadTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-nocompile", "-f", "CannotOpenForRead/CannotOpenForRead.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

TEST_F(CmdArgsCannotOpenForReadTest, ReportsCmdCannotOpenFileForRead) {
  EXPECT_NE(findEvent(EventType::CmdCannotOpenFileForRead, "."), nullptr)
      << "CmdCannotOpenFileForRead expected for: -f names a directory: it exists but cannot be read";
}

// -cd -nocache: -cd followed by another option instead of a directory.
class CmdArgsCdNoArgTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-nocompile", "-f", "CdNoArg/CdNoArg.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

TEST_F(CmdArgsCdNoArgTest, ReportsCmdCdMissingDir) {
  EXPECT_NE(findEvent(EventType::CmdCdMissingDir, "-cd"), nullptr)
      << "CmdCdMissingDir expected for: -cd followed by another option instead of a directory";
}

// -f nosuch.f: -f names a command file that does not exist.
class CmdArgsDashFMissingTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-nocompile", "-f", "DashFMissing/DashFMissing.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

TEST_F(CmdArgsDashFMissingTest, ReportsCmdDashFFileDoesNotExist) {
  EXPECT_NE(findEvent(EventType::CmdDashFFileDoesNotExist, "nosuch.f"), nullptr)
      << "CmdDashFFileDoesNotExist expected for: -f names a command file that does not exist";
}

// -d 5: -d with a level out of range.
class CmdArgsDebugLevelBadTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-nocompile", "-f", "DebugLevelBad/DebugLevelBad.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

TEST_F(CmdArgsDebugLevelBadTest, ReportsCmdDebugIncorrectLevel) {
  EXPECT_NE(findEvent(EventType::CmdDebugIncorrectLevel, "5"), nullptr)
      << "CmdDebugIncorrectLevel expected for: -d with a level out of range";
}

// -d -nocache: -d followed by another option instead of a level.
class CmdArgsDebugMissingTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-nocompile", "-f", "DebugMissing/DebugMissing.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

TEST_F(CmdArgsDebugMissingTest, ReportsCmdDebugMissingLevel) {
  EXPECT_NE(findEvent(EventType::CmdDebugMissingLevel), nullptr)
      << "CmdDebugMissingLevel expected for: -d followed by another option instead of a level";
}

// -fileunit: -fileunit is reported, and so is the log file being created.
class CmdArgsFileUnitTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-nocompile", "-f", "FileUnit/FileUnit.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

TEST_F(CmdArgsFileUnitTest, ReportsCmdSeparateCompilationUnitOn) {
  EXPECT_NE(findEvent(EventType::CmdSeparateCompilationUnitOn), nullptr)
      << "CmdSeparateCompilationUnitOn expected for: -fileunit is reported, and so is the log file being created";
  EXPECT_NE(findEvent(EventType::CmdCreatingLogFile), nullptr)
      << "CmdCreatingLogFile expected for: -fileunit is reported, and so is the log file being created";
}

// +incdir+nosuchdir: an include directory that does not exist.
class CmdArgsIncludePathMissingTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-nocompile", "-f", "IncludePathMissing/IncludePathMissing.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

TEST_F(CmdArgsIncludePathMissingTest, ReportsCmdIncludePathDoesNotExist) {
  EXPECT_NE(findEvent(EventType::CmdIncludePathDoesNotExist), nullptr)
      << "CmdIncludePathDoesNotExist expected for: an include directory that does not exist";
}

// +incdir+..: an include directory above the working directory.
class CmdArgsInputNotInWorkdirTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-nocompile", "-f", "InputNotInWorkdir/InputNotInWorkdir.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

TEST_F(CmdArgsInputNotInWorkdirTest, ReportsCmdInputNotInWorkdir) {
  EXPECT_NE(findEvent(EventType::CmdInputNotInWorkdir, ".."), nullptr)
      << "CmdInputNotInWorkdir expected for: an include directory above the working directory";
}

// -o afile.txt: an output directory that is a file.
class CmdArgsInvalidPathTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-nocompile", "-f", "InvalidPath/InvalidPath.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

TEST_F(CmdArgsInvalidPathTest, ReportsCmdInvalidPath) {
  EXPECT_NE(findEvent(EventType::CmdInvalidPath), nullptr)
      << "CmdInvalidPath expected for: an output directory that is a file";
}

// -v nosuch.v: -v names a library file that does not exist.
class CmdArgsLibFileMissingTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-nocompile", "-f", "LibFileMissing/LibFileMissing.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

TEST_F(CmdArgsLibFileMissingTest, ReportsCmdLibraryFileDoesNotExist) {
  EXPECT_NE(findEvent(EventType::CmdLibraryFileDoesNotExist), nullptr)
      << "CmdLibraryFileDoesNotExist expected for: -v names a library file that does not exist";
}

// -v -nocache: -v followed by another option instead of a file.
class CmdArgsLibFileNoArgTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-nocompile", "-f", "LibFileNoArg/LibFileNoArg.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

TEST_F(CmdArgsLibFileNoArgTest, ReportsCmdLibraryFileMissingFile) {
  EXPECT_NE(findEvent(EventType::CmdLibraryFileMissingFile, "-v"), nullptr)
      << "CmdLibraryFileMissingFile expected for: -v followed by another option instead of a file";
}

// -y nosuchdir: -y names a library directory that does not exist.
class CmdArgsLibPathMissingTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-nocompile", "-f", "LibPathMissing/LibPathMissing.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

TEST_F(CmdArgsLibPathMissingTest, ReportsCmdLibraryPathDoesNotExist) {
  EXPECT_NE(findEvent(EventType::CmdLibraryPathDoesNotExist), nullptr)
      << "CmdLibraryPathDoesNotExist expected for: -y names a library directory that does not exist";
}

// -y -nocache: -y followed by another option instead of a directory.
class CmdArgsLibPathNoArgTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-nocompile", "-f", "LibPathNoArg/LibPathNoArg.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

TEST_F(CmdArgsLibPathNoArgTest, ReportsCmdLibraryPathMissingPath) {
  EXPECT_NE(findEvent(EventType::CmdLibraryPathMissingPath, "-y"), nullptr)
      << "CmdLibraryPathMissingPath expected for: -y followed by another option instead of a directory";
}

// -l -nocache: -l (deprecated) with no log file name.
class CmdArgsLogFileNoArgTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-nocompile", "-f", "LogFileNoArg/LogFileNoArg.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

TEST_F(CmdArgsLogFileNoArgTest, ReportsCmdLogFileMissingFile) {
  EXPECT_NE(findEvent(EventType::CmdLogFileMissingFile, "-l"), nullptr)
      << "CmdLogFileMissingFile expected for: -l (deprecated) with no log file name";
}

// -o logdir: the log file cannot be created: logdir/hlc.log is a directory.
class CmdArgsLogNotCreatableTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-nocompile", "-f", "LogNotCreatable/LogNotCreatable.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

TEST_F(CmdArgsLogNotCreatableTest, ReportsCmdCannotOpenFileForWrite) {
  EXPECT_NE(findEvent(EventType::CmdCannotOpenFileForWrite), nullptr)
      << "CmdCannotOpenFileForWrite expected for: the log file cannot be created: logdir/hlc.log is a directory";
}

// -nosuchoption: an unknown -option.
class CmdArgsMinusArgIgnoredTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-nocompile", "-f", "MinusArgIgnored/MinusArgIgnored.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

TEST_F(CmdArgsMinusArgIgnoredTest, ReportsCmdMinusArgIgnored) {
  EXPECT_NE(findEvent(EventType::CmdMinusArgIgnored, "-nosuchoption"), nullptr)
      << "CmdMinusArgIgnored expected for: an unknown -option";
}

// -tar afile.txt: -tar names a file that is not a tar archive.
class CmdArgsMountFailTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-nocompile", "-f", "MountFail/MountFail.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

TEST_F(CmdArgsMountFailTest, ReportsCmdFailedToMount) {
  EXPECT_NE(findEvent(EventType::CmdFailedToMount), nullptr)
      << "CmdFailedToMount expected for: -tar names a file that is not a tar archive";
}

// -mt four: -mt with a value that is not a number.
class CmdArgsMtBadTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-nocompile", "-f", "MtBad/MtBad.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

TEST_F(CmdArgsMtBadTest, ReportsCmdMtMissingLevel) {
  EXPECT_NE(findEvent(EventType::CmdMtMissingLevel), nullptr)
      << "CmdMtMissingLevel expected for: -mt with a value that is not a number";
}

// -mt 513: -mt with a thread count above the limit.
class CmdArgsMtLevelBadTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-nocompile", "-f", "MtLevelBad/MtLevelBad.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

TEST_F(CmdArgsMtLevelBadTest, ReportsCmdMtIncorrectLevel) {
  EXPECT_NE(findEvent(EventType::CmdMtIncorrectLevel, "513"), nullptr)
      << "CmdMtIncorrectLevel expected for: -mt with a thread count above the limit";
}

// -profile -mt 2: -profile reports the number of threads.
class CmdArgsNumberThreadsTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-nocompile", "-f", "NumberThreads/NumberThreads.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

TEST_F(CmdArgsNumberThreadsTest, ReportsCmdNumberThreads) {
  EXPECT_NE(findEvent(EventType::CmdNumberThreads, "2"), nullptr)
      << "CmdNumberThreads expected for: -profile reports the number of threads";
}

// -o -nocache: -o followed by another option instead of a directory.
class CmdArgsOdirNoArgTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-nocompile", "-f", "OdirNoArg/OdirNoArg.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

TEST_F(CmdArgsOdirNoArgTest, ReportsCmdPpFileMissingOdir) {
  EXPECT_NE(findEvent(EventType::CmdPpFileMissingOdir), nullptr)
      << "CmdPpFileMissingOdir expected for: -o followed by another option instead of a directory";
}

// -o afile.txt/out: an output directory under a file.
class CmdArgsOdirNotCreatableTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-nocompile", "-f", "OdirNotCreatable/OdirNotCreatable.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

TEST_F(CmdArgsOdirNotCreatableTest, ReportsCmdPpCannotCreateOutputDir) {
  EXPECT_NE(findEvent(EventType::CmdPpCannotCreateOutputDir), nullptr)
      << "CmdPpCannotCreateOutputDir expected for: an output directory under a file";
}

// +nosuchplusarg: an unknown +argument.
class CmdArgsPlusArgIgnoredTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-nocompile", "-f", "PlusArgIgnored/PlusArgIgnored.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

TEST_F(CmdArgsPlusArgIgnoredTest, ReportsCmdPlusArgIgnored) {
  EXPECT_NE(findEvent(EventType::CmdPlusArgIgnored, "+nosuchplusarg"), nullptr)
      << "CmdPlusArgIgnored expected for: an unknown +argument";
}

// -writeppfile -nocache: -writeppfile (deprecated) with no file name.
class CmdArgsPpFileNoArgTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-nocompile", "-f", "PpFileNoArg/PpFileNoArg.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

TEST_F(CmdArgsPpFileNoArgTest, ReportsCmdPpFileMissingFile) {
  EXPECT_NE(findEvent(EventType::CmdPpFileMissingFile, "-writeppfile"), nullptr)
      << "CmdPpFileMissingFile expected for: -writeppfile (deprecated) with no file name";
}

// -split abc: -split with a value that is not a number.
class CmdArgsSplitBadTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-nocompile", "-f", "SplitBad/SplitBad.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

TEST_F(CmdArgsSplitBadTest, ReportsCmdSplitFileMissingSize) {
  EXPECT_NE(findEvent(EventType::CmdSplitFileMissingSize), nullptr)
      << "CmdSplitFileMissingSize expected for: -split with a value that is not a number";
}

// -tar nosuch.tar: -tar names an archive that does not exist.
class CmdArgsTarMissingTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-nocompile", "-f", "TarMissing/TarMissing.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

TEST_F(CmdArgsTarMissingTest, ReportsCmdFileDoesNotExist) {
  EXPECT_NE(findEvent(EventType::CmdFileDoesNotExist), nullptr)
      << "CmdFileDoesNotExist expected for: -tar names an archive that does not exist";
}

// -timescale=: -timescale= with no value.
class CmdArgsTimescaleEmptyTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-nocompile", "-f", "TimescaleEmpty/TimescaleEmpty.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

TEST_F(CmdArgsTimescaleEmptyTest, ReportsCmdTimescaleMissingSetting) {
  EXPECT_NE(findEvent(EventType::CmdTimescaleMissingSetting), nullptr)
      << "CmdTimescaleMissingSetting expected for: -timescale= with no value";
}

// nosuch.sv: a source file that does not exist.
class CmdArgsVerilogFileMissingTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-nocompile", "-f", "VerilogFileMissing/VerilogFileMissing.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

TEST_F(CmdArgsVerilogFileMissingTest, ReportsCmdVerilogFileDoesNotExist) {
  EXPECT_NE(findEvent(EventType::CmdVerilogFileDoesNotExist), nullptr)
      << "CmdVerilogFileDoesNotExist expected for: a source file that does not exist";
}

// -wd -nocache: -wd followed by another option instead of a directory.
class CmdArgsWdNoArgTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-nocompile", "-f", "WdNoArg/WdNoArg.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

TEST_F(CmdArgsWdNoArgTest, ReportsCmdWdMissingDir) {
  EXPECT_NE(findEvent(EventType::CmdWdMissingDir, "-wd"), nullptr)
      << "CmdWdMissingDir expected for: -wd followed by another option instead of a directory";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
