#ifndef GRIBDECODER_H
#define GRIBDECODER_H

#include "GribRecord.h"

//----------------------------------------------
class GribCode
{
	public:
		static zuint makeCode (zuchar dataType, zuchar levelType, zuint levelValue) {
			return ((levelValue&0xFFFF)<<16)+((levelType&0xFF)<<8)+dataType;
		}
		static zuchar getDataType (zuint code) {
			return code&0xFF;
		}
		static zuchar getLevelType (zuint code) {
			return (code>>8)&0xFF;
		}
		static zuint getLevelValue (zuint code) {
			return (code>>16)&0xFFFF;
		}
};

class GribDecoder : public GribRecord
{
protected:
        std::shared_ptr<double> TableCosAlpha;
        std::shared_ptr<double> TableSinAlpha;

};

#endif

