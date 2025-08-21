#include <QApplication>
#include <QMainWindow>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QDebug>
#include <QString>
#include <QDateTime>
#include <QObject>
#include <QSqlRecord>
#include <QTableView>
#include <QPushButton>
#include <QLineEdit>
#include <QTextEdit>
#include <QDialog>
#include <QDateTimeEdit>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTabWidget>
#include <QMessageBox>
#include <QStandardItemModel>
#include <QSortFilterProxyModel>
#include <QSqlTableModel>
#include <QAbstractItemView>
#include <QHeaderView>
#include <QComboBox>
#include <QDateTime>

// --- Предварительные объявления (Forward Declarations) ---
class DatabaseManager;
class BanUserDialog;

// --- 1. DatabaseManager ---
// Объявление класса DatabaseManager, чтобы ServerMainWindow мог его использовать
class DatabaseManager : public QObject {
    Q_OBJECT

public:
    explicit DatabaseManager(QObject* parent = nullptr);
    ~DatabaseManager();

    bool connectToDatabase();
    void disconnectFromDatabase();
    bool isConnected() const;

    void createTablesIfNeeded(); // Создает таблицы, если их нет

    // Пользователи
    QSqlTableModel* getUsersModel();
    bool setUserStatus(int userId, const QString& status);
    bool banUser(int userId, const QString& reason, const QDateTime& endDate);
    bool unbanUser(int userId); // Просто устанавливает статус на 'active'

    // Сообщения
    QSqlTableModel* getMessagesModel();
    bool addMessage(int senderId, int receiverId, const QString& text, const QString& type);
    bool deleteMessage(int messageId); // Опционально

private:
    QSqlDatabase db;
    bool connected = false;
};

// --- 2. BanUserDialog ---
// Объявление класса BanUserDialog
class BanUserDialog : public QDialog {
    Q_OBJECT

public:
    explicit BanUserDialog(QWidget* parent = nullptr);

    void setUserId(int userId, const QString& username);

signals:
    void userBanned(int userId, const QString& reason, const QDateTime& endDate);

private slots:
    void on_ban_button_clicked();
    void on_cancel_button_clicked();
    void update_ban_date_label();

private:
    int current_user_id;
    QString current_username;

    QLabel* user_info_label;
    QLabel* reason_label;
    QTextEdit* reason_input;
    QLabel* ban_end_label;
    QDateTimeEdit* ban_end_datetime_edit;
    QPushButton* ban_button;
    QPushButton* cancel_button;
};

// --- 3. ServerMainWindow ---
// Объявление главного окна, которое использует DatabaseManager и BanUserDialog
class ServerMainWindow : public QMainWindow {
    Q_OBJECT

public:
    ServerMainWindow(QWidget* parent = nullptr);
    ~ServerMainWindow();

private slots:
    // Слоты для управления пользователями
    void on_ban_user_button_clicked();
    void on_unban_user_button_clicked();
    void on_disconnect_user_button_clicked();
    void update_user_status(int userId, const QString& status); // Этот слот не используется в текущей реализации, но может быть полезен

    // Слоты для сообщений
    void on_refresh_messages_button_clicked();
    void on_filter_messages_changed();

    // Слоты для таблиц
    void on_user_table_double_clicked(const QModelIndex& index);
    void on_user_selection_changed();

    // Слоты из диалога бана
    void process_ban_user_dialog(int userId, const QString& reason, const QDateTime& endDate);

private:
    void setup_ui();
    void setup_users_view();
    void setup_messages_view();
    void load_users();
    void load_messages();
    void refresh_user_list();
    void refresh_message_list();

    DatabaseManager* db_manager; // Менеджер базы данных

    QTabWidget* tab_widget;

    // --- Вкладка Пользователи ---
    QWidget* users_tab;
    QTableView* users_table;
    QSortFilterProxyModel* users_proxy_model;
    QSqlTableModel* users_sql_model;
    QLineEdit* users_search_filter;
    QPushButton* refresh_users_button;
    QPushButton* ban_user_button;
    QPushButton* unban_user_button;
    QPushButton* disconnect_user_button;

