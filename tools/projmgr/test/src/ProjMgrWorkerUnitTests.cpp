/*
 * Copyright (c) 2020-2023 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "ProjMgr.h"
#include "ProjMgrTestEnv.h"

#include "RteFsUtils.h"

#include "CrossPlatformUtils.h"

#include "gtest/gtest.h"

using namespace std;

class ProjMgrWorkerUnitTests : public ProjMgrWorker, public ::testing::Test {
protected:
  ProjMgrWorkerUnitTests() {}
  virtual ~ProjMgrWorkerUnitTests() {}

  void SetCsolutionPacks(CsolutionItem* csolution, std::vector<std::string> packs, std::string targetType);
};

void ProjMgrWorkerUnitTests::SetCsolutionPacks(CsolutionItem* csolution, std::vector<std::string> packs, std::string targetType) {
  ContextItem context;
  static CprojectItem cproject;
  context.cproject = &cproject;
  for (auto& pack : packs) {
    csolution->packs.push_back(PackItem{pack, {}});
  }
  context.csolution = csolution;
  context.type.target = targetType;
  ProjMgrUtils::PushBackUniquely(m_ymlOrderedContexts, targetType);
  m_contexts.insert(std::pair<string, ContextItem>(string(targetType), context));
}

TEST_F(ProjMgrWorkerUnitTests, ProcessToolchain) {
  ToolchainItem expected;
  expected.name = "AC6";
  expected.version = "6.18.0";
  ProjMgrParser parser;
  ContextDesc descriptor;
  const string& filename = testinput_folder + "/TestProject/test.cproject.yml";
  EXPECT_TRUE(parser.ParseCproject(filename, true));
  EXPECT_TRUE(AddContexts(parser, descriptor, filename));
  map<string, ContextItem>* contexts;
  GetContexts(contexts);
  ContextItem context = contexts->begin()->second;
  EXPECT_TRUE(ProcessPrecedences(context));
  EXPECT_TRUE(ProcessToolchain(context));
  EXPECT_EQ(expected.name, context.toolchain.name);
  EXPECT_EQ(expected.version, context.toolchain.version);
}

TEST_F(ProjMgrWorkerUnitTests, ProcessToolchainOptions) {
  struct expectedOutput {
    bool result;
    string options, compiler, version;
  };

  std::map<std::string, expectedOutput> testInput =
  {
    { "", {false, "", "", ""}},
    { "TEST", {true, "", "TEST", "0.0.0"}},
    { "AC6", {true, "AC6", "ARMCC", "6.18.0"}}
  };

  for (auto input : testInput) {
    ContextItem context;
    CsolutionItem csolution;
    context.csolution = &csolution;
    context.compiler = input.first;

    EXPECT_EQ(input.second.result, ProcessToolchain(context));
    EXPECT_EQ(input.second.options, context.targetAttributes["Toptions"]);
    EXPECT_EQ(input.second.compiler, context.targetAttributes["Tcompiler"]);
    EXPECT_EQ(input.second.version, context.toolchain.version);
  }
}

TEST_F(ProjMgrWorkerUnitTests, ProcessDevice) {
  map<string, string> expected = {
    {"Dclock", "10000000"},
    {"Dcore", "Cortex-M0"},
    {"DcoreVersion", "r0p0"},
    {"Dendian", "Configurable"},
    {"Dfpu", "NO_FPU"},
    {"Dmpu", "NO_MPU"},
    {"Dname", "RteTest_ARMCM0"},
    {"Dvendor", "ARM:82"},
    {"Dsecure", "Non-secure"}
  };
  ProjMgrParser parser;
  ContextDesc descriptor;
  const string& filename = testinput_folder + "/TestProject/test.cproject.yml";
  EXPECT_TRUE(parser.ParseCproject(filename, true));
  EXPECT_TRUE(AddContexts(parser, descriptor, filename));
  map<string, ContextItem>* contexts;
  GetContexts(contexts);
  ContextItem context = contexts->begin()->second;
  EXPECT_TRUE(LoadPacks(context));
  EXPECT_TRUE(ProcessPrecedences(context));
  EXPECT_TRUE(ProcessDevice(context));
  for (const auto& expectedAttribute : expected) {
    EXPECT_EQ(expectedAttribute.second, context.targetAttributes[expectedAttribute.first]);
  }
}

TEST_F(ProjMgrWorkerUnitTests, ProcessComponents) {
  set<string> expected = {
    "ARM::Device:Startup&RteTest Startup@2.0.3",
    "ARM::RteTest:CORE@0.1.1",
  };
  ProjMgrParser parser;
  ContextDesc descriptor;
  const string& filename = testinput_folder + "/TestProject/test.cproject.yml";
  EXPECT_TRUE(parser.ParseCproject(filename, true));
  EXPECT_TRUE(AddContexts(parser, descriptor, filename));
  map<string, ContextItem>* contexts;
  GetContexts(contexts);
  ContextItem context = contexts->begin()->second;
  EXPECT_TRUE(LoadPacks(context));
  EXPECT_TRUE(ProcessPrecedences(context));
  EXPECT_TRUE(ProcessDevice(context));
  EXPECT_TRUE(SetTargetAttributes(context, context.targetAttributes));
  EXPECT_TRUE(ProcessComponents(context));
  ASSERT_EQ(expected.size(), context.components.size());
  auto it = context.components.begin();
  for (const auto& expectedComponent : expected) {
    EXPECT_EQ(expectedComponent, (*it++).first);
  }
}

TEST_F(ProjMgrWorkerUnitTests, ProcessComponents_Cvariant1) {
  set<string> expected = {
    "ARM::Device:Test variant@1.1.1",
    "ARM::RteTest:CORE@0.1.1",
  };
  ProjMgrParser parser;
  ContextDesc descriptor;
  // Test exact partial component identifier match without Cvendor
  const string& filename = testinput_folder + "/TestProject/test_component_variant1.cproject.yml";
  EXPECT_TRUE(parser.ParseCproject(filename, true));
  EXPECT_TRUE(AddContexts(parser, descriptor, filename));
  map<string, ContextItem>* contexts;
  GetContexts(contexts);
  ContextItem context = contexts->begin()->second;
  EXPECT_TRUE(LoadPacks(context));
  EXPECT_TRUE(ProcessPrecedences(context));
  EXPECT_TRUE(ProcessDevice(context));
  EXPECT_TRUE(SetTargetAttributes(context, context.targetAttributes));
  EXPECT_TRUE(ProcessComponents(context));
  ASSERT_EQ(expected.size(), context.components.size());
  auto it = context.components.begin();
  for (const auto& expectedComponent : expected) {
    EXPECT_EQ(expectedComponent, (*it++).first);
  }
}

TEST_F(ProjMgrWorkerUnitTests, ProcessComponents_Cvariant2) {
  set<string> expected = {
    "ARM::Device:Test variant@1.1.1",
    "ARM::RteTest:CORE@0.1.1",
  };
  ProjMgrParser parser;
  ContextDesc descriptor;
  // Test exact partial component identifier match with Cvendor
  const string& filename = testinput_folder + "/TestProject/test_component_variant2.cproject.yml";
  EXPECT_TRUE(parser.ParseCproject(filename, true));
  EXPECT_TRUE(AddContexts(parser, descriptor, filename));
  map<string, ContextItem>* contexts;
  GetContexts(contexts);
  ContextItem context = contexts->begin()->second;
  EXPECT_TRUE(LoadPacks(context));
  EXPECT_TRUE(ProcessPrecedences(context));
  EXPECT_TRUE(ProcessDevice(context));
  EXPECT_TRUE(SetTargetAttributes(context, context.targetAttributes));
  EXPECT_TRUE(ProcessComponents(context));
  ASSERT_EQ(expected.size(), context.components.size());
  auto it = context.components.begin();
  for (const auto& expectedComponent : expected) {
    EXPECT_EQ(expectedComponent, (*it++).first);
  }
}

TEST_F(ProjMgrWorkerUnitTests, ProcessComponents_EmptyVariant) {
  set<string> expected = {
    "ARM::RteTest:Dependency:Variant@0.9.9",
  };
  ProjMgrParser parser;
  ContextDesc descriptor;
  // Test exact partial component identifier match with Cvendor
  const string& filename = testinput_folder + "/TestProject/test_component_empty_variant_CM3.cproject.yml";
  EXPECT_TRUE(parser.ParseCproject(filename, true));
  EXPECT_TRUE(AddContexts(parser, descriptor, filename));
  map<string, ContextItem>* contexts;
  GetContexts(contexts);
  ContextItem context = contexts->begin()->second;
  EXPECT_TRUE(LoadPacks(context));
  EXPECT_TRUE(ProcessPrecedences(context));
  EXPECT_TRUE(ProcessDevice(context));
  EXPECT_TRUE(SetTargetAttributes(context, context.targetAttributes));
  EXPECT_TRUE(ProcessComponents(context));
  ASSERT_EQ(expected.size(), context.components.size());
  auto it = context.components.begin();
  for (const auto& expectedComponent : expected) {
    EXPECT_EQ(expectedComponent, (*it++).first);
  }
}

TEST_F(ProjMgrWorkerUnitTests, ProcessComponents_EmptyVariantDefault) {
  set<string> expected = {
    "ARM::RteTest:Dependency:Variant&Compatible@0.9.9",
  };
  ProjMgrParser parser;
  ContextDesc descriptor;
  // Test exact partial component identifier match with Cvendor
  const string& filename = testinput_folder + "/TestProject/test_component_empty_variant_CM0.cproject.yml";
  EXPECT_TRUE(parser.ParseCproject(filename, true));
  EXPECT_TRUE(AddContexts(parser, descriptor, filename));
  map<string, ContextItem>* contexts;
  GetContexts(contexts);
  ContextItem context = contexts->begin()->second;
  EXPECT_TRUE(LoadPacks(context));
  EXPECT_TRUE(ProcessPrecedences(context));
  EXPECT_TRUE(ProcessDevice(context));
  EXPECT_TRUE(SetTargetAttributes(context, context.targetAttributes));
  EXPECT_TRUE(ProcessComponents(context));
  ASSERT_EQ(expected.size(), context.components.size());
  auto it = context.components.begin();
  for (const auto& expectedComponent : expected) {
    EXPECT_EQ(expectedComponent, (*it++).first);
  }
}



TEST_F(ProjMgrWorkerUnitTests, ProcessComponents_Latest_From_MultipleMatches1) {
  set<string> expected = {
    "ARM::Device:Test variant 2@3.3.3",
    "ARM::RteTest:CORE@0.1.1",
  };
  ProjMgrParser parser;
  ContextDesc descriptor;
  // Test multiple component identifier matches, different versions
  const string& filename = testinput_folder + "/TestProject/test_component_latest_match1.cproject.yml";
  EXPECT_TRUE(parser.ParseCproject(filename, true));
  EXPECT_TRUE(AddContexts(parser, descriptor, filename));
  map<string, ContextItem>* contexts;
  GetContexts(contexts);
  ContextItem context = contexts->begin()->second;
  EXPECT_TRUE(LoadPacks(context));
  EXPECT_TRUE(ProcessPrecedences(context));
  EXPECT_TRUE(ProcessDevice(context));
  EXPECT_TRUE(SetTargetAttributes(context, context.targetAttributes));
  EXPECT_TRUE(ProcessComponents(context));
  ASSERT_EQ(expected.size(), context.components.size());
  auto it = context.components.begin();
  for (const auto& expectedComponent : expected) {
    EXPECT_EQ(expectedComponent, (*it++).first);
  }
}

TEST_F(ProjMgrWorkerUnitTests, ProcessComponents_Latest_From_MultipleMatches2) {
  set<string> expected = {
    "ARM::Device:Test variant 2@3.3.3",
    "ARM::RteTest:CORE@0.1.1",
  };
  ProjMgrParser parser;
  ContextDesc descriptor;
  // Test multiple component identifier matches
  const string& filename = testinput_folder + "/TestProject/test_component_latest_match2.cproject.yml";
  EXPECT_TRUE(parser.ParseCproject(filename, true));
  EXPECT_TRUE(AddContexts(parser, descriptor, filename));
  map<string, ContextItem>* contexts;
  GetContexts(contexts);
  ContextItem context = contexts->begin()->second;
  EXPECT_TRUE(LoadPacks(context));
  EXPECT_TRUE(ProcessPrecedences(context));
  EXPECT_TRUE(ProcessDevice(context));
  EXPECT_TRUE(SetTargetAttributes(context, context.targetAttributes));
  EXPECT_TRUE(ProcessComponents(context));
  ASSERT_EQ(expected.size(), context.components.size());
  auto it = context.components.begin();
  for (const auto& expectedComponent : expected) {
    EXPECT_EQ(expectedComponent, (*it++).first);
  }
}

TEST_F(ProjMgrWorkerUnitTests, ProcessComponents_ExactMatch) {
  set<string> expected = {
    "ARM::Device:Test variant 2@2.2.2",
    "ARM::RteTest:CORE@0.1.1",
  };
  ProjMgrParser parser;
  ContextDesc descriptor;
  // Test multiple component identifier matches
  const string& filename = testinput_folder + "/TestProject/test_component_exact_match.cproject.yml";
  EXPECT_TRUE(parser.ParseCproject(filename, true));
  EXPECT_TRUE(AddContexts(parser, descriptor, filename));
  map<string, ContextItem>* contexts;
  GetContexts(contexts);
  ContextItem context = contexts->begin()->second;
  EXPECT_TRUE(LoadPacks(context));
  EXPECT_TRUE(ProcessPrecedences(context));
  EXPECT_TRUE(ProcessDevice(context));
  EXPECT_TRUE(SetTargetAttributes(context, context.targetAttributes));
  EXPECT_TRUE(ProcessComponents(context));
  ASSERT_EQ(expected.size(), context.components.size());
  auto it = context.components.begin();
  for (const auto& expectedComponent : expected) {
    EXPECT_EQ(expectedComponent, (*it++).first);
  }
}

TEST_F(ProjMgrWorkerUnitTests, ProcessComponents_ExactMatch_NotFound) {
  ProjMgrParser parser;
  ContextDesc descriptor;
  // Test multiple component identifier matches
  const string& filename = testinput_folder + "/TestProject/test_component_exact_match_notfound.cproject.yml";
  EXPECT_TRUE(parser.ParseCproject(filename, true));
  EXPECT_TRUE(AddContexts(parser, descriptor, filename));
  map<string, ContextItem>* contexts;
  GetContexts(contexts);
  ContextItem context = contexts->begin()->second;
  EXPECT_TRUE(LoadPacks(context));
  EXPECT_TRUE(ProcessPrecedences(context));
  EXPECT_TRUE(ProcessDevice(context));
  EXPECT_TRUE(SetTargetAttributes(context, context.targetAttributes));
  EXPECT_FALSE(ProcessComponents(context));
}

TEST_F(ProjMgrWorkerUnitTests, ProcessComponents_Highest_Version_Match) {
  set<string> expected = {
    "ARM::Device:Test variant 2@3.3.3",
    "ARM::RteTest:CORE@0.1.1",
  };
  ProjMgrParser parser;
  ContextDesc descriptor;
  // Test multiple component identifier matches
  const string& filename = testinput_folder + "/TestProject/test_component_highest_version_match.cproject.yml";
  EXPECT_TRUE(parser.ParseCproject(filename, true));
  EXPECT_TRUE(AddContexts(parser, descriptor, filename));
  map<string, ContextItem>* contexts;
  GetContexts(contexts);
  ContextItem context = contexts->begin()->second;
  EXPECT_TRUE(LoadPacks(context));
  EXPECT_TRUE(ProcessPrecedences(context));
  EXPECT_TRUE(ProcessDevice(context));
  EXPECT_TRUE(SetTargetAttributes(context, context.targetAttributes));
  EXPECT_TRUE(ProcessComponents(context));
  ASSERT_EQ(expected.size(), context.components.size());
  auto it = context.components.begin();
  for (const auto& expectedComponent : expected) {
    EXPECT_EQ(expectedComponent, (*it++).first);
  }
}

TEST_F(ProjMgrWorkerUnitTests, ProcessComponents_Equal_Version_Match) {
  set<string> expected = {
    "ARM::Device:Test variant 2@3.3.3",
    "ARM::RteTest:CORE@0.1.1",
  };
  ProjMgrParser parser;
  ContextDesc descriptor;
  // Test multiple component identifier matches
  const string& filename = testinput_folder + "/TestProject/test_component_equal_version_match.cproject.yml";
  EXPECT_TRUE(parser.ParseCproject(filename, true));
  EXPECT_TRUE(AddContexts(parser, descriptor, filename));
  map<string, ContextItem>* contexts;
  GetContexts(contexts);
  ContextItem context = contexts->begin()->second;
  EXPECT_TRUE(LoadPacks(context));
  EXPECT_TRUE(ProcessPrecedences(context));
  EXPECT_TRUE(ProcessDevice(context));
  EXPECT_TRUE(SetTargetAttributes(context, context.targetAttributes));
  EXPECT_TRUE(ProcessComponents(context));
  ASSERT_EQ(expected.size(), context.components.size());
  auto it = context.components.begin();
  for (const auto& expectedComponent : expected) {
    EXPECT_EQ(expectedComponent, (*it++).first);
  }
}

TEST_F(ProjMgrWorkerUnitTests, ProcessComponents_Higher_Version_NotFound) {
  ProjMgrParser parser;
  ContextDesc descriptor;
  // Test multiple component identifier matches
  const string& filename = testinput_folder + "/TestProject/test_component_higher_version_notfound.cproject.yml";
  EXPECT_TRUE(parser.ParseCproject(filename, true));
  EXPECT_TRUE(AddContexts(parser, descriptor, filename));
  map<string, ContextItem>* contexts;
  GetContexts(contexts);
  ContextItem context = contexts->begin()->second;
  EXPECT_TRUE(LoadPacks(context));
  EXPECT_TRUE(ProcessPrecedences(context));
  EXPECT_TRUE(ProcessDevice(context));
  EXPECT_TRUE(SetTargetAttributes(context, context.targetAttributes));
  EXPECT_FALSE(ProcessComponents(context));
}

TEST_F(ProjMgrWorkerUnitTests, ProcessComponentsApi) {
  set<string> expectedComponents = {
    "ARM::Device:Startup&RteTest Startup@2.0.3",
    "ARM::RteTest:ApiExclusive:S1@0.9.9",
    "ARM::RteTest:CORE@0.1.1",
  };
  set<string> expectedPackages = {
    "ARM::RteTest@0.1.0",
    "ARM::RteTest_DFP@0.2.0",
  };
  ProjMgrParser parser;
  ContextDesc descriptor;
  const string& filename = testinput_folder + "/TestProject/test-api.cproject.yml";
  EXPECT_TRUE(parser.ParseCproject(filename, true));
  EXPECT_TRUE(AddContexts(parser, descriptor, filename));
  map<string, ContextItem>* contexts;
  GetContexts(contexts);
  ContextItem context = contexts->begin()->second;
  EXPECT_TRUE(LoadPacks(context));
  EXPECT_TRUE(ProcessPrecedences(context));
  EXPECT_TRUE(ProcessDevice(context));
  EXPECT_TRUE(SetTargetAttributes(context, context.targetAttributes));
  EXPECT_TRUE(ProcessComponents(context));
  ASSERT_EQ(expectedComponents.size(), context.components.size());
  auto componentIt = context.components.begin();
  for (const auto& expectedComponent : expectedComponents) {
    EXPECT_EQ(expectedComponent, (*componentIt++).first);
  }
  ASSERT_EQ(expectedPackages.size(), context.packages.size());
  auto packageIt = context.packages.begin();
  for (const auto& expectedPackage : expectedPackages) {
    EXPECT_EQ(expectedPackage, (*packageIt++).first);
  }
}

TEST_F(ProjMgrWorkerUnitTests, ProcessDependencies) {
  map<string, set<string>> expected = {{ "ARM::Device:Startup&RteTest Startup@2.0.3" , {"require RteTest:CORE"} }};
  ProjMgrParser parser;
  ContextDesc descriptor;
  const string& filename = testinput_folder + "/TestSolution/TestProject4/test-dependency.cproject.yml";
  EXPECT_TRUE(parser.ParseCproject(filename, true));
  EXPECT_TRUE(AddContexts(parser, descriptor, filename));
  map<string, ContextItem>* contexts;
  GetContexts(contexts);
  ContextItem context = contexts->begin()->second;
  EXPECT_TRUE(LoadPacks(context));
  EXPECT_TRUE(ProcessPrecedences(context));
  EXPECT_TRUE(ProcessDevice(context));
  EXPECT_TRUE(SetTargetAttributes(context, context.targetAttributes));
  EXPECT_TRUE(ProcessComponents(context));
  EXPECT_TRUE(ProcessGpdsc(context));
  EXPECT_FALSE(ValidateContext(context));
  ASSERT_EQ(expected.size(), context.validationResults.size());
  map<string, set<string>> dependenciesMap;
  for (const auto& [result, component, dependencies, _] : context.validationResults) {
    dependenciesMap[component] = dependencies;
  }
  for (const auto& [expectedComponent, expectedDependencies] : expected) {
    EXPECT_TRUE(dependenciesMap.find(expectedComponent) != dependenciesMap.end());
    const auto& dependencies = dependenciesMap[expectedComponent];
    for (const auto& expectedDependency : expectedDependencies) {
      EXPECT_TRUE(dependencies.find(expectedDependency) != dependencies.end());
    }
  }
}

TEST_F(ProjMgrWorkerUnitTests, ProcessDeviceFailed) {
  ProjMgrParser parser;
  ContextDesc descriptor;
  const string& filename = testinput_folder +
    "/TestSolution/TestProject4/test_device_pname_unavailable_in_board.cproject.yml";
  EXPECT_TRUE(parser.ParseCproject(filename, true));
  EXPECT_TRUE(AddContexts(parser, descriptor, filename));
  map<string, ContextItem>* contexts;
  GetContexts(contexts);
  ContextItem context = contexts->begin()->second;
  EXPECT_TRUE(LoadPacks(context));
  EXPECT_TRUE(ProcessPrecedences(context));
  EXPECT_FALSE(ProcessDevice(context));
}

TEST_F(ProjMgrWorkerUnitTests, LoadUnknownPacks) {
  CsolutionItem csolution;
  SetCsolutionPacks(&csolution, {"ARM::RteTest_Unknown@2.0.1"}, "Test");
  ContextItem context;
  EXPECT_FALSE(LoadPacks(context));
  EXPECT_EQ(0, m_loadedPacks.size());
}

TEST_F(ProjMgrWorkerUnitTests, LoadDuplicatePacks) {
  CsolutionItem csolution;
  SetCsolutionPacks(&csolution, {"ARM::RteTest_DFP@0.2.0", "ARM::RteTest_DFP"}, "Test");
  ContextItem context;
  EXPECT_TRUE(LoadPacks(context));
  // Check if only one pack is loaded
  ASSERT_EQ(1, m_loadedPacks.size());
  EXPECT_EQ("ARM::RteTest_DFP@0.2.0", (*m_loadedPacks.begin())->GetPackageID());
}

TEST_F(ProjMgrWorkerUnitTests, LoadDuplicatePacksFromDifferentPaths) {
  std::list<std::string> pdscFiles = { // Important that these 2 files contain the *same* vendor/name/version!
    testcmsispack_folder + "/ARM/RteTest_DFP/0.2.0/ARM.RteTest_DFP.pdsc",
    testinput_folder + "/SolutionSpecificPack/ARM.RteTest_DFP.pdsc"
  };

  m_kernel = ProjMgrKernel::Get();
  ASSERT_TRUE(m_kernel);
  m_model = m_kernel->GetGlobalModel();
  ASSERT_TRUE(m_kernel);

  EXPECT_TRUE(m_kernel->LoadAndInsertPacks(m_loadedPacks, pdscFiles));

  // Check if only one pack is loaded
  ASSERT_EQ(1, m_loadedPacks.size());
  EXPECT_EQ("ARM::RteTest_DFP@0.2.0", (*m_loadedPacks.begin())->GetPackageID());

  // Check that warning is issued
  EXPECT_FALSE(m_model->Validate());
  const auto& errors = m_model->GetErrors();
  EXPECT_EQ(errors.size(), 1);
  EXPECT_NE(string::npos, errors.front().find("warning #500:"));
}

TEST_F(ProjMgrWorkerUnitTests, LoadRequiredPacks) {
  CsolutionItem csolution;
  SetCsolutionPacks(&csolution, { "ARM::RteTest_DFP@0.2.0"}, "Test");
  ContextItem context;
  EXPECT_TRUE(LoadPacks(context));
  // Check if only one pack is loaded
  ASSERT_EQ(1, m_loadedPacks.size());
  EXPECT_EQ("ARM::RteTest_DFP@0.2.0", (*m_loadedPacks.begin())->GetPackageID());
}

TEST_F(ProjMgrWorkerUnitTests, LoadExactPackVersion) {
  CsolutionItem csolution;
  SetCsolutionPacks(&csolution, { "ARM::RteTest_DFP@0.1.1" }, "Test");
  ContextItem context;
  EXPECT_TRUE(LoadPacks(context));
  // Check if only one pack is loaded
  ASSERT_EQ(1, m_loadedPacks.size());
  EXPECT_EQ("ARM::RteTest_DFP@0.1.1", (*m_loadedPacks.begin())->GetPackageID());
}

TEST_F(ProjMgrWorkerUnitTests, LoadPacksNoPackage) {
  CsolutionItem csolution;
  SetCsolutionPacks(&csolution, {}, "Test");
  ContextItem context;
  EXPECT_TRUE(LoadPacks(context));
  // by default latest packs available should be loaded
  EXPECT_EQ(6, m_loadedPacks.size());
}

TEST_F(ProjMgrWorkerUnitTests, LoadFilteredPack_1) {
  CsolutionItem csolution;
  SetCsolutionPacks(&csolution, { "ARM::*Gen*" }, "Test");
  ContextItem context;
  EXPECT_TRUE(LoadPacks(context));
  // Check if only one pack is loaded
  ASSERT_EQ(1, m_loadedPacks.size());
  EXPECT_EQ("ARM::RteTestGenerator@0.1.0", (*m_loadedPacks.begin())->GetPackageID());
}

TEST_F(ProjMgrWorkerUnitTests, LoadFilteredPack_2) {
  CsolutionItem csolution;
  SetCsolutionPacks(&csolution, { "ARM" }, "Test");
  ProjMgrParser parser;
  SetParser(&parser);

  // get list of available packs
  vector<string> availablePacks;
  EXPECT_TRUE(ParseContextSelection({ "Test" }));
  EXPECT_TRUE(ListPacks(availablePacks, false));
  ContextItem context;
  EXPECT_TRUE(LoadPacks(context));
  ASSERT_EQ(availablePacks.size(), m_loadedPacks.size());
}

TEST_F(ProjMgrWorkerUnitTests, LoadFilteredPack_3) {
  CsolutionItem csolution;
  SetCsolutionPacks(&csolution, { "ARM::RteTest_D*" }, "Test");
  ContextItem context;
  EXPECT_TRUE(LoadPacks(context));
  ASSERT_EQ(1, m_loadedPacks.size());
  EXPECT_EQ("ARM::RteTest_DFP@0.2.0", (*m_loadedPacks.begin())->GetPackageID());
}

TEST_F(ProjMgrWorkerUnitTests, LoadFilteredPack_4) {
  CsolutionItem csolution;
  SetCsolutionPacks(&csolution, { "ARM::*" }, "Test");
  ProjMgrParser parser;
  SetParser(&parser);

  // get list of available packs
  vector<string> availablePacks;
  EXPECT_TRUE(ParseContextSelection({ "Test" }));
  EXPECT_TRUE(ListPacks(availablePacks, false));
  ContextItem context;
  EXPECT_TRUE(LoadPacks(context));
  ASSERT_EQ(availablePacks.size(), m_loadedPacks.size());
}

TEST_F(ProjMgrWorkerUnitTests, LoadFilteredPack_5) {
  CsolutionItem csolution;
  SetCsolutionPacks(&csolution, { "ARM::RteTest_DFP@0.2.0" }, "Test");
  ContextItem context;
  EXPECT_TRUE(LoadPacks(context));
  ASSERT_EQ(1, m_loadedPacks.size());
  EXPECT_EQ("ARM::RteTest_DFP@0.2.0", (*m_loadedPacks.begin())->GetPackageID());
}

TEST_F(ProjMgrWorkerUnitTests, LoadPack_Filter_Unknown) {
  CsolutionItem csolution;
  StdStreamRedirect streamRedirect;
  string expected = "no match found for pack filter: keil::*";
  SetCsolutionPacks(&csolution, { "keil::*" }, "Test");
  ContextItem context;
  EXPECT_FALSE(LoadPacks(context));
  ASSERT_EQ(0, m_loadedPacks.size());
  auto errStr = streamRedirect.GetErrorString();
  EXPECT_NE(string::npos, errStr.find(expected));
}

TEST_F(ProjMgrWorkerUnitTests, GetAccessSequence) {
  string src, sequence;
  size_t offset = 0;

  src = "Option=$Dname$ - $Dboard$";
  EXPECT_TRUE(GetAccessSequence(offset, src, sequence, '$', '$'));
  EXPECT_EQ(offset, 14);
  EXPECT_EQ(sequence, "Dname");
  EXPECT_TRUE(GetAccessSequence(offset, src, sequence, '$', '$'));
  EXPECT_EQ(offset, 25);
  EXPECT_EQ(sequence, "Dboard");
  EXPECT_TRUE(GetAccessSequence(offset, src, sequence, '$', '$'));
  EXPECT_EQ(offset, string::npos);

  src = "DEF=$Output(project)$";
  offset = 0;
  EXPECT_TRUE(GetAccessSequence(offset, src, sequence, '$', '$'));
  EXPECT_EQ(offset, 21);
  EXPECT_EQ(sequence, "Output(project)");
  offset = 0;
  EXPECT_TRUE(GetAccessSequence(offset, sequence, sequence, '(', ')'));
  EXPECT_EQ(offset, 15);
  EXPECT_EQ(sequence, "project");

  src = "Option=$Dname";
  offset = 0;
  EXPECT_FALSE(GetAccessSequence(offset, src, sequence, '$', '$'));
}

TEST_F(ProjMgrWorkerUnitTests, ProcessDevice_Invalid_Device_Name) {
  ProjMgrParser parser;
  ContextDesc descriptor;
  const string& filename = testinput_folder + "/TestSolution/TestProject4/test_device_unknown.cproject.yml";
  const string& expectedErrStr = R"(error csolution: specified device 'RteTest_ARM_UNKNOWN' was not found among the installed packs.
use 'cpackget' utility to install software packs.
  cpackget add Vendor.PackName --pack-root ./Path/Packs)";
  StdStreamRedirect streamRedirect;

  EXPECT_TRUE(parser.ParseCproject(filename, true));
  EXPECT_TRUE(AddContexts(parser, descriptor, filename));
  map<string, ContextItem>* contexts;
  GetContexts(contexts);
  ContextItem context = contexts->begin()->second;
  EXPECT_TRUE(LoadPacks(context));
  EXPECT_TRUE(ProcessPrecedences(context));
  EXPECT_FALSE(ProcessDevice(context));
  auto errStr = streamRedirect.GetErrorString();
  EXPECT_NE(string::npos, errStr.find(expectedErrStr));
}

TEST_F(ProjMgrWorkerUnitTests, ProcessDevice_Invalid_Device_Vendor) {
  ProjMgrParser parser;
  ContextDesc descriptor;
  const string& filename = testinput_folder +
    "/TestSolution/TestProject4/test_device_unknown_vendor.cproject.yml";
  const string& expectedErrStr = R"(error csolution: specified device 'RteTest_ARMCM0' was not found among the installed packs.
use 'cpackget' utility to install software packs.
  cpackget add Vendor.PackName --pack-root ./Path/Packs)";
  StdStreamRedirect streamRedirect;

  EXPECT_TRUE(parser.ParseCproject(filename, true));
  EXPECT_TRUE(AddContexts(parser, descriptor, filename));
  map<string, ContextItem>* contexts;
  GetContexts(contexts);
  ContextItem context = contexts->begin()->second;
  EXPECT_TRUE(LoadPacks(context));
  EXPECT_TRUE(ProcessPrecedences(context));
  EXPECT_FALSE(ProcessDevice(context));
  auto errStr = streamRedirect.GetErrorString();
  EXPECT_NE(string::npos, errStr.find(expectedErrStr));
}

TEST_F(ProjMgrWorkerUnitTests, ProcessDevice_PName) {
  ProjMgrParser parser;
  ContextDesc descriptor;
  const string& filename = testinput_folder +
    "/TestSolution/TestProject4/test_device_unknown_processor.cproject.yml";
  const string& expected = "processor name 'NOT_AVAILABLE' was not found";
  StdStreamRedirect streamRedirect;

  EXPECT_TRUE(parser.ParseCproject(filename, true));
  EXPECT_TRUE(AddContexts(parser, descriptor, filename));
  map<string, ContextItem>* contexts;
  GetContexts(contexts);
  ContextItem context = contexts->begin()->second;
  EXPECT_TRUE(LoadPacks(context));
  EXPECT_TRUE(ProcessPrecedences(context));
  EXPECT_FALSE(ProcessDevice(context));
  auto errStr = streamRedirect.GetErrorString();
  EXPECT_NE(string::npos, errStr.find(expected));
}

TEST_F(ProjMgrWorkerUnitTests, ProcessDevice_With_Board_And_Device_Info) {
  ProjMgrParser parser;
  ContextDesc descriptor;
  const string& filename = testinput_folder +
    "/TestSolution/TestProject4/test_board_and_device.cproject.yml";

  EXPECT_TRUE(parser.ParseCproject(filename, true));
  EXPECT_TRUE(AddContexts(parser, descriptor, filename));
  map<string, ContextItem>* contexts;
  GetContexts(contexts);
  ContextItem context = contexts->begin()->second;
  EXPECT_TRUE(LoadPacks(context));
  EXPECT_TRUE(ProcessPrecedences(context));
  EXPECT_TRUE(ProcessDevice(context));
}

TEST_F(ProjMgrWorkerUnitTests, ProcessPrecedences_With_Only_Board) {
  ProjMgrParser parser;
  ContextDesc descriptor;
  const string& filename = testinput_folder + "/TestSolution/TestProject4/test_only_board.cproject.yml";
  EXPECT_TRUE(parser.ParseCproject(filename, true));
  EXPECT_TRUE(AddContexts(parser, descriptor, filename));
  map<string, ContextItem>* contexts;
  GetContexts(contexts);
  ContextItem context = contexts->begin()->second;
  EXPECT_TRUE(LoadPacks(context));
  EXPECT_TRUE(ProcessPrecedences(context));
  EXPECT_TRUE(ProcessDevice(context));
}

TEST_F(ProjMgrWorkerUnitTests, ProcessDevice_Invalid_Board_Vendor) {
  ProjMgrParser parser;
  ContextDesc descriptor;
  const string& filename = testinput_folder +
    "/TestSolution/TestProject4/test_board_vendor_invalid.cproject.yml";
  const string& expected = "board 'UNKNOWN::RteTest Dummy board' was not found";
  StdStreamRedirect streamRedirect;

  EXPECT_TRUE(parser.ParseCproject(filename, true));
  EXPECT_TRUE(AddContexts(parser, descriptor, filename));
  map<string, ContextItem>* contexts;
  GetContexts(contexts);
  ContextItem context = contexts->begin()->second;
  EXPECT_TRUE(LoadPacks(context));
  EXPECT_TRUE(ProcessPrecedences(context));
  EXPECT_FALSE(ProcessDevice(context));
  auto errStr = streamRedirect.GetErrorString();
  EXPECT_NE(string::npos, errStr.find(expected));
}

TEST_F(ProjMgrWorkerUnitTests, ProcessDevice_Invalid_Board_Name) {
  ProjMgrParser parser;
  ContextDesc descriptor;
  const string& filename = testinput_folder +
    "/TestSolution/TestProject4/test_board_name_invalid.cproject.yml";
  const string& expected = "board 'Keil::RteTest_unknown' was not found";
  StdStreamRedirect streamRedirect;

  EXPECT_TRUE(parser.ParseCproject(filename, true));
  EXPECT_TRUE(AddContexts(parser, descriptor, filename));
  map<string, ContextItem>* contexts;
  GetContexts(contexts);
  ContextItem context = contexts->begin()->second;
  EXPECT_TRUE(LoadPacks(context));
  EXPECT_TRUE(ProcessPrecedences(context));
  EXPECT_FALSE(ProcessDevice(context));
  auto errStr = streamRedirect.GetErrorString();
  EXPECT_NE(string::npos, errStr.find(expected));
}

TEST_F(ProjMgrWorkerUnitTests, ProcessDevice_Exact_Board_From_Multiple_Matches) {
  ProjMgrParser parser;
  ContextDesc descriptor;
  const string& filename = testinput_folder +
    "/TestProject/test_exact_board_match.cproject.yml";
  const string& expectedBoard = "Keil::RteTest board test revision:Rev1";

  EXPECT_TRUE(parser.ParseCproject(filename, true));
  EXPECT_TRUE(AddContexts(parser, descriptor, filename));
  map<string, ContextItem>* contexts;
  GetContexts(contexts);
  ContextItem context = contexts->begin()->second;
  EXPECT_TRUE(LoadPacks(context));
  EXPECT_TRUE(ProcessPrecedences(context));
  EXPECT_TRUE(ProcessDevice(context));
  EXPECT_STREQ(context.board.c_str(), expectedBoard.c_str());
}

TEST_F(ProjMgrWorkerUnitTests, ProcessDevice_Board_Not_Found) {
  ProjMgrParser parser;
  ContextDesc descriptor;
  const string& filename = testinput_folder +
    "/TestProject/test_board_not_found.cproject.yml";
  const string& expected = "error csolution: board 'Keil::RteTest Dummy board:Rev10' was not found";
  StdStreamRedirect streamRedirect;

  EXPECT_TRUE(parser.ParseCproject(filename, true));
  EXPECT_TRUE(AddContexts(parser, descriptor, filename));
  map<string, ContextItem>* contexts;
  GetContexts(contexts);
  ContextItem context = contexts->begin()->second;
  EXPECT_TRUE(LoadPacks(context));
  EXPECT_TRUE(ProcessPrecedences(context));
  EXPECT_FALSE(ProcessDevice(context));
  auto errStr = streamRedirect.GetErrorString();
  EXPECT_NE(string::npos, errStr.find(expected));
}

TEST_F(ProjMgrWorkerUnitTests, ProcessDevice_Multiple_Board_Matches) {
  ProjMgrParser parser;
  ContextDesc descriptor;
  const string& filename = testinput_folder +
    "/TestProject/test_board_with_multiple_matches.cproject.yml";
  const string& expected = "error csolution: multiple boards were found for identifier 'Keil::RteTest board test revision'";
  StdStreamRedirect streamRedirect;

  EXPECT_TRUE(parser.ParseCproject(filename, true));
  EXPECT_TRUE(AddContexts(parser, descriptor, filename));
  map<string, ContextItem>* contexts;
  GetContexts(contexts);
  ContextItem context = contexts->begin()->second;
  EXPECT_TRUE(LoadPacks(context));
  EXPECT_TRUE(ProcessPrecedences(context));
  EXPECT_FALSE(ProcessDevice(context));
  auto errStr = streamRedirect.GetErrorString();
  EXPECT_NE(string::npos, errStr.find(expected));
}


TEST_F(ProjMgrWorkerUnitTests, GetDeviceItem) {
  std::map<std::string, DeviceItem> input = {
    // {input, expected output}
    {"Vendor::Name:Processor", {"Vendor", "Name", "Processor"}},
    {"Name:Processor",         {"", "Name", "Processor"}},
    {"::Name:Processor",       {"", "Name", "Processor"}},
    {":Processor",             {"", "", "Processor"}},
    {"Vendor::Name:",          {"Vendor", "Name", ""}},
    {"::Name:",                {"", "Name", ""}},
    {"::Name",                 {"", "Name", ""}},
    {"Name",                   {"", "Name", ""}},
  };

  for (const auto& in : input) {
    DeviceItem item;
    GetDeviceItem(in.first, item);
    EXPECT_EQ(in.second.name, item.name);
    EXPECT_EQ(in.second.vendor, item.vendor);
    EXPECT_EQ(in.second.pname, item.pname);
  }
}

TEST_F(ProjMgrWorkerUnitTests, ProcessDevicePrecedence) {
  struct TestInfo {
    std::string elem1;
    std::string elem2;
    std::string elem3;
    bool expectedReturnVal;
    std::string expectedOutput;
  };

  std::vector<TestInfo> inputs = {
    // input1, input2, input3, expectedreturnVal, expectedOutput
    // positive tests
    { "name", "", "",                                 true, "name"},
    { "", "::name", "name",                           true, "name"},
    { "name:processor", "", "",                       true, "name:processor"},
    { ":processor", "vendor::name", "",               true, "vendor::name:processor"},
    { ":processor", "::name:processor", "::name",     true, "name:processor"},
    { "vendor::name", ":processor", "name",           true, "vendor::name:processor"},
    { ":processor", "vendor::name:processor", "name", true, "vendor::name:processor"},
    { "", "", "",                                     true, ""},
    { ":processor", "", "",                           true, ":processor"},
    // negative tests
    { "name:processor", "", "name:processor1",        false, ""},
    { ":processor", "vendor::name:processor1", "name",false, ""},
    { ":processor", "vendor::name:processor", "vendor::name:processor2", false, ""},
  };

  for (auto& in : inputs) {
    std::string out   = "";
    std::string elem1 = in.elem1;
    std::string elem2 = in.elem2;
    std::string elem3 = in.elem3;

    StringCollection item = { &out, { &elem1, &elem2, &elem3 } };
    EXPECT_EQ(in.expectedReturnVal, ProcessDevicePrecedence(item));
    EXPECT_STREQ(item.assign->c_str(), in.expectedOutput.c_str());
  }
}

TEST_F(ProjMgrWorkerUnitTests, GetBoardItem) {
  std::map<std::string, BoardItem> input = {
    // {input, expected output}
    {"Vendor::Name",          {"Vendor", "Name"}},
    {"Name",                  {"", "Name"}},
    {"::Name",                {"", "Name"}},
    {"",                      {"", ""}},
    {"Vendor::Name:Revision", {"Vendor", "Name", "Revision"}},
    {"Name:Revision",         {"", "Name", "Revision"}},
    {"::Name:Revision",       {"", "Name", "Revision"}},
    {":Revision",             {"", "", "Revision"}},
  };

  for (const auto& in : input) {
    BoardItem item;
    GetBoardItem(in.first, item);
    EXPECT_EQ(in.second.name, item.name);
    EXPECT_EQ(in.second.vendor, item.vendor);
  }
}

TEST_F(ProjMgrWorkerUnitTests, ApplyFilter) {
  std::vector<std::string> input = { "TestString1", "FilteredString", "TestString2" };
  std::set<std::string> filter = { "String", "Filtered", "" };
  std::vector<std::string> expected = { "FilteredString" };
  std::vector<std::string> result;
  ApplyFilter(input, filter, result);
  EXPECT_EQ(expected, result);
}

TEST_F(ProjMgrWorkerUnitTests, ProcessComponentFilesEmpty) {
  // test ProcessComponentFiles over a component without files
  ContextItem context;
  LoadPacks(context);
  InitializeTarget(context);
  RtePackage* pack = new RtePackage(NULL);
  RteComponent* c = new RteComponent(pack);
  RteComponentInstance* ci = new RteComponentInstance(c);
  ci->InitInstance(c);
  ci->SetAttributes({ {"Cclass" , "Class"}, {"Cgroup" , "Group"} });
  context.components.insert({ "Class:Group", { ci } });
  EXPECT_TRUE(ProcessComponentFiles(context));

  // test ProcessComponentFiles over component with a non-filtered file
  ci->SetResolvedComponent(c, context.rteActiveTarget->GetName());
  RteFile file = RteFile(c);
  file.SetAttributes({ {"category" , "source"}, {"name" , "path/"} });
  context.rteActiveTarget->AddFile(&file, ci);
  EXPECT_TRUE(ProcessComponentFiles(context));
}

TEST_F(ProjMgrWorkerUnitTests, GetAllCombinations) {
  const string strOrangeA = "OrangeA";
  const string strOrangeB = "OrangeB";
  const string strOrangeC = "OrangeC";
  const string strAnanasA = "AnanasA";
  const string strBananaA = "BananaA";
  const string strBananaB = "BananaB";
  ConnectionsCollection OrangeA = { strOrangeA, RteUtils::EMPTY_STRING };
  ConnectionsCollection OrangeB = { strOrangeB, RteUtils::EMPTY_STRING };
  ConnectionsCollection OrangeC = { strOrangeC, RteUtils::EMPTY_STRING };
  ConnectionsCollection AnanasA = { strAnanasA, RteUtils::EMPTY_STRING };
  ConnectionsCollection BananaA = { strBananaA, RteUtils::EMPTY_STRING };
  ConnectionsCollection BananaB = { strBananaB, RteUtils::EMPTY_STRING };

  ConnectionsCollectionMap connections = {
    {"Orange", {OrangeA, OrangeB, OrangeC}},
    {"Ananas", {AnanasA}},
    {"Banana", {BananaA, BananaB}},
  };
  const vector<ConnectionsCollectionVec> expected = {
    {AnanasA, BananaA, OrangeA},
    {AnanasA, BananaA, OrangeB},
    {AnanasA, BananaA, OrangeC},
    {AnanasA, BananaB, OrangeA},
    {AnanasA, BananaB, OrangeB},
    {AnanasA, BananaB, OrangeC},
  };
  vector<ConnectionsCollectionVec> combinations;
  GetAllCombinations(connections, connections.begin(), combinations);

  auto it = combinations.begin();
  for (const auto& expectedItem : expected) {
    EXPECT_EQ(expectedItem.front().filename, (*it++).front().filename);
  }
}

TEST_F(ProjMgrWorkerUnitTests, GetAllSelectCombinations) {
  ConnectItem connectA = { "A" };
  ConnectItem connectB = { "B" };
  ConnectItem connectC = { "C" };
  ConnectItem connectD = { "D" };
  const ConnectItem* A = { &connectA };
  const ConnectItem* B = { &connectB };
  const ConnectItem* C = { &connectC };
  const ConnectItem* D = { &connectD };
  ConnectPtrVec connections = { A, B, C, D };
  const vector<ConnectPtrVec> expected = {
    {A},
    {A, B},
    {B},
    {A, C},
    {A, B, C},
    {B, C},
    {C},
    {A, D},
    {A, B, D},
    {B, D},
    {A, C, D},
    {A, B, C, D},
    {B, C, D},
    {C, D},
    {D},
  };
  vector<ConnectPtrVec> combinations;
  GetAllSelectCombinations(connections, connections.begin(), combinations);

  auto it = combinations.begin();
  for (const auto& expectedItem : expected) {
    EXPECT_EQ(expectedItem.front()->connect, (*it++).front()->connect);
  }
}

TEST_F(ProjMgrWorkerUnitTests, ValidateConnections) {
  ConnectionsValidationResult result;

  // valid connections
  StrPairVec consumedList = {
    {"Orange", "3"},
    {"Grape Fruit", ""},
    {"Peach", ""},
    {"Lime", "+98"},
    {"Lime", "+2"},
    {"Lemon", "+150"},
    {"Lemon", "+20"},
  };
  StrPairVec providedList = {
    {"Orange", "3"},                  // both key and value exact match
    {"Grape Fruit", "999"},           // key exact match, consumed value is empty
    {"Peach", ""},                    // key exact match, both values empty
    {"Lemon", "200"},                 // added consumed values are less than provided
    {"Lime", "100"},                  // added consumed values are equal to provided
  };

  ConnectItem validConnectItem = { RteUtils::EMPTY_STRING, RteUtils::EMPTY_STRING, RteUtils::EMPTY_STRING, providedList, consumedList };
  ConnectionsCollection validCollection = { RteUtils::EMPTY_STRING, RteUtils::EMPTY_STRING, {&validConnectItem} };
  result = ValidateConnections({ validCollection });
  EXPECT_TRUE(result.valid);

  // invalid connections
  // same interface is provided multiple times with non identical values
  consumedList = {
    {"Lemon", "+150"},
    {"Lemon", "+20"},
    {"Ananas", "98"},
    {"Grape Fruit", "1"},
  };
  providedList = {
    {"Ananas", "97"},                 // consumed interface doesn't match provided one
    {"Grape Fruit", ""},              // consumed interface doesn't match empty provided one
    {"Lemon", "160"},                 // sum of consumed added values is higher than provided value
    {"Ananas", "2"}, {"Ananas", "2"}, // same interface is provided multiple times with identical values
    {"Orange", "3"}, {"Orange", "4"}, // same interface is provided multiple times with non identical values
    {"Banana", ""}, {"Banana", "0"},  // same interface is provided multiple times with non identical values
  };
  StrVec expectedConflicts = { "Ananas", "Orange", "Banana" };
  StrPairVec expectedOverflow = {{"Lemon", "170 > 160"}};
  StrPairVec expectedIncompatibles = {{"Ananas", "98"}, {"Grape Fruit", "1"}};
  ConnectItem invalidConnectItem = { RteUtils::EMPTY_STRING, RteUtils::EMPTY_STRING, RteUtils::EMPTY_STRING, providedList, consumedList };
  ConnectionsCollection invalidCollection = { RteUtils::EMPTY_STRING, RteUtils::EMPTY_STRING, {&invalidConnectItem} };
  result = ValidateConnections({ invalidCollection });
  EXPECT_FALSE(result.valid);
  EXPECT_EQ(result.conflicts, expectedConflicts);
  EXPECT_EQ(result.overflows, expectedOverflow);
  EXPECT_EQ(result.incompatibles, expectedIncompatibles);
}

TEST_F(ProjMgrWorkerUnitTests, CollectLayersFromPacks) {
  // test CollectLayersFromPacks with an non-existent clayer file
  InitializeModel();
  ContextItem context;
  InitializeTarget(context);
  const map<string, string> packAttributes = {
    {"vendor" , "Vendor"  },
    {"name"   , "Name"    },
    {"version", "8.8.8"   }
  };
  const map<string, string> clayerAttributes = {
    {"name"   , "TestVariant"  },
    {"type"   , "TestVariant"  },
    {"file"   , "Invalid/Path" }
  };
  RteModel* model = context.rteActiveTarget->GetModel();
  RtePackage* pack = new RtePackage(model, packAttributes);
  RteItem* clayersItem = pack->CreateChild("clayers");
  RteItem* clayerItem = clayersItem->CreateChild("clayer");
  clayerItem->SetAttributes(clayerAttributes);
  model->InsertPacks(list<RtePackage*>{pack});
  context.rteActiveTarget->UpdateFilterModel();
  StrVecMap clayers;
  EXPECT_FALSE(CollectLayersFromPacks(context, clayers));
}

TEST_F(ProjMgrWorkerUnitTests, ExpandString) {
  ContextItem context;
  context.variables = {
    {"Foo", "./foo"},
    {"Bar", "./bar"},
    {"Foo Bar", "./foo-bar"},
  };

  EXPECT_EQ(ExpandString("path1: $Foo$/bar", context.variables), "path1: ./foo/bar");
  EXPECT_EQ(ExpandString("path2: $Bar$/foo", context.variables), "path2: ./bar/foo");
  EXPECT_EQ(ExpandString("$Foo$ $Bar$", context.variables), "./foo ./bar");
  EXPECT_EQ(ExpandString("$Foo Bar$", context.variables), "./foo-bar");
  EXPECT_EQ(ExpandString("$Foo$ $Foo$ $Foo$", context.variables), "./foo ./foo ./foo");
}

TEST_F(ProjMgrWorkerUnitTests, ListToolchains) {
  const string& cmsisPackRoot = CrossPlatformUtils::GetEnv("CMSIS_COMPILER_ROOT");

  StrVec envVars = {
  "AC6_TOOLCHAIN_6_18_0=" + cmsisPackRoot,
  "AC6_TOOLCHAIN_6_18_1=" + cmsisPackRoot + "/non-existent",
  "AC6_TOOLCHAIN_6_19_0=" + cmsisPackRoot,
  "AC6_TOOLCHAIN_6_6_0=" + cmsisPackRoot,
  "GCC_TOOLCHAIN_11_3_1=" + cmsisPackRoot,
  };
  SetEnvironmentVariables(envVars);

  // list all configured toolchains
  GetRegisteredToolchains();
  vector<ToolchainItem> expected {
    {"AC6", "6.18.0", "", "", cmsisPackRoot, cmsisPackRoot + "/AC6.6.18.0.cmake"},
    {"AC6", "6.19.0", "", "", cmsisPackRoot, cmsisPackRoot + "/AC6.6.18.0.cmake"},
    {"GCC", "11.3.1", "", "", cmsisPackRoot, cmsisPackRoot + "/GCC.11.2.1.cmake"}
  };
  ASSERT_EQ(m_toolchains.size(), 3);
  for (int i = 0; i < 3; i++) {
    EXPECT_EQ(m_toolchains[i].name, expected[i].name);
    EXPECT_EQ(m_toolchains[i].version, expected[i].version);
    EXPECT_EQ(m_toolchains[i].root, expected[i].root);
    EXPECT_EQ(m_toolchains[i].config, expected[i].config);
  }
  // with empty cmsis compiler root
  CrossPlatformUtils::SetEnv("CMSIS_COMPILER_ROOT", "");
  m_compilerRoot.clear();
  m_toolchains.clear();
  m_toolchainConfigFiles.clear();
  GetRegisteredToolchains();
  EXPECT_TRUE(m_toolchains.empty());

  // list latest toolchains
  CrossPlatformUtils::SetEnv("CMSIS_COMPILER_ROOT", cmsisPackRoot);
  m_compilerRoot.clear();
  m_toolchains.clear();
  m_toolchainConfigFiles.clear();
  ToolchainItem latestToolchainInfo;
  latestToolchainInfo.name = "AC6";
  GetLatestToolchain(latestToolchainInfo);
  EXPECT_EQ(latestToolchainInfo.version, "6.19.0");
  EXPECT_EQ(latestToolchainInfo.config, cmsisPackRoot + "/AC6.6.18.0.cmake");
  latestToolchainInfo.name = "GCC";
  GetLatestToolchain(latestToolchainInfo);
  EXPECT_EQ(latestToolchainInfo.version, "11.3.1");
  EXPECT_EQ(latestToolchainInfo.config, cmsisPackRoot + "/GCC.11.2.1.cmake");
}

TEST_F(ProjMgrWorkerUnitTests, CheckBoardLayer) {
  ContextItem context;
  context.board = "BoardVendor::BoardName:BoardRevision";

  // test valid forBoard values
  const vector<string> forBoardValues = {
    "",
    "BoardName",
    "BoardName:BoardRevision",
    "BoardVendor::BoardName",
    "BoardVendor::BoardName:BoardRevision",
  };
  for (const auto& forBoard : forBoardValues) {
    ClayerItem clayer;
    clayer.forBoard = forBoard;
    EXPECT_TRUE(CheckBoardDeviceInLayer(context, clayer));
  }

  // test invalid forBoard values
  const vector<string> invalidForBoardValues = {
    "InvalidBoardName",
    "InvalidBoardName:BoardRevision",
    "BoardName:InvalidBoardRevision",
    "InvalidBoardVendor::BoardName",
    "BoardVendor::InvalidBoardName",
    "InvalidBoardVendor::BoardName:BoardRevision",
    "BoardVendor::InvalidBoardName:BoardRevision",
    "BoardVendor::BoardName:InvalidBoardRevision",
  };
  for (const auto& forBoard : invalidForBoardValues) {
    ClayerItem clayer;
    clayer.forBoard = forBoard;
    EXPECT_FALSE(CheckBoardDeviceInLayer(context, clayer));
  }
};

TEST_F(ProjMgrWorkerUnitTests, CheckDeviceLayer) {
  ContextItem context;
  context.device = "DeviceVendor::DeviceName:DevicePname";

  // test valid forDevice values
  const vector<string> forDeviceValues = {
    "",
    "DeviceName"
    ":DevicePname",
    "DeviceName:DevicePname",
    "DeviceVendor::DeviceName",
    "DeviceVendor::DeviceName:DevicePname",
  };
  for (const auto& forDevice : forDeviceValues) {
    ClayerItem clayer;
    clayer.forDevice = forDevice;
    EXPECT_TRUE(CheckBoardDeviceInLayer(context, clayer));
  }

  // test invalid forDevice values
  const vector<string> invalidForDeviceValues = {
    "InvalidDeviceName"
    ":InvalidDevicePname",
    "InvalidDeviceName:DevicePname",
    "DeviceName:InvalidDevicePname",
    "InvalidDeviceVendor::DeviceName",
    "DeviceVendor::InvalidDeviceName",
    "InvalidDeviceVendor::DeviceName:DevicePname",
    "DeviceVendor::InvalidDeviceName:DevicePname",
    "DeviceVendor::DeviceName:InvalidDevicePname",
  };
  for (const auto& forDevice : invalidForDeviceValues) {
    ClayerItem clayer;
    clayer.forDevice = forDevice;
    EXPECT_FALSE(CheckBoardDeviceInLayer(context, clayer));
  }
};

TEST_F(ProjMgrWorkerUnitTests, RemoveRedundantSubsets) {
  const string strA = "A";
  const string strB = "B";
  const string strC = "C";
  ConnectionsCollection A = { strA, RteUtils::EMPTY_STRING };
  ConnectionsCollection B = { strB, RteUtils::EMPTY_STRING };
  ConnectionsCollection C = { strC, RteUtils::EMPTY_STRING };
  ConnectionsCollectionVec vecAB = { A, B };
  ConnectionsCollectionVec vecA = { A };
  ConnectionsCollectionVec vecB = { B };
  ConnectionsCollectionVec vecC = { C };
  vector<ConnectionsCollectionVec> validConnections = { vecAB, vecA, vecB, vecC };
  vector<ConnectionsCollectionVec> expected = { vecAB, vecC };
  RemoveRedundantSubsets(validConnections);
  auto it = validConnections.begin();
  for (const auto& expectedItem : expected) {
    auto it2 = (*it++).begin();
    for (const auto& expectedElement : expectedItem) {
      EXPECT_EQ(expectedElement.filename, (*it2++).filename);
    }
  }
};

TEST_F(ProjMgrWorkerUnitTests, ProcessLinkerOptions) {
  string linkerScriptFile = "path/to/linkerScript_$Compiler$.sct";
  string linkerRegionsFile = "path/to/linkerRegions_$Compiler$.h";
  ContextItem context;
  CprojectItem cproject;
  context.cproject = &cproject;
  cproject.directory = testoutput_folder;
  context.directories.cprj = cproject.directory;
  LinkerItem linker;
  linker.script = linkerScriptFile;
  linker.regions = linkerRegionsFile;
  cproject.linker.push_back(linker);
  context.compiler = "AC6";
  context.variables[ProjMgrUtils::AS_COMPILER] = context.compiler;
  string expectedLinkerScriptFile = "path/to/linkerScript_AC6.sct";
  string expectedLinkerRegionsFile = "path/to/linkerRegions_AC6.h";
  EXPECT_TRUE(ProcessLinkerOptions(context));
  EXPECT_EQ(expectedLinkerScriptFile, context.linker.script);
  EXPECT_EQ(expectedLinkerRegionsFile, context.linker.regions);
};

TEST_F(ProjMgrWorkerUnitTests, ProcessLinkerOptions_MultipleCompilers) {
  ContextItem context;
  CprojectItem cproject;
  context.cproject = &cproject;
  ClayerItem clayer;
  context.clayers.insert({ "layer", &clayer });
  SetupItem setup;
  context.cproject->setups.push_back(setup);
  LinkerItem linker;

  string linkerScriptFile = "path/to/linkerScript_AC6.sct";
  string linkerRegionsFile = "path/to/linkerRegions_AC6.h";
  linker.script = linkerScriptFile;
  linker.regions = linkerRegionsFile;
  linker.forCompiler = { "AC6" };
  cproject.linker.push_back(linker);
  cproject.setups.front().linker.push_back(linker);
  clayer.linker.push_back(linker);

  linkerScriptFile = "path/to/linkerScript_GCC.sct";
  linkerRegionsFile = "path/to/linkerRegions_GCC.h";
  linker.script = linkerScriptFile;
  linker.regions = linkerRegionsFile;
  linker.forCompiler = { "GCC" };
  cproject.linker.push_back(linker);
  cproject.setups.front().linker.push_back(linker);
  clayer.linker.push_back(linker);

  context.compiler = "AC6";
  string expectedLinkerScriptFile = "path/to/linkerScript_AC6.sct";
  string expectedLinkerRegionsFile = "path/to/linkerRegions_AC6.h";
  EXPECT_TRUE(ProcessLinkerOptions(context));
  EXPECT_EQ(expectedLinkerScriptFile, context.linker.script);
  EXPECT_EQ(expectedLinkerRegionsFile, context.linker.regions);

  context.linker.script.clear();
  context.linker.regions.clear();
  context.compiler = "GCC";
  expectedLinkerScriptFile = "path/to/linkerScript_GCC.sct";
  expectedLinkerRegionsFile = "path/to/linkerRegions_GCC.h";
  EXPECT_TRUE(ProcessLinkerOptions(context));
  EXPECT_EQ(expectedLinkerScriptFile, context.linker.script);
  EXPECT_EQ(expectedLinkerRegionsFile, context.linker.regions);
};

TEST_F(ProjMgrWorkerUnitTests, ProcessLinkerOptions_Redefinition) {
  string linkerScriptFile = "path/to/linkerScript.sct";
  ContextItem context;
  CprojectItem cproject;
  context.cproject = &cproject;
  cproject.directory = testoutput_folder;
  context.directories.cprj = cproject.directory;
  ClayerItem clayer;
  context.clayers.insert({ "layer", &clayer });
  clayer.directory = testoutput_folder + "/layer";
  SetupItem setup;
  context.cproject->setups.push_back(setup);
  LinkerItem linker;
  linker.script = linkerScriptFile;
  cproject.linker.push_back(linker);
  cproject.setups.front().linker.push_back(linker);
  clayer.linker.push_back(linker);
  StdStreamRedirect streamRedirect;

  string expectedErrStr = "error csolution: redefinition from 'path/to/linkerScript.sct' into 'layer/path/to/linkerScript.sct' is not allowed\n";
  EXPECT_FALSE(ProcessLinkerOptions(context));
  string errStr = streamRedirect.GetErrorString();
  EXPECT_TRUE(regex_match(errStr, regex(expectedErrStr)));

  context.linker.script.clear();
  context.linker.regions.clear();
  string linkerRegionsFile = "path/to/linkerRegions.h";
  linker.script.clear();
  linker.regions = linkerRegionsFile;
  cproject.linker.clear();
  cproject.linker.push_back(linker);
  clayer.linker.clear();
  clayer.linker.push_back(linker);

  streamRedirect.ClearStringStreams();
  expectedErrStr = "error csolution: redefinition from 'path/to/linkerRegions.h' into 'layer/path/to/linkerRegions.h' is not allowed\n";
  EXPECT_FALSE(ProcessLinkerOptions(context));
  errStr = streamRedirect.GetErrorString();
  EXPECT_TRUE(regex_match(errStr, regex(expectedErrStr)));
};

TEST_F(ProjMgrWorkerUnitTests, SetDefaultLinkerScript) {
  ProjMgrParser parser;
  ContextDesc descriptor;
  const string& filename = testinput_folder + "/TestProject/test.cproject.yml";
  EXPECT_TRUE(parser.ParseCproject(filename, true));
  EXPECT_TRUE(AddContexts(parser, descriptor, filename));
  map<string, ContextItem>* contexts;
  GetContexts(contexts);
  ContextItem context = contexts->begin()->second;
  EXPECT_TRUE(LoadPacks(context));
  EXPECT_TRUE(ProcessPrecedences(context));
  EXPECT_TRUE(ProcessDevice(context));
  EXPECT_TRUE(SetTargetAttributes(context, context.targetAttributes));
  context.directories.cprj = testinput_folder;
  context.directories.rte = "";

  string expectedLinkerScriptFile = "TestToolchains/ac6_linker_script.sct";
  string expectedLinkerRegionsFile = "Device/RteTest_ARMCM0/regions_RteTest_ARMCM0.h";
  SetDefaultLinkerScript(context);
  EXPECT_EQ(expectedLinkerScriptFile, context.linker.script);
  EXPECT_EQ(expectedLinkerRegionsFile, context.linker.regions);
};

TEST_F(ProjMgrWorkerUnitTests, SetDefaultLinkerScript_UnknownCompiler) {
  StdStreamRedirect streamRedirect;
  ContextItem context;
  context.toolchain.name = "Unknown";
  string expectedErrStr = "warning csolution: linker script template for compiler 'Unknown' was not found\n";
  SetDefaultLinkerScript(context);
  EXPECT_TRUE(context.linker.script.empty());
  string errStr = streamRedirect.GetErrorString();
  EXPECT_EQ(errStr, expectedErrStr);
};

TEST_F(ProjMgrWorkerUnitTests, GenerateRegionsHeader) {
  StdStreamRedirect streamRedirect;
  ContextItem context;
  LoadPacks(context);
  InitializeTarget(context);

  // Generation fails
  string generatedRegionsFile;
  EXPECT_FALSE(GenerateRegionsHeader(context, generatedRegionsFile));
  EXPECT_TRUE(generatedRegionsFile.empty());
  string expectedErrStr = "warning csolution: regions header file generation failed\n";
  string errStr = streamRedirect.GetErrorString();
  EXPECT_EQ(errStr, expectedErrStr);

  // Generation succeeds
  context.targetAttributes["Dvendor"] = "ARM";
  context.targetAttributes["Dname"] = "RteTest_ARMCM0";
  EXPECT_TRUE(SetTargetAttributes(context, context.targetAttributes));
  context.directories.cprj = testoutput_folder;
  context.directories.rte = "./RTE";
  EXPECT_TRUE(GenerateRegionsHeader(context, generatedRegionsFile));
  EXPECT_TRUE(fs::equivalent(generatedRegionsFile, testoutput_folder + "/RTE/Device/RteTest_ARMCM0/regions_RteTest_ARMCM0.h"));
};

TEST_F(ProjMgrWorkerUnitTests, CheckAndGenerateRegionsHeader) {
  StdStreamRedirect streamRedirect;
  string expectedErrStr, expectedOutStr, errStr, outStr;
  ContextItem context;
  LoadPacks(context);
  InitializeTarget(context);

  // Generation fails
  context.directories.cprj = testoutput_folder;
  context.linker.regions = "regions_RteTest_ARMCM0.h";
  CheckAndGenerateRegionsHeader(context);
  expectedErrStr = "\
warning csolution: regions header file generation failed\n\
.*/regions_RteTest_ARMCM0.h - warning csolution: specified regions header was not found\n\
";
  errStr = streamRedirect.GetErrorString();
  EXPECT_TRUE(regex_match(errStr, regex(expectedErrStr)));

  // Generation succeeds but specified file does not exist
  context.targetAttributes["Dvendor"] = "ARM";
  context.targetAttributes["Dname"] = "RteTest_ARMCM0";
  EXPECT_TRUE(SetTargetAttributes(context, context.targetAttributes));
  streamRedirect.ClearStringStreams();
  CheckAndGenerateRegionsHeader(context);
  expectedOutStr = ".*/Device/RteTest_ARMCM0/regions_RteTest_ARMCM0.h - info csolution: regions header generated successfully\n";
  expectedErrStr = ".*/regions_RteTest_ARMCM0.h - warning csolution: specified regions header was not found\n";
  outStr = streamRedirect.GetOutString();
  errStr = streamRedirect.GetErrorString();
  EXPECT_TRUE(regex_match(outStr, regex(expectedOutStr)));
  EXPECT_TRUE(regex_match(errStr, regex(expectedErrStr)));

  // Region header already exists, does nothing
  streamRedirect.ClearStringStreams();
  context.linker.regions = "./Device/RteTest_ARMCM0/regions_RteTest_ARMCM0.h";
  CheckAndGenerateRegionsHeader(context);
  EXPECT_TRUE(streamRedirect.GetOutString().empty());
  EXPECT_TRUE(streamRedirect.GetErrorString().empty());
};

