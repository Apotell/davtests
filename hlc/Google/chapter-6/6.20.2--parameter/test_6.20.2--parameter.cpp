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

// Spec-based validation of IEEE 1800-2017 §6.20.2 module parameter.
// SV: tests/Google/chapter-6/6.20.2--parameter.sv
//
//   module top();
//       parameter p = 123;
//   endmodule
//
// ── §6.20.2 constructs under test ────────────────────────────────────────────
//
// A parameter declared without an explicit type and initialised with an
// unsized decimal integer literal (IEEE 1800-2017 §6.20.2, §5.7.1):
//   • Has an inferred LogicTypespec (Surelog default for untyped parameters).
//   • Its default value "123" is an unsigned integer constant
//     (vpiConstType = vpiUIntConst = 9) with size 64.
//   • The elaborator creates exactly one ParamAssign binding the parameter
//     name (LHS RefObj → actual Parameter) to its value (RHS Constant).
//
// ── UHDM tree (from log) ──────────────────────────────────────────────────
//
//   Design name:unnamed
//   └── vpiAllModules (1 item)
//       └── Module name:top
//           ├── vpiParameter (1 item)
//           │   └── Parameter name:p
//           │       └── vpiTypespec  RefTypespec → actual: LogicTypespec
//           └── vpiParamAssign (1 item)
//               └── ParamAssign
//                   ├── vpiLhs  RefObj name:p → actual: Parameter name:p
//                   └── vpiRhs  Constant
//                       ├── vpiTypespec  RefTypespec → actual: IntTypespec
//                       ├── vpiConstType: unsigned int (9)
//                       ├── vpiSize: 64
//                       └── vpiDecompile: "123"
//
// ── VPI constants ─────────────────────────────────────────────────────────
//   vpiUIntConst = 9

#include <hlc/Common/Session.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/logic_typespec.h>
#include <hldb/module.h>
#include <hldb/param_assign.h>
#include <hldb/parameter.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>

#include <string>

namespace hlc {

class ParameterTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "6.20.2--parameter.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static const hldb::Module *getTop(const hldb::Design *d) {
  return hldb::findByName<hldb::Module>("top", d->getAllModules());
}

static const hldb::Parameter *getParam(const hldb::Design *d) {
  const hldb::Module *m = getTop(d);
  if (!m || !m->getParameters()) return nullptr;
  return hldb::findByName<hldb::Parameter>("p", m->getParameters());
}

static const hldb::ParamAssign *getParamAssign(const hldb::Design *d) {
  const hldb::Module *m = getTop(d);
  if (!m || !m->getParamAssigns() || m->getParamAssigns()->empty()) return nullptr;
  return (*m->getParamAssigns())[0];
}

// ===========================================================================
// Module
// ===========================================================================

TEST_F(ParameterTest, ModuleExists) { EXPECT_NE(getTop(m_design), nullptr); }

// ===========================================================================
// Parameter collection
// ===========================================================================

TEST_F(ParameterTest, Parameter_Collection_HasOneEntry) {
  const hldb::Module *m = getTop(m_design);
  ASSERT_NE(m, nullptr);
  ASSERT_NE(m->getParameters(), nullptr);
  EXPECT_EQ(m->getParameters()->size(), 1u);
}

TEST_F(ParameterTest, Parameter_p_Exists) { EXPECT_NE(getParam(m_design), nullptr); }

// IEEE 1800-2017 §6.20.2: a parameter without an explicit type is inferred
// as logic; Surelog represents this as LogicTypespec via RefTypespec.
TEST_F(ParameterTest, Parameter_p_HasLogicTypespec) {
  const hldb::Parameter *p = getParam(m_design);
  ASSERT_NE(p, nullptr);
  const hldb::RefTypespec *rt = p->getTypespec();
  ASSERT_NE(rt, nullptr) << "Parameter 'p' must have a RefTypespec";
  EXPECT_NE(rt->getActual<hldb::LogicTypespec>(), nullptr)
      << "RefTypespec must resolve to LogicTypespec for an untyped parameter";
}

