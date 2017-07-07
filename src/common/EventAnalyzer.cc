
#include "bril/pltslinkprocessor/EventAnalyzer.h"

EventAnalyzer::EventAnalyzer(PLTEvent *evt, std::string alignmentFile, vector<unsigned> channels)
{
    // Initialize the beam crossing counter
    _bxCounter = 0;

    // Point to the event object
    _event = evt; 

    // Set the fiducial detector region
    _fidRegionHits = PLTPlane::kFiducialRegion_FullSensor;
    _event->SetPlaneFiducialRegion(_fidRegionHits);
    _event->SetPlaneClustering(PLTPlane::kClustering_AllTouching, PLTPlane::kFiducialRegion_All);

    // Read in alignment file
    _alignment = new PLTAlignment();
    if (alignmentFile == "") {
        _alignment->ReadAlignmentFile("/nfshome0/naodell/plt/daq/bril/pltslinkprocessor/data/Trans_Alignment_4895.dat");
    } else {
        _alignment->ReadAlignmentFile(alignmentFile);

    }
    // Track quality cuts 
    _pixelDist  = 5;//<-Original
    _slopeYLow  = 0.027 - 0.01;
    _slopeXLow  = 0.0 - 0.01;
    _slopeXHigh = 0.0 + 0.01;
    _slopeYHigh = 0.027 + 0.01;

    // initialize counters and get track quality data
    cout << "Reading track quality data... " << endl;
    ifstream file("/cmsnfshome0/nfshome0/naodell/plt/daq/bril/pltslinkprocessor/data/tracks.csv");
    unsigned channel;
    float slopeXMean, slopeXSigma, slopeYMean, slopeYSigma;
    float residualXMean0, residualXSigma0; 
    float residualXMean1, residualXSigma1; 
    float residualXMean2, residualXSigma2; 
    float residualYMean0, residualYSigma0; 
    float residualYMean1, residualYSigma1; 
    float residualYMean2, residualYSigma2; 
    string line;
    unsigned count = 0;
    while (getline(file, line)) {
        if (count == 0) {
            ++count;
            continue;
        } else {
            istringstream iss(line);
            iss >> channel 
                >> slopeXMean >> slopeXSigma 
                >> slopeYMean >> slopeYSigma
                >> residualXMean0 >> residualXSigma0 
                >> residualXMean1 >> residualXSigma1 
                >> residualXMean2 >> residualXSigma2 
                >> residualYMean0 >> residualYSigma0 
                >> residualYMean1 >> residualYSigma1 
                >> residualYMean2 >> residualYSigma2; 

            _meanSlopeX[channel]        = slopeXMean;
            _meanSlopeY[channel]        = slopeYMean;
            _sigmaSlopeX[channel]       = slopeXSigma;
            _sigmaSlopeY[channel]       = slopeYSigma;
            _meanResidualX[channel][0]  = residualXMean0;
            _meanResidualX[channel][1]  = residualXMean1;
            _meanResidualX[channel][2]  = residualXMean2;
            _meanResidualY[channel][0]  = residualYMean0;
            _meanResidualY[channel][1]  = residualYMean1;
            _meanResidualY[channel][2]  = residualYMean2;
            _sigmaResidualX[channel][0] = residualXSigma0;
            _sigmaResidualX[channel][1] = residualXSigma1;
            _sigmaResidualX[channel][2] = residualXSigma2;
            _sigmaResidualY[channel][0] = residualYSigma0;
            _sigmaResidualY[channel][1] = residualYSigma1;
            _sigmaResidualY[channel][2] = residualYSigma2;
        }
    }

    for (unsigned i = 0; i < channels.size(); ++i) {
        channel = channels[i];
        _telescopeHits[channel]        = EffCounter();
        _telescopeHitsDelayed[channel] = EffCounter();
        _accidentalRates[channel]      = make_pair(0, 0);
    }


    cout << "EventAnalyzer initialization done!" << endl;
};

int EventAnalyzer::AnalyzeEvent()
{
    // Increment the crossing counter
    ++_bxCounter;

    // Loop over all telescopes 
    for (size_t it = 0; it != _event->NTelescopes(); ++it) {
        PLTTelescope* telescope = _event->Telescope(it);

        // make them clean events
        if (telescope->NHitPlanes() >= 2 && (unsigned)(telescope->NHitPlanes()) == telescope->NClusters()) {

            // Calculate rates for efficiencies 
            this->CalculateTelescopeRates(0, *telescope);
            this->CalculateTelescopeRates(1, *telescope);
            this->CalculateTelescopeRates(2, *telescope);

            if (telescope->NHitPlanes() == 3) {
                this->CalculateAccidentalRates(*telescope);
            }
        }
    }
    return 0;
}