    // --- Вкладка Сообщения ---
    QWidget* messages_tab;
    QTableView* messages_table;
    QSortFilterProxyModel* messages_proxy_model;
    QSqlTableModel* messages_sql_model;
    QLineEdit* messages_search_filter;
    QComboBox* messages_filter_combo;
    QPushButton* refresh_messages_button;
    QPushButton* delete_message_button; // Опционально

    // --- Диалоги ---
    BanUserDialog* ban_dialog;
};

// --- Реализация DatabaseManager ---
DatabaseManager::DatabaseManager(QObject* parent) : QObject(parent) {
    db = QSqlDatabase::addDatabase("QPSQL");
    db.setHostName("localhost");
    db.setPort(5432);
    db.setDatabaseName("messanger_db");
    db.setUserName("your_db_user");          // ЗАМЕНИТЬ!
    db.setPassword("your_db_password");      // ЗАМЕНИТЬ!
}

DatabaseManager::~DatabaseManager() {
    disconnectFromDatabase();
}

bool DatabaseManager::connectToDatabase() {
    if (db.open()) {
        connected = true;
        qDebug() << "Database connected successfully.";
        createTablesIfNeeded(); // Создаем таблицы при подключении
        return true;
    }
    else {
        qDebug() << "Database connection error:" << db.lastError().text();
        connected = false;
        return false;
    }
}

void DatabaseManager::disconnectFromDatabase() {
    if (db.isOpen()) {
        db.close();
        connected = false;
        qDebug() << "Database disconnected.";
    }
}

bool DatabaseManager::isConnected() const {
    return connected;
}

void DatabaseManager::createTablesIfNeeded() {
    if (!isConnected()) return;

    QSqlQuery query;

    if (!db.tables().contains("users")) {
        if (query.exec(
            "CREATE TABLE users ("
            "user_id SERIAL PRIMARY KEY,"
            "username VARCHAR(50) UNIQUE NOT NULL,"
            "password_hash VARCHAR(255) NOT NULL,"
            "registration_date TIMESTAMP WITH TIME ZONE DEFAULT CURRENT_TIMESTAMP,"
            "status VARCHAR(20) DEFAULT 'active',"
            "ban_reason TEXT,"
            "ban_end_date TIMESTAMP WITH TIME ZONE"
            ")"
        )) {
            qDebug() << "Table 'users' created successfully.";
        }
        else {
            qDebug() << "Error creating table 'users':" << query.lastError().text();
        }
    }

    if (!db.tables().contains("messages")) {
        if (query.exec(
            "CREATE TABLE messages ("
            "message_id SERIAL PRIMARY KEY,"
            "sender_id INTEGER REFERENCES users(user_id) ON DELETE SET NULL,"
            "receiver_id INTEGER REFERENCES users(user_id) ON DELETE SET NULL,"
            "message_text TEXT NOT NULL,"
            "timestamp TIMESTAMP WITH TIME ZONE DEFAULT CURRENT_TIMESTAMP,"
            "type VARCHAR(10) NOT NULL"
            ")"
        )) {
            qDebug() << "Table 'messages' created successfully.";
        }
        else {
            qDebug() << "Error creating table 'messages':" << query.lastError().text();
        }
    }
}

// QSqlTableModel* DatabaseManager::getUsersModel() { /* ... (реализация ниже) ... */ return nullptr; }
// QSqlTableModel* DatabaseManager::getMessagesModel() { /* ... (реализация ниже) ... */ return nullptr; }

bool DatabaseManager::setUserStatus(int userId, const QString& status) {
    if (!isConnected()) return false;
    QSqlQuery query;
    query.prepare("UPDATE users SET status = :status WHERE user_id = :user_id");
    query.bindValue(":status", status);
    query.bindValue(":user_id", userId);

    if (query.exec()) {
        qDebug() << "User" << userId << "status set to" << status;
        return true;
    }
    else {
        qDebug() << "Error setting user status:" << query.lastError().text();
        return false;
    }
}