// ===========================================================================
// ParamAssign collection
// ===========================================================================

TEST_F(ParameterTest, ParamAssign_Collection_HasOneEntry) {
  const hldb::Module *m = getTop(m_design);
  ASSERT_NE(m, nullptr);
  ASSERT_NE(m->getParamAssigns(), nullptr);
  EXPECT_EQ(m->getParamAssigns()->size(), 1u);
}

// ===========================================================================
// ParamAssign LHS
// ===========================================================================

TEST_F(ParameterTest, ParamAssign_Lhs_IsRefObj) {
  const hldb::ParamAssign *pa = getParamAssign(m_design);
  ASSERT_NE(pa, nullptr);
  EXPECT_NE(pa->getLhs<hldb::RefObj>(), nullptr) << "ParamAssign LHS must be a RefObj";
}

TEST_F(ParameterTest, ParamAssign_Lhs_NameIsP) {
  const hldb::ParamAssign *pa = getParamAssign(m_design);
  ASSERT_NE(pa, nullptr);
  const hldb::RefObj *lhs = pa->getLhs<hldb::RefObj>();
  ASSERT_NE(lhs, nullptr);
  EXPECT_EQ(lhs->getName(), "p");
}

// The LHS RefObj's vpiActual must resolve to the Parameter declaration.
TEST_F(ParameterTest, ParamAssign_Lhs_ActualIsParameter) {
  const hldb::ParamAssign *pa = getParamAssign(m_design);
  ASSERT_NE(pa, nullptr);
  const hldb::RefObj *lhs = pa->getLhs<hldb::RefObj>();
  ASSERT_NE(lhs, nullptr);
  EXPECT_NE(lhs->getActual<hldb::Parameter>(), nullptr) << "LHS RefObj must resolve to a Parameter";
}

// ===========================================================================
// ParamAssign RHS
// ===========================================================================

TEST_F(ParameterTest, ParamAssign_Rhs_IsConstant) {
  const hldb::ParamAssign *pa = getParamAssign(m_design);
  ASSERT_NE(pa, nullptr);
  EXPECT_NE(pa->getRhs<hldb::Constant>(), nullptr) << "ParamAssign RHS must be a Constant";
}

// IEEE 1800-2017 §5.7.1: unsized decimal integer literals without a sign
// qualifier are unsigned; Surelog encodes this as vpiUIntConst (9).
TEST_F(ParameterTest, ParamAssign_Rhs_ConstType_IsUnsignedInt) {
  const hldb::ParamAssign *pa = getParamAssign(m_design);
  ASSERT_NE(pa, nullptr);
  const hldb::Constant *rhs = pa->getRhs<hldb::Constant>();
  ASSERT_NE(rhs, nullptr);
  EXPECT_EQ(rhs->getConstType(), vpiUIntConst) << "Constant for '123' must have vpiConstType == vpiUIntConst (9)";
}

// The value string must reproduce the exact source literal.
TEST_F(ParameterTest, ParamAssign_Rhs_ValueIs_123) {
  const hldb::ParamAssign *pa = getParamAssign(m_design);
  ASSERT_NE(pa, nullptr);
  const hldb::Constant *rhs = pa->getRhs<hldb::Constant>();
  ASSERT_NE(rhs, nullptr);
  EXPECT_EQ(std::string(rhs->getValue()), "123");
}

// IEEE 1800-2017 §6.20.2: an untyped parameter takes its size from the
// constant expression. An unsized decimal literal uses the host integer
// width; Surelog represents this as 64 bits.
TEST_F(ParameterTest, ParamAssign_Rhs_SizeIs64) {
  const hldb::ParamAssign *pa = getParamAssign(m_design);
  ASSERT_NE(pa, nullptr);
  const hldb::Constant *rhs = pa->getRhs<hldb::Constant>();
  ASSERT_NE(rhs, nullptr);
  EXPECT_EQ(rhs->getSize(), 64) << "unsized decimal literal in untyped parameter must have host-int size (64)";
}
}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
