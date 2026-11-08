#include <mysql_driver.h>
#include <mysql_connection.h>
#include <cppconn/statement.h>
#include <cppconn/prepared_statement.h>
#include <cppconn/resultset.h>
#include <cppconn/exception.h>
#include <iostream>
#include <sstream>
#include <vector>
#include <limits>
using namespace std;

int user = 1;
int adminMode = 0;
int func = 2;

void changeUser(int newUser)
{
    user = newUser;
    return;
}

void changeMode()
{
    adminMode = !adminMode;
    return;
}

void waitForEnter()
{
    std::cout << "\nPress Enter to continue...\n";
    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
    std::cin.get();
    return;
}

string getUsername(sql::Connection *con)
{
    unique_ptr<sql::PreparedStatement> pstmt(
        con->prepareStatement(
            "select username from user where userId = ?"));

    pstmt->setInt(1, user);
    unique_ptr<sql::ResultSet> res(pstmt->executeQuery());
    string name = "User";

    while (res->next())
    {
        name = res->getString("username");
        std::cout << name << "\n";
    }
    return name;
}

void sendMessage(sql::Connection *con, int chat)
{
    string msg;
    std::cout << "Type your message:\n";
    std::cin.ignore();
    getline(std::cin, msg);

    std::unique_ptr<sql::PreparedStatement> pstmt(
        con->prepareStatement(
            "insert into msg (sender, chatId, msgTxt) values (?, ?, ?)"));

    pstmt->setInt(1, user);
    pstmt->setInt(2, chat);
    pstmt->setString(3, msg);

    pstmt->executeUpdate();
    return;
}

int countMessages(sql::Connection *con, int chat)
{
    std::unique_ptr<sql::PreparedStatement> pstmt(
        con->prepareStatement("SELECT COUNT(*) FROM msg WHERE chatId = ?"));
    pstmt->setInt(1, chat);

    std::unique_ptr<sql::ResultSet> res(pstmt->executeQuery());

    int total = 0;
    if (res->next())
    {
        total = res->getInt(1);
    }
    return total;
}

void dealWithMsg(sql::Connection *con, std::vector<int> selectedMsgId, std::vector<int> selectedMsgSender)
{
    bool allFromUser = 1;
    for (int i = 0; i < selectedMsgSender.size(); i++)
    {
        if (selectedMsgSender[i] != user)
        {
            allFromUser = 0;
            break;
        }
    }
    std::cout << "\n=== Menu ===\n";
    std::cout << "| 1 - Back\n";
    std::cout << "| 2 - Delete message(s) for me\n";
    if (allFromUser == 1)
    {
        std::cout << "| 3 - Delete message(s) for everyone\n";
        if (selectedMsgId.size() == 1)
        {
            std::cout << "| 4 - Edit message\n";
        }
    }
    int nextStep;
    cout << "\n";
    std::cin >> nextStep;
    cout << "\n";

    switch (nextStep)
    {
    case 2:
        for (int i = 0; i < selectedMsgId.size(); i++)
        {
            std::unique_ptr<sql::PreparedStatement> pstmt(
                con->prepareStatement(
                    "insert into hiddenMsg (msgId, userId) values (?, ?)"));

            pstmt->setInt(1, selectedMsgId[i]);
            pstmt->setInt(2, user);

            pstmt->executeUpdate();
        }
        break;
    case 4:
        if (selectedMsgId.size() > 1)
        {
            std::cout << "You can only edit one message at a time. Please select only one message.\n";
            return;
        }
        else
        {
            std::string newMsg;
            std::cout << "Type your new message:\n";
            std::cin.ignore();
            getline(std::cin, newMsg);

            std::unique_ptr<sql::PreparedStatement> pstmt(
                con->prepareStatement(
                    "update msg set msgTxt = ?, alterDt = NOW() where msgId = ?"));

            pstmt->setString(1, newMsg);
            pstmt->setInt(2, selectedMsgId[0]);

            pstmt->executeUpdate();
        }
        break;
    case 3:
        for (int i = 0; i < selectedMsgId.size(); i++)
        {
            std::unique_ptr<sql::PreparedStatement> pstmt(
                con->prepareStatement(
                    "delete from msg where msgId = ?"));

            pstmt->setInt(1, selectedMsgId[i]);
            pstmt->executeUpdate();
        }
        break;

    default:
        return;
    }
    return;
}