TEST_F(ProjMgrWorkerUnitTests, GetGeneratorDir) {
  const string& gpdscFile = testinput_folder + "/TestGenerator/RTE/Device/RteTestGen_ARMCM0/RteTest.gpdsc";
  bool validGpdsc;
  const RteGenerator* generator = ProjMgrUtils::ReadGpdscFile(gpdscFile, validGpdsc)->GetFirstGenerator();

  m_contexts[""] = ContextItem();
  ContextItem& context = m_contexts[""];
  CsolutionItem csolution;
  CprojectItem cproject;
  ClayerItem clayer;
  const string layerName = "layerName";
  const string generatorId = "RteTestGeneratorIdentifier";
  context.csolution = &csolution;
  context.cproject = &cproject;
  context.clayers[layerName] = &clayer;
  csolution.directory = testoutput_folder + "/SolutionDirectory";
  cproject.directory = csolution.directory + "/ProjectDirectory";
  clayer.directory = cproject.directory + "/LayerDirectory";
  context.directories.cprj = testoutput_folder;
  context.variables["Compiler"] = context.compiler = "AC6";
  string genDir;

  // base directory
  csolution.generators.baseDir = "BaseRelativeToSolution";
  EXPECT_TRUE(GetGeneratorDir(generator, context, layerName, genDir));
  EXPECT_EQ(genDir, "../BaseRelativeToSolution/RteTestGeneratorIdentifier");

  cproject.generators.baseDir = "BaseRelativeToProject";
  EXPECT_TRUE(GetGeneratorDir(generator, context, layerName, genDir));
  EXPECT_EQ(genDir, "BaseRelativeToProject/RteTestGeneratorIdentifier");

  clayer.generators.baseDir = "BaseRelativeToLayer";
  EXPECT_TRUE(GetGeneratorDir(generator, context, layerName, genDir));
  EXPECT_EQ(genDir, "LayerDirectory/BaseRelativeToLayer/RteTestGeneratorIdentifier");

  clayer.generators.baseDir = "$SolutionDir()$/$Compiler$";
  EXPECT_TRUE(GetGeneratorDir(generator, context, layerName, genDir));
  EXPECT_EQ(genDir, "../AC6/RteTestGeneratorIdentifier");

  clayer.generators.baseDir = "$ProjectDir(UnknownContext)$";
  EXPECT_FALSE(GetGeneratorDir(generator, context, layerName, genDir));

  // custom options directory
  csolution.generators.options[generatorId] = "CustomRelativeToSolution";
  EXPECT_TRUE(GetGeneratorDir(generator, context, layerName, genDir));
  EXPECT_EQ(genDir, "../CustomRelativeToSolution");

  cproject.generators.options[generatorId] = "CustomRelativeToProject";
  EXPECT_TRUE(GetGeneratorDir(generator, context, layerName, genDir));
  EXPECT_EQ(genDir, "CustomRelativeToProject");

  clayer.generators.options[generatorId] = "CustomRelativeToLayer";
  EXPECT_TRUE(GetGeneratorDir(generator, context, layerName, genDir));
  EXPECT_EQ(genDir, "LayerDirectory/CustomRelativeToLayer");

  clayer.generators.options[generatorId] = "$SolutionDir()$/$Compiler$";
  EXPECT_TRUE(GetGeneratorDir(generator, context, layerName, genDir));
  EXPECT_EQ(genDir, "../AC6");

  clayer.generators.options[generatorId] = "$ProjectDir(UnknownContext)$";
  EXPECT_FALSE(GetGeneratorDir(generator, context, layerName, genDir));

  m_contexts.clear();
};

