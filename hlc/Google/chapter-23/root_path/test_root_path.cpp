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

// Test for tests/Google/chapter-23/23.3.1--root-path.sv
//
// THE SOURCE
//
//   20: module leaf;
//   21:   logic x;
//   22: endmodule
//   23:
//   24: module B;
//   25:   leaf C ();
//   26: endmodule
//   27:
//   28: module A;
//   29:   B B ();
//   30: endmodule
//   31:
//   32: module A2;
//   33:   B B ();
//   34: endmodule
//   35:
//   36: module top;
//   37:   logic l, r;
//   38:   A2 A ();
//   39:   assign l = A.B.C.x;
//   40:   assign r = $root.A.B.C.x;
//   41: endmodule
//
// WHAT THE LRM (IEEE 1800-2023) SAYS ABOUT IT
//
// "The name $root is used to unambiguously refer to a top-level instance or
// to an instance path starting from the root of the instantiation tree."
// (23.3.1). "A.B.C can mean the local A.B.C or the top-level A.B.C
// (assuming there is an instance A that contains an instance B at both the
// top level and in the current module). The ambiguity is resolved by giving
// priority to the local scope and thereby preventing access to the
// top-level path. $root allows explicit access to the top level in those
// cases" (23.3.1). Module A is never instantiated, so it is a top-level
// instance named A; top also holds a local instance named A (of A2). So on
// line 39 A.B.C.x is the x in top.A.B.C, and on line 40 $root.A.B.C.x is
// the x in the top-level A.B.C. Each instance has its own x, and an object's
// hierarchical name (37.10) chains its parents' names. The references are reached
// through the continuous assignments of the top instance (37.5, relation
// cont assign).
//
// THE MODEL
//
// HLC's graph is a definition model: it holds everything the source says,
// so that an instance model can be built from it. An instantiation is an
// unelaborated instance (Module::getRefInstances()) bound through its
// typespec to the definition it names, with its parameter and port
// connections bound to that definition's parameters and ports. Those
// bindings are checked unconditionally. The top-level instances and the
// instance tree exist only in an elaborated model (23.3); those tests skip
// unless m_design->getElaborated(), so they run once HLC elaborates.
//
// WHAT IS CHECKED (one fact per test, in source order)
//
//   CompilesWithoutErrors          no fatal, syntax or error event
//   InstantiationsAreBound         top's A is bound to A2, A's and A2's B to
//                                  B, B's C to leaf
//   AAndTopAreNeverInstantiated    no instantiation names A or top (23.3.1)
//   LocalPathBindsThroughTopsA     line 39's A.B.C.x binds A to top's
//                                  instance A (an A2), then B, C and leaf's
//                                  x
//   RootPathBindsThroughModuleA    line 40's $root.A.B.C.x binds $root to the
//                                  design, A to the module A, then A's B, C
//                                  and leaf's x
//   TopLevelInstancesAreAAndTop    elaborated model only: the top-level
//                                  instances are exactly A and top
//   LocalPathIsTopABCX             elaborated model only: line 39's
//                                  reference is top.A.B.C.x
//   RootPathIsABCX                 elaborated model only: line 40's
//                                  reference is A.B.C.x
//
// SKIPPED (GTEST_SKIP; the real assertions stay below the skip)
//
//   TopLevelInstancesAreAAndTop, LocalPathIsTopABCX, RootPathIsABCX: HLC
//   builds a definition model only, so there are no top-level instances
//   and no instance tree.

#include <hlc/Common/Session.h>
#include <hlc/Events/EventDefinition.h>
#include <hlc/Events/EventListener.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/case_item.h>
#include <hldb/cont_assign.h>
#include <hldb/design.h>
#include <hldb/gen_block.h>
#include <hldb/gen_case.h>
#include <hldb/gen_for.h>
#include <hldb/gen_if.h>
#include <hldb/gen_if_else.h>
#include <hldb/gen_region.h>
#include <hldb/module.h>
#include <hldb/module_typespec.h>
#include <hldb/ref_instance.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/variable.h>