void leaveGroup(sql::Connection *con, int chat, int creator)
{
    std::cout << "Certeza que quer sair do grupo? (1 - Sim, 2 - Não)\n";
    int choice;
    std::cin >> choice;
    if (choice == 1)
    {
        std::unique_ptr<sql::PreparedStatement> pstmt(
            con->prepareStatement(
                "delete from userInChat where userId = ? and chatId = ?"));
        pstmt->setInt(1, user);
        pstmt->setInt(2, chat);
        pstmt->executeQuery();
    }

    if(user == creator){
        std::unique_ptr<sql::PreparedStatement> newCreator(
            con->prepareStatement(
                "select userId from userInChat where chatId = ? order by entranceDay limit 1"
            )
        );
        newCreator->setInt(1, chat);
        unique_ptr<sql::ResultSet> res(newCreator->executeQuery());
        if(res->next()){
            creator = res->getInt("userId");
            std::unique_ptr<sql::PreparedStatement> updateCreator(
                con->prepareStatement(
                    "update chat set creator = ? where chatId = ?"
                )
            );
            updateCreator->setInt(1, creator);
            updateCreator->setInt(2, chat);
            updateCreator->executeUpdate();
        }else{
            std::unique_ptr<sql::PreparedStatement> deleteGroup(
                con->prepareStatement(
                    "delete from chat where chatId = ?"
                )
            );
            deleteGroup->setInt(1, chat);
        }
        
    }
    return;
}

void showMoreMsg(int &total, int &initialAmount, int &start)
{
    initialAmount += 10;
    if (initialAmount > total)
    {
        initialAmount = total;
    }
    start = 0;
    return;
}

