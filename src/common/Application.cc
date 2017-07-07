// $Id$

/*************************************************************************
 * XDAQ Application Template                                             *
 * Copyright (C) 2000-2009, CERN.                                        *
 * All rights reserved.                                                  *
 * Authors: L. Orsini, A. Forrest, A. Petrucci                           *
 *                                                                       *
 * For the licensing terms see LICENSE.                                  *
 * For the list of contributors see CREDITS.                             *
 *************************************************************************/

// This version is the first update of pltslinkprocessor to work with the
// new 2017 dataflow. This skips the pltslinksource entirely and just sets
// up a zmq listener to listen to the data coming directly from FEDStreamReader.
// A zmq poll is used to listen to both the workloop zmq publisher (to get the
// TCDS data) and the slink zmq publisher.
//
// At the moment the only thing done with the data is the filling of the
// occupancy plots, but with this framework in place it should be easily
// extensible to making other plots and publishing more information to BRILDAQ.
//
//  -- Paul Lujan, 24 April 2017

// c++ stuff
#include <string>
#include <iostream>
#include <sstream>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <sys/stat.h>
#include <sys/types.h>

// xdaq stuff
#include "cgicc/CgiDefs.h"

#include "cgicc/Cgicc.h"
#include "cgicc/HTTPHTMLHeader.h"
#include "cgicc/HTMLClasses.h"
#include "xgi/framework/Method.h"
#include "eventing/api/exception/Exception.h"
#include "xdata/Properties.h"
#include "toolbox/mem/HeapAllocator.h"
#include "toolbox/mem/MemoryPoolFactory.h"
#include "xcept/tools.h"
#include "xdata/InfoSpaceFactory.h"
#include "toolbox/TimeVal.h"
#include "toolbox/task/TimerFactory.h"

#include "b2in/nub/Method.h"
//#include "toolbox/regex.h"
#include "toolbox/task/WorkLoop.h"
#include "toolbox/task/WorkLoopFactory.h"
#include "bril/pltslinkprocessor/zmq.hpp"

// ROOT stuff
#include "root/TCanvas.h"
#include "root/TStyle.h"
#include "root/TLine.h"
#include "root/TROOT.h"

// PLT stuff
#include "bril/pltslinkprocessor/PLTEvent.h"
#include "bril/pltslinkprocessor/Application.h"
#include "bril/pltslinkprocessor/exception/Exception.h"
#include "interface/bril/PLTSlinkTopics.hh"


using boost::property_tree::ptree;
using boost::property_tree::write_json;

XDAQ_INSTANTIATOR_IMPL (bril::pltslinkprocessor::Application) 

    using namespace interface::bril;

bril::pltslinkprocessor::Application::Application (xdaq::ApplicationStub* s) throw (xdaq::exception::Exception): xdaq::Application(s), xgi::framework::UIManager(this), eventing::api::Member(this)
{
    this->getEventingBus("brildata").addActionListener(this);
    xgi::framework::deferredbind(this,this,&bril::pltslinkprocessor::Application::Default, "Default");
    m_poolFactory   = toolbox::mem::getMemoryPoolFactory();
    m_appDescriptor = getApplicationDescriptor();
    m_classname     = m_appDescriptor->getClassName();
    m_instanceid    = m_appDescriptor->getInstance();

    std::string memPoolName = m_classname + m_instanceid.toString() + std::string("_memPool");
    toolbox::mem::HeapAllocator* allocator = new toolbox::mem::HeapAllocator();
    toolbox::net::URN urn("toolbox-mem-pool",memPoolName);
    m_memPool   = m_poolFactory->createPool(urn, allocator);
    m_timername = "lumiPLT_SlinkProcessorTimer";

    //	do_starttimer();

    // Initialize histograms for plane occupancies
    for(int ihist = 0; ihist<16; ihist++){
        for(int iroc = 0; iroc<3; iroc++){
            char histname[80];
            sprintf(histname, "ChannelOccupancy_CHA%02d_ROC%02d", ihist, iroc);
            m_OccupancyPlots[((ihist*3)+iroc)] = new TH2F(histname, histname, 100, 0, 100, 100, 0, 100);
        }
    }

    try{
        getApplicationInfoSpace()->fireItemAvailable("bus",&m_bus);
        getApplicationInfoSpace()->fireItemAvailable("workloopHost",&m_workloopHost);
        getApplicationInfoSpace()->fireItemAvailable("slinkHost",&m_slinkHost);
        getApplicationInfoSpace()->addListener(this, "urn:xdaq-event:setDefaultValues");
        m_publishing = toolbox::task::getWorkLoopFactory()->getWorkLoop(m_appDescriptor->getURN()+"_publishing","waiting");
    }
    catch(xcept::Exception & e){
        XCEPT_RETHROW(xdaq::exception::Exception, "Failed to add Listener", e);
    }

    // output
    m_outtopicdicts.insert( std::make_pair(pltslinklumiT::topicname(),pltslinklumiT::payloaddict()) );
}

