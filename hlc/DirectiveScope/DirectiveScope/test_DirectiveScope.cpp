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

// Tests for tests/DirectiveScope -- where compiler directives are recorded, and
// how a design element's time unit and precision are resolved, down to the
// command-line -timescale default.
//
// Compiler directives (IEEE 1800-2023 Sec 22.1): a directive's effect runs from
// where it is written through the rest of the compilation unit, following the
// source text rather than the design hierarchy. So every directive is recorded
// on the SourceFile it appears in, in source order -- also one written inside a
// module -- and one in an included file on that file's own SourceFile.
//
//   dirs.sv   `timescale 1ns/1ps
//             `pragma protect begin
//             `include "inc.svh"
//             module m_dir;  `pragma my_tool on  endmodule
//   inc.svh   `default_nettype wire
//             `pragma inc_tool
//
// Time unit and precision (Sec 3.14.2.3), each resolved on its own:
//   a) the element's own timeunit / timeprecision;
//   b) else the enclosing element's, for a nested definition (not an instance);
//   c) else the last `timescale in the compilation unit;
//   d) else the compilation-unit timeunit / timeprecision;
//   e) else "the default time unit", which is tool-specific: here the
//      command-line -timescale when given.
//
//   ts_none.sv  module m_none;                                    nothing
//   ts_unit.sv  module m_unit; timeunit 10ns;                     unit only
//   ts_prec.sv  module m_prec; timeprecision 1ps;                 precision only
//   ts_dir.sv   `timescale 1us/1ns  module m_after_ts;            rule (c)
//   ts_cu.sv    timeunit 100ps / 10ps;  module m_cu;              rule (d)
//   ts_nest.sv  module m_outer; timeunit 10us; module m_inner;    rule (b)
//   ts_inst.sv  module m_parent; timeunit 100us; m_none u_none(); not inherited
//   ts_1s.sv    module m_1s; timeunit 1s / 1ms;                   1s encodes as 0
//
// `line (Sec 22.12) "specifies the line number of the next line" and the file name
// of the following text; it is a directive, not an include:
//
//   line.sv   `define BEFORE 1
//             `line 50 "gen.sv" 1
//             `define AFTER 2          -> reported as gen.sv line 50
//             module m_line;           -> reported as gen.sv line 51
//             endmodule
//
// Across files (Sec 3.12.1): "Within a separately compiled unit, compiler directives
// once seen by a tool apply to all subsequent source text. However, compiler directives
// from one separately compiled unit shall not affect other compilation units." The
// Unit references every one of its directives in compilation order (each still owned by
// its SourceFile), and a design element's directive defaults follow that order:
//
//   u_a.sv     `default_nettype none  `include "u_inc.svh"  module u_ma
//   u_inc.svh  `celldefine
//   u_b.sv     `pragma u_tool  module u_mb  `resetall
//   u_c.sv     module u_mc
//
// The ts_*.sv files are compiled with -fileunit, so no `timescale or
// compilation-unit timeunit crosses from one file into another (Sec 3.12.1).
//
//   suite                 option file           -timescale
//   DirectivesTest        Directives.f          (none)
//   CmdTimescaleTest      CmdTimescale.f        1ns/1ps
//   NoCmdTimescaleTest    NoCmdTimescale.f      (none)
//   BadCmdTimescaleTest   BadCmdTimescale.f     3ns/1ps (illegal magnitude)
//   LineTest              Line.f                (none)
//   UnitDirectivesTest    UnitDirectives.f      (none), u_a u_b u_c in one unit
//   UnitDirectivesFile..  UnitDirectivesFileUnit.f  (none), the same with -fileunit
//
// Time units are base-10 exponents of one second: 1s == 0, 1ms == -3, 100us == -4, 10us == -5,
// 1us == -6, 10ns == -8, 1ns == -9, 100ps == -10, 10ps == -11, 1ps == -12.

#include <hlc/Common/Session.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Database.h>
#include <hldb/Utils.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/generic_directive.h>
#include <hldb/hldb_vpi_user.h>
#include <hldb/module.h>
#include <hldb/pragma_directive.h>
#include <hldb/preproc_macro_definition.h>
#include <hldb/source_file.h>
#include <hldb/unit.h>
#include <hldb/vpi_user.h>

