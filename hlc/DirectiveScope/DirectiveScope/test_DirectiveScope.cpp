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
// The ts_*.sv files are compiled with -fileunit, so no `timescale or
// compilation-unit timeunit crosses from one file into another (Sec 3.12.1).
//
//   suite                 option file           -timescale
//   DirectivesTest        Directives.f          (none)
//   CmdTimescaleTest      CmdTimescale.f        1ns/1ps
//   NoCmdTimescaleTest    NoCmdTimescale.f      (none)
//   BadCmdTimescaleTest   BadCmdTimescale.f     3ns/1ps (illegal magnitude)
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
#include <hldb/source_file.h>

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

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
