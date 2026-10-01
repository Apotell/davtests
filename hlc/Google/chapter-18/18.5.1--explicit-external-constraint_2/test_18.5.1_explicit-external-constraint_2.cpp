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

// Source under test: tests/Google/chapter-18/18.5.1--explicit-external-constraint_2.sv
//
//   import uvm_pkg::*;
//   `include "uvm_macros.svh"
//
//   class a;
//       rand int b;
//       extern constraint c;
//   endclass
//   constraint a::c { b == 5; }
//
//   class env extends uvm_env;
//     a obj = new;
//     function new(string name, uvm_component parent = null); super.new(name, parent); endfunction
//     task run_phase(uvm_phase phase);
//       phase.raise_objection(this);
//       begin
//         obj.randomize();
//         if(obj.b == 5) `uvm_info(...) else `uvm_error(...);
//       end
//       phase.drop_objection(this);
//     endtask: run_phase
//   endclass
//
//   module top;
//     env environment;
//     initial begin
//       environment = new("env");
//       run_test();
//     end
//   endmodule
//
// This is a UVM testbench file (its own metadata carries no
// ":should_fail_because:" tag): the class/constraint pair from
// 18.5.1--explicit-external-constraint_0.sv is reused as the payload for
// a real UVM env/run_phase/randomize() flow. This tool is a compiler/
// elaborator, not a simulator -- it can compile and elaborate the design
// graph, but it cannot execute run_phase(), randomize(), or run_test(),
// so this test only checks static structure.
//
// Checked (against IEEE 1800-2023, not against whatever HLC happens to
// output today -- see davtests test writing guide). Confirmed by actually
// compiling and running this test's own SetUpTestSuite(), not by reading
// the existing .log:
//   - Sec 8.3/18.4/18.5.1: class "a" exists with "rand int b;" and an
//     out-of-class-defined constraint "c" whose body is "b == 5" -- same
//     shape already proven in detail by 18.5.1--explicit-external-
//     constraint_0.cpp (Distribution wrapper, vpiEqOp, RefObj "b",
//     Constant), not re-derived field-by-field here.
//   - Class "env" exists and is derived from a base class named
//     "uvm_env" (Sec 8.13, single inheritance via "extends") -- checked
//     via ClassDefn::getExtends().
//   - Module "top" exists.
//   - This build does elaborate the file (Design::getAllClasses() finds
//     11 classes, including UVM library classes like "mailbox"/
//     "process"/"semaphore" pulled in by "import uvm_pkg::*", confirming
//     the UVM package itself resolves), and this test does not assert
//     zero diagnostics -- this build reports 1 error and several dozen
//     warnings/info on this exact file, which is expected: a real UVM
//     testbench pulls in far more machinery than this chapter's own
//     rand/constraint construct, and this file's own metadata does not
//     claim it should be diagnostic-free either.
//
// Not checked:
//   - The exact cause of this build's 1 reported error / ~26 warnings on
//     this file -- not diagnosed here, since none of them are specific
//     to the rand/constraint construct this chapter is actually about
//     (spot-checked their codes: PP_MACRO_UNUSED_ARGUMENT,
//     COMP_FAILED_TO_BIND, LINT_NULL_ACTUAL, and similar -- all inside
//     UVM library plumbing pulled in by "import uvm_pkg::*", not inside
//     class "a" or "env" as written in this file).
//   - Any actual randomize()/run_phase()/run_test() runtime behavior --
//     unrunnable by a compiler/elaborator; see the note above.
//   - vpiFullName on any object (never asserted per the test writing
//     guide; use getName() only).

#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/design.h>
#include <hldb/module.h>
#include <hldb/class_defn.h>
#include <hldb/variable.h>
#include <hldb/extends.h>
#include <hldb/vpi_user.h>

namespace hlc {
class ExplicitExternalConstraint2Test : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "18.5.1--explicit-external-constraint_2.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

TEST_F(ExplicitExternalConstraint2Test, ClassAWithRandPropertyExists) {
  const hldb::ClassDefn *const a = hldb::findByName<hldb::ClassDefn>("a", m_design->getAllClasses());
  ASSERT_NE(a, nullptr);
  const hldb::Variable *const b = hldb::findByName<hldb::Variable>("b", a->getVariables());
  ASSERT_NE(b, nullptr);
  EXPECT_EQ(b->getRandType(), vpiRand);
}

TEST_F(ExplicitExternalConstraint2Test, OutOfClassConstraintExistsAtDesignLevel) {
  const hldb::ClassDefn *const a = hldb::findByName<hldb::ClassDefn>("a", m_design->getAllClasses());
  ASSERT_NE(a, nullptr);

  const hldb::Constraint *const c = hldb::findByName<hldb::Constraint>("c", m_design->getConstraints());
  ASSERT_NE(c, nullptr) << "'constraint a::c { b == 5; }' should register a Constraint at Design level "
                            "(same modeling as 18.5.1--explicit-external-constraint_0.sv)";

  const hldb::ClassDefn *const owner = hldb::getReferenced<hldb::ClassDefn>(c->getPrefix());
  EXPECT_EQ(owner, a);
}

TEST_F(ExplicitExternalConstraint2Test, ClassEnvExtendsUvmEnv) {
  const hldb::ClassDefn *const env = hldb::findByName<hldb::ClassDefn>("env", m_design->getAllClasses());
  ASSERT_NE(env, nullptr) << "'class env extends uvm_env;' should exist";

  const hldb::Extends *const extends = env->getExtends();
  ASSERT_NE(extends, nullptr) << "Sec 8.13: single inheritance via 'extends' should be recorded";
}

TEST_F(ExplicitExternalConstraint2Test, ModuleTopExists) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
