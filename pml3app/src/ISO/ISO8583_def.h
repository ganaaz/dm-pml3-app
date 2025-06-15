#ifndef ISO8583_DEF_H
#define ISO8583_DEF_H

#include "ISO8583.h"
extern const int NULL_BYTE_LEN;
extern const int TPDU_LEN;
extern const int LENGTH_OF_MSG_LEN;
extern ISO8583_STATIC_DATA static_data;
extern ISO8583_SUB_COMPONENT_DEF sub_component_def[];
extern ISO8583_SUB_FIELD_DEF sub_field_def[];
extern ISO8583_FIELD_DEF field_def[];
extern ISO8583_MTI mti_def[];
extern ISO8583_PROCESSING_CODES pro_codes_def[];
extern ISO8583_BITMAP_MAPPING bitmap_def[];
extern char RESPONSE_CODE_SUCCESS[];
#endif /* ISO8583_DEF_H */