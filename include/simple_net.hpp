#include "net.hpp"

enum class MessageType : uint32_t {
    ClientRegister,
    ValidateClient,
    MessageToServer,
    MessageToAll,
    MessageToClient
};

struct TextMessage {
    std::string username;
    std::string content;
};

net::message<MessageType>& operator<<(net::message<MessageType>& msg, const std::string& str) {
    std::size_t len = str.length();
    std::size_t i = msg.body.size();
    msg.body.resize(msg.body.size() + len);
    std::memcpy(msg.body.data() + i, str.data(), len);
    msg.header.size = msg.size();
    return msg;
}

net::message<MessageType>& operator>>(net::message<MessageType>& msg, std::string& str) {
    std::size_t len = msg.body.size();
    str.resize(len);
    std::memcpy(str.data(), msg.body.data(), len);
    msg.body.clear();
    msg.header.size = msg.size();
    return msg;
}

net::message<MessageType>& operator<<(net::message<MessageType>& msg, const TextMessage& txtmsg) {
    msg << txtmsg.username << txtmsg.content << uint8_t(txtmsg.username.length());
    return msg;
}

net::message<MessageType>& operator>>(net::message<MessageType>& msg, TextMessage& txtmsg) {
    uint8_t len;
    msg >> len;
    std::string user_and_content;
    msg >> user_and_content;
    txtmsg.username = user_and_content.substr(0, len);
    txtmsg.content = user_and_content.substr(len);
    return msg;
}

class CustomClient : public net::client_interface<MessageType> {
public:
    CustomClient(const std::string username) : strUsername(username) {
    }

    ~CustomClient() override {
        Disconnect();
    }

    void Register() {
        while (!bRegistered) {
            Update(true);
        }
        std::cout << "Connection successful! Registered as: " << strUsername << '\n';
    }

    void SendCustomMessage(std::string strCommand, std::string strContent) {
        net::message<MessageType> msg{};
        std::string strDest = strCommand.substr(4);
        if (strDest == "server") {
            msg.header.id = MessageType::MessageToServer;
            msg << strContent;
        }
        else if (strDest == "all") {
            msg.header.id = MessageType::MessageToAll;
            msg << strContent;
        }
        else {
            TextMessage txtmsg{strDest, strContent};
            msg.header.id = MessageType::MessageToClient;
            msg << txtmsg;
        }
        Send(msg);
    }

    void Update(bool bWait = false) {
        if (bWait) {
            Incoming().wait();
        }
        while (!Incoming().empty()) {
            net::message<MessageType> msg = Incoming().pop_front().msg;
            switch (msg.header.id) {
                case MessageType::MessageToAll: {
                    std::string content;
                    msg >> content;
                    std::cout << "[ALL]: " << content << '\n';
                    break;
                }
                case MessageType::MessageToClient: {
                    TextMessage txtmsg;
                    msg >> txtmsg;
                    std::cout << "[" << txtmsg.username << "]: " << txtmsg.content << '\n';
                    break;
                }
                case MessageType::ValidateClient: {
                    net::message<MessageType> msg;
                    msg.header.id = MessageType::ClientRegister;
                    msg << strUsername;
                    Send(msg);
                    bRegistered = true;
                    break;
                }
                default: {
                    break;
                }
            }
        }
    }

private:
    const std::string strUsername;
    bool bRegistered = false;
};

class CustomServer : public net::server_interface<MessageType> {
public:
    CustomServer(uint16_t nport) : net::server_interface<MessageType>(nport) {
    }

    void DisconnectAllClients() {
        std::cout << "Disconnecting all...\n";
        for (auto& c : m_deqConnections) {
            c->Disconnect();
        }
    }

    void OnClientValidated(std::shared_ptr<net::connection<MessageType>> pClient) override {
        net::message<MessageType> msg;
        msg.header.id = MessageType::ValidateClient;
        MessageClient(pClient, msg);
    }

protected:
    std::unordered_map<uint32_t, std::string> mapUsers;

    void OnMessage(std::shared_ptr<net::connection<MessageType>> pClient, net::message<MessageType>& msg) override {
        switch(msg.header.id) {
            case MessageType::MessageToServer: {
                std::string content;
                msg >> content;
                std::cout << "[" << mapUsers[pClient->GetID()] << "] -> [SERVER]: " << content << '\n';
                break;
            }
            case MessageType::MessageToAll: {
                MessageAllClients(msg, pClient);
                std::string content;
                msg >> content;
                std::cout << "[" << mapUsers[pClient->GetID()] << "] -> [ALL]: " << content << '\n';
                break;
            }
            case MessageType::MessageToClient: {
                TextMessage txtmsg{};
                msg >> txtmsg;
                std::cout << "[" << mapUsers[pClient->GetID()] << "] -> [" << txtmsg.username << "]: " << txtmsg.content << '\n';
                for (const auto& c : m_deqConnections) {
                    if (mapUsers[c->GetID()] == txtmsg.username) {
                        txtmsg.username = mapUsers[pClient->GetID()];
                        msg << txtmsg;
                        MessageClient(c, msg);
                        break;
                    }
                }
                break;
            }
            case MessageType::ClientRegister: {
                std::string username;
                msg >> username;
                mapUsers[pClient->GetID()] = username;
                std::cout << "[" << pClient->GetID() << "] Registered as [" << username << "]\n";
                break;
            }
            default: {
                break;
            }
        }
    }

    bool OnClientConnect(std::shared_ptr<net::connection<MessageType>> pClient) override {
        return true;
    }

    void OnClientDisconnect(std::shared_ptr<net::connection<MessageType>> pClient) override {
        std::cout << "[" << pClient->GetID() << "] Disconnected.\n";
    }
};