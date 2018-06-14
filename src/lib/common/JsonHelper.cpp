/*
*
* Copyright 2013 Telefonica Investigacion y Desarrollo, S.A.U
*
* This file is part of Orion Context Broker.
*
* Orion Context Broker is free software: you can redistribute it and/or
* modify it under the terms of the GNU Affero General Public License as
* published by the Free Software Foundation, either version 3 of the
* License, or (at your option) any later version.
*
* Orion Context Broker is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Affero
* General Public License for more details.
*
* You should have received a copy of the GNU Affero General Public License
* along with Orion Context Broker. If not, see http://www.gnu.org/licenses/.
*
* For those usages not covered by this license please contact with
* iot_support at tid dot es
*
* Author: Orion dev team
*/
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <iomanip>

#include "common/JsonHelper.h"
#include "common/string.h"
#include "common/limits.h"



/* ****************************************************************************
*
* toJsonString -
*/
std::string toJsonString(const std::string& input)
{
  std::ostringstream ss;

  ss << '"';
  for (std::string::const_iterator iter = input.begin(); iter != input.end(); ++iter)
  {
    /* FIXME P3: This function ensures that if the DB holds special characters (which are
     * not supported in JSON according to its specification), they are converted to their escaped
     * representations. The process wouldn't be necessary if the DB couldn't hold such special characters,
     * but as long as we support NGSIv1, it is better to have the check (e.g. a newline could be
     * used in an attribute value using XML). Even removing NGSIv1, we have to ensure that the
     * input parser (rapidjson) doesn't inject not supported JSON characters in the DB (this needs to be
     * investigated in the rapidjson documentation)
     *
     * JSON specification is a bit obscure about the need of escaping / (what they call 'solidus'). The
     * picture at JSON specification (http://www.json.org/) seems suggesting so, but after a careful reading of
     * https://tools.ietf.org/html/rfc4627#section-2.5, we can conclude it is not mandatory. Online checkers
     * such as http://jsonlint.com confirm this. Looking in some online discussions
     * (http://andowebsit.es/blog/noteslog.com/post/the-solidus-issue/ and
     * https://groups.google.com/forum/#!topic/opensocial-and-gadgets-spec/FkLsC-2blbo) it seems that
     * escaping / may have sense in some situations related with JavaScript code, which is not the case of Orion.
     */
    switch (char ch = *iter)
    {
    case '\\': ss << "\\\\"; break;
    case '"':  ss << "\\\""; break;
    case '\b': ss << "\\b";  break;
    case '\f': ss << "\\f";  break;
    case '\n': ss << "\\n";  break;
    case '\r': ss << "\\r";  break;
    case '\t': ss << "\\t";  break;
    default:
      /* Converting the rest of special chars 0-31 to \u00xx. Note that 0x80 - 0xFF are untouched as they
       * correspond to UTF-8 multi-byte characters */
      if (ch >= 0 && ch <= 0x1F)
      {
        static const char intToHex[16] =  { '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F' } ;

        ss << "\\u00" << intToHex[(ch & 0xF0) >> 4] << intToHex[ch & 0x0F];
      }
      else
      {
        ss << ch;
      }
      break;
    }  // end-switch

  }  // end-for

  ss << '"';

  return ss.str();
}



/* ****************************************************************************
*
* vectorToJson -
*/
template <>
std::string vectorToJson(std::vector<std::string> &list)
{
  switch (list.size())
  {
  case 0:
    return "[]";

  case 1:
    return "[" + toJsonString(list[0]) + "]";

  default:
    std::ostringstream os;
    os << '[';
    os << toJsonString(list[0]);
    for (std::vector<std::string>::size_type i = 1; i != list.size(); ++i)
    {
      os << ',' << toJsonString(list[i]);
    }
    os << ']';
    return os.str();
  }
}



/* ****************************************************************************
*
* objectToJson -
*/
std::string objectToJson(std::map<std::string, std::string>& list)
{
  std::ostringstream  os;
  bool                firstTime = true;

  os << '{';

  for (std::map<std::string, std::string>::const_iterator it = list.begin(); it != list.end(); ++it)
  {
    std::string key   = it->first;
    std::string value = it->second;

    if (firstTime)
    {
      firstTime = false;
    }
    else
    {
      os << ',';
    }

    os << toJsonString(key) << ':' << toJsonString(value);
  }

  os << '}';

  return os.str();
}



/* ****************************************************************************
*
* JsonHelper -
*/
JsonHelper::JsonHelper(): empty(true)
{
  ss << '{';
}



/* ****************************************************************************
*
* JsonHelper::addString -
*/
void JsonHelper::addString(const std::string& key, const std::string& value)
{
  if (!empty)
  {
    ss << ',';
  }
  ss << toJsonString(key) << ':' << toJsonString(value);

  empty = false;
}