#include <string_view>

namespace hlc {

class DirectiveScopeTest : public Test {
 protected:
  static const hldb::SourceFile *findSourceFile(std::string_view name) {
    return hldb::findByName<hldb::SourceFile>(name, m_design->getSourceFiles());
  }

  // Any module definition, nested ones included.
  static const hldb::Module *findModule(std::string_view name) {
    for (const hldb::Any *const any : m_session->getDatabase().getObjects<hldb::Module>()) {
      const hldb::Module *const m = any_cast<hldb::Module>(any);
      if ((m != nullptr) && (m->getName() == name)) return m;
    }
    return nullptr;
  }

  static void expectTime(std::string_view name, int32_t unit, int32_t precision) {
    const hldb::Module *const m = findModule(name);
    ASSERT_NE(m, nullptr) << name;
    EXPECT_EQ(m->getTimeUnit(), unit) << name << ": time unit";
    EXPECT_EQ(m->getTimePrecision(), precision) << name << ": time precision";
  }

  static void expectGeneric(const hldb::SourceFile *sf, size_t index, int32_t type) {
    ASSERT_NE(sf, nullptr);
    ASSERT_NE(sf->getDirectives(), nullptr);
    ASSERT_LT(index, sf->getDirectives()->size());
    const hldb::GenericDirective *const gd = any_cast<hldb::GenericDirective>(sf->getDirectives()->at(index));
    ASSERT_NE(gd, nullptr) << "directive #" << index << " is not a GenericDirective";
    EXPECT_EQ(gd->getDirectiveType(), type) << "directive #" << index;
    EXPECT_EQ(gd->getParent(), sf) << "directive #" << index;
  }

