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
// tests/Google/chapter-18/18.6.2--pre-randomize-method_0.sv. Lines 1-14 are
// the license and the suite's :name:/:description:/:tags: comment; the rest
// is:
//
//   16  class a;
//   17      rand int b;
//   18      int d;
//   19
//   20      constraint c { b == 5; }
//   21      function void pre_randomize();
//   22          d = 20;
//   23      endfunction
//   24  endclass
//
// The point of the fixture is the pre_randomize() hook (IEEE 1800-2023
// 18.6.2). Every class has built-in pre_randomize() and post_randomize()
// methods; randomize() calls pre_randomize() before it computes new random
// values and post_randomize() after. A class may override them. Here class
// a overrides pre_randomize() with a void function that sets the
// non-random property 'd' to 20.
//
// What is checked, and why:
//   class a (8.3)
//     - exactly one class named "a"; user-defined; not virtual (8.21); no
//       'extends' (8.13), no parameters (8.25), no end label
//   Properties, in declaration order: b, d
//     - 'rand int b;' (18.4): a signed int (6.11), getIsRandomized() true,
//       getRandType() vpiRand, no initializer
//     - 'int d;': a signed int, not rand/randc, no initializer
//     - both public (8.18) and not const (8.19)
//   constraint c { b == 5; } (18.5)
//     - exactly one constraint block, named "c"; not 'pure' (18.5.2) and not
//       virtual
//     - exactly one constraint item, 'b == 5': vpiEqOp whose left operand
//       binds to class a's 'b' and whose right operand is the literal 5
//   function void pre_randomize(); (18.6.2, 13.4)
//     - class a declares exactly one method (the implicit 'new' left out),
//       a Function named "pre_randomize"
//     - it is a class method, public (8.18), automatic (8.6: the lifetime
//       of a class method is always automatic)
//     - not virtual: 18.6.2 says "The pre_randomize() and post_randomize()
//       methods are not virtual", and the source writes no 'virtual'
//     - return type 'void' (13.4.1): a VoidTypespec
//     - no formal arguments
//     - no end label ('endfunction' is written without ': pre_randomize')
//   d = 20; (10.4.1)
//     - the function's only statement, so not wrapped in a Begin
//     - a blocking Assignment whose LHS binds to class a's 'd' (not to
//       'b') and whose RHS is the integer literal 20
//   Design
//     - the file declares no module
//   Diagnostics
//     - no CompUndefinedVariable for 'b' or 'd'
//     - no fatal, syntax or error diagnostic points into this file
//     - the whole compilation is legal: zero fatal, syntax and error
//       diagnostics
//
// Reduction and elaboration: nothing in this file reduces or elaborates.
// There is no parameter, no instance and no class specialization; every
// binding asserted is name resolution. No check is gated on getElaborated().
//
// KNOWN FAILURE (error on a legal constraint block), not a defect in this
// test: HLC reports IllegalPropertyValue (2038) at line 20, column 18
// -- the '{' of 'constraint c { b == 5; }'. That constraint block is legal
// (18.5: constraint_declaration ::= constraint constraint_identifier
// constraint_block), and the model HLC builds for it is correct
// (ClassAHasOneConstraintBlockC and ConstraintCIsBEquals5 pass). It is the
// only error in the compilation, so NoErrorIsReportedInThisFile and
// NoFatalSyntaxOrErrorDiagnostics are expected to fail until HLC is fixed;
// they are intentionally not skipped or relaxed.
//
// What is NOT checked, and why:
//   - That randomize() calls pre_randomize() before it assigns b, the value
//     it picks for b (5, forced by 'c'), and that d becomes 20 all only
//     happen while simulation runs. Permanently out of scope. The static
//     half is covered by ConstraintCIsBEquals5,
//     PreRandomizeIsVoidFunctionWithNoFormals and
//     PreRandomizeBodyAssignsTwentyToD.
//   - Whether HLC links the user's pre_randomize() to the built-in one it
//     overrides is a tool convention with no HLDB accessor in this suite.
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
#include <hldb/assignment.h>
#include <hldb/begin.h>
#include <hldb/class_defn.h>
#include <hldb/constant.h>
#include <hldb/constraint.h>
#include <hldb/design.h>
#include <hldb/function.h>
#include <hldb/int_typespec.h>
#include <hldb/module.h>
#include <hldb/operation.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/sv_vpi_user.h>
#include <hldb/task_func.h>
#include <hldb/variable.h>
#include <hldb/void_typespec.h>
#include <hldb/vpi_user.h>

#include <cstdint>
#include <string_view>
#include <vector>

