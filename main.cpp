#include "precomp.h"

#include <string>
#include <iostream>

#include "mqtt_server_cpp.hpp"
#include "subscription_map.h"
#include "retained_topic_map.h"

using con_t = MQTT_NS::server<>::endpoint_t;
using con_sp_t = std::shared_ptr<con_t>;

struct session_t
{
    MQTT_NS::buffer client_id;
    std::weak_ptr<con_t> con;

    using session_subs_t = std::map< MQTT_NS::buffer, MQTT_NS::qos >;
    session_subs_t subscriptions;

    session_t(const std::weak_ptr<con_t> &con)
        : con(con)
    { }

    ~session_t()
    {
        std::cout << "Release session: " << client_id << std::endl;
    }

    std::shared_ptr<con_t> get_connection()
    {
        auto sp = con.lock();
        BOOST_ASSERT(sp);
        return sp;
    }

    void add_subscription(MQTT_NS::buffer topic, MQTT_NS::qos qos)
    {
        subscriptions[topic] = qos;
    }

    void remove_subscription(MQTT_NS::buffer topic)
    {
        auto j = subscriptions.find(topic);
        if (j != subscriptions.end())
            subscriptions.erase(j);
    }

    std::optional<MQTT_NS::qos> get_subscription(MQTT_NS::buffer topic) const
    {
        auto j = subscriptions.find(topic);
        if (j != subscriptions.end())
            return std::make_optional(j->second);
        else
            return std::optional<MQTT_NS::qos>();
    }

    void publish(MQTT_NS::buffer topic_name, MQTT_NS::buffer contents, MQTT_NS::publish_options options) {
        auto sp = con.lock();
        if(sp) {
            sp->publish(
                    boost::asio::buffer(topic_name),
                    boost::asio::buffer(contents),
                    std::make_pair(topic_name, contents), options);
        }
    }
};

using session_ptr_t = std::shared_ptr<session_t>;
using session_weak_ptr_t = std::weak_ptr<session_t>;

using subscription_map_t = multiple_subscription_map<std::pair<session_ptr_t, MQTT_NS::qos>, std::deque>;

inline void close_session(subscription_map_t &subs_map, std::set<session_ptr_t> &sessions, session_ptr_t const &session) {
    for(auto const &i: session->subscriptions)
        subs_map.remove(i.first, std::make_pair(session, i.second));

    sessions.erase(session);
    std::cout << "Active sessions: " << sessions.size() << std::endl;
}

