#include <iostream>
#include <vector>
#include <thread>
#include <mutex>
#include <stdexcept> // Для исключений
#include <algorithm> // Для std::min
#include <cstdlib>   // Для rand()

// --- Структура узла ---
struct Node {
    int value;
    Node* next;
    std::mutex* node_mutex; // Мьютекс для данного узла

    // Конструктор узла
    Node(int val, Node* nxt = nullptr) : value(val), next(nxt) {
        node_mutex = new std::mutex(); // Создаем мьютекс для нового узла
    }

    // Деструктор узла
    ~Node() {
        delete node_mutex; // Удаляем мьютекс при уничтожении узла
    }
};

// --- Класс очереди с мелкогранулярной блокировкой ---
class FineGrainedQueue {
private:
    Node* head;
    std::mutex* queue_mutex; // Мьютекс для доступа к head, size и т.д.

public:
    // Конструктор
    FineGrainedQueue() {
        queue_mutex = new std::mutex(); // Мьютекс для управления хвостом/головой
        // Создаем фиктивный head-узел, чтобы упростить операции
        // Он не содержит полезных данных, только для удобства навигации.
        // У head нет следующего узла, а его мьютекс защищает сам head.
        head = new Node(-1); // Фиктивный узел
        head->next = nullptr;
        head->node_mutex = new std::mutex(); // Head тоже имеет свой мьютекс
    }

    // Деструктор
    ~FineGrainedQueue() {
        // Удаляем все узлы
        Node* current = head;
        while (current != nullptr) {
            Node* temp = current;
            current = current->next;
            delete temp; // Удаляем узел (и его мьютекс)
        }
        delete queue_mutex; // Удаляем мьютекс очереди
    }

    // Метод для добавления элемента в конец (для создания списка)
    void push_back(int value) {
        Node* new_node = new Node(value); // Новый узел со своим мьютексом

        // Блокируем queue_mutex для безопасного доступа к head
        std::lock_guard<std::mutex> q_lock(*queue_mutex);

        // Ищем последний узел
        Node* current = head;
        // Чтобы избежать дедлока при одновременном push_back,
        // нам нужно гарантировать порядок блокировки.
        // Если блокируем queue_mutex, то не нужно блокировать current->node_mutex
        // в цикле, пока мы не нашли последний узел.
        // Но для более сложных операций, где нам нужен и head, и последний узел,
        // порядок важен. Здесь достаточно queue_mutex.

        if (current->next == nullptr) { // Если список пуст (кроме фиктивного head)
            // Просто привязываем новый узел к head
            // В этом случае, new_node->next будет nullptr.
            // prev_node->next = new_node
            // new_node->next = nullptr
            // Чтобы это сделать безопасно:
            // 1. Заблокировать head->node_mutex
            // 2. Привязать new_node
            // 3. Сделать new_node->next = nullptr
            // 4. Удалить старый head->next (если он был)
            // 5. Установить head->next = new_node

            // Важно: если head->next == nullptr, то prev_node это head.
            // Блокируем мьютекс head
            std::lock_guard<std::mutex> head_lock(*head->node_mutex);
            new_node->next = head->next; // head->next пока nullptr
            head->next = new_node;
            // new_node->next уже nullptr (по конструктору)
        }
        else {
            // Список не пуст, находим последний узел
            Node* tail = current->next; // Первый реальный узел
            while (tail->next != nullptr) {
                tail = tail->next; // Идем к следующему
            }
            // tail теперь последний узел.
            // Нам нужно заблокировать его мьютекс, чтобы безопасно изменить next.
            std::lock_guard<std::mutex> tail_lock(*tail->node_mutex);
            new_node->next = tail->next; // tail->next = nullptr
            tail->next = new_node;
        }
    }

    // Метод для отображения списка (для отладки)
    void printList() {
        std::cout << "List: HEAD -> ";
        Node* current = head->next; // Начинаем с первого реального узла
        while (current != nullptr) {
            std::cout << current->value << " -> ";
            current = current->next;
        }
        std::cout << "nullptr" << std::endl;
    }

