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

// Spec-based validation of IEEE 1800-2017 ss.6.20.3 type parameter.
// SV: tests/Google/chapter-6/6.20.3--parameter_type.sv
//
//   module top #(type T = real);
//   T num = 0.0;
//   endmodule
//
// -- ss.6.20.3 rules under test ----
//
// A type parameter (ss.6.20.3):
//   * Declared with the 'type' keyword in the parameter port list.
//   * 'type T = real' declares a type parameter T whose default type is 'real'.
//   * The 'type' keyword distinguishes it from a value parameter (ss.6.20.2).
//   * A type parameter is overridable at instantiation; it is NOT a localparam.
//   * UHDM represents it as a TypeParameter node (distinct from Parameter).
//   * The default type expression is a RefTypespec resolving to RealTypespec,
//     matching the declared default 'real' (ss.6.12.1).
//
// Variable using a type parameter (ss.6.20.3 + ss.6.8):
//   * A type parameter may be used as a data type inside the module body.
//   * 'T num = 0.0' declares a variable whose type is the type parameter T.
//   * In UHDM the variable's typespec is a RefTypespec pointing to the
//     TypeParameter node named "T".
//   * The initial value '0.0' is a real constant (ss.5.7.2), stored as a
//     Constant node with constType vpiRealConst (2) in the variable's expr.
//
// -- UHDM tree ----
//
//   Module name:top
//   +-- getParameters() (AnyCollection, 1 item)
//   |   +-- [0] TypeParameter name:"T"  localParam: false
//   +-- getParamAssigns() (ParamAssignCollection, 1 item)
//   |   +-- [0] ParamAssign
//   |           lhs: RefTypespec name:"T"  actual: TypeParameter name:"T"
//   |           rhs: RefTypespec -> RealTypespec
//   +-- getVariables() (VariableCollection, 1 item)
//       +-- [0] Variable name:"num"
//               typespec: RefTypespec -> TypeParameter name:"T"
//               value:    Constant { constType: vpiRealConst (2) }
//
// NOTE: TypeParameter nodes live in the same getParameters() AnyCollection as
// ordinary Parameter nodes. They are retrieved by casting Any* entries with
// any_cast<TypeParameter>. Like value parameters, type parameters store their
// default via a ParamAssign node (Scope::getParamAssigns()). The LHS of that
// ParamAssign is a RefTypespec named "T" pointing to the TypeParameter; the RHS
// is a RefTypespec whose actual typespec is the default type (RealTypespec).
// TypeParameter::getExpr() is NOT populated by Surelog -- use the ParamAssign
// RHS to access the default type.
//
// -- VPI constants ----
//   vpiRealConst     = 2    (real constant, vpi_user.h)
//   vpiTypeParameter = 609  (sv_vpi_user.h)

#include <hlc/Common/Session.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/module.h>
#include <hldb/net.h>
#include <hldb/param_assign.h>
#include <hldb/real_typespec.h>
#include <hldb/ref_typespec.h>
#include <hldb/type_parameter.h>
#include <hldb/variable.h>

#include <string>