bril::pltslinkprocessor::Application::~Application (){}

void bril::pltslinkprocessor::Application::actionPerformed(xdata::Event& e){

    std::stringstream msg;
    if(e.type() == "urn:xdaq-event:setDefaultValues"){

        std::string timername("zmqstarter");
        try{
            toolbox::TimeVal zmqStart(2,0);
            toolbox::task::Timer * timer = toolbox::task::getTimerFactory()->createTimer(timername);
            timer->schedule(this, zmqStart, 0, timername);
        }catch(xdata::exception::Exception& e){
            std::stringstream msg;
            msg << "failed to start timer" << timername;
            LOG4CPLUS_ERROR(getApplicationLogger(),msg.str()+stdformat_exception_history(e));
            XCEPT_RETHROW(bril::pltslinkprocessor::exception::Exception,msg.str(),e);
        }
    }
}

void bril::pltslinkprocessor::Application::actionPerformed(toolbox::Event& e){
    if(e.type() == "eventing::api::BusReadyToPublish" ){
        std::string busname = (static_cast<eventing::api::Bus*>(e.originator()))->getBusName();
        std::stringstream msg;
        msg <<  "event Bus '"  <<  busname  <<  "' is ready to publish";
        m_unreadybuses.erase(busname);
        if(m_unreadybuses.size()!=0) 
            return; //wait until all buses are ready
        try{    
            toolbox::task::ActionSignature* as_publishing = toolbox::task::bind(this, &bril::pltslinkprocessor::Application::publishing, "publishing");
            m_publishing->activate();
            m_publishing->submit(as_publishing);
        }catch(toolbox::task::exception::Exception& e){
            msg<<"Failed to start publishing workloop "<<stdformat_exception_history(e);
            LOG4CPLUS_ERROR(getApplicationLogger(), msg.str());
            XCEPT_RETHROW(bril::pltslinkprocessor::exception::Exception, msg.str(), e);
        }    
    }
}

/*
 * This is the default web page for this XDAQ application.
 */
void bril::pltslinkprocessor::Application::Default (xgi::Input * in, xgi::Output * out) throw (xgi::exception::Exception) {

    *out << busesToHTML();
    std::string appurl = getApplicationContext()->getContextDescriptor()->getURL()+"/"+m_appDescriptor->getURN();
    *out << appurl;
    *out << "We are publishing simulated data on frequency : "<< m_signalTopic.toString();   
    *out << cgicc::br();

    *out << cgicc::table().set("class","xdaq-table-vertical");
    *out << cgicc::caption("input/output");
    *out << cgicc::tbody();

    //*out << cgicc::tr();
    //*out << cgicc::th("in_topics");
    //*out << cgicc::td(pltslinkhistT::topicname());
    //*out << cgicc::tr();

    *out << cgicc::tr();
    *out << cgicc::th("out_topics");
    *out << cgicc::td( pltslinklumiT::topicname());
    *out << cgicc::tr();

    *out << cgicc::tr();
    *out << cgicc::th("signalTopic");
    *out << cgicc::td( m_signalTopic.toString());
    *out << cgicc::tr();

    *out << cgicc::tbody(); 
    *out << cgicc::table();
    *out << cgicc::br() << std::endl;

    *out << cgicc::img().set("src","/OccupancyPlot_CHA15_ROC2.pdf").set("alt", "Channel 15 ROC 2").set("height", "100").set("width","100") << std::endl;

}