void showMsgFromChat(sql::Connection *con, int chat)
{
    std::unique_ptr<sql::PreparedStatement> pstmt(
        con->prepareStatement(
            "select * from chat where chatId = ?"));

    pstmt->setInt(1, chat);
    unique_ptr<sql::ResultSet> res(pstmt->executeQuery());
    string chatType = "";
    bool isPersonalDm = false;
    int creator = -1;

    if (res->next())
    {
        chatType = res->getString("chatType");
        if (chatType == "DM")
        {
            if (res->getInt("user1") == user && res->getInt("user2") == user)
            {
                isPersonalDm = true;
            }
        }
        creator = res->getInt("creator");
    }

    int nextStep = 0;
    int initialAmount = 10;

    while (nextStep != 2)
    {
        std::unique_ptr<sql::PreparedStatement> pstmt(
            con->prepareStatement(
                "select sender, description, msgTxt, username, msgId, date_format(sendDt, '%H:%i') as time,  DATE_FORMAT(sendDt, '%d/%m/%Y') as date, alterDt from msg join user on msg.sender = userId where chatId = ? and not exists ( select 1 from hiddenMsg where msg.msgId = hiddenMsg.msgId and userId = ?) order by sendDt"));

        pstmt->setInt(1, chat);
        pstmt->setInt(2, user);
        unique_ptr<sql::ResultSet> res(pstmt->executeQuery());

        vector<int> msgId;
        vector<string> msgTxt;
        vector<string> username;
        vector<int> sender;
        vector<string> sendTime;
        vector<string> sendDt;
        vector<string> alterDt;
        string description;

        while (res->next())
        {
            sender.push_back(res->getInt("sender"));
            msgId.push_back(res->getInt("msgId"));
            msgTxt.push_back(res->getString("msgTxt"));
            username.push_back(res->getString("username"));
            sendTime.push_back(res->getString("time"));
            sendDt.push_back(res->getString("date"));
            if (res->isNull("alterDt"))
            {
                alterDt.push_back("NULL");
            }
            else
            {
                alterDt.push_back(res->getString("alterDt"));
            }
        }

        int total = msgId.size();

        if (initialAmount > total)
        {
            initialAmount = total;
        }

        if (total == 0)
        {
            std::cout << "Ainda não há mensagens nesse chat, seja o primeiro a enviar uma mensagem!\n";
        }

        int start = std::max(0, abs(initialAmount - total));

        std::vector<int> selectedMsgId;
        std::vector<int> selectedMsgSender;

        string msgTime = "AAA";
        string msgDt = "AA";

        for (int i = start; i < total; i++)
        {
            int numeroDaMensagem = i + 1 - start;
            if (sendDt[i] != msgDt) // formatação da mensagem
            {
                cout << "\n\t" << sendDt[i] << "\n\n";
                msgDt = sendDt[i];
            }
            if (sender[i] == user)
            {
                std::cout << "\t\t" << msgTxt[i]
                          << "\n\t\t" << (alterDt[i] != "NULL" ? alterDt[i] + " (Editado)" : sendTime[i])
                          << "\n\t\t(" << numeroDaMensagem << ")\n\n";
            }
            else
            {
                std::cout << username[i] << "\n";
                std::cout << msgTxt[i] << "\n"
                          << (alterDt[i] != "NULL" ? alterDt[i] + "(Editado)" : sendTime[i])
                          << "\n(" << numeroDaMensagem << ")\n\n";
            }
        }

        std::cout << "\n=== Menu ===\n";
        std::cout << "| 1 - Send message\n";
        std::cout << "| 2 - Back\n";
        std::cout << "| 3 - Select message(s)\n";
        int count = 4;
        if (chatType == "GROUP")
        {
            std::cout << "| 4 - View participants\n";
            count = 5;
            if (creator == user)
            {
                std::cout << "| 5 - Add participants\n";
                std::cout << "| 6 - Exclude from group\n";
                count = 7;
            }
        }
        if (total > initialAmount)
        {
            std::cout << "| " << count << " - Show more messages\n";
            count++;
        }
        std::cout << "| " << count << " - Leave Group\n";

        cout << "\n";
        std::cin >> nextStep;
        cout << "\n";

        if (nextStep == 1)
        {
            sendMessage(con, chat);
        }
        else if (nextStep == 3)
        {
            std::cout << "Select message(s) by number (press 0 to finish):\n";
            int msgNumber;
            while (std::cin >> msgNumber)
            {
                if (msgNumber == 0)
                {
                    break;
                }
                if (msgNumber < 1 || msgNumber > total - start)
                {
                    std::cout << "Invalid message number. Please try again\n";
                    continue;
                }
                selectedMsgId.push_back(msgId[start + msgNumber - 1]);
                selectedMsgSender.push_back(sender[start + msgNumber - 1]);
            }

            dealWithMsg(con, selectedMsgId, selectedMsgSender);
        }
        else if (nextStep == 4)
        {
            if (chatType != "GROUP")
            {

                if (total > initialAmount)
                {
                    showMoreMsg(total, initialAmount, start);
                }
                else
                {
                    leaveGroup(con, chat, creator);
                    return;
                }
            }
            else
            {
                std::unique_ptr<sql::PreparedStatement> pstmt(
                    con->prepareStatement(
                        "select username from userInChat join user on userInChat.userId = user.userId and chatId = ?"));

                pstmt->setInt(1, chat);

                unique_ptr<sql::ResultSet> res(pstmt->executeQuery());

                while (res->next())
                {
                    cout << res->getString("username") << "\n";
                }
                waitForEnter();
            }
        }
        else if (nextStep == 5)
        {
            if (chatType != "GROUP" || creator != user)
            {

                if (total > initialAmount)
                {
                    showMoreMsg(total, initialAmount, start);
                }
                else
                {
                    leaveGroup(con, chat, creator);
                    return;
                }
            }
            else
            {
                vector<int> userId;
                vector<int> userChoice;
                cout << "Select user(s) you want to add into the group (press 0 to end):\n";
                std::unique_ptr<sql::PreparedStatement> pstmt(
                    con->prepareStatement(
                        "select username, userId from user where not exists (select 1 from userInChat where user.userId = userInChat.userId and userInChat.chatId = ?)"));

                pstmt->setInt(1, chat);
                unique_ptr<sql::ResultSet> res(pstmt->executeQuery());
                int counter = 1;
                while (res->next())
                {
                    userId.push_back(res->getInt("userId"));
                    cout << counter << " - " << res->getString("username") << "\n";
                    counter++;
                }
                int num = -1;
                while (num != 0)
                {
                    std::cin >> num;

                    if (num == 0)
                        break; // Sai do loop e vai para a inserção

                    if (num < 1 || num >= counter)
                    {
                        std::cout << "Escolha inválida, tente novamente.\n";
                        continue;
                    }

                    userChoice.push_back(userId[num - 1]);
                }
                for (int num : userChoice)
                {
                    cout << num;
                    std::unique_ptr<sql::PreparedStatement> pstmt(
                        con->prepareStatement(
                            "insert into userInChat (userId, chatId) values (?, ?)"));

                    pstmt->setInt(1, num);
                    pstmt->setInt(2, chat);
                    unique_ptr<sql::ResultSet> res(pstmt->executeQuery());
                }
            }
        }
        else if (nextStep == 6)
        {
            if (chatType == "GROUP" && initialAmount < total)
            {
                leaveGroup(con, chat, creator);
                return;
            }
            else
            {

                std::cout << "Selecione o usuário que deseja excluir do grupo:\n";
                std::unique_ptr<sql::PreparedStatement> pstmt(
                    con->prepareStatement(
                        "select username, user.userId from userInChat join user on userInChat.userId = user.userId and chatId = ?"));

                pstmt->setInt(1, chat);
                unique_ptr<sql::ResultSet> res(pstmt->executeQuery());
                vector<int> userId;
                int counter = 1;
                while (res->next())
                {
                    userId.push_back(res->getInt("userId"));
                    cout << counter << " - " << res->getString("username") << "\n";
                    counter++;
                }
                int userChoice;
                std::cin >> userChoice;

                if (userChoice < 1 || userChoice >= counter)
                {
                    std::cout << "Escolha inválida, tente novamente depois\n";
                    waitForEnter();
                    return;
                }
                else
                {
                    std::unique_ptr<sql::PreparedStatement> pstmt(
                        con->prepareStatement(
                            "delete from userInChat where userId = ? and chatId = ?"));
                    pstmt->setInt(1, userId[userChoice - 1]);
                    pstmt->setInt(2, chat);
                    pstmt->executeQuery();
                }
            }
        }
        else if (nextStep == 7)
        {
            if (chatType == "GROUP")
            {
                if (initialAmount < total)
                {
                    showMoreMsg(total, initialAmount, start);
                }
                else
                {
                    leaveGroup(con, chat, creator);
                    return;
                }
            }
        }
        else if (nextStep == 8)
        {
            if (chatType == "GROUP")
            {
                leaveGroup(con, chat, creator);
                return;
            }
        }
        else
        {
            return;
        }
    }
    return;
}

