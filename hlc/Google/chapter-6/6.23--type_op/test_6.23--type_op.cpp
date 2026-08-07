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

// Tests for 6.23--type_op.sv (tags: 6.23)
//   module top();
//     real a = 4.76;
//     real b = 0.74;
//     var type(a+b) c;
//   endmodule
//
// What to check and why (IEEE 1800-2023 6.23 "Type operator", p.138,
// and 6.8 "Variable declarations", p.105, checked before any test code
// was written):
//   "When a type reference is used in a net declaration, it shall be
//   preceded by a net type keyword; and when it is used in a variable
//   declaration, it shall be preceded by the var keyword." followed
//   immediately by the spec's own worked example: "var type(a+b) c, d;"
//   -- this file's "var type(a+b) c;" is that exact example. The "var"
//   keyword makes this unambiguously a variable declaration, never a
//   net. "real" (for "a" and "b") is likewise a non_integer_type keyword
//   (6.8), never a net_type (6.7). This file has no
//   :should_fail_because: tag -- it is legal per spec.
//
//   A prior version of this test used hldb::Net/getNets() for "a", "b",
//   and "c" throughout -- the same net/variable misclassification bug
//   found and fixed across 6.5, 6.9.1, 6.12, 6.13, 6.14, 6.16, 6.17,
//   6.18, and 6.19 this session. For "c" specifically, the prior version
//   went further and had a dedicated test (C_NotInVariables) asserting
//   'c' must NOT appear in getVariables() -- actively encoding the
//   misclassification as a requirement, when the spec's own text says
//   the opposite ("preceded by the var keyword" -- i.e. a variable).
//   This version targets hldb::Variable for "a", "b", and "c", and
//   replaces C_NotInVariables with a real, currently-failing test
//   asserting 'c' SHOULD be a Variable.
//
// What is checked:
//   - module top has no Nets and exactly 3 Variables: "a", "b", "c"
//   - "a" (real, init 4.76) and "b" (real, init 0.74): RealTypespec,
//     Constant initial values (vpiRealConst)
//   - "c": has a typespec (type() parsed into the typespec position, not
//     evaluated as a value); no initializer; per 6.23's "shall represent
//     the self-determined result type of that expression" plus "a" and
//     "b" both being real, "c" should resolve to RealTypespec once
//     elaborated
//   - exactly one Variable named "c" (no self-reference duplication)
//   - compiler reports zero errors (this file is fully legal per 6.23)
//
// What is NOT checked and why:
//   - whether type(a+b) actually resolves to RealTypespec on "c":
//     checked conditionally on m_design->getElaborated(), since this is
//     an elaboration-phase result (HLDB only stores literal source
//     structure pre-elaboration) -- this is a real assertion either way,
//     not a skip.

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/module.h>
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
  return hldb::findByName<hldb::Module>("top", d->getAllModules());
}

static const hldb::Variable *getVar(const hldb::Design *d, std::string_view name) {
  const hldb::Module *m = getTop(d);
  if (!m || !m->getVariables()) return nullptr;
  return hldb::findByName<hldb::Variable>(name, m->getVariables());
}

// ===========================================================================
// Module
// ===========================================================================

TEST_F(TypeOpTest, ModuleExists) { ASSERT_NE(getTop(m_design), nullptr) << "module 'top' not found"; }

// ===========================================================================
// Variable collection -- a, b, c are all Variables (IEEE 1800-2023 6.8, 6.23)
// ===========================================================================

TEST_F(TypeOpTest, ModuleHasNoNets) {
  const hldb::Module *m = getTop(m_design);
  ASSERT_NE(m, nullptr);
  EXPECT_TRUE(m->getNets() == nullptr || m->getNets()->empty())
      << "none of 'a' (real), 'b' (real), 'c' (var type(...)) declare a net-type keyword";
}

TEST_F(TypeOpTest, VariableCollectionExists) {
  const hldb::Module *m = getTop(m_design);
  ASSERT_NE(m, nullptr);
  EXPECT_NE(m->getVariables(), nullptr) << "module must have a variable collection (a, b, c)";
}

TEST_F(TypeOpTest, VariableCount_IsThree) {
  const hldb::Module *m = getTop(m_design);
  ASSERT_NE(m, nullptr);
  ASSERT_NE(m->getVariables(), nullptr);
  EXPECT_EQ(m->getVariables()->size(), 3u) << "module 'top' declares exactly three variables: a, b, c";
}

// ===========================================================================
// real a = 4.76  (IEEE 1800-2023 6.12, 6.8)
// ===========================================================================

TEST_F(TypeOpTest, A_Exists) {
  EXPECT_NE(getVar(m_design, "a"), nullptr) << "Variable 'a' not found in variable collection";
}