namespace hlc {

class PreRandomizeMethod0Test : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "18.6.2--pre-randomize-method_0.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::ClassDefn *getClassA() {
    return hldb::findByName<hldb::ClassDefn>("a", m_design->getAllClasses());
  }

  static const hldb::Variable *getProperty(std::string_view name) {
    const hldb::ClassDefn *const cls = getClassA();
    if (cls == nullptr) {
      return nullptr;
    }
    return hldb::findByName<hldb::Variable>(name, cls->getVariables());
  }

  static const hldb::Constraint *getConstraintC() {
    const hldb::ClassDefn *const cls = getClassA();
    if (cls == nullptr) {
      return nullptr;
    }
    return hldb::findByName<hldb::Constraint>("c", cls->getConstraints());
  }

  static const hldb::Function *getPreRandomize() {
    const hldb::ClassDefn *const cls = getClassA();
    if (cls == nullptr) {
      return nullptr;
    }
    return hldb::findByName<hldb::Function>("pre_randomize", cls->getMethods());
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

  // Verifies 'type' is 'int': an IntTypespec that is signed.
  static void ExpectSignedInt(const hldb::RefTypespec *type, std::string_view what) {
    ASSERT_NE(type, nullptr) << what << " has no typespec";
    const hldb::IntTypespec *const ts = type->getActual<hldb::IntTypespec>();
    ASSERT_NE(ts, nullptr) << what << " is declared 'int'";
    EXPECT_TRUE(ts->getSigned()) << "6.11: 'int' is a signed type, so " << what << " is signed";
  }
};

// ---------------------------------------------------------------------------
// class a; ... endclass
// ---------------------------------------------------------------------------

