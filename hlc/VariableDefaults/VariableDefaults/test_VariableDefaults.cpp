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

// Variable properties that always have a value (tests/VariableDefaults/dut.sv):
//
//   IEEE 1800-2023 Sec 37.17 details 15/22  vpiRandType is one of vpiRand, vpiRandC or
//                                           vpiNotRand.
//   IEEE 1800-2023 Sec 37.17 detail 24      vpiVisibility is vpiPublicVis for a class member
//                                           that is not local/protected, or a non-member.

#include <hlc/Common/Session.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/class_defn.h>
#include <hldb/design.h>
#include <hldb/module.h>
#include <hldb/sv_vpi_user.h>
#include <hldb/task.h>
#include <hldb/variable.h>

#include <gtest/gtest.h>

namespace hlc {

class VariableDefaultsTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "VariableDefaults.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  const hldb::Module *top() const { return hldb::findByName<hldb::Module>("top", m_design->getAllModules()); }

  const hldb::Variable *moduleVariable(std::string_view name) const {
    const hldb::Module *const m = top();
    if ((m == nullptr) || (m->getVariables() == nullptr)) return nullptr;
    return hldb::findByName<hldb::Variable>(name, m->getVariables());
  }

  const hldb::Variable *taskVariable(std::string_view name) const {
    const hldb::Module *const m = top();
    if ((m == nullptr) || (m->getTaskFuncs() == nullptr)) return nullptr;
    const hldb::Task *const t = hldb::findByName<hldb::Task>("t", m->getTaskFuncs());
    if ((t == nullptr) || (t->getVariables() == nullptr)) return nullptr;
    return hldb::findByName<hldb::Variable>(name, t->getVariables());
  }

  const hldb::Variable *classVariable(std::string_view name) const {
    const hldb::ClassDefn *const c = hldb::findByName<hldb::ClassDefn>("cls", m_design->getAllClasses());
    if ((c == nullptr) || (c->getVariables() == nullptr)) return nullptr;
    return hldb::findByName<hldb::Variable>(name, c->getVariables());
  }
};

TEST_F(VariableDefaultsTest, ModuleVariablesAreNotRand) {
  for (std::string_view name : {"mod_logic", "mod_int"}) {
    const hldb::Variable *const v = moduleVariable(name);
    ASSERT_NE(v, nullptr) << name;
    EXPECT_EQ(v->getRandType(), vpiNotRand) << name;
  }
}

TEST_F(VariableDefaultsTest, TaskLocalVariableIsNotRand) {
  const hldb::Variable *const v = taskVariable("task_local");
  ASSERT_NE(v, nullptr);
  EXPECT_EQ(v->getRandType(), vpiNotRand);
}

TEST_F(VariableDefaultsTest, ClassPropertyRandTypes) {
  const hldb::Variable *const r = classVariable("r");
  ASSERT_NE(r, nullptr);
  EXPECT_EQ(r->getRandType(), vpiRand);
  const hldb::Variable *const rc = classVariable("rc");
  ASSERT_NE(rc, nullptr);
  EXPECT_EQ(rc->getRandType(), vpiRandC);
  const hldb::Variable *const plain = classVariable("plain");
  ASSERT_NE(plain, nullptr);
  EXPECT_EQ(plain->getRandType(), vpiNotRand);
}

TEST_F(VariableDefaultsTest, NonMemberVariablesArePublic) {
  for (std::string_view name : {"mod_logic", "mod_int"}) {
    const hldb::Variable *const v = moduleVariable(name);
    ASSERT_NE(v, nullptr) << name;
    EXPECT_EQ(v->getVisibility(), vpiPublicVis) << name;
  }
  const hldb::Variable *const t = taskVariable("task_local");
  ASSERT_NE(t, nullptr);
  EXPECT_EQ(t->getVisibility(), vpiPublicVis);
}

TEST_F(VariableDefaultsTest, ClassPropertyVisibility) {
  const hldb::Variable *const plain = classVariable("plain");
  ASSERT_NE(plain, nullptr);
  EXPECT_EQ(plain->getVisibility(), vpiPublicVis);
  const hldb::Variable *const loc = classVariable("loc");
  ASSERT_NE(loc, nullptr);
  EXPECT_EQ(loc->getVisibility(), vpiLocalVis);
  const hldb::Variable *const prot = classVariable("prot");
  ASSERT_NE(prot, nullptr);
  EXPECT_EQ(prot->getVisibility(), vpiProtectedVis);
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
