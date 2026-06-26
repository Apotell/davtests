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

// Spec-based validation of time literals per IEEE 1800-2017 §5.8.
//
// Key §5.8 rule under test:
//   "The value of a time literal shall be scaled to the current time unit
//    and rounded to the current time precision."
//
// SV source (`timescale 100ps/10ps → unit=100ps, precision=10ps):
//   time a;
//   initial begin
//     a = 1fs;   // assignment 0
//     a = 1ps;   // assignment 1
//     a = 1ns;   // assignment 2
//     a = 1us;   // assignment 3
//     a = 1ms;   // assignment 4
//     a = 1s;    // assignment 5
//     a = 2.1ms; // assignment 6 — real time literal
//   end
//
// UHDM module timescale: vpiTimeUnit = -10 (100ps), vpiTimePrecision = -11 (10ps)
//
// Spec-correct scaled values (literal / 100ps, rounded to nearest 0.1 unit):
//   1fs  → 1e-15 / 100e-12 = 1e-5   → 0
//   1ps  → 1e-12 / 100e-12 = 0.01   → 0
//   1ns  → 1e-9  / 100e-12 = 10     → 10
//   1us  → 1e-6  / 100e-12 = 10000  → 10000
//   1ms  → 1e-3  / 100e-12 = 1e7    → 10000000
//   1s   → 1     / 100e-12 = 1e10   → 10000000000
//   2.1ms → 2.1e-3 / 100e-12 = 2.1e7 → 21000000
//
// KNOWN SURELOG BUG (all 7 assignments):
//   Surelog ignores time units entirely and stores only the raw numeric part
//   of each literal. All 6 integer time literals produce "1" in UHDM; the
//   real time literal produces "2.1". The scaling tests (Assignment*_ScaledPerSpec)
//   will FAIL until Surelog implements §5.8 scaling.

#include <hlc/Common/Session.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/assignment.h>
#include <hldb/begin.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/initial.h>
#include <hldb/module.h>
#include <hldb/net.h>
#include <hldb/process_stmt.h>
#include <hldb/ref_typespec.h>
#include <hldb/time_typespec.h>

#include <string>

namespace hlc {

class TimeLiterals : public Test {
 public:
  static void SetUpTestSuite() {
    Compile(__FILE__, {"-f", "5.8-time-literals.hlc"});

    ASSERT_NE(m_session, nullptr) << "Session is null";
    ASSERT_NE(m_compiler, nullptr) << "Compiler is null";
    ASSERT_NE(m_design, nullptr) << "Design is null";
  }

