// Copyright CERN and copyright holders of ALICE O2. This software is
// distributed under the terms of the GNU General Public License v3 (GPL
// Version 3), copied verbatim in the file "COPYING".
//
// See https://alice-o2.web.cern.ch/ for full licensing information.
//
// In applying this license CERN does not waive the privileges and immunities
// granted to it by virtue of its status as an Intergovernmental Organization
// or submit itself to any jurisdiction.

/// \file DigitTime.cxx
/// \brief Implementation of the Time Bin container
/// \author Andi Mathis, TU München, andreas.mathis@ph.tum.de

#include "TPCSimulation/DigitTime.h"
#include "TPCSimulation/DigitRow.h"
#include "TPCBase/Mapper.h"

using namespace o2::TPC;

void DigitTime::setDigit(size_t hitID, int cru, int row, int pad, float charge)
{
  /// Check whether the container at this spot already contains an entry
  DigitRow *result = mRows[row].get();
  if(result != nullptr) {
    mRows[row]->setDigit(hitID, pad, charge);
  }
  else{
    const Mapper& mapper = Mapper::instance();
    mRows[row] = std::unique_ptr<DigitRow> (new DigitRow(row, mapper.getPadRegionInfo(CRU(cru).region()).getPadsInRowRegion(row)));
    mRows[row]->setDigit(hitID, pad, charge);
  }
  mTotalChargeTimeBin+=charge;
}

void DigitTime::fillOutputContainer(TClonesArray *output, TClonesArray *debug, int cru, int timeBin, float commonMode)
{
  for(auto &aRow : mRows) {
    if(aRow == nullptr) continue;
    aRow->fillOutputContainer(output, debug, cru, timeBin, aRow->getRow(), commonMode);
  }
}
