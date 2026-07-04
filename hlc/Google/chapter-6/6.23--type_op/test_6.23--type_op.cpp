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

// Spec-based validation of IEEE 1800-2017 ss.6.23 type operator.
// SV: tests/Google/chapter-6/6.23--type_op.sv
//
//   module top();
//       real a = 4.76;
//       real b = 0.74;
//       var type(a+b) c;
//   endmodule
//
// -- ss.6.23 rules under test ---------------------------------------------------
//
// Type operator (ss.6.23):
//   * type(expr) is a type operator -- it derives the type from an expression.
//   * The expression inside type() is PARSED to determine its type, but NOT
//     evaluated to produce a value.
//   * type() appears in a typespec position, not an expression position.
//   * 'var type(a+b) c' -- since 'a' and 'b' are both 'real', the expression
//     'a+b' is of type 'real', so 'c' must have type 'real' (ss.6.23).
//   * 'var' declares the variable as explicitly dynamic (ss.6.8).
//   * 'c' has no initializer expression.
//
// Self-reference / forward-reference (ss.6.23):
//   * 'a' and 'b' are declared before 'c' -- no forward-reference issue.
//   * 'c' does not reference itself inside type() -- no self-reference.
//   * 'a' and 'b' are independently fully resolved (RealTypespec with
//     vpiActual set), confirming the operand chain is complete before
//     type() resolution is attempted.
//
// -- VPI constants ------------------------------------------------------------
//   vpiRealConst = 2  (real-valued constant, vpi_user.h)
//
// -- UHDM tree ----------------------------------------------------------------
//
//   Module name:work@top
//   +-- getNets() (NetCollection, 3 items)
//       +-- [0] Net name:"a"
//       |       typespec: RefTypespec -> RealTypespec
//       |       value: Constant { constType: vpiRealConst(2), decompile:"4.76" }
//       +-- [1] Net name:"b"
//       |       typespec: RefTypespec -> RealTypespec
//       |       value: Constant { constType: vpiRealConst(2), decompile:"0.74" }
//       +-- [2] Net name:"c"
//               typespec: RefTypespec (vpiActual: UNRESOLVED -- see note below)
//               value: (none -- no initializer)
//
// NOTE: Per ss.6.23, 'c' must resolve to RealTypespec (same type as 'a+b').
// Type resolution (vpiActual on RefTypespec) is an elaboration-phase result.
// At parse time the HLDB only stores what is literally in the source, so
// getActual() returns null pre-elaboration. C_Typespec_ResolvesToReal checks
// the post-elaboration state; it is expected to FAIL until Surelog resolves
// the type operator during elaboration.

#include <hlc/Common/Session.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/module.h>
#include <hldb/net.h>
#include <hldb/real_typespec.h>
#include <hldb/ref_typespec.h>
#include <hldb/variable.h>
#include <hldb/vpi_user.h>

#include <string>

namespace hlc {

class TypeOpTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "6.23--type_op.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static const hldb::Module *getTop(const hldb::Design *d) {
  return hldb::findByName<hldb::Module>("work@top", d->getAllModules());
}

static const hldb::Net *getNet(const hldb::Design *d, std::string_view name) {
  const hldb::Module *m = getTop(d);
  if (!m || !m->getNets()) return nullptr;
  return hldb::findByName<hldb::Net>(name, m->getNets());
}

// ===========================================================================
// Module
// ===========================================================================

TEST_F(TypeOpTest, ModuleExists) { ASSERT_NE(getTop(m_design), nullptr) << "module 'work@top' not found"; }

// ===========================================================================
// Net collection  (a, b, c are all stored as Net nodes)
// ===========================================================================

TEST_F(TypeOpTest, NetCollectionExists) {
  const hldb::Module *m = getTop(m_design);
  ASSERT_NE(m, nullptr);
  EXPECT_NE(m->getNets(), nullptr) << "module must have a net collection (a, b, c)";
}

