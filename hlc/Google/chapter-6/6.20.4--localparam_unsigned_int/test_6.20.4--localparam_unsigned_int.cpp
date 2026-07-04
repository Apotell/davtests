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

// Spec-based validation of IEEE 1800-2017 ss.6.20.4 localparam with an
// explicit 'int unsigned' type.
// SV: tests/Google/chapter-6/6.20.4--localparam_unsigned_int.sv
//
//   module top();
//       localparam int unsigned q = 123;
//   endmodule
//
// -- ss.6.20.4 + ss.6.11.2 rules under test -----------------------------------
//
// Local parameter (ss.6.20.4):
//   * 'localparam' declares a constant that cannot be changed at instantiation.
//   * UHDM represents it as a Parameter node with getLocalParam() == true.
//   * The default value is stored in ParamAssign::getRhs(), not getExpr().
//
// Explicit 'int unsigned' type (ss.6.11.2):
//   * 'int' is a 2-state signed 32-bit integer data type by default (ss.6.11.2).
//   * The 'unsigned' qualifier overrides the default signedness to make it
//     unsigned. This is the defining distinction from 'localparam int'.
//   * 'int unsigned' attaches an IntTypespec to the parameter; the typespec
//     reports getSigned() == false (unsigned).
//   * The RHS Constant carries constType vpiUIntConst (9), reflecting that
//     the value is interpreted as an unsigned integer.
//
// -- UHDM tree ----------------------------------------------------------------
//
//   Module name:work@top
//   +-- getParameters() (AnyCollection, 1 item)
//   |   +-- [0] Parameter name:"q"  localParam: true
//   |           typespec: RefTypespec -> IntTypespec  (no vpiSigned == unsigned)
//   +-- getParamAssigns() (ParamAssignCollection, 1 item)
//       +-- [0] ParamAssign
//               lhs: RefObj name:"q"  actual: Parameter name:"q"
//               rhs: Constant { vpiConstType: vpiUIntConst (9)
//                               vpiDecompile: "123" }
//
// -- VPI constants ------------------------------------------------------------
//   vpiUIntConst = 9  (unsigned int constant, vpi_user.h)

#include <hlc/Common/Session.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/int_typespec.h>
#include <hldb/module.h>
#include <hldb/param_assign.h>
#include <hldb/parameter.h>
#include <hldb/ref_typespec.h>
#include <hldb/vpi_user.h>

#include <string>

