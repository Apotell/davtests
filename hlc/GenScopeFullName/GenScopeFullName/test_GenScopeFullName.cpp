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

// Tests for GenScopeFullName.hlc (tests/GenScopeFullName/dut.sv):
//
//   module dut();
//      logic [1:0] a;
//
//      for(genvar i = 0; i < 1; i++) begin : gen_modules
//         ibex_counter module_in_genscope(.b(a[i]));
//      end // block: gen_modules
//
//   endmodule // dut
//
//   module ibex_counter(input logic b);
//   endmodule // ibex_counter
//
// Compiled at "-d ast" level (no "-d inst"), so the generate-for loop
// survives as a raw GenFor on 'dut's getGenStmts() (IEEE 1800-2023 Sec
// 27.4 "Generate-loop constructs") rather than being unrolled into
// per-iteration, elaborated GenScopeArray/GenScope objects with
// index-qualified names (e.g. 'gen_modules[0]').
//
// What is nominally under test: the *full hierarchical name* (vpiFullName)
// of a generate scope. Per this repo's ".claude/test_writing_guide.md":
// "Never call getFullName() / assert on vpiFullName. It's a computed
// property that is currently wrong in HLC. Use getName() only." IEEE
// 1800-2023's VPI object model (vpiFullName, defined for scope-derived
// objects as the dot-separated hierarchical path from the top instance
// down to the object, e.g. Annex/clause on VPI properties and Sec 23.6
// "Hierarchical names") requires this property to hold the correct
// dot-separated path (here, at minimum, something like
// "dut.gen_modules"). HLC's Scope::getFullName() is a known-broken,
// project-wide computed property (not specific to generate scopes), so the
// dedicated fullName assertion below is skipped rather than locking in
// wrong output; the remaining tests exercise the same construct via
// getName() instead, which is well-defined and known-correct.
//
// No .log file was consulted; accessor names were confirmed against the
// real hldb headers under
// E:\Davenche\davtests\davtests_02\build\include\hldb.

#include <hlc/Common/Session.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/design.h>
#include <hldb/gen_for.h>
#include <hldb/module.h>
#include <hldb/ref_obj.h>
#include <hldb/vpi_user.h>

namespace hlc {

class GenScopeFullNameTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "GenScopeFullName.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getModule(std::string_view name) {
    return hldb::findByName<hldb::Module>(name, m_design->getAllModules());
  }

  static const hldb::GenFor *findGenFor(const hldb::Module *m) {
    if (m == nullptr || m->getGenStmts() == nullptr) return nullptr;
    for (const hldb::Any *const stmt : *m->getGenStmts()) {
      if (const hldb::GenFor *const gf = any_cast<hldb::GenFor>(stmt)) return gf;
    }
    return nullptr;
  }
};

TEST_F(GenScopeFullNameTest, BothModulesExist) {
  ASSERT_NE(getModule("dut"), nullptr);
  ASSERT_NE(getModule("ibex_counter"), nullptr);
}

// 'for(genvar i = 0; i < 1; i++) begin : gen_modules ... end' -- exactly
// one generate-for on 'dut' (Sec 27.4).
TEST_F(GenScopeFullNameTest, DutHasExactlyOneGenFor) {
  const hldb::Module *const dut = getModule("dut");
  ASSERT_NE(dut, nullptr);
  ASSERT_NE(dut->getGenStmts(), nullptr);
  size_t count = 0u;
  for (const hldb::Any *const stmt : *dut->getGenStmts()) {
    if (any_cast<hldb::GenFor>(stmt) != nullptr) ++count;
  }
  EXPECT_EQ(count, 1u);
}

// The generate-for scope itself is named 'gen_modules' (Sec 27.3/27.4);
// this is the scope whose full hierarchical name is nominally under test.
TEST_F(GenScopeFullNameTest, GenForIsNamedGenModules) {
  const hldb::GenFor *const gf = findGenFor(getModule("dut"));
  ASSERT_NE(gf, nullptr) << "'for (...) begin : gen_modules ... end' not found";
  EXPECT_EQ(gf->getName(), std::string_view("gen_modules"));
}

// 'ibex_counter module_in_genscope(.b(a[i]));' -- one module instance
// declared inside the generate scope, reachable via GenScope::getModules().
TEST_F(GenScopeFullNameTest, ModuleInGenscopeDeclaredInsideGenFor) {
  const hldb::GenFor *const gf = findGenFor(getModule("dut"));
  ASSERT_NE(gf, nullptr);
  ASSERT_NE(gf->getModules(), nullptr) << "'gen_modules' should carry the 'module_in_genscope' instance";
  const hldb::Module *const inst = hldb::findByName<hldb::Module>("module_in_genscope", gf->getModules());
  ASSERT_NE(inst, nullptr);
  EXPECT_EQ(inst->getDefName(), std::string_view("ibex_counter"));
}

// The nominal target of this test: vpiFullName / Scope::getFullName() for
// the 'gen_modules' generate scope. Per the test-writing guide, getFullName
// is a known-broken computed property in HLC -- this assertion is skipped
// rather than locking in incorrect output as "correct".
TEST_F(GenScopeFullNameTest, GenForFullNameIsHierarchicalPath) {
  GTEST_SKIP() << "HLC's Scope::getFullName() / vpiFullName is a known-broken computed "
                   "property (per this repo's .claude/test_writing_guide.md: 'Never call "
                   "getFullName() / assert on vpiFullName... currently wrong in HLC'). Per "
                   "IEEE 1800-2023's VPI object model for hierarchically-scoped objects "
                   "(vpiFullName) and Sec 23.6 'Hierarchical names', the full name of the "
                   "'gen_modules' generate scope should be the dot-separated path from the "
                   "top instance down to it (e.g. 'dut.gen_modules'). Fix pending; getName() "
                   "should be used instead until fixed.";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
