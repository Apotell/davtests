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

// Spec-based validation of IEEE 1800-2017 ss.6.20.4 localparam with logic
// type and packed range, and ss.11.4.10 logical left shift expression.
// SV: tests/Google/chapter-6/6.20.4--localparam_logic.sv
//
//   module top();
//       localparam [10:0] p = 1 << 5;
//       localparam logic [10:0] q = 1 << 5;
//   endmodule
//
// -- ss.6.20.4 + ss.6.3.4 + ss.6.9.1 + ss.11.4.10 rules under test ----------
//
// Local parameter (ss.6.20.4):
//   * Both p and q are localparams -- not overridable at instantiation.
//
// Logic type and packed range (ss.6.3.4 + ss.6.9.1):
//   * 'logic' is a 4-state single-bit type (ss.6.3.4).
//   * A packed dimension '[10:0]' creates an 11-bit packed vector (ss.6.9.1).
//   * 'localparam [10:0] p' uses an implicit logic type (no keyword).
//     A packed range without an explicit type keyword implies 'logic'.
//   * 'localparam logic [10:0] q' uses an explicit 'logic' keyword.
//   * Both produce LogicTypespec with a single Range [10:0]:
//     left bound = 10, right bound = 0.
//
// Constant expression with shift (ss.11.4.10 + ss.11.2.1):
//   * '1 << 5' is a constant_expression using logical left shift (ss.11.4.10).
//   * The RHS is an Operation node with opType vpiLShiftOp (22), not a
//     plain Constant, because it is a compound expression.
//
// -- UHDM tree ----------------------------------------------------------------
//
//   Module name:top
//   +-- getParameters() (AnyCollection, 2 items)
//   |   +-- [0] Parameter name:"p"  localParam: true
//   |           typespec: RefTypespec -> LogicTypespec { range [10:0] }
//   |   +-- [1] Parameter name:"q"  localParam: true
//   |           typespec: RefTypespec -> LogicTypespec { range [10:0] }
//   +-- getParamAssigns() (ParamAssignCollection, 2 items)
//       +-- [0] ParamAssign  lhs:"p"  rhs: Operation { opType: vpiLShiftOp }
//       +-- [1] ParamAssign  lhs:"q"  rhs: Operation { opType: vpiLShiftOp }
//
// -- VPI constants ------------------------------------------------------------
//   vpiLShiftOp = 22  (logical left shift <<, vpi_user.h)

#include <hlc/Common/Session.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/logic_typespec.h>
#include <hldb/module.h>
#include <hldb/operation.h>
#include <hldb/param_assign.h>
#include <hldb/parameter.h>
#include <hldb/range.h>
#include <hldb/ref_typespec.h>
#include <hldb/vpi_user.h>

#include <string>

namespace hlc {

class LocalparamLogicTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "6.20.4--localparam_logic.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static const hldb::Module *getTop(const hldb::Design *d) {
  return hldb::findByName<hldb::Module>("top", d->getAllModules());
}

static const hldb::Parameter *getParam(const hldb::Design *d, std::string_view name) {
  const hldb::Module *m = getTop(d);
  if (!m || !m->getParameters()) return nullptr;
  return hldb::findByName<hldb::Parameter>(name, m->getParameters());
}

static const hldb::ParamAssign *getParamAssign(const hldb::Design *d, std::string_view name) {
  const hldb::Module *m = getTop(d);
  if (!m) return nullptr;
  return hldb::findByName<hldb::ParamAssign>(name, m->getParamAssigns());
}

// Returns the LogicTypespec for the named parameter, or nullptr.
static const hldb::LogicTypespec *getLogicTypespec(const hldb::Design *d, std::string_view name) {
  const hldb::Parameter *p = getParam(d, name);
  if (!p || !p->getTypespec()) return nullptr;
  return p->getTypespec()->getActual<hldb::LogicTypespec>();
}

// ===========================================================================
// Module
// ===========================================================================

TEST_F(LocalparamLogicTest, ModuleExists) { ASSERT_NE(getTop(m_design), nullptr) << "module 'top' not found"; }

// ===========================================================================
// Parameter collection
// ===========================================================================

