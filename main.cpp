#include <iostream>
#include <string>
#include <iomanip>
#include <fstream> // для работы с файлами
#include <sstream>
#include <vector>
#include <unistd.h>       // для fork(), execvp()
#include <sys/wait.h>     // для wait()
#include <csignal>        // для signal()
#include <cstdlib>        // для exit()
#include <stack>          // для хранения процессов (LIFO)
#include <queue>          // для хранения процессов (LIFO)
#include <dirent.h>       // Для работы с директорией (opendir, readdir)
#include <sys/stat.h>     // Для получения информации о файлах (stat)
#include <cstring>        // Для strcmp(
#include <sys/resource.h> // для setpriority()
#include <algorithm>      // Для std::all_of

// pid_t child_pid = -1; // Идентификатор дочернего процесса
std::queue<pid_t> processQueue; // Стек для хранения запущенных процессов

std::vector<std::string> splitInput(const std::string &input)
{
    std::istringstream stream(input);
    std::vector<std::string> tokens;
    std::string token;

    while (stream >> token)
    {
        tokens.push_back(token);
    }

    return tokens;
}

// Обработчик сигнала для завершения дочернего процесса
void signalHandler(int signum)
{
    if (!processQueue.empty()) {
        pid_t pid = processQueue.front();
        processQueue.pop();
        kill(pid, SIGTERM);  // Отправляем сигнал завершения процессу
        waitpid(pid, nullptr, 0);  // Ожидаем завершения процесса
        std::cout << "Process with PID " << pid << " terminated." << std::endl;
    } else {
        std::cout << "No more processes to terminate." << std::endl;
    }
}

#pragma region ls

// Функция для получения прав доступа к файлу в стиле "ls -l"
std::string getFilePermissions(struct stat &fileStat)
{
    std::string permissions;

    // Тип файла
    if (S_ISDIR(fileStat.st_mode))
    {
        permissions += 'd';
    }
    else if (S_ISREG(fileStat.st_mode))
    {
        permissions += '-';
    }
    else if (S_ISLNK(fileStat.st_mode))
    {
        permissions += 'l';
    }
    else
    {
        permissions += '?'; // Для других типов файлов
    }

    // Права доступа для пользователя
    permissions += (fileStat.st_mode & S_IRUSR) ? 'r' : '-';
    permissions += (fileStat.st_mode & S_IWUSR) ? 'w' : '-';
    permissions += (fileStat.st_mode & S_IXUSR) ? 'x' : '-';

    // Права доступа для группы
    permissions += (fileStat.st_mode & S_IRGRP) ? 'r' : '-';
    permissions += (fileStat.st_mode & S_IWGRP) ? 'w' : '-';
    permissions += (fileStat.st_mode & S_IXGRP) ? 'x' : '-';

    // Права доступа для остальных
    permissions += (fileStat.st_mode & S_IROTH) ? 'r' : '-';
    permissions += (fileStat.st_mode & S_IWOTH) ? 'w' : '-';
    permissions += (fileStat.st_mode & S_IXOTH) ? 'x' : '-';

    return permissions;
}
// Функция для реализации команды "ls -l"
void lsDetailed(const std::string &path)
{
    DIR *dir = opendir(path.c_str()); // Открываем директорию
    if (dir == nullptr)
    {
        perror("opendir failed");
        return;
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != nullptr)
    {
        std::string filePath = path + "/" + entry->d_name;
        // Пропускаем скрытые файлы (начинаются с '.')
        if (entry->d_name[0] == '.')
        {
            continue;
        }

        struct stat fileStat;
        if (stat(filePath.c_str(), &fileStat) == -1)
        {
            perror("stat failed");
            continue;
        }

        // Выводим права доступа, размер и имя файла
        std::string permissions = getFilePermissions(fileStat);
        std::cout << permissions << " "
                  << std::setw(5) << fileStat.st_size << " " // Размер файла
                  << entry->d_name << std::endl;
    }

    closedir(dir);
}
// Базовая реализация команды "ls"
void ls(const std::string &path = ".")
{
    DIR *dir = opendir(path.c_str()); // Открываем директорию
    if (dir == nullptr)
    {
        perror("opendir failed");
        return;
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != nullptr)
    {
        // Пропускаем скрытые файлы (начинаются с '.')
        if (entry->d_name[0] != '.')
        {
            std::cout << entry->d_name << "  ";
        }
    }

    std::cout << std::endl;
    closedir(dir);
}
#pragma endregion

#pragma region cat
// Функция для вывода содержимого файла
void cat(const std::vector<std::string> &files)
{
    for (const auto &file : files)
    {
        std::ifstream infile(file); // Открываем файл

        if (!infile.is_open())
        { // Проверяем, удалось ли открыть файл
            std::cerr << "Error: Cannot open file '" << file << "' - " << strerror(errno) << std::endl;
            continue;
        }

        std::string line;
        // Читаем файл построчно и выводим на экран
        while (std::getline(infile, line))
        {
            std::cout << line << std::endl;
        }

        infile.close(); // Закрываем файл
    }
}
#pragma endregion