// onMessage has been removed, since this version of the processor doesn't need to listen
// to messages from the BRILDAQ bus any more, only the zmq receivers

void bril::pltslinkprocessor::Application::doPublish(const std::string& busname, 
        const std::string& topicname, 
        toolbox::mem::Reference* bufRef)
{
    std::stringstream msg;
    try{
        xdata::Properties plist;
        plist.setProperty("DATA_VERSION", DATA_VERSION);
        std::map<std::string, std::string>::iterator it = m_outtopicdicts.find(topicname);
        if(it != m_outtopicdicts.end()) {
            plist.setProperty("PAYLOAD_DICT", it->second);
        msg << "publish to " << busname << " , " << topicname; 
        }
        this->getEventingBus(busname).publish(topicname, bufRef, plist);

    }catch(xcept::Exception& e){
        msg << "Failed to publish " << topicname << " to " << busname;    
        LOG4CPLUS_ERROR(getApplicationLogger(), stdformat_exception_history(e));
        if(bufRef){
            bufRef->release();
            bufRef = 0;
        }
        XCEPT_DECLARE_NESTED(bril::pltslinkprocessor::exception::Exception, myerrorobj, msg.str(),e);
        this->notifyQualified("fatal", myerrorobj);
    }  
}


bool bril::pltslinkprocessor::Application::publishing(toolbox::task::WorkLoop* wl)
{
    for(QueueStoreIt it = m_topicoutqueues.begin(); it != m_topicoutqueues.end(); ++it){
        if(it->second->empty()) 
            continue;

        std::string topicname = it->first;
        LOG4CPLUS_INFO(getApplicationLogger(), "Publishing "+topicname);
        toolbox::mem::Reference* data = it->second->pop();
        std::pair<TopicStoreIt, TopicStoreIt > ret = m_out_topicTobuses.equal_range(topicname);
        for(TopicStoreIt topicit = ret.first; topicit != ret.second; ++topicit){
            if(data) 
                doPublish(topicit->second, topicname, data->duplicate());    
        }

        if(data) 
            data->release();
    }
    return true;
}


void bril::pltslinkprocessor::Application::stopTimer(){
    toolbox::task::Timer* timer = toolbox::task::TimerFactory::getInstance()->getTimer(m_timername);
    if( timer->isActive() ){
        timer->stop();
    }
}

void bril::pltslinkprocessor::Application::startTimer()
{
    if(!toolbox::task::TimerFactory::getInstance()->hasTimer(m_timername)){
        (void) toolbox::task::TimerFactory::getInstance()->createTimer(m_timername);
    }
    toolbox::task::Timer* timer = toolbox::task::TimerFactory::getInstance()->getTimer(m_timername);
    toolbox::TimeInterval interval(20,0);
    try{
        stopTimer();
        toolbox::TimeVal start = toolbox::TimeVal::gettimeofday();
        timer->start();
        timer->scheduleAtFixedRate( start, this, interval, 0, m_timername);
    }catch (toolbox::task::exception::InvalidListener& e){
        LOG4CPLUS_FATAL (this->getApplicationLogger(), xcept::stdformat_exception_history(e));
    }catch (toolbox::task::exception::InvalidSubmission& e){
        LOG4CPLUS_FATAL (this->getApplicationLogger(), xcept::stdformat_exception_history(e));
    }catch (toolbox::task::exception::NotActive& e){
        LOG4CPLUS_FATAL (this->getApplicationLogger(), xcept::stdformat_exception_history(e));
    }catch (toolbox::exception::Exception& e){
        LOG4CPLUS_FATAL (this->getApplicationLogger(), xcept::stdformat_exception_history(e));
    }
}

void bril::pltslinkprocessor::Application::timeExpired(toolbox::task::TimerEvent& e){
    LOG4CPLUS_INFO(getApplicationLogger(), "Beginning zmq listener");
    zmqClient();
}

