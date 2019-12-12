//
// Created by wkl04 on 12-12-2019.
//

#ifndef MQTTSUBSCRIPTION_RETAINED_TOPIC_MAP_H
#define MQTTSUBSCRIPTION_RETAINED_TOPIC_MAP_H

#include <mqtt/string_view.hpp>
#include <map>
#include "path_tokenizer.h"

template<typename Value >
class retained_topic_map
{
    typedef size_t node_id_type;
    typedef std::pair< node_id_type, std::string> path_entry_key;

    enum { root_node_id = 0 };

    struct path_entry
    {
        node_id_type id;
        uint32_t count;

        Value value;

        path_entry(node_id_type _id)
                : id(_id), count(1)
        { }
    };

    typedef std::map< path_entry_key, path_entry > map_type;
    typedef typename map_type::iterator map_type_iterator;
    typedef typename map_type::const_iterator map_type_const_iterator;

    map_type map;
    map_type_iterator root;
    node_id_type next_node_id;

    map_type_iterator create_topic(MQTT_NS::string_view const &topic)
    {
        boost::tokenizer< boost::char_separator<char> > tokens = mqtt_path_tokenizer(topic);

        map_type_iterator parent = root;
        for(boost::tokenizer< boost::char_separator<char> >::const_iterator t = tokens.begin(); t != tokens.end(); ++t) {
            if(*t == "+" || *t == "#")
                throw std::runtime_error("No wildcards allowed in retained topic name");

            node_id_type parent_id = parent->second.id;
            map_type_iterator entry = map.find(path_entry_key(parent_id, *t));

            if(entry == map.end())  {
                entry = map.insert({ path_entry_key(parent_id, *t), path_entry(next_node_id++) }).first;
            } else {
                boost::tokenizer< boost::char_separator<char> >::const_iterator next_t = t;
                ++next_t;
                entry->second.count++;
            }

            parent = entry;
        }

        return parent;
    }

    std::vector< std::pair<map_type_iterator, map_type_iterator> > find_topic(MQTT_NS::string_view const &topic)
    {
        boost::tokenizer< boost::char_separator<char> > tokens = mqtt_path_tokenizer(topic);

        map_type_iterator parent = root;

        std::vector< std::pair<map_type_iterator, map_type_iterator> > path;

        for (auto const  &t : tokens) {
            map_type_iterator entry = map.find(path_entry_key(parent->second.id, t));

            if(entry == map.end())
                return std::vector< std::pair<map_type_iterator, map_type_iterator> >();

            path.push_back(std::make_pair(parent, entry));
            parent = entry;
        }

        return path;
    }

    void match_hash_entries(node_id_type parent, std::deque<map_type_const_iterator> &new_entries) const {
        for(map_type_const_iterator i = map.lower_bound(path_entry_key(parent, "")); i != map.end(); ++i) {
            if(i->first.first != parent)
                return;

            new_entries.push_back(i);
            match_hash_entries(i->second.id, new_entries);
        }
    }

    // Find all values that math the specified path
    void find_match(MQTT_NS::string_view const &topic, std::function< void (Value const &) > const &callback) const
    {
        boost::tokenizer< boost::char_separator<char> > tokens = mqtt_path_tokenizer(topic);

        std::deque<map_type_const_iterator> entries;
        entries.push_back(root);

        for (auto const  &t : tokens) {
            std::deque<map_type_const_iterator> new_entries;
            new_entries.resize(0);

            bool hash_matched = false;

            for(auto const &entry: entries) {
                node_id_type parent = entry->second.id;

                if(t == "+") {
                    for(map_type_const_iterator i = map.lower_bound(path_entry_key(parent, "")); i != map.end(); ++i) {
                        if(i->first.first == parent)
                            new_entries.push_back(i);
                        else
                            break;
                    }
                } else if(t == "#") {
                    match_hash_entries(parent, new_entries);
                    hash_matched = true;
                } else {
                    map_type_const_iterator i = map.find(path_entry_key(parent, t));
                    if(i != map.end())
                        new_entries.push_back(i);
                }
            }

            if(new_entries.empty())
                return;
            entries = std::move(new_entries);

            if(hash_matched)
                break;
        }

        for(auto const &entry: entries) {
            callback(entry->second.value);
        }
    }

    // Remove a value at the specified subscription path
    bool remove_topic(MQTT_NS::string_view const &topic)
    {
        std::vector< std::pair<map_type_iterator, map_type_iterator> > path = find_topic(topic);
        if(path.empty())
            return false;

        for(size_t i = 0; i < path.size(); ++i)
        {
            map_type_iterator entry = path[path.size() - i - 1].second;

            --(entry->second.count);
            if(entry->second.count == 0)
                map.erase(entry);
        }

        return true;
    }

public:
    retained_topic_map()
            : next_node_id(root_node_id)
    {
        // Create the root node
        root = map.insert({path_entry_key(std::numeric_limits<node_id_type>::max(), ""), path_entry(root_node_id) }).first;
        ++next_node_id;
    }

    // Insert a value at the specified subscription path
    void insert_or_update(MQTT_NS::string_view const &topic, Value const &value)
    {
        std::vector< std::pair<map_type_iterator, map_type_iterator> > path = find_topic(topic);
        if(path.empty())
            this->create_topic(topic)->second.value = value;
        else
            path.back().second->second.value = value;
    }

    // Find all values that math the specified path
    void find(MQTT_NS::string_view const &topic, std::function< void (Value const &) > const &callback) const
    {
        this->find_match(topic, callback);
    }

    // Remove a stored value at the specified topic
    void remove(MQTT_NS::string_view const &topic)
    {
        this->remove_topic(topic);
    }
};

#endif //MQTTSUBSCRIPTION_RETAINED_TOPIC_MAP_H
