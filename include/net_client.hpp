#pragma once

#include "net_common.hpp"
#include "net_tsqueue.hpp"
#include "net_message.hpp"
#include "net_connection.hpp"

namespace net {

    template <typename T>
    class client_interface {
    public:
        client_interface() {
        }

        virtual ~client_interface() {
            Disconnect();
        }

        bool Connect(const std::string& host, const uint16_t port) {
            try {
                asio::ip::tcp::resolver resolver(m_asioContext);
                asio::ip::tcp::resolver::results_type endpoints = resolver.resolve(host, std::to_string(port));
                m_connection = std::make_unique<connection<T>>(connection<T>::owner::client, m_asioContext, asio::ip::tcp::socket(m_asioContext), m_qMessagesIn);
                m_connection->ConnectToServer(endpoints);
                m_thrContext = std::thread([this]() { m_asioContext.run(); });
                return true;
            }
            catch (std::exception& e) {
                std::cerr << "Client exception: " << e.what() << '\n';
                return false;
            }
        }

        void Disconnect() {
            if (IsConnected()) {
                m_connection->Disconnect();
            }
            m_asioContext.stop();
            if (m_thrContext.joinable()) {
                m_thrContext.join();
            }
            m_connection.release();
        }

        bool IsConnected() const {
            return m_connection ? m_connection->IsConnected() : false;
        }

        void Send(const message<T>& msg) const {
            if (IsConnected()) {
                m_connection->Send(msg);
            }
        }

        tsqueue<owned_message<T>>& Incoming() {
            return m_qMessagesIn;
        }

    protected:
        asio::io_context m_asioContext;
        std::thread m_thrContext;
        std::unique_ptr<connection<T>> m_connection;

    private:
        tsqueue<owned_message<T>> m_qMessagesIn;
    };

}