TEST_F(TypeOpTest, A_Typespec_IsReal) {
  const hldb::Variable *v = getVar(m_design, "a");
  ASSERT_NE(v, nullptr);
  const hldb::RefTypespec *rt = v->getTypespec();
  ASSERT_NE(rt, nullptr) << "'real a' must have a typespec";
  EXPECT_NE(rt->getActual<hldb::RealTypespec>(), nullptr) << "'real a' must resolve to RealTypespec";
}

TEST_F(TypeOpTest, A_Value_IsConstant) {
  const hldb::Variable *v = getVar(m_design, "a");
  ASSERT_NE(v, nullptr);
  EXPECT_NE(v->getValue<hldb::Constant>(), nullptr) << "'4.76' must be a Constant node";
}

TEST_F(TypeOpTest, A_Value_ConstType_IsReal) {
  const hldb::Variable *v = getVar(m_design, "a");
  ASSERT_NE(v, nullptr);
  const hldb::Constant *c = v->getValue<hldb::Constant>();
  ASSERT_NE(c, nullptr);
  EXPECT_EQ(c->getConstType(), vpiRealConst) << "'4.76' must have constType vpiRealConst (2)";
}

TEST_F(TypeOpTest, A_Value_Decompile_Is4_76) {
  const hldb::Variable *v = getVar(m_design, "a");
  ASSERT_NE(v, nullptr);
  const hldb::Constant *c = v->getValue<hldb::Constant>();
  ASSERT_NE(c, nullptr);
  EXPECT_EQ(std::string(c->getDecompile()), "4.76") << "'real a = 4.76': value must decompile to \"4.76\"";
}

// ===========================================================================
// real b = 0.74  (IEEE 1800-2023 6.12, 6.8)
// ===========================================================================

TEST_F(TypeOpTest, B_Exists) {
  EXPECT_NE(getVar(m_design, "b"), nullptr) << "Variable 'b' not found in variable collection";
}

TEST_F(TypeOpTest, B_Typespec_IsReal) {
  const hldb::Variable *v = getVar(m_design, "b");
  ASSERT_NE(v, nullptr);
  const hldb::RefTypespec *rt = v->getTypespec();
  ASSERT_NE(rt, nullptr) << "'real b' must have a typespec";
  EXPECT_NE(rt->getActual<hldb::RealTypespec>(), nullptr) << "'real b' must resolve to RealTypespec";
}

TEST_F(TypeOpTest, B_Value_IsConstant) {
  const hldb::Variable *v = getVar(m_design, "b");
  ASSERT_NE(v, nullptr);
  EXPECT_NE(v->getValue<hldb::Constant>(), nullptr) << "'0.74' must be a Constant node";
}

TEST_F(TypeOpTest, B_Value_ConstType_IsReal) {
  const hldb::Variable *v = getVar(m_design, "b");
  ASSERT_NE(v, nullptr);
  const hldb::Constant *c = v->getValue<hldb::Constant>();
  ASSERT_NE(c, nullptr);
  EXPECT_EQ(c->getConstType(), vpiRealConst) << "'0.74' must have constType vpiRealConst (2)";
}

TEST_F(TypeOpTest, B_Value_Decompile_Is0_74) {
  const hldb::Variable *v = getVar(m_design, "b");
  ASSERT_NE(v, nullptr);
  const hldb::Constant *c = v->getValue<hldb::Constant>();
  ASSERT_NE(c, nullptr);
  EXPECT_EQ(std::string(c->getDecompile()), "0.74") << "'real b = 0.74': value must decompile to \"0.74\"";
}

// ===========================================================================
// var type(a+b) c  -- type operator (IEEE 1800-2023 6.23)
// ===========================================================================

TEST_F(TypeOpTest, C_Exists) {
  EXPECT_NE(getVar(m_design, "c"), nullptr) << "Variable 'c' not found in variable collection";
}

// The actual point of this file: "var type(a+b) c;" matches the spec's own
// example verbatim ("var type(a+b) c, d;") -- "var" makes this a variable
// declaration, per "when it is used in a variable declaration, it shall be
// preceded by the var keyword." A prior version of this test asserted the
// opposite (C_NotInVariables); this is corrected here.
TEST_F(TypeOpTest, C_IsInVariables) {
  const hldb::Module *m = getTop(m_design);
  ASSERT_NE(m, nullptr);
  EXPECT_NE(getVar(m_design, "c"), nullptr)
      << "IEEE 1800-2023 6.23: 'var type(a+b) c;' matches the spec's own example -- the 'var' "
         "keyword makes 'c' a variable, not a net";
}