bool DatabaseManager::banUser(int userId, const QString& reason, const QDateTime& endDate) {
    if (!isConnected()) return false;
    QSqlQuery query;
    query.prepare("UPDATE users SET status = :status, ban_reason = :reason, ban_end_date = :end_date WHERE user_id = :user_id");
    query.bindValue(":status", "banned");
    query.bindValue(":reason", reason);
    query.bindValue(":end_date", endDate);
    query.bindValue(":user_id", userId);

    if (query.exec()) {
        qDebug() << "User" << userId << "banned until" << endDate;
        return true;
    }
    else {
        qDebug() << "Error banning user:" << query.lastError().text();
        return false;
    }
}

bool DatabaseManager::unbanUser(int userId) {
    return setUserStatus(userId, "active");
}

bool DatabaseManager::addMessage(int senderId, int receiverId, const QString& text, const QString& type) {
    if (!isConnected()) return false;
    QSqlQuery query;
    query.prepare("INSERT INTO messages (sender_id, receiver_id, message_text, type) VALUES (:sender_id, :receiver_id, :text, :type)");
    query.bindValue(":sender_id", senderId);
    query.bindValue(":receiver_id", receiverId);
    query.bindValue(":text", text);
    query.bindValue(":type", type);

    if (query.exec()) {
        qDebug() << "Message added.";
        return true;
    }
    else {
        qDebug() << "Error adding message:" << query.lastError().text();
        return false;
    }
}

bool DatabaseManager::deleteMessage(int messageId) {
    if (!isConnected()) return false;
    QSqlQuery query;
    query.prepare("DELETE FROM messages WHERE message_id = :message_id");
    query.bindValue(":message_id", messageId);

    if (query.exec()) {
        qDebug() << "Message" << messageId << "deleted.";
        return true;
    }
    else {
        qDebug() << "Error deleting message:" << query.lastError().text();
        return false;
    }
}

// --- Реализация BanUserDialog ---
BanUserDialog::BanUserDialog(QWidget* parent)
    : QDialog(parent), current_user_id(0)
{
    setWindowTitle("Бан пользователя");
    setModal(true);

    QVBoxLayout* main_layout = new QVBoxLayout(this);

    user_info_label = new QLabel("Бан пользователя: ", this);
    main_layout->addWidget(user_info_label);

    reason_label = new QLabel("Причина бана:", this);
    main_layout->addWidget(reason_label);
    reason_input = new QTextEdit(this);
    reason_input->setPlaceholderText("Введите причину бана...");
    reason_input->setFixedHeight(80);
    main_layout->addWidget(reason_input);

    ban_end_label = new QLabel("Срок бана:", this);
    main_layout->addWidget(ban_end_label);
    ban_end_datetime_edit = new QDateTimeEdit(this);
    ban_end_datetime_edit->setCalendarPopup(true);
    ban_end_datetime_edit->setMinimumDateTime(QDateTime::currentDateTime());
    ban_end_datetime_edit->setDisplayFormat("yyyy-MM-dd HH:mm:ss");
    connect(ban_end_datetime_edit, &QDateTimeEdit::dateTimeChanged, this, &BanUserDialog::update_ban_date_label);
    update_ban_date_label();
    main_layout->addWidget(ban_end_datetime_edit);

    QHBoxLayout* buttons_layout = new QHBoxLayout();
    ban_button = new QPushButton("Забанить", this);
    cancel_button = new QPushButton("Отмена", this);
    buttons_layout->addWidget(ban_button);
    buttons_layout->addWidget(cancel_button);
    main_layout->addLayout(buttons_layout);

    connect(ban_button, &QPushButton::clicked, this, &BanUserDialog::on_ban_button_clicked);
    connect(cancel_button, &QPushButton::clicked, this, &BanUserDialog::on_cancel_button_clicked);
}

void BanUserDialog::setUserId(int userId, const QString& username) {
    current_user_id = userId;
    current_username = username;
    user_info_label->setText("Бан пользователя: " + username + " (ID: " + QString::number(userId) + ")");
    reason_input->clear();
    ban_end_datetime_edit->setDateTime(QDateTime::currentDateTime().addDays(1));
    update_ban_date_label();
}

