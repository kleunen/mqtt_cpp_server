//
// Created by wkl04 on 10-12-2019.
//
#include "precomp.h"

#include "subscription_map.h"
#include "retained_topic_map.h"

#include <iostream>

void TestSingleSubscription()
{
    std::string text = "example/test/A";

    single_subscription_map< std::function< void() > > map;
    map.insert("example/test/A", [] { std::cout << "example/test/A: Subscriber 1" << std::endl; } );

    try {
        map.insert("example/test/A", [] { std::cout << "example/test/A: Subscriber 2" << std::endl; } );
    } catch(std::exception &e)
    {
        // Should fail due to duplicate subscription
        std::cout << e.what() << std::endl;
    }

    map.insert("example/test/B", [] { std::cout << "example/test/B" << std::endl; } );
    map.insert("example/+/A", [] { std::cout << "example/+/A: Plus" << std::endl; } );
    map.insert("example/#", [] { std::cout << "example/#" << std::endl; } );

    std::cout << "Test with hash, plus and full path" << std::endl;
    map.find("example/test/A", [](std::function< void() >  const &a) {
        a();
    });

    map.remove("example/+/A");

    std::cout << "Test with hash, full path" << std::endl;
    map.find("example/test/A", [](std::function< void() > const &a) {
        a();
    });

    map.remove("example/#");

    std::cout << "Test with full path only" << std::endl;
    map.find("example/test/A", [](std::function< void() >  const &a) {
        a();
    });

    map.remove("example/test/A");
    // map.remove("example/test/A");
    map.remove("example/test/B");

    std::cout << "No entries exist anymore" << std::endl;
    map.find("example/test/A", [](std::function< void() >  const &a) {
        a();
    });

    std::cout << "Remaining size should be 1 (root element only)" << std::endl;
    std::cout << "Remaining size: " << map.size() << std::endl;
}

void TestMultipleSubscription()
{
    std::string text = "example/test/A";

    multiple_subscription_map<std::string, std::deque> map;

    try {
        map.insert("doubleslash//#", "Should fail");
    } catch(std::exception &e)
    {
        // Should fail due to empty path usage
        std::cout << e.what() << std::endl;
    }

    map.insert("example/test/A", "example/test/A: Subscriber 1");
    map.insert("example/test/A", "example/test/A: Subscriber 2");
    map.insert("example/test/B", "example/test/B");
    map.insert("example/+/A", "example/+/A: Plus");
    map.insert("example/#", "example/#");

    std::cout << "Test with hash, plus and full path" << std::endl;
    map.find("example/test/A", [](std::string const &a) {
        std::cout << a << std::endl;
    });

    map.remove("example/+/A", "example/+/A: Plus");

    std::cout << "Test with hash, full path" << std::endl;
    map.find("example/test/A", [](std::string const &a) {
        std::cout << a << std::endl;
    });

    map.remove("example/#", "example/#");

    std::cout << "Test with full path only" << std::endl;
    map.find("example/test/A", [](std::string const &a) {
        std::cout << a << std::endl;
    });

    map.remove("example/test/A", "example/test/A: Subscriber 1");
    map.remove("example/test/A", "example/test/A: Subscriber 2");
    map.remove("example/test/B", "example/test/B");

    std::cout << "No entries exist anymore" << std::endl;
    map.find("example/test/A", [](std::string const &a) {
        std::cout << a << std::endl;
    });

    std::cout << "Remaining size should be 1 (root element only)" << std::endl;
    std::cout << "Remaining size: " << map.size() << std::endl;
}

void TestRetainedTopics()
{
    retained_topic_map<std::string> map;
    map.insert_or_update("example/test/A", "example/test/A: Content 1");
    map.insert_or_update("example/test/B", "example/test/B: Content 1");
    map.insert_or_update("example/A/test", "example/A/test: Content 1");
    map.insert_or_update("example/B/test", "example/B/test: Content 1");

    /*map.find("example/test/A", [](std::string const &a) {
        std::cout << a << std::endl;
    });

    map.find("example/test/B", [](std::string const &a) {
        std::cout << a << std::endl;
    });

    map.find("example/test/+", [](std::string const &a) {
        std::cout << a << std::endl;
    });

    map.find("example/+/B", [](std::string const &a) {
        std::cout << a << std::endl;
    }); */

    map.find("example/+/A", [](std::string const &a) {
        std::cout << a << std::endl;
    });

    map.remove("example/test/A");
    map.find("example/+/A", [](std::string const &a) {
        std::cout << a << std::endl;
    });

}

#include <mqtt/subscribe_options.hpp>
#include <mqtt/buffer.hpp>

struct session
{
    using session_subs_t = std::map< MQTT_NS::buffer, MQTT_NS::qos >;

    MQTT_NS::buffer client_id;
    session_subs_t subscriptions;


    session(const mqtt::buffer &clientId)
        : client_id(clientId)
    { }

    void add_subscription(MQTT_NS::buffer topic, MQTT_NS::qos qos)
    {
        subscriptions[topic] = qos;
    }
};

using session_ptr_t = std::shared_ptr<session>;

#include <boost/multi_index_container.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index/member.hpp>

namespace mi = boost::multi_index;

class session_db
{
    struct tag_client_id { };

    using session_index = mi::multi_index_container<
            session_ptr_t,
            mi::indexed_by<mi::ordered_unique< mi::tag<tag_client_id>, mi::member<session, MQTT_NS::buffer, &session::client_id> > >
    >;

    session_index sessions;
};

void TestSessions()
{

};

int main(int, char**)
{
    try {
        TestSingleSubscription();
        TestMultipleSubscription();
        TestRetainedTopics();
        TestSessions();

    } catch(std::exception &e)
    {
        std::cout << e.what() << std::endl;
    }
}