namespace hlc {

class ParameterTypeTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "6.20.3--parameter_type.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

// ----
// Helpers
// ----

static const hldb::Module *getTop(const hldb::Design *d) {
  return hldb::findByName<hldb::Module>("top", d->getAllModules());
}

// Type parameters share the AnyCollection returned by getParameters().
// findByName<TypeParameter> matches by getName() and casts via any_cast.
static const hldb::TypeParameter *getTypeParam(const hldb::Design *d, std::string_view name) {
  const hldb::Module *m = getTop(d);
  if (!m || !m->getParameters()) return nullptr;
  return hldb::findByName<hldb::TypeParameter>(name, m->getParameters());
}

// The default type for a type parameter is stored in ParamAssign::getRhs(),
// matched by getLhs()->getName() == name (the LHS RefTypespec carries the name).
static const hldb::ParamAssign *getTypeParamAssign(const hldb::Design *d, std::string_view name) {
  const hldb::Module *m = getTop(d);
  if (!m) return nullptr;
  return hldb::findByName<hldb::ParamAssign>(name, m->getParamAssigns());
}

// Returns the named variable from the module's variable collection.
static const hldb::Variable *getVar(const hldb::Design *d, std::string_view name) {
  const hldb::Module *m = getTop(d);
  if (!m || !m->getVariables()) return nullptr;
  return hldb::findByName<hldb::Variable>(name, m->getVariables());
}

// ===========================================================================
// Module
// ===========================================================================

TEST_F(ParameterTypeTest, ModuleExists) { ASSERT_NE(getTop(m_design), nullptr) << "module 'top' not found"; }

// ===========================================================================
// Parameter collection
// ===========================================================================

TEST_F(ParameterTypeTest, ParameterCollectionExists) {
  const hldb::Module *m = getTop(m_design);
  ASSERT_NE(m, nullptr);
  EXPECT_NE(m->getParameters(), nullptr) << "module 'top' must have a parameter collection";
}

// ss.6.20.3: '#(type T = real)' declares exactly one parameter entry.
TEST_F(ParameterTypeTest, ParameterCount) {
  const hldb::Module *m = getTop(m_design);
  ASSERT_NE(m, nullptr);
  ASSERT_NE(m->getParameters(), nullptr);
  EXPECT_EQ(m->getParameters()->size(), 1u) << "module 'top' declares exactly one type parameter: T";
}

// ===========================================================================
// Type parameter T = real
// ===========================================================================

// ss.6.20.3: the 'type' keyword produces a TypeParameter node, not a Parameter.
TEST_F(ParameterTypeTest, T_ExistsAsTypeParameter) {
  EXPECT_NE(getTypeParam(m_design, "T"), nullptr)
      << "ss.6.20.3: 'type T' must produce a TypeParameter node named \"T\"";
}

// ss.6.20.3: a type parameter declared in the port list is overridable and
// must NOT be a localparam.
TEST_F(ParameterTypeTest, T_IsNotLocalParam) {
  const hldb::TypeParameter *tp = getTypeParam(m_design, "T");
  ASSERT_NE(tp, nullptr);
  EXPECT_FALSE(tp->getLocalParam()) << "ss.6.20.3: '#(type T = real)' must not be marked as a localparam";
}

// ss.6.20.3: a type parameter must carry a default type expression.
// '= real' provides the default, stored as ParamAssign::getRhs<RefTypespec>().
TEST_F(ParameterTypeTest, T_DefaultTypeExprExists) {
  const hldb::ParamAssign *pa = getTypeParamAssign(m_design, "T");
  ASSERT_NE(pa, nullptr) << "ParamAssign for type parameter 'T' not found";
  EXPECT_NE(pa->getRhs<hldb::RefTypespec>(), nullptr)
      << "ss.6.20.3: 'type T = real' must have a non-null default type expr";
}

// ss.6.20.3 + ss.6.12.1: the default type 'real' resolves to RealTypespec
// via the RefTypespec stored as ParamAssign::getRhs().
TEST_F(ParameterTypeTest, T_DefaultType_IsReal) {
  const hldb::ParamAssign *pa = getTypeParamAssign(m_design, "T");
  ASSERT_NE(pa, nullptr) << "ParamAssign for type parameter 'T' not found";
  const hldb::RefTypespec *rt = pa->getRhs<hldb::RefTypespec>();
  ASSERT_NE(rt, nullptr) << "default type expr must be non-null";
  EXPECT_NE(rt->getActual<hldb::RealTypespec>(), nullptr)
      << "ss.6.12.1: default type 'real' must resolve to RealTypespec";
}

// ===========================================================================
// Variable num : T = 0.0  (ss.6.8 + ss.6.20.3)
// ===========================================================================

// ss.6.8 + ss.6.20.3: a type parameter can be used as a data type inside the
// module body; 'T num = 0.0' must appear in the module's variable collection.
TEST_F(ParameterTypeTest, Num_VariableExists) {
  EXPECT_NE(getVar(m_design, "num"), nullptr)
      << "ss.6.8: variable 'num' must appear in the module's variable collection";
}

// ss.6.20.3: the type of 'num' is the type parameter T, so its typespec must
// be a non-null RefTypespec.
TEST_F(ParameterTypeTest, Num_TypespecExists) {
  const hldb::Variable *v = getVar(m_design, "num");
  ASSERT_NE(v, nullptr);
  EXPECT_NE(v->getTypespec(), nullptr) << "ss.6.20.3: variable 'num : T' must have a non-null typespec";
}

// ss.6.20.3: the RefTypespec for 'num' must resolve to the TypeParameter node
// named "T", not to a concrete type like RealTypespec (the type is parametric).
TEST_F(ParameterTypeTest, Num_Typespec_RefersToTypeParameter) {
  const hldb::Variable *v = getVar(m_design, "num");
  ASSERT_NE(v, nullptr);
  const hldb::RefTypespec *rt = v->getTypespec();
  ASSERT_NE(rt, nullptr);
  EXPECT_NE(rt->getActual<hldb::TypeParameter>(), nullptr)
      << "ss.6.20.3: 'T num' typespec must resolve to a TypeParameter node";
}

// ss.6.20.3: the TypeParameter that 'num' refers to must be named "T",
// matching the type parameter declared in the port list.
TEST_F(ParameterTypeTest, Num_Typespec_RefersToT) {
  const hldb::Variable *v = getVar(m_design, "num");
  ASSERT_NE(v, nullptr);
  const hldb::RefTypespec *rt = v->getTypespec();
  ASSERT_NE(rt, nullptr);
  const hldb::TypeParameter *tp = rt->getActual<hldb::TypeParameter>();
  ASSERT_NE(tp, nullptr);
  EXPECT_EQ(tp->getName(), "T") << "ss.6.20.3: 'T num' must refer to the type parameter named \"T\"";
}

// ss.6.8: 'T num = 0.0' provides an initial value; the variable's initial
// value expression must be non-null.
// NOTE: Surelog stores the initial value in Variable::getValue(), not getExpr().
TEST_F(ParameterTypeTest, Num_InitValueExists) {
  const hldb::Variable *v = getVar(m_design, "num");
  ASSERT_NE(v, nullptr);
  EXPECT_NE(v->getValue(), nullptr) << "ss.6.8: 'T num = 0.0' must have a non-null initial value expression";
}

// ss.5.7.2: '0.0' is a real number literal; the initial value must be a
// Constant node with constType vpiRealConst (2).
TEST_F(ParameterTypeTest, Num_InitValue_IsRealConst) {
  const hldb::Variable *v = getVar(m_design, "num");
  ASSERT_NE(v, nullptr);
  const hldb::Constant *c = v->getValue<hldb::Constant>();
  ASSERT_NE(c, nullptr) << "initial value '0.0' must be a Constant node";
  EXPECT_EQ(c->getConstType(), vpiRealConst) << "ss.5.7.2: '0.0' must have constType vpiRealConst (2)";
}

// IEEE 1800-2023 Sec 6.7/6.8: 'T num' has no net-type keyword, so it is a
// Variable, never a Net -- confirm the name is absent from the Net collection.
TEST_F(ParameterTypeTest, Num_IsNotInNets) {
  const hldb::Module *const m = getTop(m_design);
  ASSERT_NE(m, nullptr);
  EXPECT_TRUE(m->getNets() == nullptr || hldb::findByName<hldb::Net>("num", m->getNets()) == nullptr)
      << "'num' has no net-type keyword; it must not appear in the module's Net collection";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
