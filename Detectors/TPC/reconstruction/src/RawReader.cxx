// Copyright CERN and copyright holders of ALICE O2. This software is
// distributed under the terms of the GNU General Public License v3 (GPL
// Version 3), copied verbatim in the file "COPYING".
//
// See https://alice-o2.web.cern.ch/ for full licensing information.
//
// In applying this license CERN does not waive the privileges and immunities
// granted to it by virtue of its status as an Intergovernmental Organization
// or submit itself to any jurisdiction.

/// \file RawReader.cxx
/// \author Sebastian Klewin (Sebastian.Klewin@cern.ch)

#include <boost/tokenizer.hpp>
#include <boost/lexical_cast.hpp>
#include <iostream>
#include <iomanip>
#include <bitset>
#include <queue>

#include "TPCReconstruction/RawReader.h"
#include "TPCReconstruction/GBTFrame.h"
#include "TPCReconstruction/SyncPatternMonitor.h" 
#include "TPCReconstruction/AdcClockMonitor.h" 
#include "TPCBase/Mapper.h"

#include "FairLogger.h" 

using namespace o2::TPC;

RawReader::RawReader(int region, int link, int run)
  : mRegion(region)
  , mLink(link)
  , mRun(run)
  , mLastEvent(-1)
  , mUseRawInMode3(true)
  , mApplyChannelMask(false)
  , mCheckAdcClock(false)
  , mPrintRawData(false)
  , mTimestampOfFirstData({0,0,0,0,0})
  , mEvents()
  , mData()
  , mDataIterator(mData.end())
  , mSyncPos()
  , mChannelMask(nullptr)
{
  mSyncPos.fill(-1);
}

bool RawReader::addInputFile(const std::vector<std::string>* infiles) {
  bool ret = false;
  for (const auto &f : *infiles) {
    ret |= addInputFile(f);
  }

  return ret;
}

bool RawReader::addInputFile(std::string infile) {
  typedef boost::tokenizer<boost::char_separator<char> >  tokenizer;
  boost::char_separator<char> sep{":"};
  tokenizer tok{infile,sep};
  if (std::distance(tok.begin(), tok.end()) < 3) {
    LOG(ERROR) << "Not enough arguments" << FairLogger::endl;
    return false;
  }

  tokenizer::iterator it1= tok.begin();
  std::string path = boost::lexical_cast<std::string>(*it1);
  int run = std::stoi(path.substr(path.find("run")+3,6));

  int region, link;
  try {
//    std::advance(it1,1);
    ++it1;
    region = boost::lexical_cast<int>(*it1);
  } catch(boost::bad_lexical_cast) {
    LOG(ERROR) << "Please enter a region for " << path << FairLogger::endl;
    return false;
  }

  try {
//    std::advance(it1,1);
    ++it1;
    link = boost::lexical_cast<int>(*it1);
  } catch(boost::bad_lexical_cast) {
    LOG(ERROR) << "Please enter a link number for " << FairLogger::endl;
    return false;
  }

  return addInputFile(region, link, path, run);
}

bool RawReader::addInputFile(int region, int link, std::string path, int run){

  if (mRun == -1) mRun = run;
  if (mRegion == -1 ) mRegion = region;
  if (mLink == -1) mLink = link;

  if (run != mRun) {
    LOG(DEBUG) << "Run of RawReader is " << mRun << " and not " << run << FairLogger::endl;
    return false;
  }

  if (region != mRegion) {
    LOG(DEBUG) << "Region of RawReader is " << mRegion << " and not " << region << FairLogger::endl;
    return false;
  }
  
  if (link != mLink) {
    LOG(DEBUG) << "Link of RawReader is " << mLink << " and not " << link << FairLogger::endl;
    return false;
  }

  std::ifstream file(path);
  if (!file.is_open()) {
    LOG(ERROR) << "Can't read file " << path << FairLogger::endl;
    return false;
  }

  Header h;
  int pos = 0;
  file.seekg(0,file.end);
  int length = file.tellg();
  file.seekg(0,file.beg);

  while (pos < length) {
    file.read((char*)&h, sizeof(h));
    if (h.reserved_01 != 0x0F || h.reserved_2() != 0x3fec2fec1fec0fec) {
      LOG(ERROR) << "Header does not look consistent" << FairLogger::endl;
    }
    EventInfo eD;
    eD.path = path;
    eD.posInFile =  file.tellg();
    eD.header = h;
    if (h.headerVersion == 1) {

//      auto ins = mEvents.insert(std::make_pair(h.eventCount(),std::shared_ptr<std::vector<EventInfo>>(new std::vector<EventInfo>)));
      auto ins = mEvents.insert(std::make_pair(h.eventCount(),std::make_shared<std::vector<EventInfo>>()));
      ins.first->second->push_back(eD);

      file.seekg(pos+(h.nWords*4));
      pos = file.tellg();
    } else {
      LOG(ERROR) << "Header version " << (int)h.headerVersion << " not implemented." << FairLogger::endl;
      return false;
    }
  }

  return true;
}

