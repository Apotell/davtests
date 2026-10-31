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

// Validates the HLDB model built for
// tests/Google/chapter-18/18.6.3--behavior-of-randomization-methods_0.sv.
// Lines 1-14 are the license and the suite's :name:/:description:/:tags:
// comment; the rest is:
//
//   16  class a;
//   17      static rand int b;
//   18      constraint c { b > 5; b < 12; }
//   19  endclass
//
// The point of the fixture is a static random variable (IEEE 1800-2023
// 18.6.3): "Random variables declared as static are shared by all instances
// of the class in which they are declared. Each time the randomize() method
// is called, the variable is changed in every class instance." Class a
// declares one static rand int 'b' and one constraint block 'c' that keeps
// it strictly between 5 and 12.
//
// What is checked, and why:
//   class a (8.3)
//     - exactly one class named "a"; user-defined; not virtual (8.21); no
//       'extends' (8.13), no parameters (8.25), no end label
//     - declares no methods; randomize() is built in (18.6.1)
//   static rand int b; (8.9, 18.4, 18.6.3)
//     - the only property; a signed int (6.11) with no initializer
//     - declared 'rand': getIsRandomized() true and getRandType() vpiRand,
//       not vpiRandC (18.4.1)
//     - declared 'static' (8.9): one copy shared by every instance, so its
//       lifetime is static -- getAutomatic() is false
//     - public (8.18), not const (8.19)
//   constraint c { b > 5; b < 12; } (18.5)
//     - exactly one constraint block, named "c"; not 'pure' (18.5.2) and not
//       virtual
//     - exactly two constraint items, in source order (18.5: every
//       expression in a constraint block must hold):
//         'b > 5'  -- vpiGtOp whose left operand binds to class a's 'b'
//                     and whose right operand is the literal 5
//         'b < 12' -- vpiLtOp whose left operand binds to 'b' and whose
//                     right operand is the literal 12
//   Design
//     - the file declares no module
//   Diagnostics
//     - no CompUndefinedVariable for 'b'
//     - no fatal, syntax or error diagnostic points into this file
//     - the whole compilation is legal: zero fatal, syntax and error
//       diagnostics
//
// Reduction and elaboration: nothing in this file reduces or elaborates.
// There is no parameter, no instance and no class specialization; every
// binding asserted is name resolution. No check is gated on getElaborated().
//
// KNOWN FAILURE (error on a legal constraint block), not a defect in this
// test: HLC reports IllegalPropertyValue (2038) twice, both at line
// 18, column 18 -- the '{' of 'constraint c { b > 5; b < 12; }'. That
// constraint block is legal (18.5: constraint_declaration ::= constraint
// constraint_identifier constraint_block), and the model HLC builds for it
// is correct (ClassAHasOneConstraintBlockC and
// ConstraintCHasTwoItemsBGreaterThan5AndBLessThan12 pass). The block holds
// two expressions and draws two errors; one-expression blocks in the
// 18.6.1/18.6.2 fixtures draw one. These are the only errors in the
// compilation, so NoErrorIsReportedInThisFile and
// NoFatalSyntaxOrErrorDiagnostics are expected to fail until HLC is fixed;
// they are intentionally not skipped or relaxed.
//
// What is NOT checked, and why:
//   - That one randomize() call changes b for every instance, and the value
//     it picks (6 to 11), only happen while simulation runs. Permanently out
//     of scope. The static half is covered by BIsStaticRandSignedInt and
//     ConstraintCHasTwoItemsBGreaterThan5AndBLessThan12.
//   - The constraint_mode() state of 'c' (18.9) is a run-time property of
//     each object, not source text.
//   - 8.7 provides an implicit 'new' for class a; how HLC represents it is a
//     tool convention, so the method count leaves 'new' out.
//   - Design-wide class counts: the .hlc passes -nobuiltin, a tool option,
//     so class a is found by name rather than by counting every class.
//   - The .hlc also puts UVM on the include path, but the file includes and
//     imports nothing from it, so UVM plays no part here.
//   - The warning count, source line numbers, and the license/metadata
//     comments.

