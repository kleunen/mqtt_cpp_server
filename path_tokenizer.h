//
// Created by wkl04 on 12-12-2019.
//

#ifndef MQTTSUBSCRIPTION_PATH_TOKENIZER_H
#define MQTTSUBSCRIPTION_PATH_TOKENIZER_H

#include <boost/tokenizer.hpp>
#include <boost/functional/hash.hpp>

static inline boost::tokenizer< boost::char_separator<char> > mqtt_path_tokenizer(MQTT_NS::string_view const &path)
{
    static boost::char_separator<char> mqtt_path_separator("/",  "", boost::keep_empty_tokens);
    return boost::tokenizer< boost::char_separator<char> >(path, mqtt_path_separator);
}

#endif //MQTTSUBSCRIPTION_PATH_TOKENIZER_H