  static void expectPragma(const hldb::SourceFile *sf, size_t index, std::string_view name) {
    ASSERT_NE(sf, nullptr);
    ASSERT_NE(sf->getDirectives(), nullptr);
    ASSERT_LT(index, sf->getDirectives()->size());
    const hldb::PragmaDirective *const pd = any_cast<hldb::PragmaDirective>(sf->getDirectives()->at(index));
    ASSERT_NE(pd, nullptr) << "directive #" << index << " is not a PragmaDirective";
    EXPECT_EQ(pd->getName(), name) << "directive #" << index;
    EXPECT_EQ(pd->getParent(), sf) << "directive #" << index;
  }
};

// ----
// Directives.f -- dirs.sv (includes inc.svh).

class DirectivesTest : public DirectiveScopeTest {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "Directives.f"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

TEST_F(DirectivesTest, EveryDirectiveIsOnItsSourceFileInSourceOrder) {
  // Sec 22.1. The `pragma inside m_dir is recorded on dirs.sv, not on the module:
  // its effect follows the source text, not the hierarchy (Sec 22.11).
  const hldb::SourceFile *const sf = findSourceFile("dirs.sv");
  ASSERT_NE(sf, nullptr);
  ASSERT_NE(sf->getDirectives(), nullptr);
  ASSERT_EQ(sf->getDirectives()->size(), 3u);
  expectGeneric(sf, 0, vpiDirectiveTypeTimeScale);
  expectPragma(sf, 1, "protect");
  expectPragma(sf, 2, "my_tool");
}

TEST_F(DirectivesTest, TimescaleDirectiveRecordsItsArguments) {
  // Sec 22.7: `timescale time_unit / time_precision.
  const hldb::SourceFile *const sf = findSourceFile("dirs.sv");
  ASSERT_NE(sf, nullptr);
  ASSERT_NE(sf->getDirectives(), nullptr);
  ASSERT_FALSE(sf->getDirectives()->empty());
  const hldb::GenericDirective *const gd = any_cast<hldb::GenericDirective>(sf->getDirectives()->at(0));
  ASSERT_NE(gd, nullptr);
  ASSERT_NE(gd->getValue(), nullptr);
  const hldb::Constant *const c = any_cast<hldb::Constant>(gd->getValue());
  ASSERT_NE(c, nullptr);
  EXPECT_EQ(c->getConstType(), vpiStringConst);
  EXPECT_EQ(c->getValue(), "1ns/1ps");
  EXPECT_EQ(c->getStartLine(), 1u);
}

TEST_F(DirectivesTest, IncludedFileDirectivesAreOnItsOwnSourceFile) {
  // Sec 22.4: the included text replaces the `include; its directives are
  // recorded on the included file's own SourceFile, nested under dirs.sv.
  const hldb::SourceFile *const sf = findSourceFile("dirs.sv");
  ASSERT_NE(sf, nullptr);
  const hldb::SourceFile *const inc = hldb::findByName<hldb::SourceFile>("inc.svh", sf->getIncludes());
  ASSERT_NE(inc, nullptr);
  ASSERT_NE(inc->getDirectives(), nullptr);
  ASSERT_EQ(inc->getDirectives()->size(), 2u);
  expectGeneric(inc, 0, vpiDirectiveTypeDefaultNetType);
  expectPragma(inc, 1, "inc_tool");
}

TEST_F(DirectivesTest, TimescaleStillSetsTheTimeScope) {
  // Recording the directive does not change its effect (Sec 22.7).
  expectTime("m_dir", -9, -12);
}

// ----
// CmdTimescale.f -- -timescale=1ns/1ps, -fileunit.

class CmdTimescaleTest : public DirectiveScopeTest {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "CmdTimescale.f"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

TEST_F(CmdTimescaleTest, ElementWithNothingGetsTheDefault) { expectTime("m_none", -9, -12); }

TEST_F(CmdTimescaleTest, OwnUnitKeptPrecisionFromDefault) {
  // "If a timeprecision is not specified in the current time scope, then the time
  // precision shall be determined following the same precedence as with time units."
  expectTime("m_unit", -8, -12);
}

TEST_F(CmdTimescaleTest, OwnPrecisionKeptUnitFromDefault) { expectTime("m_prec", -9, -12); }

TEST_F(CmdTimescaleTest, TimescaleDirectiveBeatsTheDefault) { expectTime("m_after_ts", -6, -9); }

TEST_F(CmdTimescaleTest, CompilationUnitTimeunitBeatsTheDefault) { expectTime("m_cu", -10, -11); }

TEST_F(CmdTimescaleTest, NestedDefinitionInheritsEnclosingUnit) {
  expectTime("m_outer", -5, -12);
  expectTime("m_inner", -5, -12);
}

TEST_F(CmdTimescaleTest, InstanceDoesNotInheritFromItsInstantiator) {
  // m_none is instantiated in m_parent (timeunit 100us) but defined at the top of
  // its own file: rule (b) is about nested definitions, not instantiation.
  expectTime("m_parent", -4, -12);
  expectTime("m_none", -9, -12);
}

TEST_F(CmdTimescaleTest, OwnOneSecondUnitIsNotReplacedByTheDefault) {
  // 1s is a legal time unit (Sec 3.14 Table 3-1) and encodes as 0; it is m_1s's own
  // declaration, so the default must not replace it.
  expectTime("m_1s", 0, -3);
}

TEST_F(CmdTimescaleTest, DefaultUseIsReportedAndNothingIsLeftUnset) {
  EXPECT_NE(findError(ErrorDefinition::CMD_USING_GLOBAL_TIMESCALE, "1ns/1ps"), nullptr);
  EXPECT_EQ(findError(ErrorDefinition::PA_NOTIMESCALE_INFO), nullptr);
  EXPECT_EQ(findError(ErrorDefinition::PA_MISSING_TIMEUNIT), nullptr);
}

// ----
// NoCmdTimescale.f -- the same files, no -timescale.

class NoCmdTimescaleTest : public DirectiveScopeTest {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "NoCmdTimescale.f"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

TEST_F(NoCmdTimescaleTest, ElementWithNothingIsReported) {
  expectTime("m_none", 0, 0);
  EXPECT_NE(findError(ErrorDefinition::PA_NOTIMESCALE_INFO, "m_none"), nullptr);
}

TEST_F(NoCmdTimescaleTest, ElementWithoutPrecisionIsReported) {
  expectTime("m_unit", -8, 0);
  EXPECT_NE(findError(ErrorDefinition::PA_NOTIMESCALE_INFO, "m_unit"), nullptr);
}

TEST_F(NoCmdTimescaleTest, ElementWithoutUnitIsReported) {
  expectTime("m_prec", 0, -12);
  EXPECT_NE(findError(ErrorDefinition::PA_MISSING_TIMEUNIT, "m_prec"), nullptr);
}

TEST_F(NoCmdTimescaleTest, ResolvedElementsAreNotReported) {
  expectTime("m_after_ts", -6, -9);
  expectTime("m_cu", -10, -11);
  expectTime("m_1s", 0, -3);
  EXPECT_EQ(findError(ErrorDefinition::PA_NOTIMESCALE_INFO, "m_after_ts"), nullptr);
  EXPECT_EQ(findError(ErrorDefinition::PA_NOTIMESCALE_INFO, "m_cu"), nullptr);
  // m_1s declares a 1s unit, which is not "missing" although it encodes as 0.
  EXPECT_EQ(findError(ErrorDefinition::PA_MISSING_TIMEUNIT, "m_1s"), nullptr);
  EXPECT_EQ(findError(ErrorDefinition::PA_NOTIMESCALE_INFO, "m_1s"), nullptr);
  EXPECT_EQ(findError(ErrorDefinition::CMD_USING_GLOBAL_TIMESCALE), nullptr);
}

// ----
// BadCmdTimescale.f -- -timescale=3ns/1ps: a magnitude must be 1, 10 or 100 (Sec 22.7).

class BadCmdTimescaleTest : public DirectiveScopeTest {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "BadCmdTimescale.f"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

TEST_F(BadCmdTimescaleTest, IllegalValueIsReportedAndNotApplied) {
  EXPECT_NE(findError(ErrorDefinition::PA_TIMESCALE_INVALID_VALUE, "3ns"), nullptr);
  expectTime("m_none", 0, 0);
  EXPECT_NE(findError(ErrorDefinition::PA_NOTIMESCALE_INFO, "m_none"), nullptr);
  EXPECT_EQ(findError(ErrorDefinition::CMD_USING_GLOBAL_TIMESCALE), nullptr);
}

// ----
// Line.f -- line.sv.

class LineTest : public DirectiveScopeTest {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "Line.f"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static bool endsWith(std::string_view text, std::string_view suffix) {
    return (text.size() >= suffix.size()) && (text.substr(text.size() - suffix.size()) == suffix);
  }
};

TEST_F(LineTest, LineFileNameIsNotAnInclude) {
  // Sec 22.12: the file name is "the new name of the file", nothing is read.
  const hldb::SourceFile *const sf = findSourceFile("line.sv");
  ASSERT_NE(sf, nullptr);
  EXPECT_EQ(sf->getIncludes(), nullptr);
}

TEST_F(LineTest, LineIsADirectiveOnItsSourceFile) {
  const hldb::SourceFile *const sf = findSourceFile("line.sv");
  ASSERT_NE(sf, nullptr);
  ASSERT_NE(sf->getDirectives(), nullptr);
  ASSERT_EQ(sf->getDirectives()->size(), 1u);
  expectGeneric(sf, 0, vpiDirectiveTypeLine);
  const hldb::GenericDirective *const gd = any_cast<hldb::GenericDirective>(sf->getDirectives()->at(0));
  ASSERT_NE(gd, nullptr);
  EXPECT_TRUE(endsWith(gd->getFile(), "line.sv")) << gd->getFile();
  EXPECT_EQ(gd->getStartLine(), 2u);
  ASSERT_NE(gd->getValue(), nullptr);
  const hldb::Constant *const c = any_cast<hldb::Constant>(gd->getValue());
  ASSERT_NE(c, nullptr);
  EXPECT_EQ(c->getValue(), "50 \"gen.sv\" 1");
}

TEST_F(LineTest, MacrosStayOnTheirSourceFile) {
  // Both macros are defined in line.sv; the one after the `line is reported at the
  // file and line the `line gave its line (Sec 22.12), but still belongs to line.sv.
  const hldb::SourceFile *const sf = findSourceFile("line.sv");
  ASSERT_NE(sf, nullptr);
  const hldb::PreprocMacroDefinition *const before =
      hldb::findByName<hldb::PreprocMacroDefinition>("BEFORE", sf->getPreprocMacroDefinitions());
  const hldb::PreprocMacroDefinition *const after =
      hldb::findByName<hldb::PreprocMacroDefinition>("AFTER", sf->getPreprocMacroDefinitions());
  ASSERT_NE(before, nullptr);
  ASSERT_NE(after, nullptr);
  EXPECT_EQ(before->getStartLine(), 1u);
  EXPECT_TRUE(endsWith(after->getFile(), "gen.sv")) << after->getFile();
  EXPECT_EQ(after->getStartLine(), 50u);
}

TEST_F(LineTest, LineNumberIsThatOfTheNextLine) {
  // "The number shall be a positive integer that specifies the line number of the next
  // line": `define AFTER is line 50, so module m_line is line 51 of gen.sv.
  const hldb::Module *const m = findModule("m_line");
  ASSERT_NE(m, nullptr);
  EXPECT_TRUE(endsWith(m->getFile(), "gen.sv")) << m->getFile();
  EXPECT_EQ(m->getStartLine(), 51u);
}

// ----
// UnitDirectives.f / UnitDirectivesFileUnit.f -- u_a.sv u_b.sv u_c.sv.

class UnitDirectivesBase : public DirectiveScopeTest {
 protected:
  // The unit that lists 'fileName' among its source files.
  static const hldb::Unit *findUnitOf(std::string_view fileName) {
    if (m_design->getUnits() == nullptr) return nullptr;
    for (const hldb::Unit *const unit : *m_design->getUnits()) {
      if (hldb::findByName<hldb::SourceFile>(fileName, unit->getSourceFiles()) != nullptr) return unit;
    }
    return nullptr;
  }

