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

#pragma link C++ class o2::mathUtils::Chebyshev3D+;
#pragma link C++ class o2::mathUtils::Chebyshev3DCalc+;

#pragma link C++ namespace o2::mathUtils::mathBase;

#pragma link C++ function o2::mathBase::mathBase::fitGaus<float>;
#pragma link C++ function o2::mathBase::mathBase::fitGaus<double>;

#pragma link C++ class o2::Base::Transform3D+;

#endif