bool RawReader::loadEvent(int64_t event) {
  LOG(DEBUG) << "Loading event " << event << FairLogger::endl;

  if (mLastEvent == -1 && event != getFirstEvent() ) {
    LOG(DEBUG) << "First event was not loaded (maybe important for decoding RAW GBT frames) lading first event." << FairLogger::endl;
    mSyncPos.fill(-1);
    mTimestampOfFirstData.fill(0);
    loadEvent(getFirstEvent());
    LOG(DEBUG) << "Continue with event " << event << FairLogger::endl;
  }
  mData.clear();

  auto ev = mEvents.find(event);

  if (ev == mEvents.end()) return false;
  mLastEvent = event;
  
  const Mapper& mapper = Mapper::instance();
  for (auto &eventInfo : *(ev->second)) {
    if (mPrintRawData) {
      LOG(INFO) << "Header:" << FairLogger::endl;
      LOG(INFO) << std::setfill(' ') << std::left << std::setw(16) << "Version" 
        << "0x" << std::hex << std::setfill('0') << std::right << std::setw(2) << (unsigned) eventInfo.header.headerVersion << FairLogger::endl;
//      LOG(INFO) << std::setfill(' ') << std::left << std::setw(16) << "Reserved"
//        << "0x" << std::hex << std::setfill('0') << std::right << std::setw(2) << (unsigned) eventInfo.header.reserved_01 << FairLogger::endl;
      LOG(INFO) << std::setfill(' ') << std::left << std::setw(16) << "Datatype"
        << "0x" << std::hex << std::setfill('0') << std::right << std::setw(4) << eventInfo.header.dataType << FairLogger::endl;
      LOG(INFO) << std::setfill(' ') << std::left << std::setw(16) << "Number of Words"
        << "0x" << std::hex << std::setfill('0') << std::right << std::setw(8) << eventInfo.header.nWords << FairLogger::endl;
      LOG(INFO) << std::setfill(' ') << std::left << std::setw(16) << "Timestamp"
        << "0x" << std::hex << std::setfill('0') << std::right << std::setw(16) << eventInfo.header.timeStamp() << FairLogger::endl;
      LOG(INFO) << std::setfill(' ') << std::left << std::setw(16) << "Event counter"
        << "0x" << std::hex << std::setfill('0') << std::right << std::setw(16) << eventInfo.header.eventCount() << FairLogger::endl;
//      LOG(INFO) << std::setfill(' ') << std::left << std::setw(16) << "Reserved2"
//        << "0x" << std::hex << std::setfill('0') << std::right << std::setw(16) << eventInfo.header.reserved_2() << FairLogger::endl;
      LOG(INFO) << std::dec << FairLogger::endl;
    }
    switch (eventInfo.header.dataType) {
      case 1: // RAW GBT frames
        { 
          LOG(DEBUG) << "Data of readout mode 1 (RAW GBT frames)" << FairLogger::endl;
          if (!decodeRawGBTFrames(eventInfo)) return false;
          break;
        }
      case 2: // Decoded data
        {
          LOG(DEBUG) << "Data of readout mode 2 (decoded data)" << FairLogger::endl;
          if (!decodePreprocessedData(eventInfo)) return false;
          break;
        }
      case 3: // both, RAW GBT frames and decoded data
        {
          if (mUseRawInMode3) {
            LOG(DEBUG) << "Data of readout mode 3 (decoding RAW GBT frames)" << FairLogger::endl;
            if (!decodeRawGBTFrames(eventInfo)) return false;
          } else {
            LOG(DEBUG) << "Data of readout mode 3 (using decoded data)" << FairLogger::endl;
            if (!decodePreprocessedData(eventInfo)) return false;
          }
          break;
        }
      default: 
        {
          LOG(DEBUG) << "Readout mode not known" << FairLogger::endl;
          break;
        }
    }
  }

  mDataIterator = mData.begin();
  return true;
}

