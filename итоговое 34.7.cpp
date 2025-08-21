#include <type_traits> // Для std::convertible_to, std::same_as, std::has_virtual_destructor
#include <string>      // Для std::string
#include <iostream>    // Для вывода

// --- Концепт ComplexConcept<T> ---
template <typename T>
concept ComplexConcept =
// 1. Наличие метода hash(), возвращающего тип, конвертируемый в long
    requires(T t) {
        { t.hash() } -> std::convertible_to<long>; // Проверяем, что hash() существует и возвращает что-то, что можно сконвертировать в long
}
// 2. Наличие метода toString(), возвращающего std::string
&& requires(T t) {
    { t.toString() } -> std::same_as<std::string>; // Проверяем, что toString() существует и возвращает ТОЧНО std::string
}
// 3. Отсутствие у типа T виртуального деструктора
&& !std::has_virtual_destructor<T>::value; // Проверяем, что у типа T нет виртуального деструктора


// --- Вспомогательные типы для тестирования ---

// Тип, удовлетворяющий концепту
struct ValidType {
    long hash() const { return 12345L; }
    std::string toString() const { return "ValidType"; }
    // Нет виртуального деструктора
};

// Тип, у которого отсутствует метод hash()
struct NoHash {
    std::string toString() const { return "NoHash"; }
    // Нет hash()
};

// Тип, у которого hash() возвращает не long
struct WrongHashReturnType {
    int hash() const { return 10; } // Возвращает int, не long
    std::string toString() const { return "WrongHashReturnType"; }
};

// Тип, у которого toString() возвращает не std::string
struct WrongToStringReturnType {
    long hash() const { return 54321L; }
    const char* toString() const { return "WrongToStringReturnType"; } // Возвращает const char*
};

// Тип с виртуальным деструктором
struct HasVirtualDestructor {
    long hash() const { return 98765L; }
    std::string toString() const { return "HasVirtualDestructor"; }
    virtual ~HasVirtualDestructor() = default; // Виртуальный деструктор
};

// Тип, у которого все есть, но виртуальный деструктор
struct AllGoodButVirtualDestructor : public HasVirtualDestructor {
    // Наследует виртуальный деструктор
};


// --- Функции, использующие концепт ---

// Шаблонная функция, которая принимает только типы, удовлетворяющие ComplexConcept
template <ComplexConcept T>
void processComplexType(const T& obj) {
    std::cout << "Тип '" << obj.toString() << "' удовлетворяет ComplexConcept." << std::endl;
    std::cout << " hash: " << obj.hash() << std::endl;
    std::cout << " toString: " << obj.toString() << std::endl;
}

// Функция, которая будет использовать концепт для ограничения
template <typename T>
void checkComplexConcept() {
    if constexpr (ComplexConcept<T>) {
        std::cout << "Тип T является ComplexConcept." << std::endl;
        // Можно вызвать processComplexType, если нужно
        // T instance; // Для этого нужен конструктор по умолчанию или создать объект другим способом
        // processComplexType(instance);
    }
    else {
        std::cout << "Тип T НЕ является ComplexConcept." << std::endl;
    }
}


int main() {
    // Тестируем с различными типами

    std::cout << "--- Тестирование ComplexConcept ---" << std::endl;

    std::cout << "\nПроверка ValidType:" << std::endl;
    checkComplexConcept<ValidType>(); // Ожидаем "Тип T является ComplexConcept."
    processComplexType(ValidType{}); // Ожидаем вывод информации

    std::cout << "\nПроверка NoHash:" << std::endl;
    checkComplexConcept<NoHash>(); // Ожидаем "Тип T НЕ является ComplexConcept."

    std::cout << "\nПроверка WrongHashReturnType:" << std::endl;
    checkComplexConcept<WrongHashReturnType>(); // Ожидаем "Тип T НЕ является ComplexConcept."

    std::cout << "\nПроверка WrongToStringReturnType:" << std::endl;
    checkComplexConcept<WrongToStringReturnType>(); // Ожидаем "Тип T НЕ является ComplexConcept."

    std::cout << "\nПроверка HasVirtualDestructor:" << std::endl;
    checkComplexConcept<HasVirtualDestructor>(); // Ожидаем "Тип T НЕ является ComplexConcept."

    std::cout << "\nПроверка AllGoodButVirtualDestructor:" << std::endl;
    checkComplexConcept<AllGoodButVirtualDestructor>(); // Ожидаем "Тип T НЕ является ComplexConcept."

    // Пример использования концепта в шаблонной функции
    std::cout << "\n--- Тестирование processComplexType ---" << std::endl;
    processComplexType(ValidType{}); // Работает

    // processComplexType(NoHash{}); // Это вызвало бы ошибку компиляции, если бы не if constexpr
    // processComplexType(WrongHashReturnType{}); // То же самое

    return 0;
}