#include <hlc/Common/FileSystem.h>
#include <hlc/Common/Session.h>
#include <hlc/Events/EventDefinition.h>
#include <hlc/Events/EventListener.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/class_defn.h>
#include <hldb/constant.h>
#include <hldb/constraint.h>
#include <hldb/design.h>
#include <hldb/int_typespec.h>
#include <hldb/module.h>
#include <hldb/operation.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/sv_vpi_user.h>
#include <hldb/task_func.h>
#include <hldb/variable.h>
#include <hldb/vpi_user.h>

#include <cstdint>
#include <string_view>
#include <vector>

namespace hlc {

class BehaviorOfRandomizationMethods0Test : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "18.6.3--behavior-of-randomization-methods_0.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::ClassDefn *getClassA() {
    return hldb::findByName<hldb::ClassDefn>("a", m_design->getAllClasses());
  }

  static const hldb::Variable *getB() {
    const hldb::ClassDefn *const cls = getClassA();
    if (cls == nullptr) {
      return nullptr;
    }
    return hldb::findByName<hldb::Variable>("b", cls->getVariables());
  }

  static const hldb::Constraint *getConstraintC() {
    const hldb::ClassDefn *const cls = getClassA();
    if (cls == nullptr) {
      return nullptr;
    }
    return hldb::findByName<hldb::Constraint>("c", cls->getConstraints());
  }

  // The methods class a declares. 8.7 provides an implicit 'new' when a
  // class writes none; whether HLC materializes it in getMethods() is a
  // tool convention, so it is left out.
  static std::vector<const hldb::TaskFunc *> getClassADeclaredMethods() {
    std::vector<const hldb::TaskFunc *> methods;
    const hldb::ClassDefn *const cls = getClassA();
    if (cls == nullptr || cls->getMethods() == nullptr) {
      return methods;
    }
    for (const hldb::TaskFunc *const method : *cls->getMethods()) {
      if (method->getName() != "new") methods.emplace_back(method);
    }
    return methods;
  }

  // Verifies 'expr' is a RefObj named 'name' bound by object identity to
  // 'target'.
  static void ExpectBoundRef(const hldb::Any *expr, std::string_view name, const hldb::Any *target) {
    const hldb::RefObj *const ref = any_cast<hldb::RefObj>(expr);
    ASSERT_NE(ref, nullptr) << "'" << name << "' should be a RefObj";
    EXPECT_EQ(ref->getName(), name);
    ASSERT_NE(target, nullptr) << "the declaration of '" << name << "' was not found";
    EXPECT_EQ(ref->getActual(), target) << "'" << name << "' must bind to its declaration";
  }

  // Verifies 'expr' is the integer literal whose source text is 'text'.
  static void ExpectIntLiteral(const hldb::Any *expr, std::string_view text) {
    const hldb::Constant *const literal = any_cast<hldb::Constant>(expr);
    ASSERT_NE(literal, nullptr) << "'" << text << "' should be a Constant";
    EXPECT_EQ(literal->getDecompile(), text);
  }

  // Verifies 'expr' is '<b> <op> <literal>': a binary Operation of type
  // 'opType' whose left operand binds to class a's 'b' and whose right
  // operand is the integer literal 'literal'.
  static void ExpectBCompare(const hldb::Any *expr, int32_t opType, std::string_view literal, std::string_view what) {
    const hldb::Operation *const op = any_cast<hldb::Operation>(expr);
    ASSERT_NE(op, nullptr) << "'" << what << "' should be an Operation";
    EXPECT_EQ(op->getOpType(), opType) << "'" << what << "'";
    ASSERT_NE(op->getOperands(), nullptr);
    ASSERT_EQ(op->getOperands()->size(), 2u) << "'" << what << "' is a binary operation";
    ExpectBoundRef(op->getOperands()->at(0), "b", getB());
    ExpectIntLiteral(op->getOperands()->at(1), literal);
  }
};

