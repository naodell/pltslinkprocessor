// $Id$

/*************************************************************************
 * XDAQ Application Template                     						 *
 * Copyright (C) 2000-2009, CERN.			               				 *
 * All rights reserved.                                                  *
 * Authors: L. Orsini, A. Forrest, A. Petrucci							 *
 *                                                                       *
 * For the licensing terms see LICENSE.		                         	 *
 * For the list of contributors see CREDITS.   					         *
 *************************************************************************/

#ifndef _bril_pltslinkprocessor_Application_h_
#define _bril_pltslinkprocessor_Application_h_

#include <string>
#include <fstream>
#include <iostream>

#include "xdaq/Application.h"

#include "xgi/Method.h"
#include "xgi/Output.h"
#include "xgi/exception/Exception.h"
#include "xgi/framework/UIManager.h"
#include "eventing/api/Member.h"

#include "root/TH2F.h"
#include "root/TFile.h"

#include "xdata/ActionListener.h"
#include "xdata/InfoSpace.h"
#include "xdata/String.h"
#include "xdata/Vector.h"
#include "xdata/UnsignedInteger32.h"

#include "toolbox/mem/Reference.h"
#include "b2in/nub/exception/Exception.h"

#include "toolbox/task/TimerListener.h"

#include "toolbox/mem/MemoryPoolFactory.h"
#include "toolbox/mem/Reference.h"
#include "toolbox/squeue.h"

#include "bril/pltslinkprocessor/EventAnalyzer.h"

namespace toolbox{
    namespace task{
        class WorkLoop;
    }
}

namespace bril
{
    namespace pltslinkprocessor
    {
        class Application : public xdaq::Application, public xgi::framework::UIManager, public eventing::api::Member, public xdata::ActionListener, public toolbox::ActionListener, public toolbox::task::TimerListener
        {
            public:

                XDAQ_INSTANTIATOR();

                Application (xdaq::ApplicationStub* s) throw (xdaq::exception::Exception);
                ~Application ();

                void Default (xgi::Input * in, xgi::Output * out) throw (xgi::exception::Exception);
                void onMessage (toolbox::mem::Reference * ref, xdata::Properties & plist) throw (b2in::nub::exception::Exception);
                virtual void actionPerformed(xdata::Event& e);
                virtual void actionPerformed(toolbox::Event& e);
                virtual void timeExpired(toolbox::task::TimerEvent& e);
                void automask(float totdata[16]);

            private:
                bool publishing(toolbox::task::WorkLoop* wl);
                void doPublish(const std::string& busname,const std::string& topicname,toolbox::mem::Reference* bufRef);
                void subscribeAll();
                void zmqClient();
                void makePlots();	

                void initializeOutputFiles(unsigned runNumber, vector<unsigned> channels);

            protected:

                void startTimer();
                void stopTimer();
                toolbox::mem::MemoryPoolFactory *m_poolFactory;
                xdaq::ApplicationDescriptor *m_appDescriptor;
                std::string m_classname;
                std::string m_timername;
                xdata::UnsignedInteger32 m_instanceid;
                toolbox::mem::Pool* m_memPool;

                xdata::UnsignedInteger32 m_currentfill;
                xdata::UnsignedInteger32 m_currentnib;
                xdata::UnsignedInteger32 m_prevnib;	
                xdata::UnsignedInteger32 m_currentls;
                xdata::UnsignedInteger32 m_currentrun;
                xdata::UnsignedInteger32 m_prevls;

                int m_nibble;
                int m_fill;
                int m_run;
                int m_ls;

                TH2F *m_OccupancyPlots[48];

                //eventinginput, eventingoutput configuration  
                xdata::Vector<xdata::Properties> m_datasources;
                xdata::Vector<xdata::Properties> m_outputtopics;
                xdata::Vector<xdata::Properties> m_tcdssources;
                xdata::String m_signalTopic;
                xdata::String m_bus;
                xdata::String m_workloopHost;
                xdata::String m_slinkHost;
                typedef std::multimap< std::string, std::string > TopicStore;
                typedef std::multimap< std::string, std::string >::iterator TopicStoreIt;
                TopicStore m_out_topicTobuses;

                //count how many outgoing buses not ready
                std::set<std::string> m_unreadybuses;

                // outgoing queue
                typedef std::map<std::string, toolbox::squeue< toolbox::mem::Reference* >* > QueueStore;
                typedef std::map<std::string, toolbox::squeue< toolbox::mem::Reference* >* >::iterator QueueStoreIt;
                QueueStore m_topicoutqueues;

                // files for validation
                ofstream effFile;
                ofstream accFile;
                ofstream lumiFile;

                std::map<std::string,std::string> m_outtopicdicts;
                toolbox::task::WorkLoop* m_publishing;
        };
    }
}

#endif
