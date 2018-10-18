#include <signal.h>
#include <syslog.h>
#include <fstream>
#include <sys/types.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <cstdio>

#include "daemon.h"

bool g_exit = false;
std::string Daemon::m_cat1;
std::string Daemon::m_cat2;
std::map<std::string, std::string> Daemon::s_confMap;

Daemon::Daemon(int argc, char** argv)
{
    setlogmask(LOG_UPTO(LOG_INFO));
    openlog(argv[0], LOG_CONS | LOG_PERROR, LOG_USER);
    syslog(LOG_INFO, "Daemon is constructed");
}

Daemon::~Daemon()
{
    syslog(LOG_INFO, "Programm is exiting");
    close(m_pidFile);
    system("rm -rf proc.pid");
}

void Daemon::sigtermActionHandler(int)
{
    syslog(LOG_INFO, "Daemonis is closeing");
    g_exit = true;
}

void Daemon::readConfigFile(int)
{
    s_confMap["interval"] = "20"; // default value 20 second
    std::ifstream infile("configuration.conf");
    std::string line;
    while (std::getline(infile, line)) {
        auto pos = line.find('=');
        s_confMap[line.substr(0, pos)] = line.substr(pos + 1, line.size() - pos);
    }
    m_cat1 = s_confMap["catalogue1"];
    m_cat2 = s_confMap["catalogue2"];
}

void Daemon::doAction()
{
    std::string command = std::string("mv ") + m_cat1 + std::string("/* ") +
        m_cat2 + std::string(" > /dev/null 2>&1");
    m_cat1.swap(m_cat2);
    system(command.c_str());
}

void Daemon::setupTimer()
{
    struct sigaction hupAction;
    hupAction.sa_handler = &Daemon::readConfigFile;
    hupAction.sa_flags = SA_RESTART;
    sigaction(SIGHUP, &hupAction, nullptr);

    struct sigaction termAction;
    termAction.sa_handler = &Daemon::sigtermActionHandler;
    termAction.sa_flags = SA_RESTART;
    sigaction(SIGTERM, &termAction, nullptr);
}

void Daemon::setup()
{
    readConfigFile(0);
    setupTimer();
    run();
    auto interval = std::stoi(s_confMap["interval"]);
    while (!g_exit) {
        sleep(interval);
        doAction();
    }
}

void Daemon::run()
{
    const char* pidfile = "proc.pid";
    if (getppid() == 1) {
        return;
    }
    int pid = fork();
    if (pid < 0) {
        std::exit(-1);
    }
    if (pid > 0) {
        std::exit(0);
    }
    int sid = setsid();
    if (sid < 0) {
        std::exit(-1);
    }
    m_pidFile = open(pidfile, O_RDWR | O_CREAT, 0600);
    if (m_pidFile == -1) {
        syslog(LOG_INFO, "Could not open PID lock file %s, exiting", pidfile);
        std::exit(-1);
    }
    char buff[10] = {};
    if (lockf(m_pidFile, F_TLOCK, 0) == -1) {
        syslog(LOG_INFO, "Daemon is currently running swapping the process");
    }
    read(m_pidFile, buff, 10);
    struct stat sts;
    if (stat(std::string(std::string("/proc/") + std::string(buff)).c_str(), &sts) != -1) {
        system(std::string(std::string("kill -9 ") + std::string(buff) + std::string(" > /dev/null 2>&1")).c_str());
        lseek(m_pidFile, 0, SEEK_DATA);
    }
    std::string str = std::to_string(getpid());
    write(m_pidFile, str.c_str(), str.size());
}