#include <algorithm>
#include <cstdint>
#include <iterator>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace hlc {
namespace {
// The reason the instance-model tests are skipped while HLC builds a definition model only.
constexpr std::string_view kNotElaborated =
    "HLC builds a definition model only, with no top-level instances or instance tree to check (IEEE 1800-2023 "
    "23.3.1)";

// True for an error-class event: fatal, syntax or error severity.
bool isErrorClass(const Event &event) {
  const EventDefinition *const eventDefinition = hldb::findEventDefinition(hlc::getEventTable(), event.getType());
  if (eventDefinition == nullptr) return false;
  const EventSeverity severity = eventDefinition->m_severity;
  return (severity == EventSeverity::Fatal) || (severity == EventSeverity::Syntax) ||
         (severity == EventSeverity::Error);
}

// One event as hldb::renderEvent() prints it, one line per output line, so that a
// failing test shows what was reported and where.
std::string describeEvent(const Event &event) {
  std::ostringstream oss;
  const std::vector<std::string> lines = hldb::renderEvent(event);
  std::copy(lines.cbegin(), lines.cend(), std::ostream_iterator<std::string>(oss, "\n"));
  return oss.str();
}

// The objects in 'collection'; none for a null collection.
template <typename T>
std::vector<const T *> toVector(const std::vector<T *> *collection) {
  std::vector<const T *> result;
  if (collection == nullptr) return result;
  for (const T *object : *collection) result.emplace_back(object);
  return result;
}

// The names of 'objects', comma-separated, sorted.
template <typename T>
std::string namesOf(const std::vector<const T *> &objects) {
  std::vector<std::string> names;
  for (const T *object : objects) names.emplace_back(object->getName());
  std::sort(names.begin(), names.end());
  std::string result;
  for (const std::string &name : names) {
    if (!result.empty()) result += ", ";
    result += name;
  }
  return result;
}
// The name of the definition an unelaborated instance is bound to (the module its typespec names), or "" when it
// is not bound to a module.
std::string_view boundDefinition(const hldb::RefInstance *instance) {
  if ((instance == nullptr) || (instance->getTypespec() == nullptr)) return "";
  const hldb::ModuleTypespec *const definition = instance->getTypespec()->getActual<hldb::ModuleTypespec>();
  return (definition == nullptr) ? std::string_view() : definition->getDefName();
}

// The hierarchical path of 'object' made of its getName() and those of its parents up to the design, e.g.
// "top.A.B.C.x" (23.6).
std::string instancePath(const hldb::Any *object) {
  std::string path;
  for (const hldb::Any *node = object; (node != nullptr) && (node->getAnyType() != hldb::AnyType::Design);
       node = node->getParent()) {
    path = path.empty() ? std::string(node->getName()) : std::string(node->getName()) + "." + path;
  }
  return path;
}

// Adds to 'instances' the unelaborated instances that 'object' declares: a definition's own and those in its
// generate constructs, at any depth.
void collectRefInstances(const hldb::Any *object, std::vector<const hldb::RefInstance *> &instances) {
  if (object == nullptr) return;
  if (const hldb::RefInstance *const instance = any_cast<hldb::RefInstance>(object)) {
    instances.emplace_back(instance);
  } else if (const hldb::Module *const module = any_cast<hldb::Module>(object)) {
    if (module->getRefInstances() != nullptr) {
      for (const hldb::RefInstance *child : *module->getRefInstances()) collectRefInstances(child, instances);
    }
    if (module->getGenStmts() != nullptr) {
      for (const hldb::Any *statement : *module->getGenStmts()) collectRefInstances(statement, instances);
    }
  } else if (const hldb::GenRegion *const region = any_cast<hldb::GenRegion>(object)) {
    if (region->getStmts() != nullptr) {
      for (const hldb::Any *statement : *region->getStmts()) collectRefInstances(statement, instances);
    }
  } else if (const hldb::GenBlock *const block = any_cast<hldb::GenBlock>(object)) {
    if (block->getStmts() != nullptr) {
      for (const hldb::Any *statement : *block->getStmts()) collectRefInstances(statement, instances);
    }
  } else if (const hldb::GenIf *const genIf = any_cast<hldb::GenIf>(object)) {
    collectRefInstances(genIf->getStmt(), instances);
  } else if (const hldb::GenIfElse *const genIfElse = any_cast<hldb::GenIfElse>(object)) {
    collectRefInstances(genIfElse->getStmt(), instances);
    collectRefInstances(genIfElse->getElseStmt(), instances);
  } else if (const hldb::GenFor *const genFor = any_cast<hldb::GenFor>(object)) {
    collectRefInstances(genFor->getStmt(), instances);
  } else if (const hldb::GenCase *const genCase = any_cast<hldb::GenCase>(object)) {
    if (genCase->getCaseItems() != nullptr) {
      for (const hldb::CaseItem *item : *genCase->getCaseItems()) collectRefInstances(item->getStmt(), instances);
    }
  }
}

// The definitions that no module instantiation statement names (23.3.1), comma-separated, sorted.
std::string neverInstantiated(const hldb::Design *design) {
  if ((design == nullptr) || (design->getAllModules() == nullptr)) return "<no design>";
  std::vector<const hldb::RefInstance *> instances;
  for (const hldb::Module *module : *design->getAllModules()) collectRefInstances(module, instances);
  std::vector<std::string> names;
  for (const hldb::Module *module : *design->getAllModules()) {
    const std::string_view name = module->getDefName();
    const bool named = std::any_of(instances.cbegin(), instances.cend(), [name](const hldb::RefInstance *instance) {
      return boundDefinition(instance) == name;
    });
    if (!named) names.emplace_back(name);
  }
  std::sort(names.begin(), names.end());
  std::string result;
  for (const std::string &name : names) {
    if (!result.empty()) result += ", ";
    result += name;
  }
  return result;
}

// What a path element of a hierarchical reference is bound to, as text: "$root" for the design, "module <name>" for
// a definition, "<name>@<parent>-><definition>" for an unelaborated instance declared in the definition <parent>,
// "<name>@<parent>" for anything else, or "<unbound>".
std::string bindingOf(const hldb::Any *actual) {
  if (actual == nullptr) return "<unbound>";
  if (actual->getAnyType() == hldb::AnyType::Design) return "$root";
  if (const hldb::Module *const module = any_cast<hldb::Module>(actual)) {
    return "module " + std::string(module->getDefName());
  }
  const hldb::Module *const parent = any_cast<hldb::Module>(actual->getParent());
  const std::string where = (parent == nullptr) ? std::string("<no module>") : std::string(parent->getDefName());
  if (const hldb::RefInstance *const instance = any_cast<hldb::RefInstance>(actual)) {
    return std::string(instance->getName()) + "@" + where + "->" + std::string(boundDefinition(instance));
  }
  return std::string(actual->getName()) + "@" + where;
}

// The path elements of a hierarchical reference, each as bindingOf() gives it, comma-separated.
std::string pathBindings(const hldb::RefObj *reference) {
  if ((reference == nullptr) || (reference->getPathElems() == nullptr)) return "<no path>";
  std::string result;
  for (const hldb::Any *element : *reference->getPathElems()) {
    const hldb::RefObj *const part = any_cast<hldb::RefObj>(element);
    if (!result.empty()) result += ", ";
    result += bindingOf((part == nullptr) ? nullptr : part->getActual());
  }
  return result;
}

}  // namespace

class RootPathTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "root_path.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static std::string allEvents() {
    hlc::EventListener *const listener = m_session->getEventListener();
    std::ostringstream oss;
    for (const Event &event : listener->getEvents()) {
      if (isErrorClass(event)) oss << describeEvent(event);
    }
    return oss.str();
  }

  // The definition of the module named 'name', or nullptr.
  static const hldb::Module *findModule(std::string_view name) {
    if (m_design == nullptr) return nullptr;
    return hldb::findByDefName<hldb::Module>(name, m_design->getAllModules());
  }

  // The unelaborated instance 'name' that the definition 'parent' declares, or nullptr.
  static const hldb::RefInstance *findRefInstance(std::string_view parent, std::string_view name) {
    const hldb::Module *const module = findModule(parent);
    return (module == nullptr) ? nullptr : hldb::findByName<hldb::RefInstance>(name, module->getRefInstances());
  }

  // The right-hand side of the definition top's "assign <lhsName> = ...", or nullptr.
  static const hldb::RefObj *rhsOfAssignTo(std::string_view lhsName) {
    const hldb::Module *const top = findModule("top");
    if ((top == nullptr) || (top->getContAssigns() == nullptr)) return nullptr;
    for (const hldb::ContAssign *assign : *top->getContAssigns()) {
      if ((assign->getLhs() != nullptr) && (assign->getLhs()->getName() == lhsName)) {
        return assign->getRhs<hldb::RefObj>();
      }
    }
    return nullptr;
  }

  // The top-level instances (37.5 detail 1); there are some only in an elaborated model.
  static std::vector<const hldb::Module *> topInstances() {
    std::vector<const hldb::Module *> result;
    if ((m_design == nullptr) || (m_design->getTopModules() == nullptr)) return result;
    for (const hldb::Module *module : *m_design->getTopModules()) result.emplace_back(module);
    return result;
  }

  // The instance path (instancePath()) of the variable that the
  // right-hand side of the top instance's 'assign <lhsName> = ...' refers to,
  // or a placeholder that says what is missing.
  static std::string rhsTargetPath(std::string_view lhsName) {
    const hldb::Module *const top = hldb::findByDefName<hldb::Module>("top", m_design->getTopModules());
    if (top == nullptr) return "<no top-level instance top>";
    for (const hldb::ContAssign *assign : toVector(top->getContAssigns())) {
      if ((assign->getLhs() == nullptr) || (assign->getLhs()->getName() != lhsName)) continue;
      const hldb::RefObj *const reference = assign->getRhs<hldb::RefObj>();
      if (reference == nullptr) return "<right-hand side is not a reference>";
      if (reference->getActual() == nullptr) return "<reference is not bound>";
      const hldb::Variable *const variable = any_cast<hldb::Variable>(reference->getActual());
      if (variable == nullptr) return "<reference is not to a variable>";
      return instancePath(variable);
    }
    return "<no continuous assignment to " + std::string(lhsName) + ">";
  }
};

