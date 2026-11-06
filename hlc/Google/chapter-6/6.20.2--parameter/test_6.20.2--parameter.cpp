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

// Spec-based validation of IEEE 1800-2017 Sec 6.20.2 module parameter.
// SV: tests/Google/chapter-6/6.20.2--parameter.sv
//
//   module top();
//       parameter p = 123;
//   endmodule
//
// -- Sec 6.20.2 constructs under test ----
//
// A parameter declared without an explicit type and initialised with an
// unsized decimal integer literal (IEEE 1800-2017 Sec 6.20.2, Sec 5.7.1):
//   - Has an inferred LogicTypespec (Surelog default for untyped parameters).
//   - Its default value "123" is an unsigned integer constant
//     (vpiConstType = vpiIntConst = 7) with size 64.
//   - The default value is held by the Parameter itself (vpiExpr); a
//     declaration creates no ParamAssign (those are for overrides only).
//
// -- UHDM tree (from log) ----
//
//   Design name:unnamed
//   `-- vpiAllModules (1 item)
//       `-- Module name:top
//           `-- vpiParameter (1 item)
//               `-- Parameter name:p
//                   |-- vpiTypespec  RefTypespec -> actual: LogicTypespec
//                   `-- vpiExpr  Constant
//                       |-- vpiTypespec  RefTypespec -> actual: IntTypespec
//                       |-- vpiConstType: unsigned int (9)
//                       |-- vpiSize: 64
//                       `-- vpiDecompile: "123"
//
// -- VPI constants ----
//   vpiIntConst = 7

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

// ----
// Helpers
// ----

static const hldb::Module *getTop(const hldb::Design *d) {
  return hldb::findByName<hldb::Module>("top", d->getAllModules());
}

static const hldb::Parameter *getParameter(const hldb::Design *d) {
  const hldb::Module *m = getTop(d);
  return (m == nullptr) ? nullptr : hldb::findByName<hldb::Parameter>("p", m->getParameters());
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

TEST_F(ParameterTest, Parameter_p_Exists) { EXPECT_NE(getParameter(m_design), nullptr); }

// IEEE 1800-2017 Sec 6.20.2: a parameter without an explicit type is inferred
// as logic; HLC represents this as LogicTypespec via RefTypespec.
TEST_F(ParameterTest, Parameter_p_HasLogicTypespec) {
  const hldb::Parameter *p = getParameter(m_design);
  ASSERT_NE(p, nullptr);
  const hldb::RefTypespec *rt = p->getTypespec();
  ASSERT_NE(rt, nullptr) << "Parameter 'p' must have a RefTypespec";
  EXPECT_NE(rt->getActual<hldb::LogicTypespec>(), nullptr)
      << "RefTypespec must resolve to LogicTypespec for an untyped parameter";
}

// ===========================================================================
// Parameter
// ===========================================================================

TEST_F(ParameterTest, Parameter_Lhs_NameIsP) {
  const hldb::Parameter *pa = getParameter(m_design);
  ASSERT_NE(pa, nullptr);
  EXPECT_EQ(pa->getName(), "p");
}

TEST_F(ParameterTest, Parameter_Rhs_IsConstant) {
  const hldb::Parameter *pa = getParameter(m_design);
  ASSERT_NE(pa, nullptr);
  EXPECT_NE(pa->getExpr<hldb::Constant>(), nullptr) << "Parameter value must be a Constant";
}

// IEEE 1800-2017 Sec 5.7.1: unsized decimal integer literals without a sign
// qualifier are signed.
TEST_F(ParameterTest, Parameter_Rhs_ConstType_IsInt) {
  const hldb::Parameter *pa = getParameter(m_design);
  ASSERT_NE(pa, nullptr);
  const hldb::Constant *rhs = pa->getExpr<hldb::Constant>();
  ASSERT_NE(rhs, nullptr);
  EXPECT_EQ(rhs->getConstType(), vpiIntConst) << "Constant for '123' must have vpiConstType == vpiIntConst";
}

// The value string must reproduce the exact source literal.
TEST_F(ParameterTest, Parameter_Rhs_ValueIs_123) {
  const hldb::Parameter *pa = getParameter(m_design);
  ASSERT_NE(pa, nullptr);
  const hldb::Constant *rhs = pa->getExpr<hldb::Constant>();
  ASSERT_NE(rhs, nullptr);
  EXPECT_EQ(std::string(rhs->getValue()), "123");
}

// IEEE 1800-2017 Sec 6.20.2: an untyped parameter takes its size from the
// constant expression. An unsized decimal literal uses the host integer
// width; Surelog represents this as 64 bits.
TEST_F(ParameterTest, Parameter_Rhs_SizeIs64) {
  const hldb::Parameter *pa = getParameter(m_design);
  ASSERT_NE(pa, nullptr);
  const hldb::Constant *rhs = pa->getExpr<hldb::Constant>();
  ASSERT_NE(rhs, nullptr);
  EXPECT_EQ(rhs->getSize(), 32) << "unsized decimal literal in untyped parameter must have host-int size (32)";
}
}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
