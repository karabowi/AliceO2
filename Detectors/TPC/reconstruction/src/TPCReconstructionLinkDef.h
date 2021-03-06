// Copyright CERN and copyright holders of ALICE O2. This software is
// distributed under the terms of the GNU General Public License v3 (GPL
// Version 3), copied verbatim in the file "COPYING".
//
// See https://alice-o2.web.cern.ch/ for full licensing information.
//
// In applying this license CERN does not waive the privileges and immunities
// granted to it by virtue of its status as an Intergovernmental Organization
// or submit itself to any jurisdiction.

#ifdef __CLING__

#pragma link off all globals;
#pragma link off all classes;
#pragma link off all functions;

#pragma link C++ class o2::TPC::AdcClockMonitor;
#pragma link C++ class o2::TPC::DigitData;
#pragma link C++ class o2::TPC::GBTFrame;
#pragma link C++ class o2::TPC::GBTFrameContainer;
#pragma link C++ class o2::TPC::HalfSAMPAData;
#pragma link C++ class o2::TPC::RawReader;
#pragma link C++ class o2::TPC::SyncPatternMonitor;
#pragma link C++ class o2::TPC::TrackTPC+;

#pragma link C++ class std::vector<o2::TPC::TrackTPC>+;

#endif
