#include <iostream>
#include <string>
#include <vector>
#include <memory>
#include <stdexcept>
#include <functional> // Для std::bind
#include <algorithm> // Для std::sort, std::is_sorted
#include <map>        // Для MyDatabase

// --- Google Test и Google Mock ---
// Импортируем их прямо в этот файл для простоты.
// В реальном проекте это было бы отдельными include-файлами.
#include "gtest/gtest.h"
#include "gmock/gmock.h"

// --- 1. IDatabase.h ---
// Абстрактный базовый класс для работы с базой данных
class IDatabase {
public:
    virtual ~IDatabase() = default; // Виртуальный деструктор

    // Подключение к базе данных
    virtual bool connect(const std::string& connection_string) = 0;

    // Отключение от базы данных
    virtual void disconnect() = 0;

    // Структура для представления результата запроса (упрощенно)
    struct User {
        int id;
        std::string name;
        std::string email;
    };

    // Выполнение SQL-запроса, возвращающего одну запись (например, пользователя)
    virtual User getUserById(int id) = 0;

    // Пример другого запроса, возвращающего, например, количество строк
    virtual int countUsers() = 0;
};

// --- 2. MyDatabase.h / .cpp (Имитация реальной БД) ---
class MyDatabase : public IDatabase {
private:
    bool connected = false;
    std::map<int, User> users_db; // Имитация таблицы пользователей

public:
    MyDatabase() {
        // Заполним имитационную базу данными при создании
        users_db[1] = { 1, "Alice Smith", "alice@example.com" };
        users_db[2] = { 2, "Bob Johnson", "bob@example.com" };
        users_db[3] = { 3, "Charlie Brown", "charlie@example.com" };
    }

    bool connect(const std::string& connection_string) override {
        //std::cout << "MyDatabase: Подключение с '" << connection_string << "'..." << std::endl;
        if (connection_string.empty()) {
            connected = false;
            return false;
        }
        connected = true;
        //std::cout << "MyDatabase: Подключено." << std::endl;
        return true;
    }

    void disconnect() override {
        if (connected) {
            //std::cout << "MyDatabase: Отключение." << std::endl;
            connected = false;
        }
    }

    User getUserById(int id) override {
        if (!connected) {
            throw std::runtime_error("MyDatabase: Не подключено.");
        }
        auto it = users_db.find(id);
        if (it != users_db.end()) {
            return it->second;
        }
        throw std::runtime_error("MyDatabase: Пользователь с id=" + std::to_string(id) + " не найден.");
    }

    int countUsers() override {
        if (!connected) {
            throw std::runtime_error("MyDatabase: Не подключено.");
        }
        return users_db.size();
    }
};

// --- 3. UserManager.h / .cpp ---
class UserManager {
private:
    std::unique_ptr<IDatabase> db;

public:
    UserManager(std::unique_ptr<IDatabase> database)
        : db(std::move(database)) {
    }

    bool initialize(const std::string& connection_string) {
        if (!db) {
            return false;
        }
        return db->connect(connection_string);
    }

    std::string getUserName(int userId) {
        if (!db) {
            throw std::runtime_error("UserManager: База данных не инициализирована.");
        }
        try {
            IDatabase::User user = db->getUserById(userId); // Используем User из IDatabase
            return user.name;
        }
        catch (const std::runtime_error& e) {
            throw;
        }
    }

    int getTotalUserCount() {
        if (!db) {
            throw std::runtime_error("UserManager: База данных не инициализирована.");
        }
        return db->countUsers();
    }

    void shutdown() {
        if (db) {
            db->disconnect();
        }
    }
};

// --- 4. MockDatabase.h ---
// Используем gmock для создания мок-объекта
class MockDatabase : public IDatabase {
public:
    MOCK_METHOD(bool, connect, (const std::string& connection_string), (override));
    MOCK_METHOD(void, disconnect, (), (override));
    MOCK_METHOD(User, getUserById, (int id), (override));
    MOCK_METHOD(int, countUsers, (), (override));
};


// --- 5. UserManagerTests.cpp ---

// Используем namespace для удобства
using ::testing::_;       // wildcard для любых аргументов
using ::testing::Return; // для возвращаемого значения
using ::testing::Throw;   // для имитации выбрасывания исключения
using ::testing::Nice;    // Позволяет не определять все методы мока
using ::testing::DefaultValue; // Можно задать значение по умолчанию для мок-методов

// Тестовый набор (Test Suite) для UserManager
TEST(UserManagerTest, GetUserName_Connected_ReturnsName) {
    auto mock_db = std::make_unique<MockDatabase>();

    EXPECT_CALL(*mock_db, connect(_))
        .Times(1)
        .WillOnce(Return(true));

    IDatabase::User expected_user = { 1, "Alice Wonderland", "alice@wonderland.com" };
    EXPECT_CALL(*mock_db, getUserById(1))
        .Times(1)
        .WillOnce(Return(expected_user));

    EXPECT_CALL(*mock_db, disconnect())
        .Times(1);

    UserManager manager(std::move(mock_db));
    ASSERT_TRUE(manager.initialize("dummy_connection_string"));

    std::string userName = manager.getUserName(1);
    ASSERT_EQ(userName, "Alice Wonderland");

    manager.shutdown();
}

TEST(UserManagerTest, GetUserName_UserNotFound_ThrowsException) {
    auto mock_db = std::make_unique<MockDatabase>();

    EXPECT_CALL(*mock_db, connect(_))
        .Times(1)
        .WillOnce(Return(true));

    EXPECT_CALL(*mock_db, getUserById(99))
        .Times(1)
        .WillOnce(Throw(std::runtime_error("User not found in mock")));

    EXPECT_CALL(*mock_db, disconnect())
        .Times(1);

    UserManager manager(std::move(mock_db));
    ASSERT_TRUE(manager.initialize("dummy_connection_string"));

    ASSERT_THROW(manager.getUserName(99), std::runtime_error);

    manager.shutdown();
}

TEST(UserManagerTest, GetUserName_NotConnected_ThrowsException) {
    auto mock_db = std::make_unique<MockDatabase>();

    EXPECT_CALL(*mock_db, connect(_))
        .Times(1)
        .WillOnce(Return(false));

    EXPECT_CALL(*mock_db, getUserById(_)).Times(0);
    EXPECT_CALL(*mock_db, countUsers()).Times(0);
    EXPECT_CALL(*mock_db, disconnect()).Times(0);

    UserManager manager(std::move(mock_db));

    ASSERT_FALSE(manager.initialize("invalid_string"));
    ASSERT_THROW(manager.getUserName(1), std::runtime_error);
    manager.shutdown();
}

TEST(UserManagerTest, GetTotalUserCount_ReturnsCount) {
    auto mock_db = std::make_unique<MockDatabase>();

    EXPECT_CALL(*mock_db, connect(_))
        .Times(1)
        .WillOnce(Return(true));

    EXPECT_CALL(*mock_db, countUsers())
        .Times(1)
        .WillOnce(Return(5));

    EXPECT_CALL(*mock_db, disconnect())
        .Times(1);

    UserManager manager(std::move(mock_db));
    ASSERT_TRUE(manager.initialize("dummy_connection_string"));

    int userCount = manager.getTotalUserCount();
    ASSERT_EQ(userCount, 5);

    manager.shutdown();
}

// --- Основная функция для запуска тестов ---
int main(int argc, char** argv) {
    ::testing::InitGoogleMock(&argc, argv);
    return RUN_ALL_TESTS();
}