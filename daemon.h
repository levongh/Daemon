#pragma once
#include <map>

class Daemon
{
public:
    Daemon(int argc, char** argv);

    ~Daemon();

    static void sigtermActionHandler(int);

    static void readConfigFile(int);

    void doAction();

    void setupTimer();

    void run();

    void setup();

private:
    int m_pidFile;
    static std::string m_cat1;
    static std::string m_cat2;
    static std::map<std::string, std::string> s_confMap;
};