namespace hlc {

class LocalparamUnsignedIntTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "6.20.4--localparam_unsigned_int.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static const hldb::Module *getTop(const hldb::Design *d) {
  return hldb::findByName<hldb::Module>("work@top", d->getAllModules());
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

// ===========================================================================
// Module
// ===========================================================================

TEST_F(LocalparamUnsignedIntTest, ModuleExists) {
  ASSERT_NE(getTop(m_design), nullptr) << "module 'work@top' not found";
}

// ===========================================================================
// Parameter collection
// ===========================================================================

TEST_F(LocalparamUnsignedIntTest, ParameterCollectionExists) {
  const hldb::Module *m = getTop(m_design);
  ASSERT_NE(m, nullptr);
  EXPECT_NE(m->getParameters(), nullptr) << "module 'top' must have a parameter collection";
}

// ss.6.20.4: 'localparam int unsigned q = 123' declares exactly one
// local parameter.
TEST_F(LocalparamUnsignedIntTest, ParameterCount) {
  const hldb::Module *m = getTop(m_design);
  ASSERT_NE(m, nullptr);
  ASSERT_NE(m->getParameters(), nullptr);
  EXPECT_EQ(m->getParameters()->size(), 1u) << "module 'top' declares exactly one localparam: q";
}

// ===========================================================================
// localparam int unsigned q = 123
// ===========================================================================

// ss.6.20.4: 'localparam' must produce a Parameter node named "q".
TEST_F(LocalparamUnsignedIntTest, Q_Exists) {
  EXPECT_NE(getParam(m_design, "q"), nullptr) << "'q' not found in parameters";
}

// ss.6.20.4: a localparam is NOT overridable at instantiation;
// getLocalParam() must return true.
TEST_F(LocalparamUnsignedIntTest, Q_IsLocalParam) {
  const hldb::Parameter *q = getParam(m_design, "q");
  ASSERT_NE(q, nullptr);
  EXPECT_TRUE(q->getLocalParam()) << "ss.6.20.4: 'localparam int unsigned q' must be marked as a localparam";
}

// ss.6.11.2: the explicit 'int unsigned' type must be recorded as a non-null
// typespec.
TEST_F(LocalparamUnsignedIntTest, Q_TypespecExists) {
  const hldb::Parameter *q = getParam(m_design, "q");
  ASSERT_NE(q, nullptr);
  EXPECT_NE(q->getTypespec(), nullptr) << "ss.6.11.2: 'localparam int unsigned q' must have a non-null typespec";
}

// ss.6.11.2: the explicit 'int' base type must resolve to an IntTypespec.
TEST_F(LocalparamUnsignedIntTest, Q_Typespec_IsIntTypespec) {
  const hldb::Parameter *q = getParam(m_design, "q");
  ASSERT_NE(q, nullptr);
  const hldb::RefTypespec *rt = q->getTypespec();
  ASSERT_NE(rt, nullptr);
  EXPECT_NE(rt->getActual<hldb::IntTypespec>(), nullptr) << "ss.6.11.2: 'int unsigned' must resolve to IntTypespec";
}

// ss.6.11.2: 'int' is signed by default; the 'unsigned' qualifier overrides
// this. The IntTypespec must report getSigned() == false.
TEST_F(LocalparamUnsignedIntTest, Q_Typespec_IsNotSigned) {
  const hldb::Parameter *q = getParam(m_design, "q");
  ASSERT_NE(q, nullptr);
  const hldb::RefTypespec *rt = q->getTypespec();
  ASSERT_NE(rt, nullptr);
  const hldb::IntTypespec *ts = rt->getActual<hldb::IntTypespec>();
  ASSERT_NE(ts, nullptr);
  EXPECT_FALSE(ts->getSigned()) << "ss.6.11.2: 'int unsigned' must NOT be marked as signed";
}

// ss.6.20.4: the value '123' is a constant_expression stored in the
// ParamAssign RHS, not in Parameter::getExpr().
TEST_F(LocalparamUnsignedIntTest, Q_RhsIsConstant) {
  const hldb::ParamAssign *pa = getParamAssign(m_design, "q");
  ASSERT_NE(pa, nullptr) << "ParamAssign for 'q' not found";
  EXPECT_NE(pa->getRhs<hldb::Constant>(), nullptr) << "'localparam int unsigned q = 123': RHS must be a Constant";
}

// ss.6.11.2: the 'unsigned' qualifier causes the integer constant to be
// interpreted as an unsigned int; constType must be vpiUIntConst (9).
TEST_F(LocalparamUnsignedIntTest, Q_Rhs_ConstType_IsUnsignedInt) {
  const hldb::ParamAssign *pa = getParamAssign(m_design, "q");
  ASSERT_NE(pa, nullptr);
  const hldb::Constant *c = pa->getRhs<hldb::Constant>();
  ASSERT_NE(c, nullptr) << "'localparam int unsigned q = 123': RHS must be a Constant";
  EXPECT_EQ(c->getConstType(), vpiUIntConst) << "ss.6.11.2: 'int unsigned' value must have constType vpiUIntConst (9)";
}

// ss.6.20.4: the declared value is 123; the Constant must decompile to "123".
TEST_F(LocalparamUnsignedIntTest, Q_RhsDecompile) {
  const hldb::ParamAssign *pa = getParamAssign(m_design, "q");
  ASSERT_NE(pa, nullptr) << "ParamAssign for 'q' not found";
  const hldb::Constant *c = pa->getRhs<hldb::Constant>();
  ASSERT_NE(c, nullptr) << "'localparam int unsigned q = 123': RHS must be a Constant";
  EXPECT_EQ(std::string(c->getDecompile()), "123") << "'localparam int unsigned q = 123': decompile must be \"123\"";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
