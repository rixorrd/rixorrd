#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <exception>
#include <limits>

using namespace std;

// Исключение для ошибок аутентификации
class AuthenticationException : public exception {
public:
    const char* what() const noexcept override {
        return "Ошибка аутентификации: неверный логин или пароль.";
    }
};

// Исключение для ошибок регистрации
class RegistrationException : public exception {
public:
    const char* what() const noexcept override {
        return "Ошибка регистрации: логин уже существует.";
    }
};

// Класс пользователя
class User {
private:
    string login;
    string password;
    string name;

public:
    User(const string& login, const string& password, const string& name)
        : login(login), password(password), name(name) {
    }

    string getLogin() const { return login; }
    string getName() const { return name; }

    bool checkPassword(const string& pass) const {
        return password == pass;
    }
};

// Шаблон класса сообщения
template<typename T>
class Message {
private:
    T sender;
    T recipient; // Может быть пустым для общего сообщения
    string text;

public:
    Message(const T& sender, const T& recipient, const string& text)
        : sender(sender), recipient(recipient), text(text) {
    }

    void display() const {
        if (recipient.getLogin().empty()) {
            // Общее сообщение
            cout << sender.getName() << " (общие): " << text << "\n";
        }
        else {
            // Личное сообщение
            cout << sender.getName() << " -> " << recipient.getName() << ": " << text << "\n";
        }
    }

    T getSender() const { return sender; }
    T getRecipient() const { return recipient; }
};

// Класс чата
class Chat {
public:
    // Хранение пользователей по логину
    map<string, shared_ptr<User>> users;

private:
    // Вектор сообщений (можно расширить до хранения истории)
    vector<Message<User>> messages;

public:
    // Регистрация нового пользователя
    void registerUser(const string& login, const string& password, const string& name) {
        if (users.find(login) != users.end()) {
            throw RegistrationException();
        }
        users[login] = make_shared<User>(login, password, name);
        cout << "Пользователь " << name << " успешно зарегистрирован.\n";
    }

    // Вход в чат
    shared_ptr<User> loginUser(const string& login, const string& password) {
        auto it = users.find(login);
        if (it == users.end() || !it->second->checkPassword(password)) {
            throw AuthenticationException();
        }
        return it->second;
    }

    // Отправка личного сообщения
    void sendPrivateMessage(const User& sender, const User& recipient, const string& text) {
        Message<User> msg(sender, recipient, text);
        messages.push_back(msg);
        msg.display();
    }

    // Отправка общего сообщения всем пользователям
    void sendBroadcastMessage(const User& sender, const string& text) {
        for (const auto& pair : users) {
            if (pair.second->getLogin() != sender.getLogin()) { // Не отправлять себе
                Message<User> msg(sender, *pair.second, text);
                messages.push_back(msg);
                msg.display();
            }
        }
        // Также можно вывести сообщение о том, что сообщение отправлено всем
        cout << sender.getName() << " отправил(а) общее сообщение.\n";
    }

    // Получение списка пользователей
    void listUsers() const {
        cout << "Список зарегистрированных пользователей:\n";
        for (const auto& pair : users) {
            cout << "- " << pair.second->getName() << " (логин: " << pair.second->getLogin() << ")\n";
        }
    }
};

void showMenu() {
    cout << "\nМеню:\n"
        << "1. Регистрация\n"
        << "2. Вход в чат\n"
        << "0. Выход\n"
        << "Выберите действие: ";
}

void chatMenu() {
    cout << "\nЧат:\n"
        << "1. Отправить личное сообщение\n"
        << "2. Отправить общее сообщение\n"
        << "3. Посмотреть список пользователей\n"
        << "0. Выйти из чата\n"
        << "Выберите действие: ";
}

int main() {
    setlocale(LC_ALL, "Russian");
    Chat chat;

    while (true) {
        try {
            showMenu();
            int choice;
            cin >> choice;
            if (choice == 1) { // Регистрация
                string login, password, name;
                cout << "Введите логин: ";
                cin >> login;
                cout << "Введите пароль: ";
                cin >> password;
                cout << "Введите имя: ";
                cin >> name;
                chat.registerUser(login, password, name);
            }
            else if (choice == 2) { // Вход в чат
                string login, password;
                cout << "Введите логин: ";
                cin >> login;
                cout << "Введите пароль: ";
                cin >> password;

                auto user = chat.loginUser(login, password);
                bool inChat = true;

                while (inChat) {
                    chatMenu();
                    int chatChoice;
                    cin >> chatChoice;

                    switch (chatChoice) {
                    case 1: { // Личное сообщение
                        chat.listUsers();
                        string recipientLogin;
                        cout << "Введите логин получателя: ";
                        cin >> recipientLogin;

                        auto it = chat.users.find(recipientLogin);
                        if (it == chat.users.end()) {
                            throw AuthenticationException();
                        }

                        auto recipient = it->second;

                        if (recipient->getLogin() == user->getLogin()) {
                            cout << "Нельзя отправить сообщение самому себе.\n";
                            break;
                        }

                        string messageText;
                        cout << "Введите сообщение: ";
                        cin.ignore(numeric_limits<streamsize>::max(), '\n');
                        getline(cin, messageText);

                        chat.sendPrivateMessage(*user, *recipient, messageText);
                        break;
                    }
                    case 2: { // Общее сообщение
                        string messageText;
                        cout << "Введите сообщение: ";
                        cin.ignore(numeric_limits<streamsize>::max(), '\n');
                        getline(cin, messageText);

                        chat.sendBroadcastMessage(*user, messageText);
                        break;
                    }
                    case 3:
                        chat.listUsers();
                        break;
                    case 0:
                        inChat = false;
                        break;
                    default:
                        cout << "Некорректный выбор.\n";
                    }
                }

            }
            else if (choice == 0) { // Выход из программы
                break;
            }
            else {
                cout << "Некорректный выбор.\n";
            }
        }
        catch (const RegistrationException& e) {
            cout << "Ошибка регистрации: " << e.what() << "\n";
        }
        catch (const AuthenticationException& e) {
            cout << "Ошибка аутентификации: " << e.what() << "\n";
        }
        catch (...) {
            cout << "Произошла неизвестная ошибка.\n";
        }

        // Очистка буфера ввода перед следующей итерацией
        while (cin.peek() != '\n' && !cin.eof())
            (cin).ignore(numeric_limits<streamsize>::max(), '\n');
    }

    cout << "Выход из программы.\n";

    return 0;
}