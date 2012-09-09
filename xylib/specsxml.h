// Specslabs XML data file
// Licence: Lesser GNU Public License 2.1 (LGPL)
// $Id$


#ifndef XYLIB_SPECSXML_H_
#define XYLIB_SPECSXML_H_
#include "xylib.h"

namespace xylib {

    class SpecsxmlDataSet : public DataSet
    {
        OBLIGATORY_DATASET_MEMBERS(SpecsxmlDataSet)
        virtual bool is_valid_option(std::string const& t);
    };

}
#endif // XYLIB_SPECSXML_H_