TEST_F(RootPathTest, CompilesWithoutErrors) { EXPECT_EQ(allEvents(), ""); }

// "A2 A ();" in top, "B B ();" in A and in A2, "leaf C ();" in B.
TEST_F(RootPathTest, InstantiationsAreBound) {
  EXPECT_EQ(boundDefinition(findRefInstance("top", "A")), std::string_view("A2"));
  EXPECT_EQ(boundDefinition(findRefInstance("A", "B")), std::string_view("B"));
  EXPECT_EQ(boundDefinition(findRefInstance("A2", "B")), std::string_view("B"));
  EXPECT_EQ(boundDefinition(findRefInstance("B", "C")), std::string_view("leaf"));
}

// "Top-level modules are modules that ... do not appear in any module instantiation statement" (23.3.1).
TEST_F(RootPathTest, AAndTopAreNeverInstantiated) { EXPECT_EQ(neverInstantiated(m_design), "A, top"); }

// "A.B.C.x" in top starts at top's own instance A, an instance of A2 (23.3.1, 23.6).
TEST_F(RootPathTest, LocalPathBindsThroughTopsA) {
  const hldb::RefObj *const reference = rhsOfAssignTo("l");
  ASSERT_NE(reference, nullptr);
  EXPECT_EQ(pathBindings(reference), "A@top->A2, B@A2->B, C@B->leaf, x@leaf");
}

// "$root.A.B.C.x" starts at the top-level module A, not at top's instance A (23.3.1).
TEST_F(RootPathTest, RootPathBindsThroughModuleA) {
  const hldb::RefObj *const reference = rhsOfAssignTo("r");
  ASSERT_NE(reference, nullptr);
  EXPECT_EQ(pathBindings(reference), "$root, module A, B@A->B, C@B->leaf, x@leaf");
}

TEST_F(RootPathTest, TopLevelInstancesAreAAndTop) {
  ASSERT_NE(m_design, nullptr);
  if (!m_design->getElaborated()) GTEST_SKIP() << kNotElaborated;
  EXPECT_EQ(namesOf(topInstances()), "A, top");
}

TEST_F(RootPathTest, LocalPathIsTopABCX) {
  ASSERT_NE(m_design, nullptr);
  if (!m_design->getElaborated()) GTEST_SKIP() << kNotElaborated;
  EXPECT_EQ(rhsTargetPath("l"), "top.A.B.C.x");
}

TEST_F(RootPathTest, RootPathIsABCX) {
  ASSERT_NE(m_design, nullptr);
  if (!m_design->getElaborated()) GTEST_SKIP() << kNotElaborated;
  EXPECT_EQ(rhsTargetPath("r"), "A.B.C.x");
}
}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