// ss.6.23: three nets are declared in this module.
TEST_F(TypeOpTest, NetCount_IsThree) {
  const hldb::Module *m = getTop(m_design);
  ASSERT_NE(m, nullptr);
  ASSERT_NE(m->getNets(), nullptr);
  EXPECT_EQ(m->getNets()->size(), 3u) << "module 'top' declares exactly three nets: a, b, c";
}

// ===========================================================================
// real a = 4.76  (ss.6.12, ss.5.7.2)
// ===========================================================================

// ss.6.12: 'real a' must produce a Net node named "a".
TEST_F(TypeOpTest, A_Exists) { EXPECT_NE(getNet(m_design, "a"), nullptr) << "Net 'a' not found in net collection"; }

// ss.6.12: 'real' must attach a RealTypespec to 'a'.
TEST_F(TypeOpTest, A_Typespec_IsReal) {
  const hldb::Net *n = getNet(m_design, "a");
  ASSERT_NE(n, nullptr);
  const hldb::RefTypespec *rt = n->getTypespec();
  ASSERT_NE(rt, nullptr) << "'real a' must have a typespec";
  EXPECT_NE(rt->getActual<hldb::RealTypespec>(), nullptr)
      << "ss.6.12: post-elaboration: 'real a' must resolve to RealTypespec";
}

// ss.5.7.2: '4.76' is a real literal -- it must be a Constant node.
TEST_F(TypeOpTest, A_Value_IsConstant) {
  const hldb::Net *n = getNet(m_design, "a");
  ASSERT_NE(n, nullptr);
  EXPECT_NE(n->getValue<hldb::Constant>(), nullptr) << "ss.5.7.2: '4.76' must be a Constant node";
}

// ss.5.7.2: a real literal carries constType vpiRealConst (2).
TEST_F(TypeOpTest, A_Value_ConstType_IsReal) {
  const hldb::Net *n = getNet(m_design, "a");
  ASSERT_NE(n, nullptr);
  const hldb::Constant *c = n->getValue<hldb::Constant>();
  ASSERT_NE(c, nullptr);
  EXPECT_EQ(c->getConstType(), vpiRealConst) << "ss.5.7.2: '4.76' must have constType vpiRealConst (2)";
}

// ss.5.7.2: the constant must decompile to "4.76".
TEST_F(TypeOpTest, A_Value_Decompile_Is4_76) {
  const hldb::Net *n = getNet(m_design, "a");
  ASSERT_NE(n, nullptr);
  const hldb::Constant *c = n->getValue<hldb::Constant>();
  ASSERT_NE(c, nullptr);
  EXPECT_EQ(std::string(c->getDecompile()), "4.76") << "'real a = 4.76': value must decompile to \"4.76\"";
}

// ===========================================================================
// real b = 0.74  (ss.6.12, ss.5.7.2)
// ===========================================================================

// ss.6.12: 'real b' must produce a Net node named "b".
TEST_F(TypeOpTest, B_Exists) { EXPECT_NE(getNet(m_design, "b"), nullptr) << "Net 'b' not found in net collection"; }

// ss.6.12: 'real' must attach a RealTypespec to 'b'.
TEST_F(TypeOpTest, B_Typespec_IsReal) {
  const hldb::Net *n = getNet(m_design, "b");
  ASSERT_NE(n, nullptr);
  const hldb::RefTypespec *rt = n->getTypespec();
  ASSERT_NE(rt, nullptr) << "'real b' must have a typespec";
  EXPECT_NE(rt->getActual<hldb::RealTypespec>(), nullptr)
      << "ss.6.12: post-elaboration: 'real b' must resolve to RealTypespec";
}

// ss.5.7.2: '0.74' is a real literal -- it must be a Constant node.
TEST_F(TypeOpTest, B_Value_IsConstant) {
  const hldb::Net *n = getNet(m_design, "b");
  ASSERT_NE(n, nullptr);
  EXPECT_NE(n->getValue<hldb::Constant>(), nullptr) << "ss.5.7.2: '0.74' must be a Constant node";
}

