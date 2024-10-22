#include <iostream>
#include <string>
#include <sstream>
#include <vector>
#include <unistd.h>   // для fork(), execvp()
#include <sys/wait.h> // для wait()
#include <csignal>    // для signal()
#include <cstdlib>    // для exit()

pid_t child_pid = -1; // Идентификатор дочернего процесса

void executeCommand(const char* command, char* const args[]) {
    pid_t pid = fork(); // Создаем процесс

    if (pid == 0) { // Дочерний процесс
        if (execvp(command, args) == -1) {
            perror("execvp failed");
        }
        exit(EXIT_FAILURE);
    } else if (pid > 0) { // Родительский процесс
        int status;
        waitpid(pid, &status, 0); // Ожидаем завершения дочернего процесса
    } else {
        perror("fork failed");
    }
}

std::vector<std::string> splitInput(const std::string& input) {
    std::istringstream stream(input);
    std::vector<std::string> tokens;
    std::string token;

    while (stream >> token) {
        tokens.push_back(token);
    }
    
    return tokens;
}

// Обработчик сигнала для завершения дочернего процесса
void signalHandler(int signum) {
    if (child_pid > 0) {
        std::cout << "\nTerminating process with PID: " << child_pid << std::endl;
        kill(child_pid, SIGTERM); // Завершаем дочерний процесс
        child_pid = -1; // Сбрасываем идентификатор процесса
    }
}

int main() {
    std::string input;

    // Настраиваем обработчик сигнала SIGINT (CTRL+C)
    std::signal(SIGINT, signalHandler);

    while (true) {
        std::cout << "Terminal> ";
        std::getline(std::cin, input); // Считываем ввод пользователя

        if (input == "exit") {
            break; // Выходим из программы, если команда "exit"
        }

        auto tokens = splitInput(input); // Разбиваем строку на токены
        if (tokens.empty()) {
            continue; // Игнорируем пустые строки
        }

        std::vector<char*> args;
        for (const auto& token : tokens) {
            args.push_back(const_cast<char*>(token.c_str()));
        }
        args.push_back(nullptr); // Завершаем массив аргументов

        const std::string& command = tokens[0]; // Первая часть — команда

        // Проверяем, поддерживается ли команда
        if (command == "ls") {
            executeCommand("ls", args.data());
        } else if (command == "cat") {
            executeCommand("cat", args.data());
        } else if (command == "nice") {
            executeCommand("nice", args.data());
        } else if (command == "killall") {
            executeCommand("killall", args.data());
        } else {
            executeCommand(command.c_str(), args.data());
            // std::cout << "Unknown command: " << command << std::endl;
        }
    }

    return 0;
}