void EventAnalyzer::CalculateTelescopeRates(unsigned iPlane, PLTTelescope &telescope)
{
    PLTPlane *tags[2] = {0x0, 0x0};
    PLTPlane *probe   = 0x0;
    unsigned channel  = telescope.Channel();
    unsigned ix = 0;
    for (size_t ip = 0; ip != telescope.NPlanes(); ++ip) {
        if (ip == iPlane) {
            probe = telescope.Plane(ip);
        } else {
            tags[ix] = telescope.Plane(ip);
            ++ix;
        }
    }

    if (tags[0]->NClusters() > 0 && tags[1]->NClusters() > 0) {

        PLTTrack twoHitTrack;
        twoHitTrack.AddCluster(tags[0]->Cluster(0));
        twoHitTrack.AddCluster(tags[1]->Cluster(0));

        twoHitTrack.MakeTrack(*_alignment);
        // record two-hit track slopes
        std::pair<float, float> slopes;
        slopes = std::make_pair(twoHitTrack.fTVX/twoHitTrack.fTVZ, twoHitTrack.fTVY/twoHitTrack.fTVZ);
        _twoHitTrackSlopes[channel].push_back(slopes);

        // Keep track of number of two/three-hit tracks for this plane
        if (
                // quality cuts
                twoHitTrack.IsFiducial(channel, iPlane, *_alignment, _event->PixelMask()) 
                && twoHitTrack.NHits() == 2 
                && slopes.first  > _slopeXLow 
                && slopes.first  < _slopeXHigh 
                && slopes.second > _slopeYLow 
                && slopes.second < _slopeYHigh 
           ) {

            std::pair<float, float> LXY;
            std::pair<int, int> PXY;
            PLTAlignment::CP *CP = _alignment->GetCP(channel, iPlane);
            LXY= _alignment->TtoLXY(twoHitTrack.TX(CP->LZ), twoHitTrack.TY(CP->LZ), channel, iPlane);
            PXY= _alignment->PXYfromLXY(LXY);

            ++_telescopeHits[channel].denom[iPlane];
            if (_bxCounter > 1e7) {
                ++_telescopeHitsDelayed[channel].denom[iPlane];
            }

            if (probe->NClusters() > 0) {
                std::pair<float, float> ResXY = twoHitTrack.LResiduals(*(probe->Cluster(0)), *_alignment);
                std::pair<float, float> RPXY = _alignment->PXYDistFromLXYDist(ResXY);
                if (fabs(RPXY.first) <= _pixelDist && fabs(RPXY.second) <= _pixelDist) {
                    ++_telescopeHits[channel].numer[iPlane];
                    if (_bxCounter > 1e7) {
                        ++_telescopeHitsDelayed[channel].numer[iPlane];
                    }
                }
            }
        }
    }
}


void EventAnalyzer::CalculateAccidentalRates(PLTTelescope &telescope)
{
    unsigned channel  = telescope.Channel();
    if (telescope.NTracks() > 0) {
        for (size_t itrack = 0; itrack < telescope.NTracks(); ++itrack) {
            PLTTrack *tr = telescope.Track(itrack);

            // apply track selection cuts
            float slopeX = tr->fTVX/tr->fTVZ;
            float slopeY = tr->fTVY/tr->fTVZ;
            if (isnan(slopeX) || isnan(slopeY)) continue;

            float dxSlope = fabs(slopeX - _meanSlopeX[channel])/_sigmaSlopeX[channel];
            float dySlope = fabs(slopeY - _meanSlopeY[channel])/_sigmaSlopeY[channel];
            float dxRes0 = (tr->LResidualX(0) - _meanResidualX[channel][0])/_sigmaResidualX[channel][0];
            float dxRes1 = (tr->LResidualX(1) - _meanResidualX[channel][1])/_sigmaResidualX[channel][1];
            float dxRes2 = (tr->LResidualX(2) - _meanResidualX[channel][2])/_sigmaResidualX[channel][2];
            float dyRes0 = (tr->LResidualY(0) - _meanResidualY[channel][0])/_sigmaResidualY[channel][0];
            float dyRes1 = (tr->LResidualY(1) - _meanResidualY[channel][1])/_sigmaResidualY[channel][1];
            float dyRes2 = (tr->LResidualY(2) - _meanResidualY[channel][2])/_sigmaResidualY[channel][2];

            if (
                    sqrt(pow(dxSlope,2) + pow(dySlope,2)) > 5.
                    && dxRes0 < 5. && dxRes1 < 5. && dxRes2 < 5.  
                    && dyRes0 < 5. && dyRes1 < 5. && dyRes2 < 5.
               ) {
                ++_accidentalRates[channel].first;
            } else {
                ++_accidentalRates[channel].second;
            }

            // record three-hit track slopes
            pair<float, float> slopes;
            slopes.first  = tr->fTVX/tr->fTVZ;
            slopes.second = tr->fTVY/tr->fTVZ;
            _threeHitTrackSlopes[channel].push_back(slopes);
            //cout << "channel " << channel << ": " << slopes.first << " :: " << slopes.second << endl;
            
            break;
        }
    }
}

vector<float> EventAnalyzer::GetTelescopeEfficiency(int channel)
{
    EffCounter planeCounts        = _telescopeHits[channel];
    EffCounter planeCountsDelayed = _telescopeHitsDelayed[channel];
    vector<float> eff (3, 0);
    for (unsigned i = 0; i < 3; ++i) {
        if (planeCounts.denom[i] - planeCountsDelayed.denom[i] > 0.) {
            eff[i]  = (planeCounts.numer[i] - planeCountsDelayed.numer[i]);
            eff[i] /= (planeCounts.denom[i] - planeCountsDelayed.denom[i]);
        }
    }
    return eff;
}

float EventAnalyzer::GetTelescopeAccidentals(int channel)
{
    float fakes = 0.;
    if (_accidentalRates[channel].second > 0) {
        fakes = _accidentalRates[channel].first/(_accidentalRates[channel].second + _accidentalRates[channel].second);
    }
    return fakes;
}

float EventAnalyzer::GetZeroCounting(int channel)
{
    if (_bxCounter > 0.) {
        return _accidentalRates[channel].second/_bxCounter;
    } else {
        return 0.;
    }
}
