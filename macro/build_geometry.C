// Copyright CERN and copyright holders of ALICE O2. This software is
// distributed under the terms of the GNU General Public License v3 (GPL
// Version 3), copied verbatim in the file "COPYING".
//
// See https://alice-o2.web.cern.ch/ for full licensing information.
//
// In applying this license CERN does not waive the privileges and immunities
// granted to it by virtue of its status as an Intergovernmental Organization
// or submit itself to any jurisdiction.

#if (!defined(__CINT__) && !defined(__CLING__)) || defined(__MAKECINT__)
#include "TGeoManager.h"
#include "TString.h"
#include "TSystem.h"

#include "DetectorsPassive/Cave.h"
#include "DetectorsPassive/FrameStructure.h"
#include "DetectorsPassive/Magnet.h"
#include "DetectorsPassive/Absorber.h"
#include <Field/MagneticField.h>
#include <TPCSimulation/Detector.h>
#include <EMCALSimulation/Detector.h>
#include <TOFSimulation/Detector.h>
#include <TRDSimulation/Detector.h>
#include <DetectorsPassive/Cave.h>
#include <DetectorsPassive/FrameStructureRun3.h>
#include <Configuration/SimConfig.h>
#include "FairRunSim.h"
#include <algorithm>
#endif

bool isActivated(std::string s) {
// access user configuration for list of wanted modules
  auto& modulelist = o2::conf::SimConfig::Instance().getActiveDetectors();
  auto active = std::find(modulelist.begin(), modulelist.end(), s)!=modulelist.end();
  if (active) {
    std::cout << "Activating " << s << " module \n";
  }
  return active;
}

// a "factory" like macro to instantiate the O2 geometry
void build_geometry(FairRunSim* run = nullptr)
{
  bool geomonly = (run == nullptr);

  // minimal macro to test setup of the geometry
  
  TString dir = getenv("VMCWORKDIR");
  TString geom_dir = dir + "/Detectors/Geometry/";
  gSystem->Setenv("GEOMPATH", geom_dir.Data());

  TString tut_configdir = dir + "/Detectors/gconfig";
  gSystem->Setenv("CONFIG_DIR", tut_configdir.Data());

  // Create simulation run if it does not exist
  if (run == nullptr) {
    run = new FairRunSim();
    run->SetOutputFile("foo.root"); // Output file
    run->SetName("TGeant3");        // Transport engine
  }
  // Create media
  run->SetMaterials("media.geo"); // Materials

  // we need a field to properly init the media
  auto field = new o2::field::MagneticField("Maps", "Maps", -1., -1., o2::field::MagFieldParam::k5kG);
  run->SetField(field);

  // Create geometry
  // we always need the gave
  o2::Passive::Cave* cave = new o2::Passive::Cave("CAVE");
  // the experiment hall (cave)
  cave->SetGeometryFileName("cave.geo");
  run->AddModule(cave);

  // the magnet
  if (isActivated("MAG")) {
    // the frame structure to support other detectors
    auto magnet = new o2::passive::Magnet("Magnet", "L3 Magnet");
    run->AddModule(magnet);
  }

  // the absorber
  if (isActivated("ABSO")) {
    // the frame structure to support other detectors
    auto abso = new o2::passive::Absorber("Absorber", "Absorber");
    run->AddModule(abso);
  }
  
  if (isActivated("TOF") || isActivated("TRD") || isActivated("FRAME")) {
    // the frame structure to support other detectors
    auto frame = new o2::passive::FrameStructure("Frame", "Frame");
    run->AddModule(frame);
  }

  if (isActivated("TOF")){
    // TOF
    auto tof = new o2::tof::Detector("TOF", true);
    run->AddModule(tof);
  }

  if (isActivated("TRD")) {
    // TRD
    auto trd = new o2::trd::Detector("TRD", true);
    run->AddModule(trd);
  }

  if (isActivated("TPC")){
    // tpc
    auto tpc = new o2::TPC::Detector("TPC", true);
    run->AddModule(tpc);
  }

  if (isActivated("EMCAL")){
    // emcal
    run->AddModule(new o2::EMCAL::Detector("EMCAL", true));
  }
   
  if (geomonly) {
    run->Init();
    gGeoManager->Export("O2geometry.root");
  }
}
