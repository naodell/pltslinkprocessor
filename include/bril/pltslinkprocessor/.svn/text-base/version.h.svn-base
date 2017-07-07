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

#ifndef _bril_pltslinkprocessor_version_h_
#define _bril_pltslinkprocessor_version_h_

#include "config/PackageInfo.h"

// !!! Edit this line to reflect the latest package version !!!

#define BRILPLTSLINKPROCESSOR_VERSION_MAJOR 2
#define BRILPLTSLINKPROCESSOR_VERSION_MINOR 0
#define BRILPLTSLINKPROCESSOR_VERSION_PATCH 0
// If any previous versions available E.g. #define ESOURCE_PREVIOUS_VERSIONS "3.8.0,3.8.1"
#define BRILPLTSLINKPROCESSOR_PREVIOUS_VERSIONS "1.0.0,1.0.1,1.0.2"

//
// Template macros
//
#define BRILPLTSLINKPROCESSOR_VERSION_CODE PACKAGE_VERSION_CODE(BRILPLTSLINKPROCESSOR_VERSION_MAJOR,BRILPLTSLINKPROCESSOR_VERSION_MINOR,BRILPLTSLINKPROCESSOR_VERSION_PATCH)
#ifndef BRILPLTSLINKPROCESSOR_PREVIOUS_VERSIONS
#define BRILPLTSLINKPROCESSOR_FULL_VERSION_LIST  PACKAGE_VERSION_STRING(BRILPLTSLINKPROCESSOR_VERSION_MAJOR,BRILPLTSLINKPROCESSOR_VERSION_MINOR,BRILPLTSLINKPROCESSOR_VERSION_PATCH)
#else
#define BRILPLTSLINKPROCESSOR_FULL_VERSION_LIST  BRILPLTSLINKPROCESSOR_PREVIOUS_VERSIONS "," PACKAGE_VERSION_STRING(BRILPLTSLINKPROCESSOR_VERSION_MAJOR,BRILPLTSLINKPROCESSOR_VERSION_MINOR,BRILPLTSLINKPROCESSOR_VERSION_PATCH)
#endif

namespace brilpltslinkprocessor
{
	const std::string package = "brilpltslinkprocessor";
	const std::string versions = BRILPLTSLINKPROCESSOR_FULL_VERSION_LIST;
	const std::string summary = "XDAQ brilDAQ pltslinkprocessor";
	const std::string description = "collect and process histograms from plt slink";
	const std::string authors = "K. J. Rose, A. Gomez, P. Lujan";
	const std::string link = "http://xdaqwiki.cern.ch/";
	config::PackageInfo getPackageInfo ();
	void checkPackageDependencies () throw (config::PackageInfo::VersionException);
	std::set<std::string, std::less<std::string> > getPackageDependencies ();
}

#endif
