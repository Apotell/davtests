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

// Source under test: tests/Google/chapter-18/18.12.1--adding-constraints-to-scope-variables_1.sv
//
//   import uvm_pkg::*;
//   `include "uvm_macros.svh"
//
//   class a;
//       function int do_randomize(int y);
//           int x, success;
//           success = std::randomize(x) with {x > 0; x < y;};
//           return success;
//       endfunction
//   endclass
//
//   class env extends uvm_env;
//     a obj = new;
//     int ret, y = 20;
//     function new(string name, uvm_component parent = null); super.new(name, parent); endfunction
//     task run_phase(uvm_phase phase);
//       phase.raise_objection(this);
//       begin
//         ret = obj.do_randomize(y);
//         if(ret == 1) `uvm_info(...) else `uvm_error(...);
//       end
//       phase.drop_objection(this);
//     endtask: run_phase
//   endclass
//
//   module top;
//     env environment;
//     initial begin environment = new("env"); run_test(); end
//   endmodule
//
// Same class "a"/"do_randomize" already checked in detail by the sibling
// 18.12.1--adding-constraints-to-scope-variables_0.sv, wrapped in a UVM
// env/run_phase/run_test() shell. This tool is a compiler/elaborator,
// not a simulator, so this test only checks static structure.
//
// Checked (against IEEE 1800-2023, not against whatever HLC happens to
// output today -- see davtests test writing guide):
//   - Class "a" still has its "do_randomize" method with an "int y"
//     formal argument (same shape as the sibling "_0" file; not
//     re-derived field-by-field here).
//   - Class "env" is derived from a base class named "uvm_env" (Sec
//     8.13) -- checked via ClassDefn::getExtends().
//   - Class "env" has its own property "y" (a plain int, initialized in
//     source to 20 -- not asserted here, see "Not checked") and a method
//     "run_phase" (found via ClassDefn::getMethods(), since it has a
//     task body).
//   - Module "top" exists.
//
// Not checked:
//   - The declaration-time initializer (20) of env::y, and any statement
//     tree inside run_phase() (e.g. the "ret = obj.do_randomize(y);"
//     call and its if/else) -- not investigated this session (build was
//     skipped per request).
//   - Any actual randomize()/run_phase()/run_test() runtime behavior --
//     unrunnable by a compiler/elaborator.
//   - vpiFullName on any object (never asserted per the test writing
//     guide; use getName() only).

#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/design.h>
#include <hldb/module.h>
#include <hldb/class_defn.h>
#include <hldb/variable.h>
#include <hldb/function.h>
#include <hldb/task.h>
#include <hldb/io_decl.h>
#include <hldb/int_typespec.h>
#include <hldb/extends.h>
#include <hldb/vpi_user.h>

namespace hlc {
class AddingConstraintsToScopeVariables1Test : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "18.12.1--adding-constraints-to-scope-variables_1.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

TEST_F(AddingConstraintsToScopeVariables1Test, ClassAStillHasDoRandomizeWithArgY) {
  const hldb::ClassDefn *const a = hldb::findByName<hldb::ClassDefn>("a", m_design->getAllClasses());
  ASSERT_NE(a, nullptr);

  const hldb::Function *const fn = hldb::findByName<hldb::Function>("do_randomize", a->getMethods());
  ASSERT_NE(fn, nullptr);
  ASSERT_NE(fn->getIODecls(), nullptr);
  EXPECT_NE(hldb::findByName<hldb::IODecl>("y", fn->getIODecls()), nullptr);
}

TEST_F(AddingConstraintsToScopeVariables1Test, ClassEnvExtendsUvmEnvAndHasPropertyYAndRunPhase) {
  const hldb::ClassDefn *const env = hldb::findByName<hldb::ClassDefn>("env", m_design->getAllClasses());
  ASSERT_NE(env, nullptr);
  EXPECT_NE(env->getExtends(), nullptr) << "Sec 8.13: 'class env extends uvm_env;' should record its base class";

  const hldb::Variable *const y = hldb::findByName<hldb::Variable>("y", env->getVariables());
  ASSERT_NE(y, nullptr) << "'int ret, y = 20;' declares 'y' on env";
  const hldb::IntTypespec *const yTs = hldb::getTypespec<hldb::IntTypespec>(y);
  ASSERT_NE(yTs, nullptr);
  EXPECT_TRUE(yTs->getSigned());

  const hldb::Task *const runPhase = hldb::findByName<hldb::Task>("run_phase", env->getMethods());
  ASSERT_NE(runPhase, nullptr) << "'task run_phase(uvm_phase phase);' should be a method with a body";
}

TEST_F(AddingConstraintsToScopeVariables1Test, ModuleTopExists) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
