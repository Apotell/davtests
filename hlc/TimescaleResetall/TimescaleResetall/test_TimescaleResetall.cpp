/*
 Copyright 2026 Apotell

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

// Time unit / precision of design elements around `timescale and `resetall
// (tests/TimescaleResetall, compiled with -fileunit so each file is its own
// compilation unit).
//
// IEEE 1800-2023 Sec 3.14.2.3: with no timeunit of its own, a design element takes
//   a) the enclosing module's, b) else the last `timescale, c) else the
//   compilation-unit timeunit, d) else the default. "The time unit of the
//   compilation-unit scope can only be set by a timeunit declaration, not a
//   `timescale directive." Precision follows the same precedence on its own.
// IEEE 1800-2023 Sec 22.7: "If there is no `timescale specified or it has been reset
//   by a `resetall directive, the default time unit and precision are tool-specific."
//   HLC leaves the default unset (0).
// IEEE 1800-2023 Sec 22.3: `resetall resets compiler directives only; a timeunit /
//   timeprecision declaration is not a directive.
//
// Values are vpiTimeUnit/vpiTimePrecision encodings: 10us = -5, 1us = -6, 1ns = -9,
// 100ps = -10, 1ps = -12.

#include <hlc/Common/Session.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/design.h>
#include <hldb/module.h>
#include <hldb/source_file.h>

#include <gtest/gtest.h>

namespace hlc {

class TimescaleResetallTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "TimescaleResetall.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *findModule(std::string_view name) {
    return hldb::findByName<hldb::Module>(name, m_design->getAllModules());
  }
  static const hldb::SourceFile *findSourceFile(std::string_view name) {
    return hldb::findByName<hldb::SourceFile>(name, m_design->getSourceFiles());
  }
};

// ---- cu_timeunit.sv ----

// The compilation unit declares "timeunit 10us; timeprecision 1us;". The later
// `timescale 1ns/1ps must not change it (Sec 3.14.2.3).
TEST_F(TimescaleResetallTest, CompilationUnitKeepsItsOwnTimeunit) {
  const hldb::SourceFile *const sf = findSourceFile("cu_timeunit.sv");
  ASSERT_NE(sf, nullptr);
  EXPECT_EQ(sf->getTimeUnit(), -5);
  EXPECT_EQ(sf->getTimePrecision(), -6);
}

// No `timescale yet: rule c, the compilation-unit timeunit.
TEST_F(TimescaleResetallTest, ModuleBeforeTimescaleUsesCompilationUnit) {
  const hldb::Module *const m = findModule("cu_before_timescale");
  ASSERT_NE(m, nullptr);
  EXPECT_EQ(m->getTimeUnit(), -5);
  EXPECT_EQ(m->getTimePrecision(), -6);
}

// Rule b (last `timescale) takes precedence over rule c.
TEST_F(TimescaleResetallTest, TimescaleBeatsCompilationUnit) {
  const hldb::Module *const m = findModule("after_timescale");
  ASSERT_NE(m, nullptr);
  EXPECT_EQ(m->getTimeUnit(), -9);
  EXPECT_EQ(m->getTimePrecision(), -12);
}

// Own "timeunit 100ps;" wins for the unit; the precision, not declared, comes from
// the `timescale (Sec 3.14.2.3: precision follows the same precedence on its own).
TEST_F(TimescaleResetallTest, OwnTimeunitWithPrecisionFromTimescale) {
  const hldb::Module *const m = findModule("own_timeunit");
  ASSERT_NE(m, nullptr);
  EXPECT_EQ(m->getTimeUnit(), -10);
  EXPECT_EQ(m->getTimePrecision(), -12);
}

// `resetall removes the `timescale but not the compilation-unit timeunit, so rule c
// applies again.
TEST_F(TimescaleResetallTest, ResetallFallsBackToCompilationUnit) {
  const hldb::Module *const m = findModule("cu_after_resetall");
  ASSERT_NE(m, nullptr);
  EXPECT_EQ(m->getTimeUnit(), -5);
  EXPECT_EQ(m->getTimePrecision(), -6);
}

// ---- no_cu_timeunit.sv ----

// Only a `timescale, which never sets the compilation-unit scope.
TEST_F(TimescaleResetallTest, TimescaleDoesNotSetCompilationUnit) {
  const hldb::SourceFile *const sf = findSourceFile("no_cu_timeunit.sv");
  ASSERT_NE(sf, nullptr);
  EXPECT_EQ(sf->getTimeUnit(), 0);
  EXPECT_EQ(sf->getTimePrecision(), 0);
}

TEST_F(TimescaleResetallTest, ModuleBeforeResetallUsesTimescale) {
  const hldb::Module *const m = findModule("timescale_before_resetall");
  ASSERT_NE(m, nullptr);
  EXPECT_EQ(m->getTimeUnit(), -9);
  EXPECT_EQ(m->getTimePrecision(), -12);
}

// `resetall with no compilation-unit timeunit: the tool default, left unset.
TEST_F(TimescaleResetallTest, ResetallLeavesDefaultTimescale) {
  const hldb::Module *const m = findModule("default_after_resetall");
  ASSERT_NE(m, nullptr);
  EXPECT_EQ(m->getTimeUnit(), 0);
  EXPECT_EQ(m->getTimePrecision(), 0);
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
