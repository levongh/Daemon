#pragma once
#include <map>

/**
 * @class  Daemon
 * @file   daemon.h
 * @brief  TODO add class description
 * @author Levon Ghukasyan
 */
class Daemon
{
public:
    //!@brief constructor
    Daemon(int argc, char** argv);

    //!@brief destructor
    ~Daemon();

    //!@brief function to handle SIGTERM signals
    static void sigtermActionHandler(int);

    //!@brief function to handle SIGTERM signals
    static void readConfigFile(int);

    void doAction();

    //!@brief set signal handlers
    void setupHandlers();

    //!@brief daemonize the object
    void daemonize();

    //!@brief function designed to run the daemon
    void run();

private:
    int m_pidFile;
    static std::string m_cat1;
    static std::string m_cat2;
    static std::map<std::string, std::string> s_confMap;
};