bool RawReader::decodePreprocessedData(EventInfo eventInfo) {
  std::ifstream file(eventInfo.path);
  if (!file.is_open()) {
    LOG(ERROR) << "Can't read file " << eventInfo.path << FairLogger::endl;
    return false;
  }

  int nWords = eventInfo.header.nWords-8;
  uint32_t words[nWords];
  LOG(DEBUG) << "reading " << nWords << " words from position " << eventInfo.posInFile << " in file " << eventInfo.path << FairLogger::endl;
  file.seekg(eventInfo.posInFile);
  file.read((char*)&words, nWords*sizeof(words[0]));

  const Mapper& mapper = Mapper::instance();

  std::array<char,5> sampas;
  std::array<char,5> sampaChannelStart;
  for (char i=0; i<5; ++i) {
    sampas[i] = (i == 4) ? 2 : (mRegion%2) ? i/2+3 : i/2;
    sampaChannelStart[i] = (i == 4) ?   // 5th half SAMPA corresponds to  SAMPA2
      ((mRegion%2) ? 16 : 0) :      // every even CRU receives channel 0-15 from SAMPA 2, the odd ones channel 16-31
      ((i%2) ? 16 : 0);                 // every even half SAMPA containes channel 0-15, the odd ones channel 16-31
  }


  std::array<uint32_t,5> ids;
  std::array<bool,5> writeValue;
  writeValue.fill(false);
  std::array<std::array<uint16_t,16>,5> adcValues;


  int indexStep = (eventInfo.header.dataType == 3) ? 8 : 4;
  int offset = (eventInfo.header.dataType == 3) ? 4: 0;

  for (int i=0; i<nWords; i=i+indexStep) {
    ids[4] = (words[i+offset] >> 4) & 0xF;
    ids[3] = (words[i+offset] >> 8) & 0xF;
    ids[2] = (words[i+offset] >> 12) & 0xF;
    ids[1] = (words[i+offset] >> 16) & 0xF;
    ids[0] = (words[i+offset] >> 20) & 0xF;

    adcValues[4][((ids[4] & 0x7)*2)+1] = (((ids[4]>>3)&0x1) == 0) ? 0 :   words[i+offset+3] & 0x3FF;
    adcValues[4][((ids[4] & 0x7)*2)  ] = (((ids[4]>>3)&0x1) == 0) ? 0 :  (words[i+offset+3] >> 10) & 0x3FF;
    adcValues[3][((ids[3] & 0x7)*2)+1] = (((ids[3]>>3)&0x1) == 0) ? 0 :  (words[i+offset+3] >> 20) & 0x3FF;
    adcValues[3][((ids[3] & 0x7)*2)  ] = (((ids[3]>>3)&0x1) == 0) ? 0 : ((words[i+offset+2] & 0xFF) << 2) | ((words[i+offset+3] >> 30) & 0x3);
    adcValues[2][((ids[2] & 0x7)*2)+1] = (((ids[2]>>3)&0x1) == 0) ? 0 :  (words[i+offset+2] >> 8) & 0x3FF;
    adcValues[2][((ids[2] & 0x7)*2)  ] = (((ids[2]>>3)&0x1) == 0) ? 0 :  (words[i+offset+2] >> 18) & 0x3FF;
    adcValues[1][((ids[1] & 0x7)*2)+1] = (((ids[1]>>3)&0x1) == 0) ? 0 : ((words[i+offset+1] & 0x3F) << 4) | ((words[i+offset+2] >> 28) & 0xF);
    adcValues[1][((ids[1] & 0x7)*2)  ] = (((ids[1]>>3)&0x1) == 0) ? 0 :  (words[i+offset+1] >> 6) & 0x3FF;
    adcValues[0][((ids[0] & 0x7)*2)+1] = (((ids[0]>>3)&0x1) == 0) ? 0 :  (words[i+offset+1] >> 16) & 0x3FF;
    adcValues[0][((ids[0] & 0x7)*2)  ] = (((ids[0]>>3)&0x1) == 0) ? 0 : ((words[i+offset+0] & 0xF) << 6) | ((words[i+offset+1] >> 26) & 0x3F);

    if (mPrintRawData) {
      LOG(INFO) << std::dec << std::setfill('0') << std::right << std::setw(5) << (i/indexStep*2) << ": "
        << "(" << ((ids[0] >> 3) & 0x1) << ") " 
        << "0x" << std::hex << std::setfill('0') << std::right << std::setw(3) << adcValues[0][((ids[0] & 0x7)*2)] << " "
        << "(" << std::hex << std::setfill('0') << std::right << std::setw(1) << (ids[0] & 0x7) << ")  " 

        << "(" << ((ids[0] >> 3) & 0x1) << ") "
        << "0x" << std::hex << std::setfill('0') << std::right << std::setw(3) << adcValues[1][((ids[1] & 0x7)*2)] << " "
        << "(" << std::hex << std::setfill('0') << std::right << std::setw(1) << (ids[1] & 0x7) << ")  " 

        << "(" << ((ids[0] >> 3) & 0x1) << ") "
        << "0x" << std::hex << std::setfill('0') << std::right << std::setw(3) << adcValues[2][((ids[2] & 0x7)*2)] << " "
        << "(" << std::hex << std::setfill('0') << std::right << std::setw(1) << (ids[2] & 0x7) << ")  " 
        
        << "(" << ((ids[0] >> 3) & 0x1) << ") "
        << "0x" << std::hex << std::setfill('0') << std::right << std::setw(3) << adcValues[3][((ids[3] & 0x7)*2)] << " "
        << "(" << std::hex << std::setfill('0') << std::right << std::setw(1) << (ids[3] & 0x7) << ")  " 
        
        << "(" << ((ids[0] >> 3) & 0x1) << ") "
        << "0x" << std::hex << std::setfill('0') << std::right << std::setw(3) << adcValues[4][((ids[4] & 0x7)*2)] << " " 
        << "(" << std::hex << std::setfill('0') << std::right << std::setw(1) << (ids[4] & 0x7) << ")" << std::dec << FairLogger::endl;

      LOG(INFO) << std::dec << std::setfill('0') << std::right << std::setw(5) << (i/indexStep*2+1) << ":     "
        << "0x" << std::hex << std::setfill('0') << std::right << std::setw(3) << adcValues[0][((ids[0] & 0x7)*2)+1] << "          "
        << "0x" << std::hex << std::setfill('0') << std::right << std::setw(3) << adcValues[1][((ids[1] & 0x7)*2)+1] << "          "
        << "0x" << std::hex << std::setfill('0') << std::right << std::setw(3) << adcValues[2][((ids[2] & 0x7)*2)+1] << "          "
        << "0x" << std::hex << std::setfill('0') << std::right << std::setw(3) << adcValues[3][((ids[3] & 0x7)*2)+1] << "          "
        << "0x" << std::hex << std::setfill('0') << std::right << std::setw(3) << adcValues[4][((ids[4] & 0x7)*2)+1] << std::dec << FairLogger::endl;
    }

    for (char j=0; j<5; ++j) {
      if (ids[j] == 0x8) {
        writeValue[j] = true;
        // header TS is one before first word -> +1
        if (mTimestampOfFirstData[j] == 0) mTimestampOfFirstData[j] = eventInfo.header.timeStamp() + 1 + i/8;
      }
    }

    for (char j=0; j<5; ++j) {
      if (writeValue[j] & (ids[j] == 0xF)) {
        for (int k=0; k<16; ++k) {
          const PadPos padPos = mapper.padPosRegion(
              mRegion, mLink,sampas[j],k+sampaChannelStart[j]);

          if (mApplyChannelMask &&          // channel mask should be applied
              (mChannelMask != nullptr) &&  // channel mask is available
              !mChannelMask->getValue(CRU(mRegion),padPos.getPad(),padPos.getRow())) { // channel mask
              continue;
          }

//          auto ins = mData.insert(std::make_pair(padPos, std::shared_ptr<std::vector<uint16_t>>(new std::vector<uint16_t>)));
          auto ins = mData.insert(std::make_pair(padPos, std::make_shared<std::vector<uint16_t>>()));
          ins.first->second->push_back(adcValues[j][k]);
        }
      }
    }

  }
  return true;
}

