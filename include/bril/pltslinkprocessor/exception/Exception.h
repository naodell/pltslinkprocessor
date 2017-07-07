#ifndef _bril_pltslinkprocessor_exception_Exception_h_
#define _bril_pltslinkprocessor_exception_Exception_h_
#include "xcept/Exception.h"
namespace bril { 
  namespace pltslinkprocessor {
    namespace exception { 
      class Exception: public xcept::Exception{
      public: 
      Exception( std::string name, std::string message, std::string module, int line, std::string function ):xcept::Exception(name, message, module, line, function) {} 
      Exception( std::string name, std::string message, std::string module, int line, std::string function, xcept::Exception & e ):xcept::Exception(name, message, module, line, function, e) {} 
      }; 
    }//ns exception
  }//ns pltslinkprocessor

}//ns bril
#endif