    // --- Реализация insertIntoMiddle ---
    void insertIntoMiddle(int value, int pos) {
        Node* new_node = new Node(value); // Создаем новый узел

        // Обработка случая, если pos больше длины списка
        // Для этого нужно сначала определить длину, но это может быть дорого.
        // Проще пройти до конца и, если pos не достигнут, вставить в конец.

        // Блокируем queue_mutex для безопасного доступа к head
        std::lock_guard<std::mutex> q_lock(*queue_mutex);

        Node* prev_node = head; // Начинаем с фиктивного head
        int current_pos = 0;

        // Ищем предыдущий узел.
        // Важно: при обходе списка, нам нужно блокировать текущий узел,
        // чтобы гарантировать, что его `next` не изменится, пока мы его читаем.
        // А затем, когда мы переходим к следующему, мы должны разблокировать предыдущий.
        // Это называется "скользящая блокировка" (lock coupling).

        // Для вставки, нам нужно заблокировать prev_node и new_node.
        // Порядок блокировки: prev_node, затем new_node.

        // Проходим до позиции или до конца списка
        while (prev_node->next != nullptr && current_pos < pos) {
            // Находим узел, который будет ПРЕДШЕСТВОВАТЬ вставляемому элементу
            // и который мы будем модифицировать (prev_node).
            // Нам нужно заблокировать prev_node, чтобы безопасно прочитать его next.

            // Получаем блокировку на текущий узел (prev_node).
            // Здесь нужно быть осторожным с тем, какой узел мы блокируем.
            // Мы хотим найти узел, который будет *перед* новой позицией.
            // Если pos = 0, prev_node = head.
            // Если pos = 1, prev_node = head->next.

            // Если мы вставляем на позицию `pos`, то `prev_node` будет узел с индексом `pos-1`.
            // После того как мы нашли `prev_node`, нам нужно заблокировать его.
            // И также заблокировать `new_node`.
            // Важно соблюдать порядок блокировки: prev_node, затем new_node.

            // Давайте пересмотрим. Нам нужен `prev_node` и `new_node`.
            // `prev_node` -> `new_node` -> `old_next`

            // 1. Находим `prev_node`.
            // 2. Создаем `new_node`.
            // 3. Блокируем `prev_node->node_mutex`.
            // 4. Блокируем `new_node->node_mutex`.
            // 5. Обновляем `prev_node->next = new_node`.
            // 6. Обновляем `new_node->next = old_next`.

            // Проблема: как получить `old_next` безопасно?
            // Мы можем прочитать `old_next` перед блокировкой `prev_node->node_mutex`,
            // но тогда `old_next` может измениться.
            // Правильнее:
            // 1. Найти `prev_node`.
            // 2. Создать `new_node`.
            // 3. Заблокировать `prev_node->node_mutex`.
            // 4. Заблокировать `new_node->node_mutex`.
            // 5. Прочитать `old_next = prev_node->next`.
            // 6. Привязать `new_node` между `prev_node` и `old_next`.
            //    `new_node->next = old_next`
            //    `prev_node->next = new_node`

            // Находим prev_node.
            // Если pos = 0, prev_node = head.
            // Если pos = 1, prev_node = head->next.

            // Переход к следующему узлу
            // Нам нужно безопасно перейти к prev_node->next.
            // Это означает, что мы должны заблокировать prev_node.

            // Если мы уже нашли prev_node, но еще не заблокировали его,
            // а хотим перейти к следующему, нам нужно сначала разблокировать
            // старый prev_node, а затем заблокировать новый prev_node.
            // Но это может создать окно для гонки.

            // Проходим до узла, который будет ПРЕДШЕСТВОВАТЬ новому узлу.
            // Если pos = 1, то prev_node должен быть head->next (первый реальный элемент).
            // Если pos = 2, то prev_node должен быть head->next->next (второй реальный элемент).
            // То есть, prev_node будет узлом с индексом pos - 1.

            // Но мы начинаем с head (индекс -1, если так считать).
            // Поэтому prev_node будет узлом с индексом `pos-1` относительно *реальных* элементов.
            // Или, если считать от head (индекс 0), то prev_node будет узлом с индексом `pos`.

            // Пусть prev_node - это узел, на который мы хотим вставить *после*.
            // Если pos=0 (не рассматриваем), prev_node = head.
            // Если pos=1, prev_node = head->next.
            // Если pos=2, prev_node = head->next->next.

            // Мы ищем `prev_node` такой, что `prev_node->next` будет новым узлом.
            // Идем до тех пор, пока `current_pos < pos`.

            // Перемещаем prev_node к следующему узлу
            prev_node = prev_node->next; // Переходим к следующему узлу
            current_pos++;
        }

        // Теперь `prev_node` указывает на узел, после которого мы должны вставить.
        // Если `pos` был больше длины, `prev_node` теперь последний узел.

        // --- Блокировка и вставка ---
        // 1. Заблокировать мьютекс предыдущего узла (`prev_node`).
        //    Мы уже держим queue_mutex, который защищает head.
        //    Порядок блокировки: queue_mutex, prev_node->node_mutex, new_node->node_mutex.
        //    Но queue_mutex нам больше не нужен после определения prev_node.
        //    Нам нужно разблокировать queue_mutex и получить lock на prev_node.
        //    Если prev_node = head, то мы заблокируем head->node_mutex.

        // Разблокируем queue_mutex (хотя lock_guard сделает это автоматически при выходе из области)
        // Но нам нужно держать блокировки на prev_node и new_node.
        // Порядок: prev_node->node_mutex, затем new_node->node_mutex.

        // Получаем указатель на следующий узел *до* блокировки prev_node
        Node* old_next = prev_node->next;

        // Блокируем мьютекс предыдущего узла
        std::lock_guard<std::mutex> prev_lock(*(prev_node->node_mutex));

        // Блокируем мьютекс нового узла
        std::lock_guard<std::mutex> new_lock(*(new_node->node_mutex));

        // Теперь, когда оба узла заблокированы, безопасно выполняем модификации

        // 1. Связываем новый узел с его следующим элементом
        new_node->next = old_next; // old_next может быть nullptr, если вставляем в конец

        // 2. Связываем предыдущий узел с новым узлом
        prev_node->next = new_node;

        // Блокировки prev_lock и new_lock автоматически снимутся при выходе из функции.
    }

