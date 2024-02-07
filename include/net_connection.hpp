#pragma once

#include "net_common.hpp"
#include "net_tsqueue.hpp"
#include "net_message.hpp"

namespace net {

    template <typename T>
    class server_interface;
    
    template <typename T>
    class connection : public std::enable_shared_from_this<connection<T>> {
    public:
        enum class owner {
            server,
            client
        };

        connection(owner parent, asio::io_context& asioContext, asio::ip::tcp::socket socket, tsqueue<owned_message<T>>& qIn) :
            m_asioContext(asioContext), m_socket(std::move(socket)), m_qMessagesIn(qIn), m_nOwnerType(parent) {
                if (m_nOwnerType == owner::server) {
                    m_nHandshakeOut = uint64_t(std::chrono::system_clock::now().time_since_epoch().count());
                    m_nHandshakeCheck = scramble(m_nHandshakeOut);
                }
                else {

                }
        }

        virtual ~connection() {
            Disconnect();
        }

        uint32_t GetID() {
            return id;
        }

        void ConnectToClient(server_interface<T>* server, uint32_t uid = 0) {
            if (m_nOwnerType == owner::server) {
                if (m_socket.is_open()) {
                    id = uid;
                    WriteValidation();
                    ReadValidation(server);
                }
            }
        }

        void ConnectToServer(const asio::ip::tcp::resolver::results_type& endpoints) {
            if (m_nOwnerType == owner::client) {
                asio::async_connect(m_socket, endpoints, 
                [this](std::error_code ec, asio::ip::tcp::endpoint endpoint) {
                    if (!ec) {
                        ReadValidation();
                    }
                    else {
                        std::cout << "Failed to connect to server: " << ec.message() << '\n';
                    }
                });
            }
        }

        void Disconnect() {
            if (IsConnected()) {
                asio::post(m_asioContext, [this]() { m_socket.close(); });
            }
        }

        bool IsConnected() const {
            return m_socket.is_open();
        }

        void Send(const message<T>& msg) {
            asio::post(m_asioContext, 
            [this, msg]() {
                bool bWritingMessage = !m_qMessagesOut.empty();
                m_qMessagesOut.emplace_back(msg);
                if (!bWritingMessage) {
                    WriteHeader();
                }
            });
        }

    protected:
        owner m_nOwnerType = owner::server;
        asio::io_context& m_asioContext;
        asio::ip::tcp::socket m_socket;
        tsqueue<owned_message<T>>& m_qMessagesIn;
        tsqueue<message<T>> m_qMessagesOut;
        message<T> m_msgTemporaryIn;
        uint32_t id = 0;

    private:
        uint64_t m_nHandshakeOut = 0;
        uint64_t m_nHandshakeIn = 0;
        uint64_t m_nHandshakeCheck = 0;

        void ReadHeader() {
            asio::async_read(m_socket, asio::buffer(&m_msgTemporaryIn.header, sizeof(message_header<T>)), 
            [this](std::error_code ec, std::size_t length) {
                if (!ec) {
                    if (m_msgTemporaryIn.header.size > sizeof(message_header<T>)) {
                        m_msgTemporaryIn.body.resize(m_msgTemporaryIn.header.size - sizeof(message_header<T>));
                        ReadBody();
                    }
                    else {
                        AddToIncomingMessagesQueue();
                    }
                }
                else {
                    std::cout << "[" << id << "] Read header failed: " << ec.message() <<  '\n';
                    m_socket.close();
                }
            });
        }

        void ReadBody() {
            asio::async_read(m_socket, asio::buffer(m_msgTemporaryIn.body.data(), m_msgTemporaryIn.header.size - sizeof(message_header<T>)), 
            [this](std::error_code ec, std::size_t length){
                if (!ec) {
                    AddToIncomingMessagesQueue();
                }
                else {
                    std::cout << "[" << id << "] Read body failed.\n";
                    m_socket.close();
                }
            });
        }

        void AddToIncomingMessagesQueue() {
            if (m_nOwnerType == owner::server) {
                m_qMessagesIn.emplace_back({this->shared_from_this(), m_msgTemporaryIn});
            }
            else {
                m_qMessagesIn.emplace_back({nullptr, m_msgTemporaryIn});
            }
            ReadHeader();
        }

        void WriteHeader() {
            asio::async_write(m_socket, asio::buffer(&m_qMessagesOut.front().header, sizeof(message_header<T>)),
            [this](std::error_code ec, std::size_t length) {
                if (!ec) {
                    if (m_qMessagesOut.front().body.size() > 0) {
                        WriteBody();
                    }
                    else {
                        m_qMessagesOut.pop_front();
                        if (!m_qMessagesOut.empty()) {
                            WriteHeader();
                        }
                    }
                }
                else {
                    std::cout << "[" << id << "] Write header failed: " << ec.message() << '\n';
                    m_socket.close();
                }
            });
        }

        void WriteBody() {
            asio::async_write(m_socket, asio::buffer(m_qMessagesOut.front().body.data(), m_qMessagesOut.front().body.size()),
            [this](std::error_code ec, std::size_t length) {
                if (!ec) {
                    m_qMessagesOut.pop_front();
                    if (!m_qMessagesOut.empty()) {
                        WriteHeader();
                    }
                }
                else {
                    std::cout << "[" << id << "] Write body failed.\n";
                    m_socket.close();
                }
            });
        }

        uint64_t scramble(uint64_t nInput) {
            uint64_t out = nInput ^ 0xDEADBEEFC0DECAFE;
            out = (out & 0xF0F0F0F0F0F0F0) >> 4 | (out & 0x0F0F0F0F0F0F0F) << 4;
            return out ^ 0xC0DEFACE12345678;
        }

        void WriteValidation() {
            asio::async_write(m_socket, asio::buffer(&m_nHandshakeOut, sizeof(uint64_t)), 
            [this](std::error_code ec, std::size_t length) {
                if (!ec) {
                    if (m_nOwnerType == owner::client) {
                        ReadHeader();
                    }
                }
                else {
                    m_socket.close();
                }
            });
        }

        void ReadValidation(server_interface<T>* server = nullptr) {
            asio::async_read(m_socket, asio::buffer(&m_nHandshakeIn, sizeof(uint64_t)), 
            [this, server](std::error_code ec, std::size_t length) {
                if (!ec) {
                    if (m_nOwnerType == owner::server) {
                        if (m_nHandshakeIn == m_nHandshakeCheck) {
                            std::cout << "[" << id << "] Client validated.\n";
                            server->OnClientValidated(this->shared_from_this());
                            ReadHeader();
                        }
                        else {
                            std::cout << "[" << id << "] Client disconnected (validation failed).\n";
                            m_socket.close();
                        }
                    }
                    else {
                        m_nHandshakeOut = scramble(m_nHandshakeIn);
                        WriteValidation();
                    }
                }
                else {
                    std::cout << "Client disconnected (ReadValidation)\n";
                    m_socket.close();
                }
            });
        }
    };
}