#pragma region nice
// Функция, которая реализует функциональность команды "nice"
void nice(int priority, const std::string &command, char *const args[])
{
    // Устанавливаем приоритет процесса
    if (setpriority(PRIO_PROCESS, 0, priority) == -1)
    {
        std::cerr << "Error setting priority: " << strerror(errno) << std::endl;
        exit(EXIT_FAILURE);
    }

    // Запускаем указанную команду с новым приоритетом
    if (execvp(command.c_str(), args) == -1)
    {
        std::cerr << "Error executing command: " << strerror(errno) << std::endl;
        exit(EXIT_FAILURE);
    }
}
#pragma endregion

#pragma region killall
// Функция, которая отправляет сигнал всем процессам с определенным именем
void killall(const std::string &processName)
{
    DIR *procDir = opendir("/proc");
    if (procDir == nullptr)
    {
        std::cerr << "Error: Cannot open /proc directory - " << strerror(errno) << std::endl;
        return;
    }

    struct dirent *entry;
    while ((entry = readdir(procDir)) != nullptr)
    {
        // Проверяем, что имя директории — это число (PID процесса)
        if (entry->d_type == DT_DIR && std::all_of(entry->d_name, entry->d_name + strlen(entry->d_name), ::isdigit))
        {
            std::string pid = entry->d_name;
            std::string commPath = "/proc/" + pid + "/comm"; // Путь к файлу comm

            std::ifstream commFile(commPath);
            if (!commFile.is_open())
            {
                continue; // Если файл не удалось открыть, пропускаем
            }

            std::string process;
            std::getline(commFile, process); // Читаем имя процесса
            commFile.close();

            // Если имя процесса совпадает с искомым
            if (process == processName)
            {
                pid_t pidNum = std::stoi(pid); // Преобразуем PID в число
                if (kill(pidNum, SIGKILL) == 0)
                {
                    std::cout << "Killed process " << processName << " with PID: " << pidNum << std::endl;
                }
                else
                {
                    std::cerr << "Error: Failed to kill process " << pidNum << " - " << strerror(errno) << std::endl;
                }
            }
        }
    }

    closedir(procDir); // Закрываем директорию /proc
}
#pragma endregion

int main()
{
    std::string input;

    // Настраиваем обработчик сигнала SIGINT (CTRL+C)
    std::signal(SIGINT, signalHandler);

    while (true)
    {
        std::cout << "Terminal> ";
        std::getline(std::cin, input); // Считываем ввод пользователя

        if (input == "exit")
        {
            break; // Выходим из программы, если команда "exit"
        }

        auto tokens = splitInput(input); // Разбиваем строку на токены
        if (tokens.empty())
        {
            continue; // Игнорируем пустые строки
        }

        std::vector<char *> args;
        for (const auto &token : tokens)
        {
            args.push_back(const_cast<char *>(token.c_str()));
        }
        args.push_back(nullptr); // Завершаем массив аргументов

        const std::string &command = tokens[0]; // Первая часть — команда

        pid_t pid = fork(); // Создаем процесс

        if (pid == 0)
        {
            setpgid(0, 0);
            // Проверяем, поддерживается ли команда
            if (command == "ls")
            {
                if (tokens.size() > 1 && tokens[1] == "-l")
                {
                    lsDetailed(tokens[2]); // Вызываем ls -l
                }
                else if (tokens.size() > 1)
                {
                    ls(tokens[1]); // Вызываем ls с указанным путем
                }
                else
                {
                    ls(); // Вызываем базовый ls
                }
            }
            else if (command == "cat")
            {
                if (tokens.size() < 2)
                {
                    std::cerr << "Usage: cat <file1> [file2] ..." << std::endl;
                    continue;
                }

                std::vector<std::string> files(tokens.begin() + 1, tokens.end());
                cat(files); // Вызываем свою функцию myCat для вывода содержимого файлов
            }
            else if (command == "nice")
            {
                if (tokens.size() < 3)
                {
                    std::cerr << "Usage: nice <priority> <command> [args...]" << std::endl;
                    continue;
                }

                int priority = std::stoi(tokens[1]); // Получаем приоритет из аргументов
                std::vector<char *> args;

                // Собираем аргументы для команды, начиная с tokens[2]
                for (size_t i = 2; i < tokens.size(); ++i)
                {
                    args.push_back(const_cast<char *>(tokens[i].c_str()));
                }
                args.push_back(nullptr); // Завершаем массив аргументов

                // Вызываем свою функцию myNice с приоритетом и командой
                nice(priority, tokens[2], args.data());
            }
            else if (command == "killall")
            {
                if (tokens.size() < 2)
                {
                    std::cerr << "Usage: killall <process_name>" << std::endl;
                    continue;
                }

                const std::string &processName = tokens[1];
                killall(processName); // Вызываем свою функцию myKillall
            }
            else if (command != "exit")
            {
                if (execvp(command.c_str(), args.data()) == -1)
                {
                    perror("execvp failed");
                }
            }
        }
        else if (pid > 0)
        {
            processQueue.push(pid);
            std::cout << "Process started with pid: " << pid << std::endl;
            // waitpid(pid, nullptr, 0);
            // processQueue.pop();
        }
        else
        {
            perror("fork errored");
        }
    }

    // while (!processQueue.empty())
    // {
    //     pid_t pid = processQueue.top(); // Получаем последний запущенный процесс
    //     processQueue.pop();             // Убираем его из стека
    //     kill(pid, SIGTERM);       // Отправляем сигнал завершения процессу
    //     waitpid(pid, nullptr, 0); // Ждем завершения процесса
    // }

    return 0;
}