    // --- Дополнительные вспомогательные методы (для тестирования) ---
    // Метод для получения длины списка (не потокобезопасный сам по себе,
    // но может быть использован в контексте, где блокировки уже взяты)
    int getSize() {
        int count = 0;
        Node* current = head->next;
        while (current != nullptr) {
            count++;
            current = current->next;
        }
        return count;
    }

    // Метод для получения узла по индексу (используется для тестирования,
    // требует блокировки на пути обхода)
    Node* getNodeAtIndex(int index) {
        if (index < 0) return nullptr;

        // Блокируем queue_mutex для доступа к head
        std::lock_guard<std::mutex> q_lock(*queue_mutex);

        Node* current = head; // Начинаем с фиктивного head
        int current_index = -1; // Фиктивный head можно считать индексом -1

        // Проходим по списку, блокируя узлы по мере продвижения
        // Это аналог скользящей блокировки (lock coupling)

        while (current != nullptr && current_index < index) {
            // Если это не head, блокируем его мьютекс
            if (current != head) {
                std::lock_guard<std::mutex> node_lock(*(current->node_mutex));
                // Теперь current заблокирован, безопасно смотрим на current->next
                if (current->next == nullptr && current_index < index) {
                    // Достигли конца списка раньше, чем нужный индекс
                    return nullptr;
                }
                current = current->next; // Переходим к следующему
            }
            else {
                // Для head, просто переходим к первому реальному узлу
                current = current->next;
            }
            current_index++;
        }

        // Если current == nullptr, значит, индекс был за пределами списка
        if (current == nullptr && current_index <= index) return nullptr;

        // current теперь указывает на узел с нужным индексом (или nullptr, если пропустили)
        return current;
    }
};