TEST_F(TypeOpTest, C_TypeExpression_ParsedAsType) {
  const hldb::Variable *v = getVar(m_design, "c");
  ASSERT_NE(v, nullptr);
  EXPECT_NE(v->getTypespec(), nullptr) << "IEEE 1800-2023 6.23: type() must be parsed in the typespec "
                                           "position -- 'c' must have a non-null typespec";
}

// IEEE 1800-2023 6.23: "The expression shall not be evaluated" -- 'c' has no
// initializer, so getValue() must return null.
TEST_F(TypeOpTest, C_TypeExpression_NotEvaluatedAsValue) {
  const hldb::Variable *v = getVar(m_design, "c");
  ASSERT_NE(v, nullptr);
  EXPECT_EQ(v->getValue<hldb::Constant>(), nullptr)
      << "IEEE 1800-2023 6.23: the expression inside type() must NOT be evaluated as a value -- "
         "'c' has no initializer";
}

// IEEE 1800-2023 6.23: "The type operator applied to an expression shall
// represent the self-determined result type of that expression." 'a' and 'b'
// are both 'real', so type(a+b) must resolve to RealTypespec. Resolving
// type() to a concrete typespec is an elaboration-phase operation.
TEST_F(TypeOpTest, C_Typespec_ResolvesToReal) {
  const hldb::Variable *v = getVar(m_design, "c");
  ASSERT_NE(v, nullptr);
  const hldb::RefTypespec *rt = v->getTypespec();
  ASSERT_NE(rt, nullptr) << "'c' must have a typespec";
  if (m_design->getElaborated()) {
    EXPECT_NE(rt->getActual<hldb::RealTypespec>(), nullptr)
        << "IEEE 1800-2023 6.23: post-elaboration: type(a+b) where a,b are 'real' must resolve to "
           "RealTypespec";
  } else {
    EXPECT_EQ(rt->getActual<hldb::RealTypespec>(), nullptr)
        << "pre-elaboration: vpiActual not yet resolved -- type() resolution happens at "
           "elaboration time";
  }
}

TEST_F(TypeOpTest, C_HasNoInitializer) {
  const hldb::Variable *v = getVar(m_design, "c");
  ASSERT_NE(v, nullptr);
  EXPECT_EQ(v->getValue<hldb::Constant>(), nullptr)
      << "'var type(a+b) c' has no explicit initializer -- getValue() is null";
}

// ===========================================================================
// Operand resolution -- forward/self-reference checks (IEEE 1800-2023 6.23)
// ===========================================================================

// 'a' and 'b' are declared before 'c' (no forward-reference). Both must be
// fully resolved with RealTypespec, confirming the operand chain for
// type(a+b) is complete before 'c' is declared.
TEST_F(TypeOpTest, A_FullyResolved_BeforeC) {
  const hldb::Variable *a = getVar(m_design, "a");
  ASSERT_NE(a, nullptr);
  const hldb::RefTypespec *rt = a->getTypespec();
  ASSERT_NE(rt, nullptr);
  EXPECT_NE(rt->getActual<hldb::RealTypespec>(), nullptr)
      << "operand 'a' of type(a+b) must be fully resolved to RealTypespec";
}

TEST_F(TypeOpTest, B_FullyResolved_BeforeC) {
  const hldb::Variable *b = getVar(m_design, "b");
  ASSERT_NE(b, nullptr);
  const hldb::RefTypespec *rt = b->getTypespec();
  ASSERT_NE(rt, nullptr);
  EXPECT_NE(rt->getActual<hldb::RealTypespec>(), nullptr)
      << "operand 'b' of type(a+b) must be fully resolved to RealTypespec";
}

// 'c' does not reference itself inside type(). Only one Variable named "c"
// should exist (no self-referential duplication).
TEST_F(TypeOpTest, C_NoSelfReference_SingleEntry) {
  const hldb::Module *m = getTop(m_design);
  ASSERT_NE(m, nullptr);
  ASSERT_NE(m->getVariables(), nullptr);
  int count = 0;
  for (const hldb::Variable *v : *m->getVariables()) {
    if (v && v->getName() == "c") ++count;
  }
  EXPECT_EQ(count, 1) << "'c' must appear exactly once in the variable collection -- no "
                         "self-referential duplication";
}

TEST_F(TypeOpTest, CompilerReportsZeroErrors) {
  ASSERT_NE(m_session->getErrorContainer(), nullptr);
  const ErrorContainer::Stats stats = m_session->getErrorContainer()->getErrorStats();
  EXPECT_EQ(stats.nbFatal, 0);
  EXPECT_EQ(stats.nbSyntax, 0);
  EXPECT_EQ(stats.nbError, 0);
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
