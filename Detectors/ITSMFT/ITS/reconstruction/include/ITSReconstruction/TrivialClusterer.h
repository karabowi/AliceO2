// Copyright CERN and copyright holders of ALICE O2. This software is
// distributed under the terms of the GNU General Public License v3 (GPL
// Version 3), copied verbatim in the file "COPYING".
//
// See https://alice-o2.web.cern.ch/ for full licensing information.
//
// In applying this license CERN does not waive the privileges and immunities
// granted to it by virtue of its status as an Intergovernmental Organization
// or submit itself to any jurisdiction.

/// \file TrivialClusterer.h
/// \brief Definition of the ITS cluster finder
#ifndef ALICEO2_ITS_TRIVIALCLUSTERER_H
#define ALICEO2_ITS_TRIVIALCLUSTERER_H

#include "Rtypes.h"  // for TrivialClusterer::Class, Double_t, ClassDef, etc

class TClonesArray;

namespace o2 {
  namespace ITSMFT {
    class SegmentationPixel;
  }
}

namespace o2
{
namespace ITS
{
  class TrivialClusterer
{
 public:
  TrivialClusterer();
  ~TrivialClusterer();

  TrivialClusterer(const TrivialClusterer&) = delete;
  TrivialClusterer& operator=(const TrivialClusterer&) = delete;

  /// Steer conversion of points to digits
  /// @param points Container with ITS points
  /// @return digits container
  void process(const o2::ITSMFT::SegmentationPixel *seg, const TClonesArray* digits, TClonesArray* clusters);

};
}
}

#endif /* ALICEO2_ITS_TRIVIALCLUSTERER_H */