// --- Функция для тестирования ---
void testInsert(FineGrainedQueue& queue, int value, int pos, int expected_pos_val = -1) {
    std::cout << "Вставка: value=" << value << ", pos=" << pos << std::endl;
    queue.insertIntoMiddle(value, pos);
    // queue.printList(); // Можно раскомментировать для отладки

    // Проверка: найти вставленный элемент и посмотреть его значение
    // Для корректной проверки, нужно найти узел, который был *после* prev_node
    // до вставки, а затем посмотреть на `prev_node->next`.

    // Более простой способ: найти новый узел по его значению (если значения уникальны)
    // или по его позиции (что сложнее).

    // Если pos был в конце, то prev_node теперь последний, а `prev_node->next` должен быть новым узлом.

    // Попробуем найти узел по позиции, используя getNodeAtIndex (который должен быть потокобезопасным)
    // Если pos был больше длины, то вставляем в конец.
    // Если pos был, например, 3, мы ищем узел с индексом 3.

    // Определим, где должен быть новый узел
    int actual_insert_pos = pos;
    // Если pos был больше текущей длины, он будет в конце.
    // Получение длины не является потокобезопасной операцией без блокировок.
    // Мы предполагаем, что в момент вызова insertIntoMiddle, список не изменяется другими потоками
    // вне этого теста.

    // Вместо точного отслеживания позиции, проверим, что `value` присутствует в списке
    // и что узел до него (если он есть) корректно связан.

    // Получим узел, который должен быть *перед* вставленным узлом.
    // Это узел с индексом pos-1 (относительно реальных элементов).
    // Или, если считать от head, то узел с индексом pos.

    Node* prev_node_after_insert = queue.getNodeAtIndex(pos); // Это узел, который стал prev_node

    if (prev_node_after_insert && prev_node_after_insert->value == value) {
        std::cout << " Успех: Элемент " << value << " найден на позиции " << pos << std::endl;
    }
    else if (pos >= queue.getSize() - 1) { // Проверяем, если вставили в конец
        // Если pos был больше длины, новый элемент должен быть последним.
        // Ищем последний элемент.
        Node* last_node = queue.getNodeAtIndex(queue.getSize() - 1);
        if (last_node && last_node->value == value) {
            std::cout << " Успех: Элемент " << value << " вставлен в конец (ожидалось pos=" << pos << ")" << std::endl;
        }
        else {
            std::cerr << " Ошибка: Элемент " << value << " не найден в конце списка." << std::endl;
        }
    }
    else {
        std::cerr << " Ошибка: Элемент " << value << " не найден на ожидаемой позиции " << pos << std::endl;
    }
}


int main() {
    FineGrainedQueue queue;

    // Добавляем несколько элементов для формирования списка
    queue.push_back(10);
    queue.push_back(20);
    queue.push_back(30);
    queue.push_back(40);

    std::cout << "Исходный список: ";
    queue.printList(); // HEAD -> 10 -> 20 -> 30 -> 40 -> nullptr

    // Тестируем вставку

    // Вставляем 15 на позицию 1 (между 10 и 20)
    // prev_node будет 10 (индекс 0). new_node = 15. old_next = 20.
    // 10->next = 15. 15->next = 20.
    testInsert(queue, 15, 1);
    std::cout << "Список после вставки 15 на pos=1: ";
    queue.printList(); // HEAD -> 10 -> 15 -> 20 -> 30 -> 40 -> nullptr

    // Вставляем 25 на позицию 3 (между 20 и 30)
    // prev_node будет 20 (индекс 2). new_node = 25. old_next = 30.
    // 20->next = 25. 25->next = 30.
    testInsert(queue, 25, 3);
    std::cout << "Список после вставки 25 на pos=3: ";
    queue.printList(); // HEAD -> 10 -> 15 -> 20 -> 25 -> 30 -> 40 -> nullptr

    // Вставляем 50 в конец (pos больше длины)
    // Длина сейчас 6 (HEAD + 5 реальных элементов).
    // pos = 10. prev_node будет 40 (последний). new_node = 50. old_next = nullptr.
    // 40->next = 50. 50->next = nullptr.
    testInsert(queue, 50, 10);
    std::cout << "Список после вставки 50 на pos=10 (в конец): ";
    queue.printList(); // HEAD -> 10 -> 15 -> 20 -> 25 -> 30 -> 40 -> 50 -> nullptr

    // Тест с pos=0 (хотя по условию не требуется, но чтобы проверить логику)
    // Если pos = 0, prev_node = head. new_node = 5. old_next = 10.
    // head->next = 5. 5->next = 10.
    testInsert(queue, 5, 0);
    std::cout << "Список после вставки 5 на pos=0: ";
    queue.printList(); // HEAD -> 5 -> 10 -> 15 -> 20 -> 25 -> 30 -> 40 -> 50 -> nullptr

    return 0;
}