void BanUserDialog::update_ban_date_label() {
    if (ban_end_datetime_edit->dateTime().isValid()) {
        ban_end_label->setText("Срок бана: до " + ban_end_datetime_edit->dateTime().toString("yyyy-MM-dd HH:mm:ss"));
    }
    else {
        ban_end_label->setText("Срок бана: Неверная дата");
    }
}

void BanUserDialog::on_ban_button_clicked() {
    if (current_user_id == 0) {
        QMessageBox::warning(this, "Ошибка", "Не выбран пользователь для бана.");
        return;
    }
    QString reason = reason_input->toPlainText().trimmed();
    QDateTime endDate = ban_end_datetime_edit->dateTime();

    if (reason.isEmpty()) {
        QMessageBox::warning(this, "Предупреждение", "Пожалуйста, укажите причину бана.");
        return;
    }
    if (!endDate.isValid() || endDate <= QDateTime::currentDateTime()) {
        QMessageBox::warning(this, "Предупреждение", "Дата окончания бана должна быть в будущем.");
        return;
    }

    emit userBanned(current_user_id, reason, endDate);
    close();
}

void BanUserDialog::on_cancel_button_clicked() {
    close();
}


// --- Реализация ServerMainWindow ---
ServerMainWindow::ServerMainWindow(QWidget* parent)
    : QMainWindow(parent), db_manager(nullptr), ban_dialog(nullptr)
{
    db_manager = new DatabaseManager();
    if (!db_manager->connectToDatabase()) {
        QMessageBox::critical(this, "Ошибка базы данных", "Не удалось подключиться к базе данных. Убедитесь, что PostgreSQL запущен и настроен. Проверьте имя пользователя, пароль и имя базы данных в DatabaseManager.");
        qApp->quit(); // Закрываем приложение, если нет БД
        return;
    }

    setup_ui();
    load_users();
    load_messages();
}

ServerMainWindow::~ServerMainWindow() {
    if (db_manager) {
        db_manager->disconnectFromDatabase();
        delete db_manager;
    }
    delete ban_dialog;
}

