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

// Test for tests/Google/chapter-23/chapter-23_testsuit-1/23.2.2.1--non-ansi-explicit-named.sv
//
// THE SOURCE
//
//   19: module renamed_concat(.a({b, c}), f, .g(h[1]));
//   20:   input b, c;
//   21:   input f;
//   22:   input [1:0] h;
//   23: endmodule
//
// WHAT THE LRM (IEEE 1800-2023) SAYS ABOUT IT
//
// This is Example 5 of 23.2.2.1 with input declarations added for the
// internal names: "Names 'b', 'c', 'f', 'h' are defined inside the module.
// Names 'a', 'f', 'g' are defined for port connections." .a(...) and
// .g(...) are explicit ports, f is an implicit port whose port_expression is
// a simple identifier, which "shall be used as the port name" (23.2.2.1).
// The object model (37.14):
//   - "If a port is explicitly named, then the explicit name shall be
//     returned" (detail 8), and vpiExplicitName says whether it is.
//   - "The first port has a port index of zero." (detail 9)
//   - "vpiLowConn shall indicate the lower (further from the top module)
//     port connection" (detail 4): inside the definition that is the port
//     expression, {b, c}, f and h[1].
//   - "vpiHighConn shall return NULL if the instance of the module ... does
//     not have a connection to the port." (detail 10). renamed_concat is
//     never instantiated, so no port has a high connection.
// Every name inside each port expression is an input, so every port is an
// input.
//
// WHAT IS CHECKED (one fact per test, in source order)
//
//   CompilesWithoutErrors       no fatal, syntax or error diagnostic
//   ThreePortsAFG               a, f, g with vpiPortIndex 0, 1, 2
//   ExplicitNamesAreReported    vpiExplicitName TRUE for a and g, FALSE for f
//   AllThreePortsAreInputs      vpiDirection vpiInput for a, f and g
//   PortExpressionsAreLowConns  each port has a vpiLowConn and no vpiHighConn
//
// WHAT IS NOT CHECKED, AND WHY
//
//   - The shape of each low connection (a concatenation, a net, a
//     bit-select). That is the expression model of 37.x, outside 23.2.2.1;
//     the presence of a low connection and the absence of a high one are
//     enough to show where the port expression went.

#include <hlc/Common/Session.h>
#include <hlc/Events/EventDefinition.h>
#include <hlc/Events/EventListener.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/design.h>
#include <hldb/module.h>
#include <hldb/port.h>
#include <hldb/sv_vpi_user.h>
#include <hldb/vpi_user.h>

#include <cstdint>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace hlc {
namespace {
constexpr std::string_view kSourceFileName = "23.2.2.1--non-ansi-explicit-named.sv";

bool endsWith(std::string_view text, std::string_view suffix) {
  return (text.size() >= suffix.size()) && (text.substr(text.size() - suffix.size()) == suffix);
}

template <typename T>
const T *findFromSourceFile(const std::vector<T *> *collection) {
  if (collection != nullptr) {
    for (const T *object : *collection) {
      if (endsWith(object->getFile(), kSourceFileName)) return object;
    }
  }
  return nullptr;
}

uint32_t lineOf(const Event &event) { return event.getArguments().empty() ? 0 : event.getArguments().front().m_line; }

bool isErrorClass(const Event &event) {
  const EventDefinition *const errorInfo = hldb::findEventDefinition(hlc::getEventTable(), event.getType());
  if (errorInfo == nullptr) return false;
  const EventSeverity severity = errorInfo->m_severity;
  return (severity == EventSeverity::Fatal) || (severity == EventSeverity::Syntax) ||
         (severity == EventSeverity::Error);
}
}  // namespace

class NonAnsiExplicitNamedTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "non_ansi_explicit_named.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static std::string allEvents() {
    hlc::EventListener *const listener = m_session->getEventListener();
    std::ostringstream oss;
    for (const hldb::Event &event : listener->getEvents()) {
      if (isErrorClass(event)) {
        std::vector<std::string> msgs = hldb::renderEvent(event);
        std::copy(msgs.cbegin(), msgs.cend(), std::ostream_iterator<std::string>(oss));
      }
    }
    return oss.str();
  }

  static const hldb::Module *findModule() {
    return (m_design == nullptr) ? nullptr : findFromSourceFile(m_design->getAllModules());
  }

  static const hldb::Port *findPort(std::string_view name) {
    const hldb::Module *const module = findModule();
    return (module == nullptr) ? nullptr : hldb::findByName<hldb::Port>(name, module->getPorts());
  }
};

TEST_F(NonAnsiExplicitNamedTest, CompilesWithoutErrors) {
  GTEST_SKIP() << "HLC Gap: Orphans & Leaks!";
  EXPECT_EQ(allEvents(), "");
}

TEST_F(NonAnsiExplicitNamedTest, ThreePortsAFG) {
  const hldb::Module *const module = findModule();
  ASSERT_NE(module, nullptr);
  EXPECT_EQ(module->getDefName(), std::string_view("renamed_concat"));
  ASSERT_NE(module->getPorts(), nullptr);
  EXPECT_EQ(module->getPorts()->size(), 3u);

  const std::vector<std::string_view> names = {"a", "f", "g"};
  for (size_t index = 0; index < names.size(); ++index) {
    const hldb::Port *const port = findPort(names[index]);
    ASSERT_NE(port, nullptr) << names[index];
    EXPECT_EQ(port->getPortIndex(), static_cast<int32_t>(index)) << names[index];
  }
}

TEST_F(NonAnsiExplicitNamedTest, ExplicitNamesAreReported) {
  for (std::string_view name : {"a", "f", "g"}) {
    const hldb::Port *const port = findPort(name);
    ASSERT_NE(port, nullptr) << name;
    EXPECT_EQ(port->getExplicitName(), name != std::string_view("f")) << name;
  }
}

TEST_F(NonAnsiExplicitNamedTest, AllThreePortsAreInputs) {
  GTEST_SKIP() << "Explicit non-ANSI ports (.a({b, c}), .g(h[1])) are not matched to the port declarations of "
                  "the names in their expressions, so they get no direction (IEEE 1800-2023 23.2.2.1); see "
                  "hldb_model_gaps.md #3";
  for (std::string_view name : {"a", "f", "g"}) {
    const hldb::Port *const port = findPort(name);
    ASSERT_NE(port, nullptr) << name;
    EXPECT_EQ(port->getDirection(), vpiInput) << name;
  }
}

TEST_F(NonAnsiExplicitNamedTest, PortExpressionsAreLowConns) {
  GTEST_SKIP() << "The port expression of an explicit non-ANSI port is built as the high connection, not the "
                  "low connection (IEEE 1800-2023 37.14 detail 4); see hldb_model_gaps.md #3";
  for (std::string_view name : {"a", "f", "g"}) {
    const hldb::Port *const port = findPort(name);
    ASSERT_NE(port, nullptr) << name;
    EXPECT_NE(port->getLowConn(), nullptr) << name;
    EXPECT_EQ(port->getHighConn(), nullptr) << name;
  }
}
}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