bool RawReader::decodeRawGBTFrames(EventInfo eventInfo) {
  std::ifstream file(eventInfo.path);
  if (!file.is_open()) {
    LOG(ERROR) << "Can't read file " << eventInfo.path << FairLogger::endl;
    return false;
  }

  int nWords = eventInfo.header.nWords-8;
  uint32_t words[nWords];
  LOG(DEBUG) << "reading " << nWords << " words from position " << eventInfo.posInFile << " in file " << eventInfo.path << FairLogger::endl;
  file.seekg(eventInfo.posInFile);
  file.read((char*)&words, nWords*sizeof(words[0]));

  const Mapper& mapper = Mapper::instance();

  std::array<char,5> sampas;
  std::array<char,5> sampaChannelStart;
  for (char i=0; i<5; ++i) {
    sampas[i] = (i == 4) ? 2 : (mRegion%2) ? i/2+3 : i/2;
    sampaChannelStart[i] = (i == 4) ?   // 5th half SAMPA corresponds to  SAMPA2
      ((mRegion%2) ? 16 : 0) :      // every even CRU receives channel 0-15 from SAMPA 2, the odd ones channel 16-31
      ((i%2) ? 16 : 0);                 // every even half SAMPA containes channel 0-15, the odd ones channel 16-31
  }

  std::array<SyncPatternMonitor,5> syncMon{
    SyncPatternMonitor(0,0),
    SyncPatternMonitor(0,1),
    SyncPatternMonitor(1,0),
    SyncPatternMonitor(1,1),
    SyncPatternMonitor(2,0)};
  std::array<AdcClockMonitor,3> adcClockMon{
    AdcClockMonitor(0),
    AdcClockMonitor(1),
    AdcClockMonitor(2)};
  std::array<bool,3> adcClockFound{false, false, false};
  std::array<uint64_t,3> adcSequence{0,0,0};

  std::array<short,5> lastSyncPos;
  std::array<std::queue<uint16_t>,5> adcValues;
  GBTFrame frame; 
  GBTFrame lastFrame; 
  mAdcError.clear();
  uint64_t timebin = 0;

  int indexStep = (eventInfo.header.dataType == 3) ? 8 : 4;
  for (int i=0; i<nWords; i=i+indexStep) {

    for (char j=0; j<5; ++j) {
      if ((mTimestampOfFirstData[j] != 0) && 
          ((mTimestampOfFirstData[j] & 0x7) == ((eventInfo.header.timeStamp() + 1 + i/indexStep) & 0x7))) {
        if (adcValues[j].size() < 16) {
          std::queue<uint16_t> empty;
          std::swap(adcValues[j], empty);
          continue;
        }
        ++timebin;
        for (int k=0; k<16; ++k) {
          const PadPos padPos = mapper.padPosRegion(
              mRegion, mLink,sampas[j],k+sampaChannelStart[j]);

          if (mApplyChannelMask &&          // channel mask should be applied
              (mChannelMask != nullptr) &&  // channel mask is available
              !mChannelMask->getValue(CRU(mRegion),padPos.getPad(),padPos.getRow())) {     // channel mask
              adcValues[j].pop();           // discard value
              continue;
          }

//          auto ins = mData.insert(std::make_pair(padPos, std::shared_ptr<std::vector<uint16_t>>(new std::vector<uint16_t>)));
          auto ins = mData.insert(std::make_pair(padPos, std::make_shared<std::vector<uint16_t>>()));
          ins.first->second->push_back(adcValues[j].front());
          adcValues[j].pop();
        }
      }
    }

    lastSyncPos = mSyncPos;
    lastFrame = frame;
    frame.setData(words[i],words[i+1],words[i+2],words[i+3]);
    

    if (mPrintRawData) {
      LOG(INFO) << std::dec << std::setfill('0') << std::right << std::setw(5) << (i/indexStep*4) << ": ";
      LOG(INFO)
        << "0x" << std::hex << std::setfill('0') << std::right << std::setw(2) << frame.getHalfWord(0,0,0) << " (" << ((frame.getAdcClock(0) >> 3) & 0x1) << ") "
        << "0x" << std::hex << std::setfill('0') << std::right << std::setw(2) << frame.getHalfWord(0,0,1) << " (" << ((frame.getAdcClock(0) >> 3) & 0x1) << ") "
        << "0x" << std::hex << std::setfill('0') << std::right << std::setw(2) << frame.getHalfWord(1,0,0) << " (" << ((frame.getAdcClock(1) >> 3) & 0x1) << ") "
        << "0x" << std::hex << std::setfill('0') << std::right << std::setw(2) << frame.getHalfWord(1,0,1) << " (" << ((frame.getAdcClock(1) >> 3) & 0x1) << ") "
        << "0x" << std::hex << std::setfill('0') << std::right << std::setw(2) << frame.getHalfWord(0,0,0) << " (" << ((frame.getAdcClock(2) >> 3) & 0x1) << ") "
        << std::dec << FairLogger::endl;
      LOG(INFO) << std::dec << std::setfill('0') << std::right << std::setw(5) << (i/indexStep*4+1) << ": ";
      LOG(INFO)
        << "0x" << std::hex << std::setfill('0') << std::right << std::setw(2) << frame.getHalfWord(0,1,0) << " (" << ((frame.getAdcClock(0) >> 2) & 0x1) << ") "
        << "0x" << std::hex << std::setfill('0') << std::right << std::setw(2) << frame.getHalfWord(0,1,1) << " (" << ((frame.getAdcClock(0) >> 2) & 0x1) << ") "
        << "0x" << std::hex << std::setfill('0') << std::right << std::setw(2) << frame.getHalfWord(1,1,0) << " (" << ((frame.getAdcClock(1) >> 2) & 0x1) << ") "
        << "0x" << std::hex << std::setfill('0') << std::right << std::setw(2) << frame.getHalfWord(1,1,1) << " (" << ((frame.getAdcClock(1) >> 2) & 0x1) << ") "
        << "0x" << std::hex << std::setfill('0') << std::right << std::setw(2) << frame.getHalfWord(0,1,0) << " (" << ((frame.getAdcClock(2) >> 2) & 0x1) << ") "
        << std::dec << FairLogger::endl;
      LOG(INFO) << std::dec << std::setfill('0') << std::right << std::setw(5) << (i/indexStep*4+2) << ": ";
      LOG(INFO)
        << "0x" << std::hex << std::setfill('0') << std::right << std::setw(2) << frame.getHalfWord(0,2,0) << " (" << ((frame.getAdcClock(0) >> 1) & 0x1) << ") "
        << "0x" << std::hex << std::setfill('0') << std::right << std::setw(2) << frame.getHalfWord(0,2,1) << " (" << ((frame.getAdcClock(0) >> 1) & 0x1) << ") "
        << "0x" << std::hex << std::setfill('0') << std::right << std::setw(2) << frame.getHalfWord(1,2,0) << " (" << ((frame.getAdcClock(1) >> 1) & 0x1) << ") "
        << "0x" << std::hex << std::setfill('0') << std::right << std::setw(2) << frame.getHalfWord(1,2,1) << " (" << ((frame.getAdcClock(1) >> 1) & 0x1) << ") "
        << "0x" << std::hex << std::setfill('0') << std::right << std::setw(2) << frame.getHalfWord(0,2,0) << " (" << ((frame.getAdcClock(2) >> 1) & 0x1) << ") "
        << std::dec << FairLogger::endl;
      LOG(INFO) << std::dec << std::setfill('0') << std::right << std::setw(5) << (i/indexStep*4+3) << ": ";
      LOG(INFO)
        << "0x" << std::hex << std::setfill('0') << std::right << std::setw(2) << frame.getHalfWord(0,3,0) << " (" << ((frame.getAdcClock(0) >> 0) & 0x1) << ") "
        << "0x" << std::hex << std::setfill('0') << std::right << std::setw(2) << frame.getHalfWord(0,3,1) << " (" << ((frame.getAdcClock(0) >> 0) & 0x1) << ") "
        << "0x" << std::hex << std::setfill('0') << std::right << std::setw(2) << frame.getHalfWord(1,3,0) << " (" << ((frame.getAdcClock(1) >> 0) & 0x1) << ") "
        << "0x" << std::hex << std::setfill('0') << std::right << std::setw(2) << frame.getHalfWord(1,3,1) << " (" << ((frame.getAdcClock(1) >> 0) & 0x1) << ") "
        << "0x" << std::hex << std::setfill('0') << std::right << std::setw(2) << frame.getHalfWord(0,3,0) << " (" << ((frame.getAdcClock(2) >> 0) & 0x1) << ") "
        << std::dec << FairLogger::endl;

    }


    if (syncMon[0].addSequence(
          frame.getHalfWord(0,0,0),
          frame.getHalfWord(0,1,0),
          frame.getHalfWord(0,2,0),
          frame.getHalfWord(0,3,0))) mSyncPos[0] = syncMon[0].getPosition();

    if (syncMon[1].addSequence(
          frame.getHalfWord(0,0,1),
          frame.getHalfWord(0,1,1),
          frame.getHalfWord(0,2,1),
          frame.getHalfWord(0,3,1))) mSyncPos[1] = syncMon[1].getPosition();

    if (syncMon[2].addSequence(
          frame.getHalfWord(1,0,0),
          frame.getHalfWord(1,1,0),
          frame.getHalfWord(1,2,0),
          frame.getHalfWord(1,3,0))) mSyncPos[2] = syncMon[2].getPosition();

    if (syncMon[3].addSequence(
          frame.getHalfWord(1,0,1),
          frame.getHalfWord(1,1,1),
          frame.getHalfWord(1,2,1),
          frame.getHalfWord(1,3,1))) mSyncPos[3] = syncMon[3].getPosition();

    if (syncMon[4].addSequence(
          frame.getHalfWord(2,0),
          frame.getHalfWord(2,1),
          frame.getHalfWord(2,2),
          frame.getHalfWord(2,3))) mSyncPos[4] = syncMon[4].getPosition();

    if (mCheckAdcClock) {
      adcSequence[0] = (adcSequence[0] << 4) | (frame.getAdcClock(0) & 0xF);
      adcSequence[1] = (adcSequence[1] << 4) | (frame.getAdcClock(1) & 0xF);
      adcSequence[2] = (adcSequence[2] << 4) | (frame.getAdcClock(2) & 0xF);

      bool adcCheckErr0 = adcClockMon[0].addSequence(frame.getAdcClock(0));
      bool adcCheckErr1 = adcClockMon[1].addSequence(frame.getAdcClock(1));
      bool adcCheckErr2 = adcClockMon[2].addSequence(frame.getAdcClock(2));

//      std::cout << std::bitset<4>(frame.getAdcClock(0)) << " " << adcCheckErr0 << "\t" 
//        << std::bitset<4>(frame.getAdcClock(1)) << " " << adcCheckErr1 << "\t"
//        << std::bitset<4>(frame.getAdcClock(2)) << " " << adcCheckErr2 << std::endl;

//      std::cout << adcCheckErr0 << " " << adcCheckErr1 << " " << adcCheckErr2 << std::endl;
      if (mSyncPos[0] >= 0) adcClockFound[0] = adcClockFound[0] | !adcCheckErr0;
      if (mSyncPos[2] >= 0) adcClockFound[1] = adcClockFound[1] | !adcCheckErr1;
      if (mSyncPos[4] >= 0) adcClockFound[2] = adcClockFound[2] | !adcCheckErr2;
//      std::cout << adcClockFound[0] << " " << adcClockFound[1] << " " << adcClockFound[2] << std::endl;
      if(adcClockFound[0] & adcCheckErr0) {
        adcClockFound[0] = false;
        LOG(DEBUG) << "ADC clock 0 mon error in " << i << " " << adcValues[0].size() << " " << i/indexStep << " " << timebin/5 << FairLogger::endl;
//        std::cout << std::bitset<32>(adcSequence[0]&0xFFFFFFFF) << std::endl;
        mAdcError.emplace_back(std::make_tuple(0,timebin/5,mSyncPos[0]));
//        std::cout << 0 << " " 
//          << adcValues[0].size() << " "
//          << adcValues[1].size() << " "
//          << adcValues[2].size() << " "
//          << adcValues[3].size() << " "
//          << adcValues[4].size() << std::endl;
      };
      if(adcClockFound[1] & adcCheckErr1) {
        adcClockFound[1] = false;
        LOG(DEBUG) << "ADC clock 1 mon error in " << i << " " << adcValues[1].size() << " " << i/indexStep << " " << timebin/5 << FairLogger::endl;
//        std::cout << std::bitset<32>(adcSequence[1]&0xFFFFFFFF) << std::endl;
        mAdcError.emplace_back(std::make_tuple(1,timebin/5,mSyncPos[2]));
//       std::cout << 1 << " " 
//         << adcValues[0].size() << " "
//         << adcValues[1].size() << " "
//         << adcValues[2].size() << " "
//         << adcValues[3].size() << " "
//         << adcValues[4].size() << std::endl;
      };
      if(adcClockFound[2] & adcCheckErr2) {
        adcClockFound[2] = false;
        LOG(DEBUG) << "ADC clock 2 mon error in " << i << " " << adcValues[4].size() << " " << i/indexStep << " " << timebin/5 << FairLogger::endl;
//        std::cout << std::bitset<32>(adcSequence[2]&0xFFFFFFFF) << std::endl;
        mAdcError.emplace_back(std::make_tuple(2,timebin/5,mSyncPos[4]));
//       std::cout << 2 << " " 
//         << adcValues[0].size() << " "
//         << adcValues[1].size() << " "
//         << adcValues[2].size() << " "
//         << adcValues[3].size() << " "
//         << adcValues[4].size() << std::endl;
      };
    }

    short value1;
    short value2;
    for (short iHalfSampa = 0; iHalfSampa < 5; ++iHalfSampa) {
      if (mSyncPos[iHalfSampa] < 0 ) continue;
      if (lastSyncPos[iHalfSampa] < 0 ) continue;
      if (mTimestampOfFirstData[iHalfSampa] == 0) mTimestampOfFirstData[iHalfSampa] = eventInfo.header.timeStamp() + 1 + i/indexStep;

      switch(mSyncPos[iHalfSampa]) {
        case 0:
          value1 = (frame.getHalfWord(iHalfSampa/2,1,iHalfSampa%2) << 5) |
            frame.getHalfWord(iHalfSampa/2,0,iHalfSampa%2);
          value2 = (frame.getHalfWord(iHalfSampa/2,3,iHalfSampa%2) << 5) | 
            frame.getHalfWord(iHalfSampa/2,2,iHalfSampa%2);
          break;

        case 1:
          if (i==0) continue;
          value1 = (lastFrame.getHalfWord(iHalfSampa/2,2,iHalfSampa%2) << 5) |
            lastFrame.getHalfWord(iHalfSampa/2,1,iHalfSampa%2);
          value2 = (frame.getHalfWord(iHalfSampa/2,0,iHalfSampa%2) << 5) |
            lastFrame.getHalfWord(iHalfSampa/2,3,iHalfSampa%2);
          break;

        case 2:
          if (i==0) continue;
          value1 = (lastFrame.getHalfWord(iHalfSampa/2,3,iHalfSampa%2) << 5) |
            lastFrame.getHalfWord(iHalfSampa/2,2,iHalfSampa%2);
          value2 = (frame.getHalfWord(iHalfSampa/2,1,iHalfSampa%2) << 5) | 
            frame.getHalfWord(iHalfSampa/2,0,iHalfSampa%2);
          break;

        case 3:
          if (i==0) continue;
          value1 = (frame.getHalfWord(iHalfSampa/2,0,iHalfSampa%2) << 5) |
            lastFrame.getHalfWord(iHalfSampa/2,3,iHalfSampa%2);
          value2 = (frame.getHalfWord(iHalfSampa/2,2,iHalfSampa%2) << 5) | 
            frame.getHalfWord(iHalfSampa/2,1,iHalfSampa%2);
          break;

        default:
          return false;
      }

      adcValues[iHalfSampa].emplace ( value1 ^ (1 << 9));
      adcValues[iHalfSampa].emplace ( value2 ^ (1 << 9));
    }
  }
  return true;
}