int main(int argc, char** argv) {
    if (argc != 2) {
        std::cout << argv[0] << " port" << std::endl;
        return -1;
    }
    boost::asio::io_context ioc;

    auto server_endpoint = boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(),boost::lexical_cast<std::uint16_t>(argv[1]));
    auto s = MQTT_NS::server<>(server_endpoint, ioc);

    s.set_error_handler(
            [](MQTT_NS::error_code ec) {
                std::cout << "error: " << ec.message() << std::endl;
            }
    );

    subscription_map_t subs_map;
    retained_topic_map<MQTT_NS::buffer> retained_map;

    std::set<session_ptr_t> sessions;

    s.set_accept_handler(
            [&subs_map, &retained_map, &sessions](con_sp_t spep) {
                auto& ep = *spep;

                session_ptr_t session = std::make_shared<session_t>(std::weak_ptr<con_t>(spep));

                using packet_id_t = typename std::remove_reference_t<decltype(ep)>::packet_id_t;
                std::cout << "accept" << std::endl;

                // Pass spep to keep lifetime.
                // It makes sure wp.lock() never return nullptr in the handlers below
                // including close_handler and error_handler.
                ep.start_session(session);

                // set connection (lower than MQTT) level handlers
                ep.set_close_handler(
                        [&subs_map, &sessions, session]() {
                            std::cout << "[server] closed session: " << session->client_id << std::endl;
                            close_session(subs_map, sessions, session);
                        });

                ep.set_error_handler(
                        [&subs_map, &sessions, session](MQTT_NS::error_code ec) {
                            std::cout << "[server] error: " << ec.message() << " " << session->client_id << std::endl;
                            close_session(subs_map, sessions, session);
                        });

                // set MQTT level handlers
                ep.set_connect_handler(
                        [&sessions, session](MQTT_NS::buffer client_id, MQTT_NS::optional<MQTT_NS::buffer> username, MQTT_NS::optional<MQTT_NS::buffer> password, MQTT_NS::optional<MQTT_NS::will>, bool clean_session, std::uint16_t keep_alive) {
                            auto sp = session->get_connection();

                            using namespace MQTT_NS::literals;
                            std::cout << "[server] client_id    : " << client_id << std::endl;
                            std::cout << "[server] username     : " << (username ? username.value() : "none"_mb) << std::endl;
                            std::cout << "[server] password     : " << (password ? password.value() : "none"_mb) << std::endl;
                            std::cout << "[server] clean_session: " << std::boolalpha << clean_session << std::endl;
                            std::cout << "[server] keep_alive   : " << keep_alive << std::endl;

                            session->client_id = client_id;
                            sessions.insert(session);
                            sp->connack(false, MQTT_NS::connect_return_code::accepted);
                            return true;
                        }
                );

                ep.set_pingreq_handler([session]() {
                    auto sp = session->get_connection();
                    sp->async_pingresp();
                    return true;
                });

                ep.set_disconnect_handler(
                        [&subs_map, &sessions, session]() {
                            auto sp = session->get_connection();
                            std::cout << "[server] disconnect received." << std::endl;
                            close_session(subs_map, sessions, session);
                        });

                ep.set_puback_handler(
                        [](packet_id_t packet_id){
                            std::cout << "[server] puback received. packet_id: " << packet_id << std::endl;
                            return true;
                        });

                ep.set_pubrec_handler(
                        [](packet_id_t packet_id){
                            std::cout << "[server] pubrec received. packet_id: " << packet_id << std::endl;
                            return true;
                        });

                ep.set_pubrel_handler(
                        [](packet_id_t packet_id){
                            std::cout << "[server] pubrel received. packet_id: " << packet_id << std::endl;
                            return true;
                        });

                ep.set_pubcomp_handler(
                        [](packet_id_t packet_id){
                            std::cout << "[server] pubcomp received. packet_id: " << packet_id << std::endl;
                            return true;
                        });

                ep.set_publish_handler(
                        [&subs_map, &retained_map, session]
                                (MQTT_NS::optional<packet_id_t> packet_id,
                                 MQTT_NS::publish_options pubopts,
                                 MQTT_NS::buffer topic_name,
                                 MQTT_NS::buffer contents){
                            std::cout << "[server] publish received."
                                      << " dup: "    << pubopts.get_dup()
                                      << " qos: "    << pubopts.get_qos()
                                      << " retain: " << pubopts.get_retain() << std::endl;
                            if (packet_id)
                                std::cout << "[server] packet_id: " << *packet_id << std::endl;
                            std::cout << "[server] topic_name: " << topic_name << std::endl;
                            std::cout << "[server] contents: " << contents << std::endl;

                            std::map< session_ptr_t, MQTT_NS::qos > subscribers;

                            if(pubopts.get_retain() == MQTT_NS::retain::yes)
                                retained_map.insert_or_update(topic_name, contents);

                            subs_map.find(std::string(topic_name.data(), topic_name.size()), [&topic_name, &contents, &pubopts, &subscribers]( std::pair<session_ptr_t, MQTT_NS::qos> const &r){
                                subscribers[r.first] = std::max(r.second, subscribers[r.first]);
                            });

                            std::cout << "Subscribers found: " << subscribers.size() << std::endl;
                            for(auto const &r: subscribers)
                                r.first->publish(topic_name, contents, std::min(r.second, pubopts.get_qos()) |  pubopts.get_retain());

                            return true;
                        });

                ep.set_subscribe_handler(
                        [&subs_map, &retained_map, session] (packet_id_t packet_id, std::vector<std::tuple<MQTT_NS::buffer, MQTT_NS::subscribe_options>> entries) {
                            std::cout << "[server]subscribe received. packet_id: " << packet_id << std::endl;
                            std::vector<MQTT_NS::suback_return_code> res;
                            res.reserve(entries.size());

                            auto sp = session->get_connection();

                            for (auto const& e : entries) {
                                MQTT_NS::buffer topic = std::get<0>(e);
                                MQTT_NS::qos qos_value = std::get<1>(e).get_qos();
                                std::cout << "[server] topic: " << topic  << " qos: " << qos_value << std::endl;

                                session->add_subscription(topic, qos_value);
                                subs_map.insert(topic, std::make_pair(session, qos_value));

                                res.emplace_back(MQTT_NS::qos_to_suback_return_code(qos_value));

                                retained_map.find(topic, [&session, &topic, qos_value](MQTT_NS::buffer const &content) {
                                    session->publish(topic, content, qos_value);
                                });
                            }

                            sp->suback(packet_id, res);
                            return true;
                        }
                );

                ep.set_unsubscribe_handler([&subs_map, session](packet_id_t packet_id, std::vector<MQTT_NS::buffer> topics) {
                            std::cout << "[server]unsubscribe received. packet_id: " << packet_id << ", client id: " << session->client_id << std::endl;

                            auto sp = session->get_connection();

                            for (auto const& topic : topics) {
                                auto j = session->get_subscription(topic);
                                if(j)
                                    subs_map.remove(topic, std::make_pair(session, *j));
                                session->remove_subscription(topic);
                            }

                            sp->unsuback(packet_id);
                            return true;
                        }
                );
            }
    );

    s.listen();

    ioc.run();
}