/* ****************************************************************************
*
* JsonHelper::addRaw -
*/
void JsonHelper::addRaw(const std::string& key, const std::string& value)
{
  if (!empty)
  {
    ss << ',';
  }
  ss << toJsonString(key) << ':' << value;

  empty = false;
}



/* ****************************************************************************
*
* JsonHelper::addNumber(long long) -
*/
void JsonHelper::addNumber(const std::string& key, long long value)
{
  if (!empty)
  {
    ss << ',';
  }
  ss << toJsonString(key) << ':' << value;

  empty = false;
}



/* ****************************************************************************
*
* decimalDigits
*
* This function counts the number of decimal digits of a given float, to a maximum of
* PRECISION_DIGITS. The algorithm is inspired in http://stackoverflow.com/a/1083316/1485926
* but with a "cutting condition" needed due to float representation may have an infinite
* number of decimals, e.g. 3.14 could be internally coded as 3.1399999.
*
* FIXME #2425: this function is not perfect and could be improved. For example,
* considering the following
*
*   "A1":  42.9,
*   "A2":  42.99,
*   "A3":  42.999,
*   "A4":  42.9999,
*   "A5":  42.99999,
*   "A6":  42.999999,
*   "A7":  42.9999999,
*   "A8":  42.99999999,
*   "A9":  42.999999999,
*   "A10": 42.9999999999,,
*
*   "A1":  42.1,
*   "A2":  42.01,
*   "A3":  42.001,
*   "A4":  42.0001,
*   "A5":  42.00001,
*   "A6":  42.000001,
*   "A7":  42.0000001,
*   "A8":  42.00000001,
*   "A9":  42.000000001,
*   "A10": 42.0000000001,
*
* what we get is:
*
*   "A1":  42.9,
*   "A2":  42.99,
*   "A3":  42.999,
*   "A4":  42.9999,
*   "A5":  42.99999,
*   "A6":  42.999999000, (fail)
*   "A7":  42.999999900, (fail)
*   "A8":  42.999999990, (fail)
*   "A9":  42.999999999,
*   "A10": 43.000000000, (fail, although probably not due to this function but the caller)
*
*   "A1":  42.1,
*   "A2":  42.01,
*   "A3":  42.001,
*   "A4":  42.0001,
*   "A5":  42.00001,
*   "A6":  42.000001000, (fail)
*   "A7":  42.000000100, (fail)
*   "A8":  42.000000010, (fail)
*   "A9":  42,           (fail)
*   "A10": 42,
*
*/
static unsigned int decimalDigits(double d)
{
  unsigned int digits = 0;

  double intPart;
  double decimalPart = fabs(modf(d, &intPart));

  while (decimalPart > PRECISION)
  {
    digits++;
    decimalPart *= 10;
    decimalPart = modf(decimalPart, &intPart);
    if (fabs(1 - decimalPart ) < PRECISION)
    {
      // Using a greater threshold (e.g. 0.01) would cause rounding errors,
      // e.g. 42.9999 -> 43. This can be easily checked with the
      // cases/2176_not_print_spurious_decimals/one_to_nine_decimals.test test
      // (try to use PRECISION * 10 and check how the test fails).
      //
      break;
    }
  }

  if (digits > PRECISION_DIGITS)
  {
    return PRECISION_DIGITS;
  }
  else
  {
    return digits;
  }
}
/* ****************************************************************************
*
* JsonHelper::addFloat -
*
* FIXME P4: This method is to be removed, the float version of addNumber()
*           should be used instead.
*           See issue #3058
*/
void JsonHelper::addFloat(const std::string& key, float  value)
{
  unsigned int oldPrecision = ss.precision();
  ss << std::fixed << std::setprecision(decimalDigits(value));

  if (!empty)
  {
    ss << ',';
  }
  ss << toJsonString(key) << ':' << value;

  // Reset stream to old parameters (whichever they are...)
  ss.unsetf(std::ios_base::fixed);
  ss << std::setprecision(oldPrecision);

  empty = false;
}




/* ****************************************************************************
*
* JsonHelper::addDate -
*/
void JsonHelper::addDate(const std::string& key, long long timestamp)
{
  if (!empty)
  {
    ss << ',';
  }
  ss << toJsonString(key) << ':' << toJsonString(isodate2str(timestamp));

  empty = false;
}



/* ****************************************************************************
*
* JsonHelper::addBool -
*/
void JsonHelper::addBool(const std::string& key, bool b)
{
  addRaw(key, b? "true" : "false");
}



/* ****************************************************************************
*
* JsonHelper::str -
*/
std::string JsonHelper::str()
{
  ss << '}';
  return ss.str();
}
