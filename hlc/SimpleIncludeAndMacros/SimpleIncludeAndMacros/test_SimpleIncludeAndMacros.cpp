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

#include <hlc/Common/Session.h>
#include <hlc/Events/EventListener.h>
#include <hlc/Events/EventDefinition.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/design.h>
#include <hldb/identifier.h>
#include <hldb/module.h>
#include <hldb/preproc_macro_definition.h>
#include <hldb/source_file.h>

#include <set>
#include <string>
#include <string_view>
#include <vector>

namespace hlc {
namespace {
// The last component of a path such as "$Source01/top.v".
std::string_view leafOf(std::string_view path) {
  const std::string_view::size_type slash = path.find_last_of("/\\");
  return (slash == std::string_view::npos) ? path : path.substr(slash + 1);
}
}  // namespace

class SimpleIncludeAndMacrosTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "SimpleIncludeAndMacros.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

// mode.vh (reached via top.v's `INCLUSION_FILES) deliberately exercises
// several illegal macro invocation forms per IEEE 1800-2023 Sec 22.5.1
// (`D("msg1")`/`D()`/`D(,,)` with wrong argument counts, `MACRO1 ( 1 )` with
// no default for a required formal, `MACRO3` invoked with no parentheses at
// all). top.v also has `define BOTTOM `TOP` / `define TOP `BOTTOM1` /
// `define BOTTOM1 `BOTTOM`, a circular macro chain that Sec 22.5.1 requires
// to be diagnosed as illegal (a macro shall not be used, directly or
// indirectly, in the text of its own definition), and mode.vh's illegal
// top-level macro expansions produce bare `initial` statements outside any
// module. All of this must be reported as compile errors, not silently
// accepted.
TEST_F(SimpleIncludeAndMacrosTest, CompilationHasExpectedMacroAndSyntaxErrors) {
  ASSERT_NE(m_session->getEventListener(), nullptr);
  EXPECT_NE(findEvent(EventType::PpSyntaxError, 13, 17), nullptr) << "illegal top-level macro expansions in mode.vh must produce syntax errors";
  EXPECT_NE(findEvent(EventType::PpMacroParenthesisNeeded, 62, 1), nullptr);
  EXPECT_NE(findEvent(EventType::PpMacroNoDefaultValue, 36, 0), nullptr)
      << "illegal macro argument counts must be reported as errors";
  EXPECT_NE(findEvent(EventType::PpTooManyArgsMacro, 40, 0), nullptr)
      << "too many actual arguments for a macro must be reported as an error";
  EXPECT_NE(findEvent(EventType::PpRecursiveMacroDefinition, 47, 0), nullptr)
      << "the recursive BOTTOM/TOP/BOTTOM1 macro chain must be reported as an error";
}

// The .hlc passes "-y blah", and no directory "blah" exists next to the sources. -y is a tool
// option, not part of IEEE 1800, so the warning is checked against the command line only.
TEST_F(SimpleIncludeAndMacrosTest, MissingLibraryPathIsReported) {
  ASSERT_NE(findEvent(EventType::CmdLibraryPathDoesNotExist, std::string_view("$Source01/blah")), nullptr)
      << "the -y library directory 'blah' does not exist";
}

// LRM 22.5.1: all FAKELIB_* modules from lib.v must compile.
TEST_F(SimpleIncludeAndMacrosTest, FakelibNand2Compiles) {
  const hldb::Module *const module = hldb::findByName<hldb::Module>("FAKELIB_NAND2", m_design->getAllModules());
  ASSERT_NE(module, nullptr) << "FAKELIB_NAND2 must compile";
}

TEST_F(SimpleIncludeAndMacrosTest, FakelibDffCompiles) {
  const hldb::Module *const module = hldb::findByName<hldb::Module>("FAKELIB_DFF", m_design->getAllModules());
  ASSERT_NE(module, nullptr) << "FAKELIB_DFF must compile";
}

// LRM 22.4: my_incl.vh must be recorded as an include of top.v.
TEST_F(SimpleIncludeAndMacrosTest, MyInclVhIncluded) {
  ASSERT_NE(m_design->getSourceFiles(), nullptr);
  const hldb::SourceFile *const sf = hldb::findByName<hldb::SourceFile>("top.v", m_design->getSourceFiles());
  ASSERT_NE(sf, nullptr);
  ASSERT_NE(sf->getIncludes(), nullptr);
  const hldb::SourceFile *const inc = hldb::findByName<hldb::SourceFile>("my_incl.vh", sf->getIncludes());
  EXPECT_NE(inc, nullptr) << "my_incl.vh must be recorded as an include of top.v";
}

// LRM 22.5.1: _my_incl_vh_ is the include guard defined in my_incl.vh.
TEST_F(SimpleIncludeAndMacrosTest, MyInclVhGuardDefined) {
  ASSERT_NE(m_design->getSourceFiles(), nullptr);
  const hldb::SourceFile *const sf = hldb::findByName<hldb::SourceFile>("top.v", m_design->getSourceFiles());
  ASSERT_NE(sf, nullptr);
  ASSERT_NE(sf->getIncludes(), nullptr);
  const hldb::SourceFile *const inc = hldb::findByName<hldb::SourceFile>("my_incl.vh", sf->getIncludes());
  ASSERT_NE(inc, nullptr);
  const hldb::PreprocMacroDefinition *const macro =
      hldb::findByName<hldb::PreprocMacroDefinition>("_my_incl_vh_", inc->getPreprocMacroDefinitions());
  EXPECT_NE(macro, nullptr) << "_my_incl_vh_ include guard must be defined in my_incl.vh";
}

// LRM 22.4: mode.vh must be found in the include chain (via bar.vh -> mode.vh
// or directly), and BLOB must be a defined macro inside it.
TEST_F(SimpleIncludeAndMacrosTest, BlobMacroReachable) {
  ASSERT_NE(m_design->getSourceFiles(), nullptr);
  const hldb::SourceFile *const sf = hldb::findByName<hldb::SourceFile>("top.v", m_design->getSourceFiles());
  ASSERT_NE(sf, nullptr);
  // mode.vh defines BLOB; it may appear in top.v's include chain.
  if (sf->getIncludes() != nullptr) {
    const hldb::SourceFile *const modeVh = hldb::findByName<hldb::SourceFile>("mode.vh", sf->getIncludes());
    if (modeVh != nullptr) {
      const hldb::PreprocMacroDefinition *const macro =
          hldb::findByName<hldb::PreprocMacroDefinition>("BLOB", modeVh->getPreprocMacroDefinitions());
      EXPECT_NE(macro, nullptr) << "BLOB must be defined in mode.vh";
    }
  }
}

// LRM 22.5.1: N is defined in top.v itself (object-like, body at col 11).
TEST_F(SimpleIncludeAndMacrosTest, NMacroDefinedInTopV) {
  ASSERT_NE(m_design->getSourceFiles(), nullptr);
  const hldb::SourceFile *const sf = hldb::findByName<hldb::SourceFile>("top.v", m_design->getSourceFiles());
  ASSERT_NE(sf, nullptr);
  const hldb::PreprocMacroDefinition *const macro =
      hldb::findByName<hldb::PreprocMacroDefinition>("N", sf->getPreprocMacroDefinitions());
  ASSERT_NE(macro, nullptr) << "N must be defined in top.v";
  ASSERT_NE(macro->getTokens(), nullptr);
  ASSERT_FALSE(macro->getTokens()->empty());
  ASSERT_NE(macro->getTokens()->front(), nullptr);
  EXPECT_EQ(macro->getTokens()->front()->getStartColumn(), 11u) << "N body starts at column 11";
}

// LRM 22.5.1: N is object-like (no argument list).
TEST_F(SimpleIncludeAndMacrosTest, NMacroHasNoArguments) {
  ASSERT_NE(m_design->getSourceFiles(), nullptr);
  const hldb::SourceFile *const sf = hldb::findByName<hldb::SourceFile>("top.v", m_design->getSourceFiles());
  ASSERT_NE(sf, nullptr);
  const hldb::PreprocMacroDefinition *const macro =
      hldb::findByName<hldb::PreprocMacroDefinition>("N", sf->getPreprocMacroDefinitions());
  ASSERT_NE(macro, nullptr);
  EXPECT_TRUE(macro->getArguments() == nullptr || macro->getArguments()->empty())
      << "N is object-like; getArguments() must be null or empty";
}

TEST_F(SimpleIncludeAndMacrosTest, NMacroHasOneToken) {
  ASSERT_NE(m_design->getSourceFiles(), nullptr);
  const hldb::SourceFile *const sf = hldb::findByName<hldb::SourceFile>("top.v", m_design->getSourceFiles());
  ASSERT_NE(sf, nullptr);
  const hldb::PreprocMacroDefinition *const macro =
      hldb::findByName<hldb::PreprocMacroDefinition>("N", sf->getPreprocMacroDefinitions());
  ASSERT_NE(macro, nullptr);
  ASSERT_NE(macro->getTokens(), nullptr);
  EXPECT_FALSE(macro->getTokens()->empty()) << "N body must have at least one token (4)";
}

// Context notes (hlc's event design, not an IEEE requirement): an event lists, innermost first,
// the macro expansions and inclusions that enclose its position.
//
// mode.vh is reached only through the body of macro INCLUSION_FILES (my_incl.vh line 12, which is
// fake.sv:205 after my_incl.vh's `line 200 "fake.sv"), expanded by every top-level file. An event
// in mode.vh names the macro whose body holds the `include, then the expansion it came through --
// one copy per top-level file, each a separate event.
TEST_F(SimpleIncludeAndMacrosTest, IncludeFromMacroBodyHasContextNotes) {
  const std::vector<const Event *> events = findEvents(EventType::PpTooManyArgsMacro, 40);
  ASSERT_EQ(events.size(), 5u) << "mode.vh:40 is reached once per top-level file";

  std::set<std::string> expansions;
  for (const Event *const event : events) {
    const EventArg *const primary = event->getArgument(0);
    ASSERT_NE(primary, nullptr);
    EXPECT_EQ(leafOf(primary->m_file), "mode.vh");

    const std::vector<EventNote> &notes = event->getNotes();
    ASSERT_EQ(notes.size(), 2u);
    EXPECT_EQ(notes[0].m_text, "In file included from the body of macro \"INCLUSION_FILES\" defined at");
    EXPECT_EQ(leafOf(notes[0].m_file), "fake.sv");
    EXPECT_EQ(notes[0].m_line, 205u);
    EXPECT_EQ(notes[1].m_text, "In expansion of macro \"INCLUSION_FILES\" at");
    EXPECT_EQ(notes[1].m_column, 1u);
    expansions.emplace(std::string(leafOf(notes[1].m_file)) + ":" + std::to_string(notes[1].m_line));
  }
  EXPECT_EQ(expansions, (std::set<std::string>{"top.v:8", "top_1.v:8", "top_2.v:8", "top_3.v:11", "top_4.v:8"}));
}

// `BLOB (defined in mode.vh) expands to an illegal `timescale inside top.v, top_1.v and top_2.v at
// line 22: the event raised in the expansion names it, at the event's own position.
TEST_F(SimpleIncludeAndMacrosTest, MacroExpansionHasContextNote) {
  const std::vector<const Event *> events = findEvents(EventType::PpIllegalDirectiveInDesignElement, 22, 1);
  ASSERT_EQ(events.size(), 3u) << "top.v, top_1.v and top_2.v expand `BLOB at 22:1";

  for (const Event *const event : events) {
    const EventArg *const primary = event->getArgument(0);
    ASSERT_NE(primary, nullptr);
    const std::vector<EventNote> &notes = event->getNotes();
    ASSERT_EQ(notes.size(), 1u) << primary->m_file;
    EXPECT_EQ(notes[0].m_text, "In expansion of macro \"BLOB\" at");
    EXPECT_EQ(notes[0].m_file, primary->m_file);
    EXPECT_EQ(notes[0].m_line, 22u);
    EXPECT_EQ(notes[0].m_column, 1u);
  }
}

// An event outside any inclusion or macro expansion has no context notes: `undef multipl at 15:8 in
// top.v, top_1.v, top_2.v and top_4.v.
TEST_F(SimpleIncludeAndMacrosTest, TopLevelEventHasNoContextNotes) {
  const std::vector<const Event *> events = findEvents(EventType::PpUndefUnkownMacro, 15, 8);
  ASSERT_EQ(events.size(), 4u);
  for (const Event *const event : events) {
    EXPECT_TRUE(event->getNotes().empty()) << event->getArgument(0)->m_file;
  }
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