  static void TearDownTestSuite() {
    m_design = nullptr;
    delete m_compiler;
    m_compiler = nullptr;
    delete m_session;
    m_session = nullptr;
  }
};

static const hldb::Module *getTop(const hldb::Design *d) {
  return hldb::findByName<hldb::Module>("work@top", d->getAllModules());
}

static const hldb::Net *getNetA(const hldb::Design *d) {
  const hldb::Module *m = getTop(d);
  if (!m || !m->getNets()) return nullptr;
  return hldb::findByName<hldb::Net>("a", m->getNets());
}

static const hldb::Begin *getBegin(const hldb::Design *d) {
  const hldb::Module *m = getTop(d);
  if (!m || !m->getProcesses() || m->getProcesses()->empty()) return nullptr;
  const auto *initial =
      any_cast<const hldb::Initial *>((*m->getProcesses())[0]);
  if (!initial) return nullptr;
  return initial->getStmt<hldb::Begin>();
}

static const hldb::Assignment *getAssignment(const hldb::Design *d,
                                              std::size_t index) {
  const hldb::Begin *begin = getBegin(d);
  if (!begin || !begin->getStmts()) return nullptr;
  if (index >= begin->getStmts()->size()) return nullptr;
  return any_cast<const hldb::Assignment *>((*begin->getStmts())[index]);
}

// ---------------------------------------------------------------------------
// Module structure
// ---------------------------------------------------------------------------
TEST_F(TimeLiterals, ModuleExists) {
  ASSERT_NE(getTop(m_design), nullptr) << "module 'work@top' not found";
}

TEST_F(TimeLiterals, OneNetExists) {
  const hldb::Module *const m = getTop(m_design);
  ASSERT_NE(m, nullptr);
  ASSERT_NE(m->getNets(), nullptr);
  EXPECT_EQ(m->getNets()->size(), 1u) << "expected 1 net: a";
}

// ---------------------------------------------------------------------------
// §5.8: the 'time' keyword declares a 64-bit unsigned simulation-time type.
// UHDM must represent it as TimeTypespec, not LogicTypespec or RealTypespec.
// ---------------------------------------------------------------------------
TEST_F(TimeLiterals, NetA_HasTimeTypespec) {
  const hldb::Net *const net = getNetA(m_design);
  ASSERT_NE(net, nullptr);
  ASSERT_NE(net->getTypespec(), nullptr) << "net 'a' has no typespec";
  EXPECT_NE(net->getTypespec()->getActual<hldb::TimeTypespec>(), nullptr)
      << "§5.8: 'time a' must produce a TimeTypespec in UHDM";
}

// ---------------------------------------------------------------------------
// Initial block
// ---------------------------------------------------------------------------
TEST_F(TimeLiterals, InitialBlockHasBegin) {
  ASSERT_NE(getBegin(m_design), nullptr);
}

TEST_F(TimeLiterals, BeginHasSevenStatements) {
  const hldb::Begin *const begin = getBegin(m_design);
  ASSERT_NE(begin, nullptr);
  ASSERT_NE(begin->getStmts(), nullptr);
  EXPECT_EQ(begin->getStmts()->size(), 7u)
      << "expected 7 time literal assignments: 1fs 1ps 1ns 1us 1ms 1s 2.1ms";
}

TEST_F(TimeLiterals, AllAssignmentsAreBlocking) {
  const hldb::Begin *const begin = getBegin(m_design);
  ASSERT_NE(begin, nullptr);
  ASSERT_NE(begin->getStmts(), nullptr);
  for (std::size_t i = 0; i < begin->getStmts()->size(); ++i) {
    const auto *assign =
        any_cast<const hldb::Assignment *>((*begin->getStmts())[i]);
    ASSERT_NE(assign, nullptr) << "stmt[" << i << "] is not an Assignment";
    EXPECT_TRUE(assign->getBlocking())
        << "assignment[" << i << "] should be blocking (=)";
  }
}

// ---------------------------------------------------------------------------
// §5.8: integer time literals (unsigned_number time_unit) must be stored
// as constType = vpiUIntConst (9), size = 64.
// These structural checks pass — the scaling checks below are what fail.
// ---------------------------------------------------------------------------
TEST_F(TimeLiterals, IntegerTimeLiterals_ConstTypeIsUnsignedInt) {
  for (std::size_t i = 0; i <= 5; ++i) {
    const auto *assign = getAssignment(m_design, i);
    ASSERT_NE(assign, nullptr) << "stmt[" << i << "] is null";
    const auto *c = assign->getRhs<hldb::Constant>();
    ASSERT_NE(c, nullptr) << "stmt[" << i << "] RHS is not a Constant";
    EXPECT_EQ(c->getConstType(), 9)
        << "stmt[" << i << "]: integer time literal must be unsigned int (9)";
  }
}

TEST_F(TimeLiterals, IntegerTimeLiterals_SizeIs64) {
  for (std::size_t i = 0; i <= 5; ++i) {
    const auto *assign = getAssignment(m_design, i);
    ASSERT_NE(assign, nullptr) << "stmt[" << i << "] is null";
    const auto *c = assign->getRhs<hldb::Constant>();
    ASSERT_NE(c, nullptr) << "stmt[" << i << "] RHS is not a Constant";
    EXPECT_EQ(c->getSize(), 64)
        << "stmt[" << i << "]: §5.8 'time' is 64-bit — size must be 64";
  }
}

// ---------------------------------------------------------------------------
// §5.8 scaling: value = literal_in_seconds / time_unit, rounded to precision.
// timescale 100ps/10ps → unit=100ps, precision step=0.1 units.
//
// SURELOG BUG: Surelog ignores the time unit and stores only the raw numeric
// part. getValue() returns "1" for all 6 integer literals instead of the
// scaled integer. All tests below will FAIL until the bug is fixed.
// ---------------------------------------------------------------------------

// a = 1fs — 1e-15s / 100e-12s = 1e-5 → rounds to 0
TEST_F(TimeLiterals, Assignment0_1fs_ScaledPerSpec) {
  const auto *c = getAssignment(m_design, 0)->getRhs<hldb::Constant>();
  ASSERT_NE(c, nullptr);
  EXPECT_EQ(std::stoll(std::string(c->getValue())), 0LL)
      << "§5.8: 1fs / 100ps = 1e-5, rounds to 0 — Surelog bug: unit ignored, "
         "stores 1";
}

// a = 1ps — 1e-12s / 100e-12s = 0.01 → rounds to 0
TEST_F(TimeLiterals, Assignment1_1ps_ScaledPerSpec) {
  const auto *c = getAssignment(m_design, 1)->getRhs<hldb::Constant>();
  ASSERT_NE(c, nullptr);
  EXPECT_EQ(std::stoll(std::string(c->getValue())), 0LL)
      << "§5.8: 1ps / 100ps = 0.01, rounds to 0 — Surelog bug: unit ignored, "
         "stores 1";
}

// a = 1ns — 1e-9s / 100e-12s = 10
TEST_F(TimeLiterals, Assignment2_1ns_ScaledPerSpec) {
  const auto *c = getAssignment(m_design, 2)->getRhs<hldb::Constant>();
  ASSERT_NE(c, nullptr);
  EXPECT_EQ(std::stoll(std::string(c->getValue())), 10LL)
      << "§5.8: 1ns / 100ps = 10 — Surelog bug: unit ignored, stores 1";
}

// a = 1us — 1e-6s / 100e-12s = 10000
TEST_F(TimeLiterals, Assignment3_1us_ScaledPerSpec) {
  const auto *c = getAssignment(m_design, 3)->getRhs<hldb::Constant>();
  ASSERT_NE(c, nullptr);
  EXPECT_EQ(std::stoll(std::string(c->getValue())), 10000LL)
      << "§5.8: 1us / 100ps = 10000 — Surelog bug: unit ignored, stores 1";
}

// a = 1ms — 1e-3s / 100e-12s = 10000000
TEST_F(TimeLiterals, Assignment4_1ms_ScaledPerSpec) {
  const auto *c = getAssignment(m_design, 4)->getRhs<hldb::Constant>();
  ASSERT_NE(c, nullptr);
  EXPECT_EQ(std::stoll(std::string(c->getValue())), 10000000LL)
      << "§5.8: 1ms / 100ps = 10000000 — Surelog bug: unit ignored, stores 1";
}

// a = 1s — 1s / 100e-12s = 10000000000
TEST_F(TimeLiterals, Assignment5_1s_ScaledPerSpec) {
  const auto *c = getAssignment(m_design, 5)->getRhs<hldb::Constant>();
  ASSERT_NE(c, nullptr);
  EXPECT_EQ(std::stoll(std::string(c->getValue())), 10000000000LL)
      << "§5.8: 1s / 100ps = 1e10 — Surelog bug: unit ignored, stores 1";
}

// ---------------------------------------------------------------------------
// §5.8 real time literal: fixed_point_number time_unit.
// Assignment 6: a = 2.1ms — constType = vpiRealConst (2), size = 64.
// ---------------------------------------------------------------------------
TEST_F(TimeLiterals, RealTimeLiteral_ConstTypeIsReal) {
  const auto *c = getAssignment(m_design, 6)->getRhs<hldb::Constant>();
  ASSERT_NE(c, nullptr);
  EXPECT_EQ(c->getConstType(), 2)
      << "§5.8: real time literal (2.1ms) must be stored as real const (2)";
}

TEST_F(TimeLiterals, RealTimeLiteral_SizeIs64) {
  const auto *c = getAssignment(m_design, 6)->getRhs<hldb::Constant>();
  ASSERT_NE(c, nullptr);
  EXPECT_EQ(c->getSize(), 64)
      << "§5.8: real time literal must be 64-bit (IEEE 754 double-precision)";
}

// a = 2.1ms — 2.1e-3s / 100e-12s = 21000000
// SURELOG BUG: unit ignored, stores 2.1 instead of 21000000.
TEST_F(TimeLiterals, Assignment6_2p1ms_ScaledPerSpec) {
  const auto *c = getAssignment(m_design, 6)->getRhs<hldb::Constant>();
  ASSERT_NE(c, nullptr);
  EXPECT_NEAR(std::stod(std::string(c->getValue())), 21000000.0, 0.5)
      << "§5.8: 2.1ms / 100ps = 21000000 — Surelog bug: unit ignored, "
         "stores 2.1";
}

}  // namespace SURELOG

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
