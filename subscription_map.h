//
// Created by wkl04 on 10-12-2019.
//

#ifndef MQTTSUBSCRIPTION_SUBSCRIPTION_MAP_H
#define MQTTSUBSCRIPTION_SUBSCRIPTION_MAP_H

#include <mqtt/string_view.hpp>

#include <unordered_map>
#include "path_tokenizer.h"

template<typename Value>
class subscription_map_base
{
    typedef size_t node_id;
    typedef std::pair< node_id, std::string> path_entry_key;

    enum { root_node_id = 0 };

    struct path_entry
    {
        node_id id;

        bool has_hash_child : 1;
        bool has_plus_child : 1;
        uint32_t count : 30;

        Value value;

        path_entry(node_id _id)
                : id(_id), has_hash_child(false), has_plus_child(false), count(1)
        { }
    };

    typedef std::unordered_map< path_entry_key, path_entry, boost::hash< path_entry_key > > map_type;
    typedef typename map_type::iterator map_type_iterator;
    typedef typename map_type::const_iterator map_type_const_iterator;

    map_type map;
    map_type_iterator root;
    node_id next_node_id;

protected:
    map_type_iterator end() { return map.end(); }

    std::vector< std::pair<map_type_iterator, map_type_iterator> > find_subscription(MQTT_NS::string_view const &topic)
    {
        auto tokens = mqtt_path_tokenizer(topic);
        auto parent = root;

        std::vector< std::pair<map_type_iterator, map_type_iterator> > path;

        for (auto const  &t : tokens) {
            auto entry = map.find(path_entry_key(parent->second.id, t));

            if(entry == map.end())
                return std::vector< std::pair<map_type_iterator, map_type_iterator> >();

            path.push_back(std::make_pair(parent, entry));
            parent = entry;
        }

        return path;
    }

    map_type_iterator create_subscription(MQTT_NS::string_view const &topic)
    {
        auto tokens = mqtt_path_tokenizer(topic);

        auto parent = root;
        for(auto t = tokens.begin(); t != tokens.end(); ++t) {
            auto parent_id = parent->second.id;
            auto entry = map.find(path_entry_key(parent_id, *t));

            if(entry == map.end())  {
                entry = map.insert({ path_entry_key(parent_id, *t), path_entry(next_node_id++) }).first;
                if(*t == "+")
                    parent->second.has_plus_child = true;
                if(*t == "#")
                    parent->second.has_hash_child = true;

            } else {
                auto next_t = t;
                ++next_t;
                entry->second.count++;
            }

            parent = entry;
        }

        return parent;
    }

    // Remove a value at the specified subscription path
    map_type_iterator remove_subscription(MQTT_NS::string_view const &topic)
    {
        auto path = find_subscription(topic);
        if(path.empty())
            return map.end();

        for(size_t i = 0; i < path.size(); ++i)
        {
            auto parent = path[path.size() - i - 1].first;
            auto entry = path[path.size() - i - 1].second;

            --(entry->second.count);
            if(entry->second.count == 0) {
                if(entry->first.second == "+")
                    parent->second.has_plus_child = false;
                if(entry->first.second == "#")
                    parent->second.has_hash_child = false;

                map.erase(entry);
                if(i == 0)
                    return map.end();
            }
        }

        return path.back().second;
    }

    // Find all values that math the specified path
    void find_match(MQTT_NS::string_view const &topic, std::function< void (Value const &) > const &callback) const
    {
        auto tokens = mqtt_path_tokenizer(topic);

        std::deque<map_type_const_iterator> entries;
        entries.push_back(root);

        std::deque<map_type_const_iterator> new_entries;

        for (auto const  &t : tokens) {
            new_entries.resize(0);

            for(auto const &entry: entries) {
                auto parent = entry->second.id;
                auto i = map.find(path_entry_key(parent, t));
                if(i != map.end())
                    new_entries.push_back(i);

                if(entry->second.has_plus_child)
                {
                    i = map.find(path_entry_key(parent, "+"));
                    if(i != map.end())
                        new_entries.push_back(i);
                }

                if(entry->second.has_hash_child)
                {
                    i = map.find(path_entry_key(parent, "#"));
                    if(i != map.end())
                    {
                        callback(i->second.value);
                    }
                }
            }

            if(new_entries.empty())
                return;
            entries = new_entries;
        }

        for(auto const &entry: entries)
            callback(entry->second.value);
    }

    subscription_map_base()
            : next_node_id(root_node_id)
    {
        // Create the root node
        root = map.insert({path_entry_key(std::numeric_limits<node_id>::max(), ""), path_entry(root_node_id) }).first;
        ++next_node_id;
    }

public:
    // Return the number of elements in the tree
    size_t size() const { return map.size(); }
};

template<typename Value>
class single_subscription_map
        : public subscription_map_base<Value>
{

public:

    // Insert a value at the specified subscription path
    void insert(MQTT_NS::string_view const &topic, Value const &value)
    {
        if(!this->find_subscription(topic).empty())
            throw std::runtime_error(std::string("Subscription already exists in map: ").append(topic.data(), topic.size()));
        this->create_subscription(topic)->second.value = value;
    }

    // Remove a value at the specified subscription path
    void remove(MQTT_NS::string_view const &topic)
    {
        this->remove_subscription(topic);
    }

    // Find all values that math the specified path
    void find(MQTT_NS::string_view const &topic, std::function< void (Value const &) > const &callback) const
    {
        this->find_match(topic, callback);
    }
};


template<typename Value, template <typename, typename> class Cont = std::vector >
class multiple_subscription_map
        : public subscription_map_base< Cont<Value, std::allocator<Value> > >
{

public:

    // Insert a value at the specified subscription path
    void insert(MQTT_NS::string_view const &topic, Value const &value)
    {
        this->create_subscription(topic)->second.value.push_back(value);
    }

    // Remove a value at the specified subscription path
    void remove(MQTT_NS::string_view const &topic, Value const &value)
    {
        auto i = this->remove_subscription(topic);
        if(i != this->end())
            i->second.value.erase(std::find(i->second.value.begin(), i->second.value.end(), value));
    }

    // Find all values that math the specified path
    void find(MQTT_NS::string_view const &topic, std::function< void (Value const &) > const &callback) const
    {
        this->find_match(topic, [&callback]( Cont<Value, std::allocator<Value> > const &values ) {
            for(Value const &i: values)
                callback(i);
        });
    }
};

#endif //MQTTSUBSCRIPTION_SUBSCRIPTION_MAP_H