TEST_F(LocalparamLogicTest, ParameterCollectionExists) {
  const hldb::Module *m = getTop(m_design);
  ASSERT_NE(m, nullptr);
  EXPECT_NE(m->getParameters(), nullptr) << "module 'top' must have a parameter collection";
}

// ss.6.20.4: two localparams p and q are declared.
TEST_F(LocalparamLogicTest, ParameterCount) {
  const hldb::Module *m = getTop(m_design);
  ASSERT_NE(m, nullptr);
  ASSERT_NE(m->getParameters(), nullptr);
  EXPECT_EQ(m->getParameters()->size(), 2u) << "module 'top' declares exactly two localparams: p and q";
}

// ===========================================================================
// localparam [10:0] p = 1 << 5  (implicit logic)
// ===========================================================================

TEST_F(LocalparamLogicTest, P_Exists) { EXPECT_NE(getParam(m_design, "p"), nullptr) << "'p' not found in parameters"; }

// ss.6.20.4: p is a localparam and must NOT be overridable.
TEST_F(LocalparamLogicTest, P_IsLocalParam) {
  const hldb::Parameter *p = getParam(m_design, "p");
  ASSERT_NE(p, nullptr);
  EXPECT_TRUE(p->getLocalParam()) << "ss.6.20.4: 'localparam [10:0] p' must be marked as a localparam";
}

// ss.6.3.4 + ss.6.9.1: a packed range without an explicit type keyword
// implies 'logic'; the typespec must resolve to LogicTypespec.
TEST_F(LocalparamLogicTest, P_Typespec_IsLogicTypespec) {
  const hldb::Parameter *p = getParam(m_design, "p");
  ASSERT_NE(p, nullptr);
  const hldb::RefTypespec *rt = p->getTypespec();
  ASSERT_NE(rt, nullptr) << "'localparam [10:0] p' must have a typespec";
  EXPECT_NE(rt->getActual<hldb::LogicTypespec>(), nullptr)
      << "ss.6.3.4: implicit type with packed range must resolve to "
         "LogicTypespec";
}

// ss.6.9.1: the packed dimension '[10:0]' must produce a range collection
// with exactly one range entry.
TEST_F(LocalparamLogicTest, P_Typespec_HasRange) {
  const hldb::LogicTypespec *lt = getLogicTypespec(m_design, "p");
  ASSERT_NE(lt, nullptr);
  const hldb::RangeCollection *ranges = lt->getRanges();
  ASSERT_NE(ranges, nullptr) << "'[10:0]' must produce a range collection";
  EXPECT_EQ(ranges->size(), 1u) << "ss.6.9.1: '[10:0]' is a single packed dimension";
}

// ss.6.9.1: the left bound of '[10:0]' is 10.
TEST_F(LocalparamLogicTest, P_Typespec_LeftBound_Is10) {
  const hldb::LogicTypespec *lt = getLogicTypespec(m_design, "p");
  ASSERT_NE(lt, nullptr);
  const hldb::RangeCollection *ranges = lt->getRanges();
  ASSERT_NE(ranges, nullptr);
  ASSERT_FALSE(ranges->empty());
  const hldb::Constant *left = (*ranges)[0]->getLeftExpr<hldb::Constant>();
  ASSERT_NE(left, nullptr) << "left bound of '[10:0]' must be a Constant";
  EXPECT_EQ(std::string(left->getDecompile()), "10") << "ss.6.9.1: left bound of '[10:0]' must be 10";
}

// ss.6.9.1: the right bound of '[10:0]' is 0.
TEST_F(LocalparamLogicTest, P_Typespec_RightBound_Is0) {
  const hldb::LogicTypespec *lt = getLogicTypespec(m_design, "p");
  ASSERT_NE(lt, nullptr);
  const hldb::RangeCollection *ranges = lt->getRanges();
  ASSERT_NE(ranges, nullptr);
  ASSERT_FALSE(ranges->empty());
  const hldb::Constant *right = (*ranges)[0]->getRightExpr<hldb::Constant>();
  ASSERT_NE(right, nullptr) << "right bound of '[10:0]' must be a Constant";
  EXPECT_EQ(std::string(right->getDecompile()), "0") << "ss.6.9.1: right bound of '[10:0]' must be 0";
}