void settings(sql::Connection *con)
{
    std::cout << "\n=== Menu ===\n";
    std::cout << "| 1 - Back   \n";
    std::cout << "| 2 - Create Group\n";
    std::cout << "| 3 - Deactive group\n";
    std::cout << "| 4 - Deactive user\n";
    std::cout << "\n";

    int nextStep;
    std::cin >> nextStep;
    cout << "\n";

    vector<int> userId;

    if (nextStep == 2)
    {
        cout << "\nAre you sure? (1 - Yes, 0 - No)\n\n";
        int choice;
        std::cin >> choice;
        if (choice != 1)
        {
            return;
        }
        
        std::cout << "\n\nType the group name (max: 30):\n";
        string groupName = "";
        string description = "";
        std::cin >> groupName;
        if (groupName.length() > 30 || groupName.length() == 0)
        {
            std::cout << "\n\nInput inválido. Tente novamente depois\n\n";
            return;
        }
        
        cout << "\nDo you want the insert a description to the group? (1 - Yes, 0 - No)\n\n";
        std::cin >> choice;
        if (choice == 1)
        {
            std::cout << "\nType the group's description (max: 200):\n";
            std::cin >> description;
            
            if (description.length() > 200)
            {
                std::cout << "\n\nInput inválido. Tente novamente depois\n\n";
                return;
            }
            return;
        }

        unique_ptr<sql::PreparedStatement> createGroup(
            con->prepareStatement(
                "insert into chat (chatType, groupName, description, creator, dtCreation) values ('GROUP', ?, ?, ?, current_date())"
            )
        );

        createGroup->setString(1, groupName);
        createGroup->setString(2, description);
        createGroup->setInt(3, user);

        createGroup->executeQuery();

        unique_ptr<sql::PreparedStatement> getCreatorInGroup(
            con->prepareStatement(
                "insert into userInChat (userId, chatId) values (?, LAST_INSERT_ID())"
            )
        );

        getCreatorInGroup->setInt(1, user);
        getCreatorInGroup->executeQuery();
    }
    else if (nextStep == 3)
    {
        std::cout << "Selecione o usuário que deseja:\n";
        std::unique_ptr<sql::PreparedStatement> pstmt(
            con->prepareStatement(
                "select username, userId from user"));
        unique_ptr<sql::ResultSet> res(pstmt->executeQuery());

        int counter = 1;
        while (res->next())
        {
            userId.push_back(res->getInt("userId"));
            cout << counter << " - " << res->getString("username");
            counter++;
        }

        int userChoice;
        std::cin >> userChoice;

        if (userChoice < 1 || userChoice >= counter)
        {
            std::cout << "Escolha inválida, tente novamente depois\n";
            waitForEnter();
        }
        else
        {
            user = userId[userChoice - 1];
        }
    }
    
    return;
}