  // The directive at 'index' of the unit is the same object as the one at
  // 'sourceIndex' of the SourceFile that owns it.
  static void expectUnitDirective(const hldb::Unit *unit, size_t index, const hldb::SourceFile *owner,
                                  size_t sourceIndex, int32_t type) {
    ASSERT_NE(unit, nullptr);
    ASSERT_NE(owner, nullptr);
    ASSERT_NE(unit->getDirectives(), nullptr);
    ASSERT_LT(index, unit->getDirectives()->size());
    ASSERT_NE(owner->getDirectives(), nullptr);
    ASSERT_LT(sourceIndex, owner->getDirectives()->size());
    const hldb::Directive *const directive = unit->getDirectives()->at(index);
    EXPECT_EQ(directive, owner->getDirectives()->at(sourceIndex)) << "unit directive #" << index;
    EXPECT_EQ(directive->getParent(), owner) << "unit directive #" << index;
    if (type != 0) {
      const hldb::GenericDirective *const gd = any_cast<hldb::GenericDirective>(directive);
      ASSERT_NE(gd, nullptr) << "unit directive #" << index;
      EXPECT_EQ(gd->getDirectiveType(), type) << "unit directive #" << index;
    } else {
      EXPECT_NE(any_cast<hldb::PragmaDirective>(directive), nullptr) << "unit directive #" << index;
    }
  }

