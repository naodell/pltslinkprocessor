#ifndef GUARD_EventAnalyzer_h
#define GUARD_EventAnalyzer_h

#include <vector>
#include <iostream>
#include <fstream>
#include <string>
#include <cmath>

#include "PLTEvent.h"
#include "PLTPlane.h"
#include "PLTBinaryFileReader.h"

using namespace std;
class EffCounter

{
    public:
        EffCounter () {
            denom[0] = 0;
            denom[1] = 0;
            denom[2] = 0;
            numer[0] = 0;
            numer[1] = 0;
            numer[2] = 0;
        }
        ~EffCounter () {}

        unsigned denom[3];
        unsigned numer[3];
};

class EventAnalyzer
{
    public:
        EventAnalyzer() {};
        EventAnalyzer(PLTEvent*, string, vector<unsigned>);
        ~EventAnalyzer() {};

        void          ReinitializeCounters();
        int           AnalyzeEvent();
        void          CalculateTelescopeRates(unsigned, PLTTelescope&);
        void          CalculateAccidentalRates(PLTTelescope&);
        vector<float> GetTelescopeEfficiency(int);
        float         GetTelescopeAccidentals(int);
        float         GetZeroCounting(int);

    private:
        PLTEvent                 *_event;
        PLTAlignment             *_alignment;
        PLTPlane::FiducialRegion _fidRegionHits; 

        // counters
        unsigned long _bxCounter, _delay;
        std::map<unsigned, EffCounter> _telescopeHits;
        std::map<unsigned, EffCounter> _telescopeHitsDelayed;
        std::map<unsigned, std::pair<unsigned, unsigned> > _accidentalRates;
        std::map<unsigned, std::vector<std::pair<float, float> > >  _twoHitTrackSlopes;
        std::map<unsigned, std::vector<std::pair<float, float> > >  _threeHitTrackSlopes;

        // track quality selection parameters
        float _pixelDist;
        float _slopeXLow;
        float _slopeYLow;
        float _slopeXHigh;
        float _slopeYHigh;

        std::map<unsigned, float> _meanSlopeX, _meanSlopeY;
        std::map<unsigned, float> _sigmaSlopeX, _sigmaSlopeY;
        std::map<unsigned, std::map<unsigned, float> > _meanResidualX, _meanResidualY;
        std::map<unsigned, std::map<unsigned, float> > _sigmaResidualX, _sigmaResidualY;
};

#endif