// ss.11.2.1: '1 << 5' is a constant_expression; in UHDM the RHS is an
// Operation node, not a plain Constant, because it is a compound expression.
TEST_F(LocalparamLogicTest, P_RhsIsOperation) {
  const hldb::ParamAssign *pa = getParamAssign(m_design, "p");
  ASSERT_NE(pa, nullptr) << "ParamAssign for 'p' not found";
  EXPECT_NE(pa->getRhs<hldb::Operation>(), nullptr) << "'1 << 5' must be represented as an Operation node";
}

// ss.11.4.10: '<<' is a logical left shift; the operation type must be
// vpiLShiftOp (22).
TEST_F(LocalparamLogicTest, P_Rhs_IsLeftShift) {
  const hldb::ParamAssign *pa = getParamAssign(m_design, "p");
  ASSERT_NE(pa, nullptr);
  const hldb::Operation *op = pa->getRhs<hldb::Operation>();
  ASSERT_NE(op, nullptr);
  EXPECT_EQ(op->getOpType(), vpiLShiftOp) << "ss.11.4.10: '<<' must have opType vpiLShiftOp (22)";
}

// ===========================================================================
// localparam logic [10:0] q = 1 << 5  (explicit logic)
// ===========================================================================

TEST_F(LocalparamLogicTest, Q_Exists) { EXPECT_NE(getParam(m_design, "q"), nullptr) << "'q' not found in parameters"; }

// ss.6.20.4: q is a localparam and must NOT be overridable.
TEST_F(LocalparamLogicTest, Q_IsLocalParam) {
  const hldb::Parameter *q = getParam(m_design, "q");
  ASSERT_NE(q, nullptr);
  EXPECT_TRUE(q->getLocalParam()) << "ss.6.20.4: 'localparam logic [10:0] q' must be marked as a "
                                     "localparam";
}

// ss.6.3.4: the explicit 'logic' keyword must produce a LogicTypespec.
TEST_F(LocalparamLogicTest, Q_Typespec_IsLogicTypespec) {
  const hldb::Parameter *q = getParam(m_design, "q");
  ASSERT_NE(q, nullptr);
  const hldb::RefTypespec *rt = q->getTypespec();
  ASSERT_NE(rt, nullptr) << "'localparam logic [10:0] q' must have a typespec";
  EXPECT_NE(rt->getActual<hldb::LogicTypespec>(), nullptr) << "ss.6.3.4: 'logic' keyword must resolve to LogicTypespec";
}

// ss.6.9.1: the '[10:0]' range on q must produce exactly one range entry.
TEST_F(LocalparamLogicTest, Q_Typespec_HasRange) {
  const hldb::LogicTypespec *lt = getLogicTypespec(m_design, "q");
  ASSERT_NE(lt, nullptr);
  const hldb::RangeCollection *ranges = lt->getRanges();
  ASSERT_NE(ranges, nullptr) << "'[10:0]' must produce a range collection";
  EXPECT_EQ(ranges->size(), 1u) << "ss.6.9.1: '[10:0]' is a single packed dimension";
}

// ss.11.2.1 + ss.11.4.10: '1 << 5' on q must be an Operation with
// vpiLShiftOp.
TEST_F(LocalparamLogicTest, Q_RhsIsOperation) {
  const hldb::ParamAssign *pa = getParamAssign(m_design, "q");
  ASSERT_NE(pa, nullptr) << "ParamAssign for 'q' not found";
  EXPECT_NE(pa->getRhs<hldb::Operation>(), nullptr) << "'1 << 5' must be represented as an Operation node";
}

TEST_F(LocalparamLogicTest, Q_Rhs_IsLeftShift) {
  const hldb::ParamAssign *pa = getParamAssign(m_design, "q");
  ASSERT_NE(pa, nullptr);
  const hldb::Operation *op = pa->getRhs<hldb::Operation>();
  ASSERT_NE(op, nullptr);
  EXPECT_EQ(op->getOpType(), vpiLShiftOp) << "ss.11.4.10: '<<' must have opType vpiLShiftOp (22)";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