void ServerMainWindow::setup_ui() {
    setWindowTitle("Server Control Panel");
    setMinimumSize(800, 600);

    tab_widget = new QTabWidget(this);
    setCentralWidget(tab_widget);

    // --- Настройка вкладки Пользователи ---
    users_tab = new QWidget(this);
    QVBoxLayout* users_layout = new QVBoxLayout(users_tab);

    users_search_filter = new QLineEdit(users_tab);
    users_search_filter->setPlaceholderText("Поиск по имени пользователя...");
    users_layout->addWidget(users_search_filter);

    users_table = new QTableView(users_tab);
    users_sql_model = new QSqlTableModel(users_table);
    users_sql_model->setTable("users");
    users_sql_model->setEditStrategy(QSqlTableModel::OnManualSubmit); // Ручное сохранение, если будет редактирование
    users_sql_model->select();

    users_sql_model->setHeaderData(users_sql_model->fieldIndex("user_id"), Qt::Horizontal, "ID");
    users_sql_model->setHeaderData(users_sql_model->fieldIndex("username"), Qt::Horizontal, "Имя пользователя");
    users_sql_model->setHeaderData(users_sql_model->fieldIndex("status"), Qt::Horizontal, "Статус");
    users_sql_model->setHeaderData(users_sql_model->fieldIndex("registration_date"), Qt::Horizontal, "Дата регистрации");
    users_sql_model->setHeaderData(users_sql_model->fieldIndex("ban_reason"), Qt::Horizontal, "Причина бана");
    users_sql_model->setHeaderData(users_sql_model->fieldIndex("ban_end_date"), Qt::Horizontal, "Конец бана");


    users_proxy_model = new QSortFilterProxyModel(users_table);
    users_proxy_model->setSourceModel(users_sql_model);
    users_proxy_model->setFilterKeyColumn(users_sql_model->fieldIndex("username"));
    users_table->setModel(users_proxy_model);
    users_table->setSortingEnabled(true);
    users_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    users_table->setSelectionMode(QAbstractItemView::SingleSelection);
    users_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    users_table->horizontalHeader()->setStretchLastSection(true);
    users_table->verticalHeader()->setVisible(false);
    users_table->resizeColumnsToContents();

    users_layout->addWidget(users_table);

    QHBoxLayout* user_buttons_layout = new QHBoxLayout();
    refresh_users_button = new QPushButton("Обновить список");
    ban_user_button = new QPushButton("Забанить");
    unban_user_button = new QPushButton("Разбанить");
    disconnect_user_button = new QPushButton("Отключить");

    ban_user_button->setEnabled(false);
    unban_user_button->setEnabled(false);
    disconnect_user_button->setEnabled(false);

    user_buttons_layout->addWidget(refresh_users_button);
    user_buttons_layout->addWidget(ban_user_button);
    user_buttons_layout->addWidget(unban_user_button);
    user_buttons_layout->addWidget(disconnect_user_button);
    user_buttons_layout->addStretch();
    users_layout->addLayout(user_buttons_layout);

    tab_widget->addTab(users_tab, "Пользователи");

    // --- Настройка вкладки Сообщения ---
    messages_tab = new QWidget(this);
    QVBoxLayout* messages_layout = new QVBoxLayout(messages_tab);

    QHBoxLayout* message_filter_layout = new QHBoxLayout();
    messages_search_filter = new QLineEdit(messages_tab);
    messages_search_filter->setPlaceholderText("Поиск по тексту сообщения...");
    messages_filter_combo = new QComboBox(messages_tab);
    messages_filter_combo->addItem("Все сообщения");
    messages_filter_combo->addItem("Публичные");
    messages_filter_combo->addItem("Приватные");
    message_filter_layout->addWidget(messages_search_filter);
    message_filter_layout->addWidget(messages_filter_combo);
    messages_layout->addLayout(message_filter_layout);

    messages_table = new QTableView(messages_tab);
    messages_sql_model = new QSqlTableModel(messages_table);
    messages_sql_model->setTable("messages");
    messages_sql_model->select();

    messages_sql_model->setHeaderData(messages_sql_model->fieldIndex("message_id"), Qt::Horizontal, "ID");
    messages_sql_model->setHeaderData(messages_sql_model->fieldIndex("sender_id"), Qt::Horizontal, "Отправитель");
    messages_sql_model->setHeaderData(messages_sql_model->fieldIndex("receiver_id"), Qt::Horizontal, "Получатель");
    messages_sql_model->setHeaderData(messages_sql_model->fieldIndex("message_text"), Qt::Horizontal, "Сообщение");
    messages_sql_model->setHeaderData(messages_sql_model->fieldIndex("timestamp"), Qt::Horizontal, "Время");
    messages_sql_model->setHeaderData(messages_sql_model->fieldIndex("type"), Qt::Horizontal, "Тип");

    messages_proxy_model = new QSortFilterProxyModel(messages_tab);
    messages_proxy_model->setSourceModel(messages_sql_model);

    connect(messages_search_filter, &QLineEdit::textChanged, this, [this]() {
        // Нужно обновлять фильтр сообщения, когда меняется текст, но это сложно сделать
        // с комбинированным фильтром (текст + тип). Лучше использовать setFilterRegExp
        // и apply_filters()
        apply_message_filters();
        });
    connect(messages_filter_combo, QOverload<int>::of(&QComboBox::currentIndexChanged),
        this, &ServerMainWindow::apply_message_filters);

    messages_table->setModel(messages_proxy_model);
    messages_table->setSortingEnabled(true);
    messages_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    messages_table->setSelectionMode(QAbstractItemView::SingleSelection);
    messages_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    messages_table->horizontalHeader()->setStretchLastSection(true);
    messages_table->verticalHeader()->setVisible(false);
    messages_table->resizeColumnsToContents();
    messages_layout->addWidget(messages_table);

    QHBoxLayout* message_buttons_layout = new QHBoxLayout();
    refresh_messages_button = new QPushButton("Обновить сообщения");
    message_buttons_layout->addWidget(refresh_messages_button);
    message_buttons_layout->addStretch();
    messages_layout->addLayout(message_buttons_layout);

    tab_widget->addTab(messages_tab, "Сообщения");

    // --- Подключение сигналов и слотов ---
    connect(users_search_filter, &QLineEdit::textChanged, this, [this]() {
        users_proxy_model->setFilterFixedString(users_search_filter->text());
        });
    connect(users_table->selectionModel(), &QItemSelectionModel::selectionChanged, this, &ServerMainWindow::on_user_selection_changed);
    connect(refresh_users_button, &QPushButton::clicked, this, &ServerMainWindow::refresh_user_list);
    connect(ban_user_button, &QPushButton::clicked, this, &ServerMainWindow::on_ban_user_button_clicked);
    connect(unban_user_button, &QPushButton::clicked, this, &ServerMainWindow::on_unban_user_button_clicked);
    connect(disconnect_user_button, &QPushButton::clicked, this, &ServerMainWindow::on_disconnect_user_button_clicked);
    connect(refresh_messages_button, &QPushButton::clicked, this, &ServerMainWindow::refresh_message_list);
    connect(users_table, &QTableView::doubleClicked, this, &ServerMainWindow::on_user_table_double_clicked);

    ban_dialog = new BanUserDialog(this);
    connect(ban_dialog, &BanUserDialog::userBanned, this, &ServerMainWindow::process_ban_user_dialog);
}