  static const hldb::SourceFile *findInclude(const hldb::SourceFile *sf, std::string_view name) {
    return (sf == nullptr) ? nullptr : hldb::findByName<hldb::SourceFile>(name, sf->getIncludes());
  }
};

class UnitDirectivesTest : public UnitDirectivesBase {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "UnitDirectives.f"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

TEST_F(UnitDirectivesTest, UnitListsItsDirectivesInCompilationOrder) {
  // One compilation unit: its directives are those of u_a.sv (with u_inc.svh where it is
  // included), then u_b.sv, each the same object its SourceFile owns.
  const hldb::SourceFile *const a = findSourceFile("u_a.sv");
  const hldb::SourceFile *const b = findSourceFile("u_b.sv");
  const hldb::SourceFile *const inc = findInclude(a, "u_inc.svh");
  ASSERT_NE(inc, nullptr);
  const hldb::Unit *const unit = findUnitOf("u_a.sv");
  ASSERT_NE(unit, nullptr);
  EXPECT_EQ(findUnitOf("u_c.sv"), unit);
  ASSERT_NE(unit->getDirectives(), nullptr);
  ASSERT_EQ(unit->getDirectives()->size(), 4u);
  expectUnitDirective(unit, 0, a, 0, vpiDirectiveTypeDefaultNetType);
  expectUnitDirective(unit, 1, inc, 0, vpiDirectiveTypeCellDefine);
  expectUnitDirective(unit, 2, b, 0, 0);  // `pragma u_tool
  expectUnitDirective(unit, 3, b, 1, vpiDirectiveTypeResetAll);
}

TEST_F(UnitDirectivesTest, DirectivesCarryIntoLaterFiles) {
  // u_mb is in u_b.sv, after u_a.sv's `default_nettype none and u_inc.svh's `celldefine.
  const hldb::Module *const ma = findModule("u_ma");
  const hldb::Module *const mb = findModule("u_mb");
  ASSERT_NE(ma, nullptr);
  ASSERT_NE(mb, nullptr);
  EXPECT_EQ(ma->getDefNetType(), vpiNone);
  EXPECT_TRUE(ma->getCellInstance());
  EXPECT_EQ(mb->getDefNetType(), vpiNone);
  EXPECT_TRUE(mb->getCellInstance());
}

TEST_F(UnitDirectivesTest, ResetallEndsThemForLaterFiles) {
  // Sec 22.3: `resetall at the end of u_b.sv sets every directive back to its default
  // for u_c.sv.
  const hldb::Module *const mc = findModule("u_mc");
  ASSERT_NE(mc, nullptr);
  EXPECT_EQ(mc->getDefNetType(), vpiWire);
  EXPECT_FALSE(mc->getCellInstance());
}

class UnitDirectivesFileUnitTest : public UnitDirectivesBase {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "UnitDirectivesFileUnit.f"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

TEST_F(UnitDirectivesFileUnitTest, EachUnitListsOnlyItsOwnDirectives) {
  const hldb::SourceFile *const a = findSourceFile("u_a.sv");
  const hldb::SourceFile *const b = findSourceFile("u_b.sv");
  const hldb::SourceFile *const inc = findInclude(a, "u_inc.svh");
  ASSERT_NE(inc, nullptr);

  const hldb::Unit *const unitA = findUnitOf("u_a.sv");
  ASSERT_NE(unitA, nullptr);
  ASSERT_NE(unitA->getDirectives(), nullptr);
  ASSERT_EQ(unitA->getDirectives()->size(), 2u);
  expectUnitDirective(unitA, 0, a, 0, vpiDirectiveTypeDefaultNetType);
  expectUnitDirective(unitA, 1, inc, 0, vpiDirectiveTypeCellDefine);

  const hldb::Unit *const unitB = findUnitOf("u_b.sv");
  ASSERT_NE(unitB, nullptr);
  EXPECT_NE(unitB, unitA);
  ASSERT_NE(unitB->getDirectives(), nullptr);
  ASSERT_EQ(unitB->getDirectives()->size(), 2u);
  expectUnitDirective(unitB, 0, b, 0, 0);  // `pragma u_tool
  expectUnitDirective(unitB, 1, b, 1, vpiDirectiveTypeResetAll);

  const hldb::Unit *const unitC = findUnitOf("u_c.sv");
  ASSERT_NE(unitC, nullptr);
  EXPECT_EQ(unitC->getDirectives(), nullptr);
}

TEST_F(UnitDirectivesFileUnitTest, DirectivesDoNotCrossUnits) {
  // "compiler directives from one separately compiled unit shall not affect other
  // compilation units": only u_ma, in u_a.sv's own unit, sees them.
  const hldb::Module *const ma = findModule("u_ma");
  const hldb::Module *const mb = findModule("u_mb");
  ASSERT_NE(ma, nullptr);
  ASSERT_NE(mb, nullptr);
  EXPECT_EQ(ma->getDefNetType(), vpiNone);
  EXPECT_TRUE(ma->getCellInstance());
  EXPECT_EQ(mb->getDefNetType(), vpiWire);
  EXPECT_FALSE(mb->getCellInstance());
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