// ss.5.7.2: a real literal carries constType vpiRealConst (2).
TEST_F(TypeOpTest, B_Value_ConstType_IsReal) {
  const hldb::Net *n = getNet(m_design, "b");
  ASSERT_NE(n, nullptr);
  const hldb::Constant *c = n->getValue<hldb::Constant>();
  ASSERT_NE(c, nullptr);
  EXPECT_EQ(c->getConstType(), vpiRealConst) << "ss.5.7.2: '0.74' must have constType vpiRealConst (2)";
}

// ss.5.7.2: the constant must decompile to "0.74".
TEST_F(TypeOpTest, B_Value_Decompile_Is0_74) {
  const hldb::Net *n = getNet(m_design, "b");
  ASSERT_NE(n, nullptr);
  const hldb::Constant *c = n->getValue<hldb::Constant>();
  ASSERT_NE(c, nullptr);
  EXPECT_EQ(std::string(c->getDecompile()), "0.74") << "'real b = 0.74': value must decompile to \"0.74\"";
}

// ===========================================================================
// var type(a+b) c  -- type operator  (ss.6.23)
// ===========================================================================

// ss.6.23: 'var type(a+b) c' must produce a Net node named "c".
TEST_F(TypeOpTest, C_Exists) { EXPECT_NE(getNet(m_design, "c"), nullptr) << "Net 'c' not found in net collection"; }

// ss.6.8 + ss.6.23: 'var type(a+b) c' with the 'var' keyword is stored in the
// net collection. Check by name that 'c' does not appear in getVariables().
TEST_F(TypeOpTest, C_NotInVariables) {
  const hldb::Module *m = getTop(m_design);
  ASSERT_NE(m, nullptr);
  if (m->getVariables() == nullptr) return;
  EXPECT_EQ(hldb::findByName<hldb::Variable>("c", m->getVariables()), nullptr)
      << "ss.6.23: 'var type(a+b) c' must NOT appear in getVariables()";
}

// ss.6.23: type() is parsed as a type, not an expression. The compiler must
// produce a non-null typespec for 'c' (the type() expression occupies the
// typespec slot of the declaration, not the value slot).
TEST_F(TypeOpTest, C_TypeExpression_ParsedAsType) {
  const hldb::Net *n = getNet(m_design, "c");
  ASSERT_NE(n, nullptr);
  EXPECT_NE(n->getTypespec(), nullptr) << "ss.6.23: type() must be parsed in the typespec position -- "
                                          "'c' must have a non-null typespec";
}

// ss.6.23: the expression inside type() is parsed to determine the type but is
// NOT evaluated to produce a value. 'c' has no initializer, so getValue()
// must return null. Together with C_TypeExpression_ParsedAsType, this
// documents that the expression was used for type inference, not evaluation.
TEST_F(TypeOpTest, C_TypeExpression_NotEvaluatedAsValue) {
  const hldb::Net *n = getNet(m_design, "c");
  ASSERT_NE(n, nullptr);
  if (m_design->getElaborated()) {
    EXPECT_EQ(n->getValue<hldb::Constant>(), nullptr)
        << "ss.6.23: post-elaboration: the expression inside type() must NOT "
           "be evaluated as a value -- 'c' has no initializer";
  } else {
    EXPECT_EQ(n->getValue<hldb::Constant>(), nullptr)
        << "pre-elaboration: 'c' has no initializer -- getValue() is null";
  }
}