// ---------------------------------------------------------------------------
// class a; ... endclass
// ---------------------------------------------------------------------------

TEST_F(BehaviorOfRandomizationMethods0Test, ClassAIsDeclaredOnceAsUserDefinedClass) {
  ASSERT_NE(m_design->getAllClasses(), nullptr);
  size_t count = 0;
  for (const hldb::ClassDefn *const cls : *m_design->getAllClasses()) {
    if (cls->getName() == "a") ++count;
  }
  EXPECT_EQ(count, 1u) << "the source declares class 'a' exactly once";
  const hldb::ClassDefn *const cls = getClassA();
  ASSERT_NE(cls, nullptr);
  EXPECT_EQ(cls->getClassType(), vpiUserDefinedClass) << "8.3: 'class a;' is a user-defined class";
}

TEST_F(BehaviorOfRandomizationMethods0Test, ClassAHasNoBaseClassParametersOrEndLabel) {
  const hldb::ClassDefn *const cls = getClassA();
  ASSERT_NE(cls, nullptr);
  EXPECT_FALSE(cls->getVirtual()) << "8.21: 'class a' is not declared 'virtual class'";
  EXPECT_EQ(cls->getExtends(), nullptr) << "8.13: 'class a;' has no 'extends' clause";
  EXPECT_TRUE(cls->getParameters() == nullptr || cls->getParameters()->empty())
      << "8.25: 'class a;' has no parameter port list";
  EXPECT_EQ(cls->getEndLabelObj(), nullptr) << "'endclass' is written without ': a'";
}

TEST_F(BehaviorOfRandomizationMethods0Test, ClassADeclaresNoMethods) {
  ASSERT_NE(getClassA(), nullptr);
  EXPECT_EQ(getClassADeclaredMethods().size(), 0u)
      << "class a declares no task or function; randomize() is built in to every class (18.6.1)";
}

// ---------------------------------------------------------------------------
// static rand int b;
// ---------------------------------------------------------------------------

TEST_F(BehaviorOfRandomizationMethods0Test, ClassAHasOnePropertyB) {
  const hldb::ClassDefn *const cls = getClassA();
  ASSERT_NE(cls, nullptr);
  ASSERT_NE(cls->getVariables(), nullptr);
  ASSERT_EQ(cls->getVariables()->size(), 1u) << "'static rand int b;' is the only property";
  EXPECT_EQ(cls->getVariables()->at(0)->getName(), "b");
  EXPECT_EQ(cls->getVariables()->at(0), getB());
}

TEST_F(BehaviorOfRandomizationMethods0Test, BIsStaticRandSignedInt) {
  const hldb::Variable *const b = getB();
  ASSERT_NE(b, nullptr);
  const hldb::RefTypespec *const type = b->getTypespec();
  ASSERT_NE(type, nullptr) << "'b' has no typespec";
  const hldb::IntTypespec *const ts = type->getActual<hldb::IntTypespec>();
  ASSERT_NE(ts, nullptr) << "'b' is declared 'int'";
  EXPECT_TRUE(ts->getSigned()) << "6.11: 'int' is a signed type";
  EXPECT_TRUE(b->getIsRandomized()) << "18.4: 'b' is declared with the 'rand' qualifier";
  EXPECT_EQ(b->getRandType(), vpiRand) << "18.4.1: 'rand', not 'randc' (18.4.2)";
  EXPECT_FALSE(b->getAutomatic())
      << "8.9 / 18.6.3: 'static' makes b a single copy shared by all instances, so its lifetime is static";
  EXPECT_EQ(b->getValue(), nullptr) << "'static rand int b;' has no initializer";
}

TEST_F(BehaviorOfRandomizationMethods0Test, BIsPublicAndNotConst) {
  const hldb::Variable *const b = getB();
  ASSERT_NE(b, nullptr);
  EXPECT_EQ(b->getVisibility(), vpiPublicVis) << "8.18: 'b' has no 'local'/'protected' qualifier";
  EXPECT_FALSE(b->getConstantVariable()) << "8.19: 'b' has no 'const' qualifier";
}