void sendSimultaneousMsg(sql::Connection *con)
{
    cout << "\nAre you sure? (1 - Yes, 0 - No)\n\n";
    int choice;
    std::cin >> choice;
    if (choice != 1)
    {
        return;
    }
    std::cout << "\nType your message:\n\n";
    std::string msg;
    std::cin.ignore();
    getline(std::cin, msg);

    std::unique_ptr<sql::PreparedStatement> pstmt(
        con->prepareStatement(
            "select chat.chatId, username as chatName from chat join user on chat.user1 = user.userId or chat.user2 = user.userId where (chat.user1 = ? and chat.user2 = userId) or (chat.user2 = ? and chat.user1 = userId) union select chat.chatId, groupName as chatName from chat join userInChat on chat.chatId = userInChat.chatId where userId = ?"));

    pstmt->setInt(1, user);
    pstmt->setInt(2, user);
    pstmt->setInt(3, user);

    unique_ptr<sql::ResultSet> res(pstmt->executeQuery());

    int counter = 1;
    vector<int> chatIds;
    while (res->next())
    {
        cout << counter << " - " << res->getString("chatName") << "\n";
        chatIds.push_back(res->getInt("chatId"));
        counter++;
    }
    std::cout << "\nSelect the chat(s) you want to send the message to (press 0 to finish):\n";
    int chatChoice = -1;
    vector<int> selectedChatsId;
    while (chatChoice != 0)
    {
        std::cin >> chatChoice;
        if (chatChoice == 0)
        {
            break;
        }
        else if (chatChoice < 1 || chatChoice >= counter)
        {
            std::cout << "Invalid chat number. Please try again later\n";
            return;
        }
        selectedChatsId.push_back(chatIds[chatChoice - 1]);
    }

    std::unique_ptr<sql::PreparedStatement> insertStmt(
        con->prepareStatement(
            "insert into msg (sender, chatId, msgTxt) values (?, ?, ?)"));

    for (int chatId : selectedChatsId)
    {
        cout << "Sending message to chat ID: " << chatId << "\n";
        insertStmt->setInt(1, user);
        insertStmt->setInt(2, chatId);
        insertStmt->setString(3, msg);
        insertStmt->executeUpdate();
    }
    return;
}

void showChats(sql::Connection *con)
{
    while (func != 0)
    {
        std::unique_ptr<sql::PreparedStatement> pstmt(
            con->prepareStatement(
                "select chatId, chatType, username as chatName from chat join user on chat.user1 = user.userId or chat.user2 = user.userId where (chat.user1 = ? and chat.user2 = userId) or (chat.user2 = ? and chat.user1 = userId) union select chat.chatId, chatType, groupName as chatName from chat join userInChat on chat.chatId = userInChat.chatId where userId = ?"));

        pstmt->setInt(1, user);
        pstmt->setInt(2, user);
        pstmt->setInt(3, user);

        unique_ptr<sql::ResultSet> res(pstmt->executeQuery());

        getUsername(con);
        std::vector<int> chatIds;
        std::vector<std::string> chatTypes;
        std::vector<std::string> chatNames;
        std::cout << "\nChats\t\t*\n";
        while (res->next())
        {
            int id = res->getInt("chatId");
            std::string tipo = res->getString("chatType");
            std::string nome = res->getString("chatName");

            chatIds.push_back(id);
            chatTypes.push_back(tipo);
            chatNames.push_back(nome);
            std::cout << "| " << nome << "\n";
        }

        std::cout << "\n=== Menu ===\n";
        std::cout << "| 0 - Exit   \n";
        std::cout << "| 1 - Select chat\n";
        std::cout << "| 2 - Go into settings\n";
        std::cout << "| 3 - Send message to multiple chats\n\n";
        std::cin >> func;
        std::cout << "\n";

        switch (func)
        {
        case 0:
            return;
        case 1:
            std::cout << "\nSelect a chat:\n";
            for (int i = 0; i < chatNames.size(); i++)
            {
                std::cout << "| " << i + 1 << " - " << chatNames[i] << "\n";
            }
            int chatChoice;
            std::cout << '\n';
            std::cin >> chatChoice;
            std::cout << '\t' << chatNames[chatChoice - 1] << "\n\n";
            showMsgFromChat(con, chatIds[chatChoice - 1]);
            break;
        case 2:
            settings(con);
            break;
        case 3:
            sendSimultaneousMsg(con);
            break;
        default:
            showChats(con);
            break;
        }
    }

    return;
}

int main()
{
    try
    {
        sql::mysql::MySQL_Driver *driver = sql::mysql::get_mysql_driver_instance();

        unique_ptr<sql::Connection> con(
            driver->connect("tcp://127.0.0.1:3306", "root", "F!rmiano2007"));
        con->setSchema("app");

        showChats(con.get());
    }
    catch (sql::SQLException &e)
    {
        cerr << "Error: " << e.what()
             << " (code " << e.getErrorCode() << ")\n";
    }
    return 0;
}