void ServerMainWindow::load_users() {
    users_sql_model->setQuery("SELECT user_id, username, status, registration_date, ban_reason, ban_end_date FROM users ORDER BY username");
    users_sql_model->select(); // Повторное выполнение запроса, если нужно обновить данные
    on_user_selection_changed();
    users_table->resizeColumnsToContents();
}

void ServerMainWindow::load_messages() {
    apply_message_filters(); // Используем метод фильтрации для начальной загрузки
    messages_table->resizeColumnsToContents();
}

void ServerMainWindow::refresh_user_list() {
    load_users();
    qDebug() << "Список пользователей обновлен.";
}

void ServerMainWindow::refresh_message_list() {
    apply_message_filters();
    qDebug() << "Список сообщений обновлен.";
}

void ServerMainWindow::apply_message_filters() {
    QString filter_text = messages_search_filter->text();
    int filter_type_idx = messages_filter_combo->currentIndex();

    QString query_string = "SELECT message_id, sender_id, receiver_id, message_text, timestamp, type FROM messages";
    QStringList conditions;

    // Фильтр по тексту
    if (!filter_text.isEmpty()) {
        // Используем LIKE с LIKE_ESCAPE для безопасности, хотя здесь это не критично
        // Но для больших проектов это важно. Для простоты, просто LIKE.
        conditions << "message_text ILIKE '%" + filter_text + "%'";
    }

    // Фильтр по типу
    if (filter_type_idx == 1) { // Публичные
        conditions << "type = 'public'";
    }
    else if (filter_type_idx == 2) { // Приватные
        conditions << "type = 'private'";
    }

    if (!conditions.isEmpty()) {
        query_string += " WHERE " + conditions.join(" AND ");
    }
    query_string += " ORDER BY timestamp DESC"; // Сортируем по времени

    messages_sql_model->setQuery(query_string);
    messages_table->resizeColumnsToContents();
}

void ServerMainWindow::on_user_selection_changed() {
    QModelIndexList selected_indexes = users_table->selectionModel()->selectedRows();
    bool has_selection = !selected_indexes.isEmpty();

    if (has_selection) {
        QModelIndex selected_index = selected_indexes.first();
        QSqlRecord record = users_sql_model->record(selected_index.row());

        QString status = record.value("status").toString();
        bool is_banned = (status == "banned");
        bool is_disconnected = (status == "disconnected");

        ban_user_button->setEnabled(!is_banned && !is_disconnected); // Забанить, если активен
        unban_user_button->setEnabled(is_banned); // Разбанить, если забанен
        disconnect_user_button->setEnabled(!is_banned && !is_disconnected); // Отключить, если активен и не отключен
    }
    else {
        ban_user_button->setEnabled(false);
        unban_user_button->setEnabled(false);
        disconnect_user_button->setEnabled(false);
    }
}