TEST_F(ProjMgrWorkerUnitTests, GetGeneratorDirDefault) {
  ProjMgrParser parser;
  ContextDesc descriptor;
  const string& filename = testinput_folder + "/TestProject/test.cproject.yml";
  EXPECT_TRUE(parser.ParseCproject(filename, true));
  EXPECT_TRUE(AddContexts(parser, descriptor, filename));
  map<string, ContextItem>* contexts;
  GetContexts(contexts);
  ContextItem context = contexts->begin()->second;
  EXPECT_TRUE(LoadPacks(context));
  EXPECT_TRUE(ProcessPrecedences(context));
  EXPECT_TRUE(ProcessDevice(context));
  EXPECT_TRUE(SetTargetAttributes(context, context.targetAttributes));

  const string& gpdscFile = testinput_folder + "/TestGenerator/RTE/Device/RteTestGen_ARMCM0/RteTestNoWorkingDir.gpdsc";
  bool validGpdsc;
  const RteGenerator* generator = ProjMgrUtils::ReadGpdscFile(gpdscFile, validGpdsc)->GetFirstGenerator();
  string genDir;

  // default: $ProjectDir()$/generated/<generator-id>
  EXPECT_TRUE(GetGeneratorDir(generator, context, "", genDir));
  EXPECT_EQ(genDir, "generated/RteTestGeneratorIdentifier");
};