// ---------------------------------------------------------------------------
// constraint c { b > 5; b < 12; }
// ---------------------------------------------------------------------------

TEST_F(BehaviorOfRandomizationMethods0Test, ClassAHasOneConstraintBlockC) {
  const hldb::ClassDefn *const cls = getClassA();
  ASSERT_NE(cls, nullptr);
  ASSERT_NE(cls->getConstraints(), nullptr) << "18.5: 'constraint c { ... }' is declared in class a";
  ASSERT_EQ(cls->getConstraints()->size(), 1u) << "class a declares exactly one constraint block";
  const hldb::Constraint *const c = getConstraintC();
  ASSERT_NE(c, nullptr) << "the constraint block is named 'c'";
  EXPECT_EQ(cls->getConstraints()->at(0), c);
  EXPECT_FALSE(c->getPure()) << "18.5.2: 'c' is not declared 'pure'";
  EXPECT_FALSE(c->getVirtual()) << "'c' is not declared 'virtual'";
}

TEST_F(BehaviorOfRandomizationMethods0Test, ConstraintCHasTwoItemsBGreaterThan5AndBLessThan12) {
  const hldb::Constraint *const c = getConstraintC();
  ASSERT_NE(c, nullptr);
  ASSERT_NE(c->getConstraintItems(), nullptr) << "18.5: '{ b > 5; b < 12; }' holds constraint expressions";
  ASSERT_EQ(c->getConstraintItems()->size(), 2u) << "two expressions, each terminated by ';'";
  ExpectBCompare(c->getConstraintItems()->at(0), vpiGtOp, "5", "b > 5");
  ExpectBCompare(c->getConstraintItems()->at(1), vpiLtOp, "12", "b < 12");
}

// ---------------------------------------------------------------------------
// Design
// ---------------------------------------------------------------------------

TEST_F(BehaviorOfRandomizationMethods0Test, FileDeclaresNoModule) {
  EXPECT_TRUE(m_design->getAllModules() == nullptr || m_design->getAllModules()->empty())
      << "the source declares only class a";
}

// ---------------------------------------------------------------------------
// Diagnostics
// ---------------------------------------------------------------------------

// The file is legal SystemVerilog, so no fatal, syntax or error diagnostic
// may point into it.
// Expected to fail until HLC is fixed -- see KNOWN FAILURE (error on a legal
// constraint block) in the file header.
TEST_F(BehaviorOfRandomizationMethods0Test, NoErrorIsReportedInThisFile) {
  size_t count = 0;
  for (const Event &error : m_session->getEventListener()->getEvents()) {
    const EventDefinition *const info = hldb::findEventDefinition(hlc::getEventTable(), error.getType());
    if (info == nullptr || info->m_severity > EventSeverity::Error) continue;
    for (const EventArg &loc : error.getArguments()) {
      const std::string_view path = loc.m_file;
      if (path.find("18.6.3--behavior-of-randomization-methods_0.sv") == std::string_view::npos) continue;
      ++count;
      ADD_FAILURE() << "error " << error.getType() << " reported at " << path << ":" << loc.m_line << ":"
                    << loc.m_column;
    }
  }
  EXPECT_EQ(count, 0u) << "the source is legal SystemVerilog";
}

// Expected to fail until HLC is fixed -- see KNOWN FAILURE (error on a legal
// constraint block) in the file header.
TEST_F(BehaviorOfRandomizationMethods0Test, NoFatalSyntaxOrErrorDiagnostics) {
  const EventListener::Stats stats = m_session->getEventListener()->getStats();
  EXPECT_EQ(stats.m_fatals, 0);
  EXPECT_EQ(stats.m_syntaxes, 0);
  EXPECT_EQ(stats.m_errors, 0) << "the source is legal SystemVerilog and should compile without errors";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
