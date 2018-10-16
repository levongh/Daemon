#include <signal.h>
#include <syslog.h>
#include <iostream>
#include <fstream>
#include <sys/types.h>
#include <unistd.h>
#include <sys/time.h>
#include <fcntl.h>
#include <cstdio>
#include <map>

bool g_exit = false;

class Daemon
{
public:
    Daemon(int argc, char** argv)
    {
        setlogmask(LOG_UPTO(LOG_INFO));
        openlog(argv[0], LOG_CONS | LOG_PERROR, LOG_USER);
        syslog(LOG_INFO, "Daemon is constructed");
    }

    ~Daemon()
    {
        syslog(LOG_INFO, "Programm is exiting");
        close(m_pidFile);
    }

    static void sigtermActionHandler(int)
    {
        syslog(LOG_INFO, "Daemonis is closeing");
        g_exit = true;
    }

    static void readConfigFile(int)
    {
        s_confMap["interval"] = "20"; // default value 20 second
        std::ifstream infile("configuration.conf");
        std::string line;
        while (std::getline(infile, line)) {
            auto pos = line.find('=');
            s_confMap[line.substr(0, pos)] = line.substr(pos + 1, line.size() - pos);
        }
    }

    void setupTimer()
    {
        //std::cout << std::stoi(s_confMap["interval"]) << std::endl;
        struct itimerval daemonTimer;
        daemonTimer.it_value.tv_sec     = std::stoi(s_confMap["interval"]);
        daemonTimer.it_value.tv_usec    = 0;
        daemonTimer.it_interval.tv_sec  = std::stoi(s_confMap["interval"]);
        daemonTimer.it_interval.tv_usec = 0;
        setitimer(ITIMER_REAL, &daemonTimer, nullptr);

        struct sigaction hupAction;
        hupAction.sa_handler = &Daemon::readConfigFile;
        hupAction.sa_flags = SA_RESTART;
        sigaction(SIGINT, &hupAction, nullptr);
        struct sigaction termAction;
        termAction.sa_handler = &Daemon::sigtermActionHandler;
        termAction.sa_flags = SA_RESTART;
        sigaction(SIGTERM, &termAction, nullptr);

        struct sigaction alarmAction;
        alarmAction.sa_handler = &Daemon::readConfigFile;
        alarmAction.sa_flags = SA_RESTART;
        sigaction(SIGALRM, &alarmAction, nullptr);
    }

    void runDaemon()
    {
        const char* pidfile = "proc.pid";
        int i;
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
        //umask(027);
        int sid = setsid();
        if (sid < 0) {
            std::exit(-1);
        }
        m_pidFile = open(pidfile, O_RDWR|O_CREAT, 0600);
        if (m_pidFile == -1) {
            syslog(LOG_INFO, "Could not open PID lock file %s, exiting", pidfile);
            std::exit(-1);
        }
        if (lockf(m_pidFile,F_TLOCK,0) == -1) {
            syslog(LOG_INFO, "Could not lock PID lock file %s, exiting", pidfile);
            std::exit(-1);
        }
        std::string str = std::to_string(getpid());
        write(m_pidFile, str.c_str(), str.size());
    }

    void setup()
    {
        readConfigFile(0);
        setupTimer();
        runDaemon();
        while (!g_exit) {
            sleep(10);
        }
    }

private:
    int m_pidFile;
    static std::map<std::string, std::string> s_confMap;
};

std::map<std::string, std::string> Daemon::s_confMap;

int main(int argc, char** argv)
{
    Daemon d(argc, argv);
    d.setup();
    return 0;
}
