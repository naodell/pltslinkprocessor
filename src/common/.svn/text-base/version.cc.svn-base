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

#include "bril/pltslinkprocessor/version.h"
#include "config/version.h"
#include "xcept/version.h"
#include "toolbox/version.h"
#include "xdaq/version.h"
#include "xdata/version.h"
#include "xoap/version.h"
#include "xgi/version.h"
#include "b2in/nub/version.h"

GETPACKAGEINFO(brilpltslinkprocessor)

void brilpltslinkprocessor::checkPackageDependencies() throw (config::PackageInfo::VersionException)
{
	CHECKDEPENDENCY(config);
	CHECKDEPENDENCY(xcept);
	CHECKDEPENDENCY(toolbox);
	CHECKDEPENDENCY(xdaq);
	CHECKDEPENDENCY(xdata);
	CHECKDEPENDENCY(xoap);
	CHECKDEPENDENCY(xgi);
	CHECKDEPENDENCY(b2innub);
}

std::set<std::string, std::less<std::string> > brilpltslinkprocessor::getPackageDependencies()
{
	std::set<std::string, std::less<std::string> > dependencies;

	ADDDEPENDENCY(dependencies,config);
	ADDDEPENDENCY(dependencies,xcept);
	ADDDEPENDENCY(dependencies,toolbox);
	ADDDEPENDENCY(dependencies,xdaq);
	ADDDEPENDENCY(dependencies,xdata);
	ADDDEPENDENCY(dependencies,xoap);
	ADDDEPENDENCY(dependencies,xgi);
	ADDDEPENDENCY(dependencies,b2innub);

	return dependencies;
}