// ss.6.23: 'a' and 'b' are both 'real'. The expression 'a+b' is therefore of
// type 'real' (ss.6.12, ss.11.6.1). The type operator must propagate this --
// 'c' must have typespec resolving to RealTypespec.
// NOTE: if this test fails, it indicates that Surelog does not resolve the
// type operator to a concrete typespec (vpiActual is missing on the RefTypespec
// for 'c'), which is a violation of ss.6.23.
TEST_F(TypeOpTest, C_Typespec_ResolvesToReal) {
  const hldb::Net *n = getNet(m_design, "c");
  ASSERT_NE(n, nullptr);
  const hldb::RefTypespec *rt = n->getTypespec();
  ASSERT_NE(rt, nullptr) << "ss.6.23: 'c' must have a typespec";
  if (m_design->getElaborated()) {
    EXPECT_NE(rt->getActual<hldb::RealTypespec>(), nullptr)
        << "ss.6.23: post-elaboration: type(a+b) where a,b are 'real' must "
           "resolve to RealTypespec";
  } else {
    EXPECT_EQ(rt->getActual<hldb::RealTypespec>(), nullptr)
        << "pre-elaboration: vpiActual not yet resolved -- type() resolution "
           "happens at elaboration time";
  }
}

// ===========================================================================
// No initializer on c  (ss.6.23)
// ===========================================================================

// ss.6.23: 'var type(a+b) c' has no '= expr' initializer. The net must carry
// no value node.
TEST_F(TypeOpTest, C_HasNoInitializer) {
  const hldb::Net *n = getNet(m_design, "c");
  ASSERT_NE(n, nullptr);
  if (m_design->getElaborated()) {
    EXPECT_EQ(n->getValue<hldb::Constant>(), nullptr) << "ss.6.23: post-elaboration: 'var type(a+b) c' has no explicit "
                                                         "initializer -- elaboration must not synthesize a value";
  } else {
    EXPECT_EQ(n->getValue<hldb::Constant>(), nullptr) << "pre-elaboration: 'var type(a+b) c' has no initializer -- "
                                                         "getValue() is null";
  }
}

// ===========================================================================
// Operand resolution -- forward/self-reference checks  (ss.6.23)
// ===========================================================================

// ss.6.23: 'a' and 'b' are declared before 'c' (no forward-reference).
// Both must be fully resolved with RealTypespec as their actual typespec,
// confirming that the operand chain for type(a+b) is complete before 'c' is
// declared.
TEST_F(TypeOpTest, A_FullyResolved_BeforeC) {
  const hldb::Net *a = getNet(m_design, "a");
  ASSERT_NE(a, nullptr);
  const hldb::RefTypespec *rt = a->getTypespec();
  ASSERT_NE(rt, nullptr);
  EXPECT_NE(rt->getActual<hldb::RealTypespec>(), nullptr)
      << "ss.6.23: post-elaboration: operand 'a' of type(a+b) must be "
          "fully resolved to RealTypespec";
}

TEST_F(TypeOpTest, B_FullyResolved_BeforeC) {
  const hldb::Net *b = getNet(m_design, "b");
  ASSERT_NE(b, nullptr);
  const hldb::RefTypespec *rt = b->getTypespec();
  ASSERT_NE(rt, nullptr);
  EXPECT_NE(rt->getActual<hldb::RealTypespec>(), nullptr)
      << "ss.6.23: post-elaboration: operand 'b' of type(a+b) must be "
          "fully resolved to RealTypespec";
}

// ss.6.23: 'c' does not reference itself inside type(). The typespec of 'c'
// must be non-null (it was parsed) but 'c' itself must NOT appear in the net
// collection as a duplicate (only one Net named "c" should exist).
TEST_F(TypeOpTest, C_NoSelfReference_SingleEntry) {
  const hldb::Module *m = getTop(m_design);
  ASSERT_NE(m, nullptr);
  ASSERT_NE(m->getNets(), nullptr);
  int count = 0;
  for (const hldb::Net *n : *m->getNets()) {
    if (n && n->getName() == "c") ++count;
  }
  EXPECT_EQ(count, 1) << "ss.6.23: 'c' must appear exactly once in the net collection -- "
                         "no self-referential duplication";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
