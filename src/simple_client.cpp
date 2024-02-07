#include "simple_net.hpp"

int main(int argc, char* argv[]) {
    if (argc != 2) { return 0; }

    std::string username(argv[1]);
    CustomClient client(username);
    client.Connect("127.0.0.1", 60000);
    client.Register();

    std::string strCommand;
    std::string strContent;

    while (std::getline(std::cin, strCommand)) {
        if (strCommand == "up") {
            client.Update();
        }
        else if (strCommand == "disconnect") {
            break;
        }
        else if (strCommand.substr(0, 3) == "msg") {
            std::getline(std::cin, strContent);
            client.SendCustomMessage(strCommand, strContent);
        }
        else {

        }
    }
    return 0;
}