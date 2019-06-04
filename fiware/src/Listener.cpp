/*
 * Copyright 2019 Proyectos y Sistemas de Mantenimiento SL (eProsima).
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
*/

#include "Listener.hpp"

#include <iostream>

namespace soss {
namespace fiware {


Listener::Listener(
        uint16_t port,
        DataReceivedCallback callback)

    : port_{port}
    , listen_thread_{}
    , running_{false}
    , errors_{false}
    , read_callback_{callback}
{
}

Listener::~Listener()
{
    stop();
}

void Listener::run()
{
    listen_thread_ = std::thread(&Listener::listen, this);
    running_ = true;

    //TODO: should wait until the port was open, use a condition variable?
}

void Listener::stop()
{
    if(running_ && listen_thread_.joinable())
    {
        service_.stop();
        running_ = false;
        listen_thread_.join();

        for (std::thread& th: message_threads_)
        {
            if (th.joinable())
            {
                th.join();
            }
        }
    }
}

void Listener::listen()
{
    try
    {
        asio::ip::tcp::endpoint endpoint(asio::ip::tcp::v4(), port_);
        asio::error_code error;
        tcp::acceptor acceptor(service_, endpoint);

        std::cout << "[soss-fiware][listener]: listening fiware at port " << port_ << std::endl;

        start_accept(acceptor);
        service_.run();
    }
    catch (std::exception& e)
    {
        std::cerr << "[soss-fiware][listener]: connection error: " << e.what() << std::endl;
        errors_ = true;
    }

    std::cout << "[soss-fiware][listener]: stop listening" << std::endl;
}

void Listener::start_accept(tcp::acceptor& acceptor)
{
    std::shared_ptr<tcp::socket> socket(new tcp::socket (service_));

    acceptor.async_accept(*socket, std::bind(&Listener::accept_handler, this, socket, std::ref(acceptor)));
}


void Listener::accept_handler(
        std::shared_ptr<tcp::socket> socket,
        tcp::acceptor& acceptor)
{
    message_threads_.push_back(std::thread(&Listener::read_msg, this, socket));
    start_accept(acceptor);
}

void Listener::read_msg(std::shared_ptr<tcp::socket> socket)
{
    try
    {
        //wait for some data
        socket->read_some(asio::null_buffers());

        std::string message;
        message.resize(socket->available());

        socket->read_some(asio::buffer(&message[0], message.size()));

        read_callback_(message);
    }
    catch (std::exception& e)
    {
        std::cerr << "[soss-fiware][listener]: connection error at read thread: " << e.what() << std::endl;
    }
}

} // namespace fiware
} // namespace soss