void bril::pltslinkprocessor::Application::zmqClient()
{
    // Use zmq_poll to get zmq messages from the multiple sources.
    zmq::context_t zmq_context(1);

    // Workloop zmq listener: used for getting TCDS info from workloop
    zmq::socket_t workloop_socket(zmq_context, ZMQ_SUB);
    workloop_socket.connect(m_workloopHost.c_str());
    workloop_socket.setsockopt(ZMQ_SUBSCRIBE, 0, 0);

    uint32_t EventHisto[3573];
    uint32_t tcds_info[4];
    uint32_t channel;
    int old_run = 1; // Set these high so we don't immediately publish
    int old_ls  = 999999;   // the first LS

    // Slink zmq listener: used for getting actual data from slink
    zmq::socket_t slink_socket(zmq_context, ZMQ_SUB);
    slink_socket.connect(m_slinkHost.c_str());
    slink_socket.setsockopt(ZMQ_SUBSCRIBE, 0, 0);

    uint32_t slinkBuffer[1024];
    // Translation of pixel FED channel number to readout channel number.
    int nevents = 0; // counter of slink items received
    const unsigned validChannels[] = {2, 4, 5, 8, 10, 11, 13, 14, 16, 17, 19, 20};
    const int channelNumber[37] = {-1, 0, 1, -1, 2, 3, -1, 4, 5, -1, 6, 7,  // 0-11
        -1, 8, 9, -1, 10, 11, -1, 12, 13, -1, 14, 15, // 12-23
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1
    };

    // Set up poll object
    zmq::pollitem_t pollItems[2] = {
        {workloop_socket, 0, ZMQ_POLLIN, 0},
        {slink_socket, 0, ZMQ_POLLIN, 0}
    };

    // Set up the PLTEvent object to do the event decoding. Note: for testing
    // purposes we don't have a gaincal, alignment, or tracking, but we will
    // want to implement these in order to do more interesting things.

    // Maybe put these in the configuration xml
    string gcFile = "/nfshome0/naodell/plt/daq/bril/pltslinkprocessor/data/GainCalFits_20160501.155303.dat";
    string alFile = "/nfshome0/naodell/plt/daq/bril/pltslinkprocessor/data/Trans_Alignment_4895.dat";

    PLTEvent *event = new PLTEvent("", gcFile, alFile, kBuffer);
    event->ReadOnlinePixelMask("/nfshome0/naodell/plt/daq/bril/pltslinkprocessor/data/Mask_2016_VdM_v1.txt");
    event->SetPlaneClustering(PLTPlane::kClustering_NoClustering, PLTPlane::kFiducialRegion_All);
    event->SetPlaneFiducialRegion(PLTPlane::kFiducialRegion_All);
    event->SetTrackingAlgorithm(PLTTracking::kTrackingAlgorithm_01to2_AllCombs);
    //event->SetTrackingAlgorithm(PLTTracking::kTrackingAlgorithm_NoTracking);

    // Initialize tools
    vector<unsigned> channels(validChannels, validChannels + sizeof(validChannels)/sizeof(unsigned));
    EventAnalyzer *eventAnalyzer = new EventAnalyzer(event, "", channels);

    // Loop and receive messages
    while (1) {
        zmq_poll(&pollItems[0],  2,  -1);

        // Check for workloop message
        if (pollItems[0].revents & ZMQ_POLLIN) {
            zmq::message_t zmq_EventHisto(3573*sizeof(uint32_t));
            workloop_socket.recv(&zmq_EventHisto);

            memcpy(EventHisto, zmq_EventHisto.data(), 3573*sizeof(uint32_t));
            memcpy(&channel, (uint32_t*)EventHisto+2, sizeof(uint32_t));
            memcpy(&tcds_info, (uint32_t*)EventHisto+3, 4*sizeof(uint32_t));

            m_nibble = tcds_info[0];
            m_ls     = tcds_info[1];
            m_run    = tcds_info[2];
            m_fill   = tcds_info[3];

            //std::cout << "Received tcds message ch " << channel << " fill " << m_fill << " run " << m_run << " LS " << m_ls << " nibble " << m_nibble << std::endl;
            if (old_run == 1) {
                this->initializeOutputFiles(m_run, channels);
            } else if (m_run > old_run) {
                effFile.close();
                accFile.close();
                lumiFile.close();
                this->initializeOutputFiles(m_run, channels);
            }

            // If we've started a new run or a new lumisection, then go ahead
            // and publish what we have.
            if (m_ls > old_ls or m_run > old_run) {
                std::cout << "Publishing plots with fill " << m_fill 
                    << " run " << m_run 
                    << " LS " << m_ls 
                    << " events " << nevents << std::endl;
                //makePlots();

                effFile << m_ls;
                accFile << m_ls;
                lumiFile << m_ls;

                const unsigned nChannels = sizeof(validChannels) / sizeof(validChannels[0]);
                vector<float> eff;
                float acc = 0.;
                float pzero = 0.;
                for (unsigned i = 0; i < nChannels; ++i) {
                    eff   = eventAnalyzer->GetTelescopeEfficiency(validChannels[i]);
                    acc   = eventAnalyzer->GetTelescopeAccidentals(validChannels[i]);
                    pzero = eventAnalyzer->GetZeroCounting(validChannels[i]);

                    cout << "channel " << validChannels[i] 
                         << " :efficiency: " << eff[0] << ", " << eff[1] << ", " << eff[2] 
                         << " :accidental: " << acc
                         << " :lumi: " << pzero << endl;

                    effFile  << "," << eff[0] << "," << eff[1] << "," << eff[2];
                    accFile  << "," << acc;
                    lumiFile << "," << pzero;
                }

                effFile  << "\n" << std::flush;
                accFile  << "\n" << std::flush;
                lumiFile << "\n" << std::flush;

                toolbox::TimeVal timeStamp = toolbox::TimeVal::gettimeofday();

                // prepare output buffer and publish
                xdata::Properties plist;
                plist.setProperty("DATA_VERSION", DATA_VERSION);
                plist.setProperty("PAYLOAD_DICT", pltslinklumiT::payloaddict());

                size_t bufferSize = pltslinklumiT::maxsize();
                toolbox::mem::Reference* bufferRef = m_poolFactory->getFrame(m_memPool, bufferSize);
                bufferRef->setDataSize(bufferSize);

                pltslinklumiT* payload = (pltslinklumiT*) (bufferRef->getDataLocation());
                payload->setTime(m_fill, m_run, m_ls, m_nibble, timeStamp.sec(), timeStamp.millisec());
                payload->setResource(DataSource::PLT, 0, 1, StorageType::COMPOUND);
                payload->setTotalsize(pltslinklumiT::maxsize());
                //payload->setFrequency(4);

                CompoundDataStreamer streamer(pltslinklumiT::payloaddict()); 
                char calibtag[] = "default";
                streamer.insert_field(payload->payloadanchor, "calibtag" , &calibtag);
                streamer.insert_field(payload->payloadanchor, "avgraw", &pzero);
                streamer.insert_field(payload->payloadanchor, "avg", &pzero);
                this->getEventingBus("brildata").publish("pltslinklumi", bufferRef, plist);
                //doPublish("brildata", interface::bril::pltslinklumiT::topicname(), bufferRef);
                std::cout << "Done sending publishing data to 'brildata'" << std::endl;

            }
            old_ls  = m_ls;
            old_run = m_run;
        }

        // Check for slink message
        if (pollItems[1].revents & ZMQ_POLLIN) {
            int eventSize = slink_socket.recv((void*)slinkBuffer, sizeof(slinkBuffer)*sizeof(uint32_t), 0);

            // feed it to Event.GetNextEvent()
            event->GetNextEvent(slinkBuffer, eventSize/sizeof(uint32_t));

            // Now the event is fully processed and we can do things with the
            // processed information.  For now all we do is put the hits in the
            // occupancy plots, but we can do all sorts of other things (work
            // with tracks, pulse heights, etc.)

            //if (event->NHits() > 20) {
            //    std::cout << "Event " << event->EventNumber() << " BX " << event->BX() 
            //        << " Time " << event->Time() << " has " << event->NHits() << " hits and " 
            //        << event->GetErrors().size() << " errors" << std::endl;
            //}

            // Calculate efficiency and accidental rate per telescope
            eventAnalyzer->AnalyzeEvent();

            // fill occupancy plots
            for (size_t ip = 0; ip != event->NPlanes(); ++ip) {
                PLTPlane* Plane = event->Plane(ip);
                int nhits = 0;
                for (size_t ih = 0; ih != Plane->NHits(); ++ih) {
                    PLTHit* Hit = Plane->Hit(ih);

                    // Convert pixel FED channel number to readout channel number
                    if (
                            Hit->Channel() < 0 
                            || Hit->Channel() > 36 
                            || channelNumber[Hit->Channel()] < 0
                       ) {
                        // std::cout << "Found hit with invalid channel number: " << Hit->Channel();
                    } else {
                        int readoutChan = channelNumber[Hit->Channel()];
                        // std::cout << "Hit  " << nhits << " FED chan " << Hit->Channel() << " readout chan " << readoutChan << " ROC " << Hit->ROC() << " col " << Hit->Column() << " row " << Hit->Row() << std::endl;
                        nhits++;
                        m_OccupancyPlots[(readoutChan*3) + Hit->ROC()]->Fill(Hit->Column(), Hit->Row());
                    }
                }
            }

            // const std::vector<PLTError>& errors = Event.GetErrors();
            // if (errors.size() > 0) {
            // 	for (std::vector<PLTError>::const_iterator it = errors.begin(); it != errors.end(); ++it) {
            //  	  it->Print();
            //  	}
            // }
            nevents++;
        } // slink message
    } // message loop  
}