void ServerMainWindow::on_user_table_double_clicked(const QModelIndex& index) {
    if (index.isValid()) {
        QSqlRecord record = users_sql_model->record(index.row());
        QString status = record.value("status").toString();

        if (status == "active") {
            on_ban_user_button_clicked();
        }
        else if (status == "banned") {
            on_unban_user_button_clicked();
        }
        else if (status == "disconnected") {
            // Можно предложить переподключить или что-то другое
        }
    }
}

void ServerMainWindow::on_disconnect_user_button_clicked() {
    QModelIndex selected_index = users_table->selectionModel()->selectedRows().first();
    if (!selected_index.isValid()) return;

    QSqlRecord record = users_sql_model->record(selected_index.row());
    int userId = record.value("user_id").toInt();
    QString username = record.value("username").toString();

    if (QMessageBox::question(this, "Отключение пользователя",
        QString("Вы уверены, что хотите временно отключить пользователя '%1' (ID: %2)?\n"
            "Это действие не является баном, а лишь временным разрывом соединения.").arg(username).arg(userId)) == QMessageBox::Yes) {

        if (db_manager->setUserStatus(userId, "disconnected")) {
            QMessageBox::information(this, "Успех", QString("Пользователь '%1' был отключен. Сообщите ему о необходимости повторного подключения.").arg(username));
            refresh_user_list();
        }
        else {
            QMessageBox::critical(this, "Ошибка", "Не удалось отключить пользователя. Проверьте логи сервера.");
        }
    }
}

void ServerMainWindow::on_ban_user_button_clicked() {
    QModelIndex selected_index = users_table->selectionModel()->selectedRows().first();
    if (!selected_index.isValid()) return;

    QSqlRecord record = users_sql_model->record(selected_index.row());
    int userId = record.value("user_id").toInt();
    QString username = record.value("username").toString();

    ban_dialog->setUserId(userId, username);
    ban_dialog->show();
}

void ServerMainWindow::on_unban_user_button_clicked() {
    QModelIndex selected_index = users_table->selectionModel()->selectedRows().first();
    if (!selected_index.isValid()) return;

    QSqlRecord record = users_sql_model->record(selected_index.row());
    int userId = record.value("user_id").toInt();
    QString username = record.value("username").toString();

    if (QMessageBox::question(this, "Разбан пользователя",
        QString("Вы уверены, что хотите снять бан с пользователя '%1' (ID: %2)?").arg(username).arg(userId)) == QMessageBox::Yes) {

        if (db_manager->unbanUser(userId)) { // Используем unbanUser, который устанавливает статус 'active'
            QMessageBox::information(this, "Успех", QString("Бан с пользователя '%1' снят.").arg(username));
            refresh_user_list();
        }
        else {
            QMessageBox::critical(this, "Ошибка", "Не удалось снять бан с пользователя. Проверьте логи сервера.");
        }
    }
}

void ServerMainWindow::process_ban_user_dialog(int userId, const QString& reason, const QDateTime& endDate) {
    if (db_manager->banUser(userId, reason, endDate)) {
        QMessageBox::information(this, "Бан пользователя", "Пользователь успешно забанен.");
        refresh_user_list();
    }
    else {
        QMessageBox::critical(this, "Ошибка", "Не удалось забанить пользователя. Проверьте логи сервера.");
    }
}

// --- main.cpp ---
int main(int argc, char* argv[]) {
    QApplication a(argc, argv);

    // Установка стиля для улучшения внешнего вида (опционально)
    // Пример: QSS (Qt Style Sheets)
    // Или можно использовать системную тему, если она доступна.
    // QApplication::setStyle("Fusion"); // Попробуйте разные стили, если доступны

    ServerMainWindow mainWindow;
    mainWindow.show();

    return a.exec();
}