TEST_F(PreRandomizeMethod0Test, ClassAIsDeclaredOnceAsUserDefinedClass) {
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

TEST_F(PreRandomizeMethod0Test, ClassAHasNoBaseClassParametersOrEndLabel) {
  const hldb::ClassDefn *const cls = getClassA();
  ASSERT_NE(cls, nullptr);
  EXPECT_FALSE(cls->getVirtual()) << "8.21: 'class a' is not declared 'virtual class'";
  EXPECT_EQ(cls->getExtends(), nullptr) << "8.13: 'class a;' has no 'extends' clause";
  EXPECT_TRUE(cls->getParameters() == nullptr || cls->getParameters()->empty())
      << "8.25: 'class a;' has no parameter port list";
  EXPECT_EQ(cls->getEndLabelObj(), nullptr) << "'endclass' is written without ': a'";
}

// ---------------------------------------------------------------------------
// rand int b;  int d;
// ---------------------------------------------------------------------------

TEST_F(PreRandomizeMethod0Test, ClassAHasPropertiesBAndDInOrder) {
  const hldb::ClassDefn *const cls = getClassA();
  ASSERT_NE(cls, nullptr);
  ASSERT_NE(cls->getVariables(), nullptr);
  ASSERT_EQ(cls->getVariables()->size(), 2u) << "'rand int b;' and 'int d;'";
  const std::vector<std::string_view> expected = {"b", "d"};
  for (size_t i = 0; i < expected.size(); ++i) {
    const hldb::Variable *const prop = cls->getVariables()->at(i);
    ASSERT_NE(prop, nullptr) << "property " << i;
    EXPECT_EQ(prop->getName(), expected[i]) << "property " << i;
    EXPECT_EQ(prop->getVisibility(), vpiPublicVis) << "8.18: '" << expected[i] << "' has no 'local'/'protected'";
    EXPECT_FALSE(prop->getConstantVariable()) << "8.19: '" << expected[i] << "' has no 'const' qualifier";
  }
}

TEST_F(PreRandomizeMethod0Test, BIsRandSignedIntWithoutInitializer) {
  const hldb::Variable *const b = getProperty("b");
  ASSERT_NE(b, nullptr);
  ExpectSignedInt(b->getTypespec(), "'b'");
  EXPECT_TRUE(b->getIsRandomized()) << "18.4: 'b' is declared with the 'rand' qualifier";
  EXPECT_EQ(b->getRandType(), vpiRand) << "18.4.1: 'rand', not 'randc' (18.4.2)";
  EXPECT_EQ(b->getValue(), nullptr) << "'rand int b;' has no initializer";
}

TEST_F(PreRandomizeMethod0Test, DIsNonRandomSignedIntWithoutInitializer) {
  const hldb::Variable *const d = getProperty("d");
  ASSERT_NE(d, nullptr);
  ExpectSignedInt(d->getTypespec(), "'d'");
  EXPECT_FALSE(d->getIsRandomized()) << "18.4: 'int d;' has no 'rand'/'randc' qualifier";
  EXPECT_EQ(d->getValue(), nullptr) << "'int d;' has no initializer";
}

// ---------------------------------------------------------------------------
// constraint c { b == 5; }
// ---------------------------------------------------------------------------

TEST_F(PreRandomizeMethod0Test, ClassAHasOneConstraintBlockC) {
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

TEST_F(PreRandomizeMethod0Test, ConstraintCIsBEquals5) {
  const hldb::Constraint *const c = getConstraintC();
  ASSERT_NE(c, nullptr);
  ASSERT_NE(c->getConstraintItems(), nullptr) << "18.5: '{ b == 5; }' holds one constraint expression";
  ASSERT_EQ(c->getConstraintItems()->size(), 1u);
  const hldb::Operation *const op = any_cast<hldb::Operation>(c->getConstraintItems()->at(0));
  ASSERT_NE(op, nullptr) << "'b == 5' should be an Operation";
  EXPECT_EQ(op->getOpType(), vpiEqOp) << "11.4.5: '==' is logical equality";
  ASSERT_NE(op->getOperands(), nullptr);
  ASSERT_EQ(op->getOperands()->size(), 2u) << "'b == 5' is a binary operation";
  ExpectBoundRef(op->getOperands()->at(0), "b", getProperty("b"));
  ExpectIntLiteral(op->getOperands()->at(1), "5");
}

// ---------------------------------------------------------------------------
// function void pre_randomize(); d = 20; endfunction
// ---------------------------------------------------------------------------

TEST_F(PreRandomizeMethod0Test, ClassADeclaresOnlyPreRandomize) {
  ASSERT_NE(getClassA(), nullptr);
  const std::vector<const hldb::TaskFunc *> methods = getClassADeclaredMethods();
  ASSERT_EQ(methods.size(), 1u) << "class a declares one method, pre_randomize";
  EXPECT_EQ(methods[0]->getName(), "pre_randomize");
  const hldb::Function *const fn = getPreRandomize();
  ASSERT_NE(fn, nullptr) << "13.4: 'function void pre_randomize();' is a Function";
  EXPECT_EQ(methods[0], fn);
}

TEST_F(PreRandomizeMethod0Test, PreRandomizeIsPublicAutomaticNonVirtualMethod) {
  const hldb::Function *const fn = getPreRandomize();
  ASSERT_NE(fn, nullptr);
  EXPECT_TRUE(fn->getMethod()) << "pre_randomize is declared inside class a";
  EXPECT_EQ(fn->getVisibility(), vpiPublicVis) << "8.18: no 'local'/'protected' qualifier";
  EXPECT_TRUE(fn->getAutomatic()) << "8.6: the lifetime of a method declared in a class is always automatic";
  EXPECT_FALSE(fn->getVirtual()) << "18.6.2: pre_randomize() and post_randomize() are not virtual";
}

TEST_F(PreRandomizeMethod0Test, PreRandomizeIsVoidFunctionWithNoFormals) {
  const hldb::Function *const fn = getPreRandomize();
  ASSERT_NE(fn, nullptr);
  ASSERT_NE(fn->getReturn(), nullptr) << "13.4.1: the return type 'void' is written";
  EXPECT_NE(fn->getReturn()->getActual<hldb::VoidTypespec>(), nullptr) << "13.4.1: 'void' return type";
  EXPECT_TRUE(fn->getIODecls() == nullptr || fn->getIODecls()->empty())
      << "'pre_randomize()' declares no formal arguments";
  EXPECT_EQ(fn->getEndLabelObj(), nullptr) << "'endfunction' is written without ': pre_randomize'";
}

TEST_F(PreRandomizeMethod0Test, PreRandomizeBodyAssignsTwentyToD) {
  const hldb::Function *const fn = getPreRandomize();
  ASSERT_NE(fn, nullptr);
  const hldb::Assignment *const assign = fn->getStmt<hldb::Assignment>();
  ASSERT_NE(assign, nullptr) << "'d = 20;' is the only statement, so it is not wrapped in a Begin";
  EXPECT_TRUE(assign->getBlocking()) << "10.4.1: '=' in a procedural context is a blocking assignment";
  ExpectBoundRef(assign->getLhs(), "d", getProperty("d"));
  ExpectIntLiteral(assign->getRhs(), "20");
}

// ---------------------------------------------------------------------------
// Design
// ---------------------------------------------------------------------------

TEST_F(PreRandomizeMethod0Test, FileDeclaresNoModule) {
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
TEST_F(PreRandomizeMethod0Test, NoErrorIsReportedInThisFile) {
  size_t count = 0;
  for (const Event &error : m_session->getEventListener()->getEvents()) {
    const EventDefinition *const info = hldb::findEventDefinition(hlc::getEventTable(), error.getType());
    if (info == nullptr || info->m_severity > EventSeverity::Error) continue;
    for (const EventArg &loc : error.getArguments()) {
      const std::string_view path = loc.m_file;
      if (path.find("18.6.2--pre-randomize-method_0.sv") == std::string_view::npos) continue;
      ++count;
      ADD_FAILURE() << "error " << error.getType() << " reported at " << path << ":" << loc.m_line << ":"
                    << loc.m_column;
    }
  }
  EXPECT_EQ(count, 0u) << "the source is legal SystemVerilog";
}

// Expected to fail until HLC is fixed -- see KNOWN FAILURE (error on a legal
// constraint block) in the file header.
TEST_F(PreRandomizeMethod0Test, NoFatalSyntaxOrErrorDiagnostics) {
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