void bril::pltslinkprocessor::Application::makePlots()
{

    //string baseDir = "/cmsnfsscratch/globalscratch/cmsbril/PLT/DQM/run%06d";

    // prepare the output directory
    string baseDir = "/nfshome0/naodell/test_data";
    std::stringstream ss;
    ss << baseDir << "/run_" << m_run;
    string outDir = ss.str();
    string command = "mkdir -p -m 777 " + outDir;
    system(command.c_str());

    ss << "/test_" << m_ls << ".root";
    string rootFilename = ss.str();
    TFile dqmFile(rootFilename.c_str(), "RECREATE");
    ss.flush();

    ptree dataJsonPtree;
    ptree plots;
    for(int ihist = 0; ihist < 16; ihist++){
        for(int iroc = 0; iroc < 3; iroc++){
            m_OccupancyPlots[((ihist*3)+iroc)]->Draw("colz");
            m_OccupancyPlots[((ihist*3)+iroc)]->Write();

            ptree eachPlot;

            eachPlot.put( "type", "2D" );
            char titles[80];
            sprintf( titles, "OccupancyPlot_CHA%02d_ROC%02d, Row (ROC %02d), Column (ROC %02d)", ihist, iroc, iroc, iroc );
            eachPlot.put( "titles", titles );
            char names[80];
            sprintf( names, m_OccupancyPlots[((ihist*3)+iroc)]->GetName());
            eachPlot.put( "names", names );
            ptree nbinsX, nbinsY;
            ptree plotNbins;
            nbinsX.put("", 100);
            nbinsY.put("", 100);
            plotNbins.push_back(std::make_pair("", nbinsX));
            plotNbins.push_back(std::make_pair("", nbinsY));
            eachPlot.add_child( "nbins", plotNbins );
            ptree minX, maxX;
            ptree rangeX;
            minX.put("", 0);
            maxX.put("", 100);
            rangeX.push_back(std::make_pair("", minX));
            rangeX.push_back(std::make_pair("", maxX));
            eachPlot.add_child( "xrange", rangeX );
            ptree minY, maxY;
            ptree rangeY;
            minY.put("", 0);
            maxY.put("", 100);
            rangeY.push_back(std::make_pair("", minY));
            rangeY.push_back(std::make_pair("", maxY));
            eachPlot.add_child( "yrange", rangeY );

            double w;
            ptree plotData;
            for (int biny=0; biny<= m_OccupancyPlots[((ihist*3)+iroc)]->GetNbinsY()+1; biny++) {
                for (int binx=0; binx<= m_OccupancyPlots[((ihist*3)+iroc)]->GetNbinsX()+1; binx++) {
                    w = m_OccupancyPlots[((ihist*3)+iroc)]->GetBinContent(binx,biny);
                    if(w!=0) {
                        ptree binContent;
                        ptree X, Y, W; 
                        X.put("", binx);
                        Y.put("", biny);
                        W.put("", w);
                        binContent.push_back(std::make_pair("", X));
                        binContent.push_back(std::make_pair("", Y));
                        binContent.push_back(std::make_pair("", W));
                        plotData.push_back(std::make_pair("", binContent));

                    }
                }
            }
            eachPlot.add_child("data", plotData);

            plots.push_back(std::make_pair("", eachPlot ));
        }
    }

    string datFilename = outDir + "/test.dat";
    dataJsonPtree.add_child("OccupancyPlots", plots);
    write_json(datFilename, dataJsonPtree);

    /*
       char outFolder[160] = "/cmsnfsscratch/globalscratch/cmsbril/PLT/DQM/";
       char filenameJSN[160];
       sprintf(filenameJSN, "run%06d/run%06d_ls%04d_streamDQMPLT_eventing-bus.jsn", m_run, m_run, m_ls );
       char tmpOutFolder[160] = "/cmsnfsscratch/globalscratch/cmsbril/PLT/DQM/";
       char tmpFilenameJSN[160];
       sprintf(tmpFilenameJSN, "run%06d/run%06d_ls%04d_streamDQMPLT_eventing-bus.tmp", m_run, m_run, m_ls );
       char filenameDATv2[160];
       sprintf(filenameDATv2, "run%06d_ls%04d_streamDQMPLT_eventing-bus.dat", m_run, m_ls );

       ptree jsonPtree;
       ptree infoData, infoEntries, infoFilename, infoZeros, infoEmpty;
       infoEntries.put<int>("", 0);
       infoFilename.put("", filenameDATv2);
       infoEmpty.put("", "");
       infoZeros.put<int>("", 0);
       infoData.push_back(std::make_pair("", infoEntries));
       infoData.push_back(std::make_pair("", infoEntries));
       infoData.push_back(std::make_pair("", infoZeros));
       infoData.push_back(std::make_pair("", infoFilename));
       infoData.push_back(std::make_pair("", infoZeros));
       infoData.push_back(std::make_pair("", infoZeros));
       infoData.push_back(std::make_pair("", infoZeros));
       infoData.push_back(std::make_pair("", infoZeros));
       infoData.push_back(std::make_pair("", infoZeros));
       infoData.push_back(std::make_pair("", infoEmpty));

       jsonPtree.add_child( "data", infoData );
       write_json( strcat(tmpOutFolder,tmpFilenameJSN), jsonPtree);
       std::rename( tmpOutFolder, strcat(outFolder,filenameJSN) );
       */


    dqmFile.Write();
    dqmFile.Close();
    //std::cout << "Output complete. DQM files " << filenameJSN << ", " << filenameDAT << " and " << filenameROOT << " created." << std::endl;

}

void bril::pltslinkprocessor::Application::initializeOutputFiles(unsigned runNumber, vector<unsigned> channels)
{
    std::stringstream fname;
    string baseDir = "/nfshome0/naodell/test_data";

    fname << baseDir << "/efficiencies_" << m_run << ".csv";
    string fname_str = fname.str();
    effFile.open(fname_str.c_str(), ios::out);
    fname.str("");

    fname << baseDir << "/accidentals_" << m_run << ".csv";
    fname_str = fname.str();
    accFile.open(fname_str.c_str(), ios::out);
    fname.str("");

    fname << baseDir << "/lumi_rates_" << m_run << ".csv";
    fname_str = fname.str();
    lumiFile.open(fname_str.c_str(), ios::out);
    fname.str("");

    effFile  << "lumi_section";
    accFile  << "lumi_section";
    lumiFile << "lumi_section";

    for (unsigned i = 0; i < channels.size(); ++i) {
        unsigned ch = channels[i];
        effFile  << ",ch" << ch << "_0,ch" << ch << "_1,ch" << ch << "_2";
        accFile  << ",ch" << ch;
        lumiFile << ",ch" << ch;
    }
    effFile  << "\n";
    accFile  << "\n";
    lumiFile << "\n";
}
