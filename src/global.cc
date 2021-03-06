/*
 *   Larbin - is a web crawler
 *   Copyright (C) 2013  ictxiangxin
 *
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>
#include <fcntl.h>
#include <iostream>
#include <string.h>
#include <adns.h>
#include <netdb.h>
#include <sys/socket.h>
#include <string.h>
#include <signal.h>
#include <ctype.h>

#include "options.h"

#include "types.h"
#include "global.h"
#include "utils/text.h"
#include "utils/Fifo.h"
#include "utils/debug.h"
#include "fetch/site.h"
#include "interf/output.h"
#include "interf/input.h"


///////////////////////////////////////////////////////////
// Struct global
///////////////////////////////////////////////////////////

// define all the static variables
time_t      global::now;
hashTable   *global::seen;
#ifdef NO_DUP
hashDup *global::hDuplicate;
#endif // NO_DUP
SyncFifo<url>   *global::URLsPriority;
SyncFifo<url>   *global::URLsPriorityWait;
uint            global::readPriorityWait = 0;
PersistentFifo  *global::URLsDisk;
PersistentFifo  *global::URLsDiskWait;
uint            global::readWait=0;
IPSite          *global::IPSiteList;
NamedSite       *global::namedSiteList;
Fifo<IPSite>    *global::okSites;
Fifo<NamedSite> *global::dnsSites;
Connexion       *global::connexions;
adns_state      global::ads;
uint            global::nbDnsCalls = 0;
ConstantSizedFifo<Connexion> *global::freeConns;
#ifdef THREAD_OUTPUT
ConstantSizedFifo<Connexion> *global::userConns;
#endif
Interval        *global::inter;
int8_t          global::depthInSite;
bool            global::externalLinks = true;
bool            global::ignoreRobots = false;
uint            global::limitTime = 0;
uint            global::closeLevel = 0;
bool            global::searchOn = false;
bool            global::webServerOn = false;
bool            global::highLevelWebServer = false;
time_t          global::waitDuration;
char            *global::userAgent;
char            *global::sender;
char            *global::headers;
char            *global::headersRobots;
sockaddr_in     *global::proxyAddr;
Vector<char>    *global::domains;
Vector<char>    global::forbExt;
uint            global::nb_conn;
uint            global::dnsConn;
ushort          global::httpPort;
ushort          global::inputPort;
struct pollfd   *global::pollfds;
uint            global::posPoll;
uint            global::sizePoll;
short           *global::ansPoll;
uint            global::maxFds;
#ifdef MAXBANDWIDTH
long            global::remainBand = MAXBANDWIDTH;
#endif // MAXBANDWIDTH
pthread_t       global::limitTimeThread = 0;
#ifndef NOWEBSERVER
pthread_t       global::webServerThread = 0;
#endif // NOWEBSERVER
int             global::IPUrl = 0;

/** Constructor : initialize almost everything
 * Everything is read from the config file (larbin.conf by default)
 */
global::global (int argc, char *argv[])
{
    char *configFile = (char*)"larbin.conf";
#ifdef RELOAD
    bool reload = true;
#else
    bool reload = false;
#endif
    now = time(NULL);
    // verification of arguments
    int pos = 1;
    while (pos < argc)
    {
        if (!strcmp(argv[pos], "-c") && argc > pos + 1)
        {
            configFile = argv[pos + 1];
            pos += 2;
        }
        else if (!strcmp(argv[pos], "-scratch"))
        {
            reload = false;
            pos++;
        }
        else
            break;
    }
    if (pos != argc)
    {
        std::cerr << "usage : " << argv[0];
        std::cerr << " [-c configFile] [-scratch]\n";
        exit(1);
    }

    // Standard values
    waitDuration = 60;
    depthInSite = 5;
    userAgent = (char*)"larbin";
    sender = (char*)"larbin@unspecified.mail";
    nb_conn = 20;
    dnsConn = 3;
    httpPort = 0;
    inputPort = 0;  // by default, no input available
    proxyAddr = NULL;
    domains = NULL;
    // FIFOs
    URLsDisk = new PersistentFifo(reload, (char*)fifoFile);
    URLsDiskWait = new PersistentFifo(reload, (char*)fifoFileWait);
    URLsPriority = new SyncFifo<url>;
    URLsPriorityWait = new SyncFifo<url>;
    inter = new Interval(ramUrls);
    namedSiteList = new NamedSite[namedSiteListSize];
    IPSiteList = new IPSite[IPSiteListSize];
    okSites = new Fifo<IPSite>(2000);
    dnsSites = new Fifo<NamedSite>(2000);
    seen = new hashTable(!reload);
#ifdef NO_DUP
    hDuplicate = new hashDup(dupSize, dupFile, !reload);
#endif // NO_DUP
    // Read the configuration file
    crash("Read the configuration file");
    parseFile(configFile);
    // Initialize everything
    crash("Create global values");
    // Headers
    LarbinString strtmp;
    strtmp.addString((char*)"\r\nUser-Agent: ");
    strtmp.addString(userAgent);
    strtmp.addString((char*)" ");
    strtmp.addString(sender);
#ifdef SPECIFICSEARCH
    strtmp.addString((char*)"\r\nAccept: text/html");
    for (uint i = 0; contentTypes[i] != NULL; i++)
    {
        strtmp.addString((char*)", ");
        strtmp.addString(contentTypes[i]);
    }
#elif !defined(IMAGES) && !defined(ANYTYPE)
    strtmp.addString((char*)"\r\nAccept: text/html");
#endif // SPECIFICSEARCH
    strtmp.addString((char*)"\r\n\r\n");
    headers = strtmp.giveString();
    // Headers robots.txt
    strtmp.recycle();
    strtmp.addString((char*)"\r\nUser-Agent: ");
    strtmp.addString(userAgent);
    strtmp.addString((char*)" (");
    strtmp.addString(sender);
    strtmp.addString((char*)")\r\n\r\n");
    headersRobots = strtmp.giveString();
#ifdef THREAD_OUTPUT
    userConns = new ConstantSizedFifo<Connexion>(nb_conn);
#endif
    freeConns = new ConstantSizedFifo<Connexion>(nb_conn);
    connexions = new Connexion [nb_conn];
    for (uint i = 0; i < nb_conn; i++)
        freeConns->put(connexions + i);
    // init poll structures
    sizePoll = nb_conn + maxInput;
    pollfds = new struct pollfd[sizePoll];
    posPoll = 0;
    maxFds = sizePoll;
    ansPoll = new short[maxFds];
    // init non blocking dns calls
    adns_initflags flags =
        adns_initflags (adns_if_nosigpipe | adns_if_noerrprint);
    adns_init(&ads, flags, NULL);
    // call init functions of all modules
    initSpecific();
    initInput();
    initOutput();
    initSite();
    // let's ignore SIGPIPE
    static struct sigaction sn, so;
    sigemptyset(&sn.sa_mask);
    sn.sa_flags = SA_RESTART;
    sn.sa_handler = SIG_IGN;
    if (sigaction(SIGPIPE, &sn, &so))
        std::cerr << "Unable to disable SIGPIPE : " << strerror(errno) << std::endl;
}

/** If time out, this function will be invoked.
 */
global::~global ()
{
}

/** parse configuration file */
void global::parseFile (char *file)
{
    printf("Configure: %s\n", file);
    int fds = open(file, O_RDONLY);
    if (fds < 0)
    {
        std::cerr << "Cannot open config file (" << file << ") : " << strerror(errno) << std::endl;
        exit(1);
    }
    char *tmp = readfile(fds);
    close(fds);
    // suppress commentary
    bool eff = false;
    for (int i=0; tmp[i] != 0; i++)
        switch (tmp[i])
        {
            case '\n':
                eff = false;
                break;
            case '#':
                eff = true;
                // no break !!!
            default:
                if (eff)
                    tmp[i] = ' ';
        }
    char *posParse = tmp;
    char *tok = nextToken(&posParse);
    while (tok != NULL)
    {
        if (!strcasecmp(tok, "UserAgent"))
            userAgent = newString(nextToken(&posParse));
        else if (!strcasecmp(tok, "From"))
            sender = newString(nextToken(&posParse));
        else if (!strcasecmp(tok, "startUrl"))
        {
            tok = nextToken(&posParse);
            url *u = new url(tok, global::depthInSite, (url *) NULL);
            if (u->isValid())
            {
                check(u);
            }
            else
            {
                std::cerr << "The start url " << tok << " is invalid\n";
                exit(1);
            }
        }
        else if (!strcasecmp(tok, "waitduration"))
        {
            tok = nextToken(&posParse);
            waitDuration = atoi(tok);
        }
        else if (!strcasecmp(tok, "proxy"))
        {
            // host name and dns call
            tok = nextToken(&posParse);
            struct hostent* hp;
            proxyAddr = new sockaddr_in;
            memset((char *) proxyAddr, 0, sizeof (struct sockaddr_in));
            if ((hp = gethostbyname(tok)) == NULL)
            {
                endhostent();
                std::cerr << "Unable to find proxy ip address (" << tok << ")\n";
                exit(1);
            }
            else
            {
                proxyAddr->sin_family = hp->h_addrtype;
                memcpy ((char*) &proxyAddr->sin_addr, hp->h_addr, hp->h_length);
            }
            endhostent();
            // port number
            tok = nextToken(&posParse);
            proxyAddr->sin_port = htons(atoi(tok));
        }
        else if (!strcasecmp(tok, "pagesConnexions"))
        {
            tok = nextToken(&posParse);
            nb_conn = atoi(tok);
        }
        else if (!strcasecmp(tok, "dnsConnexions"))
        {
            tok = nextToken(&posParse);
            dnsConn = atoi(tok);
        }
        else if (!strcasecmp(tok, "httpPort"))
        {
            tok = nextToken(&posParse);
            httpPort = atoi(tok);
        }
        else if (!strcasecmp(tok, "inputPort"))
        {
            tok = nextToken(&posParse);
            inputPort = atoi(tok);
        }
        else if (!strcasecmp(tok, "depthInSite"))
        {
            tok = nextToken(&posParse);
            depthInSite = atoi(tok);
        }
        else if (!strcasecmp(tok, "limitToDomain"))
            manageDomain(&posParse);
        else if (!strcasecmp(tok, "forbiddenExtensions"))
            manageExt(&posParse);
        else if (!strcasecmp(tok, "noExternalLinks"))
            externalLinks = false;
        else if (!strcasecmp(tok, "ignoreRobots"))
            ignoreRobots = true;
        else if (!strcasecmp(tok, "highLevelWebServer"))
            highLevelWebServer = true;
        else if (!strcasecmp(tok, "limitTime"))
        {
            tok = nextToken(&posParse);
            limitTime = atoi(tok) * 60;
        }
        else
        {
            std::cerr << "bad configuration file : " << tok << "\n";
            exit(1);
        }
        tok = nextToken(&posParse);
    }
    delete [] tmp;
}

/** read the domain limit */
void global::manageDomain (char **posParse)
{
    char *tok = nextToken(posParse);
    if (domains == NULL)
        domains = new Vector<char>;
    while (tok != NULL && strcasecmp(tok, "end"))
    {
        domains->addElement(newString(tok));
        tok = nextToken(posParse);
    }
    if (tok == NULL)
    {
        std::cerr << "Bad configuration file : no end to limitToDomain\n";
        exit(1);
    }
}

/** read the forbidden extensions */
void global::manageExt (char **posParse)
{
    char *tok = nextToken(posParse);
    while (tok != NULL && strcasecmp(tok, "end"))
    {
        uint l = strlen(tok);
        for (uint i = 0; i < l; i++)
            tok[i] = tolower(tok[i]);
        if (!matchPrivExt(tok))
            forbExt.addElement(newString(tok));
        tok = nextToken(posParse);
    }
    if (tok == NULL)
    {
        std::cerr << "Bad configuration file : no end to forbiddenExtensions\n";
        exit(1);
    }
}

/** make sure the max fds has not been reached */
void global::verifMax (uint fd)
{
    if (fd >= maxFds)
    {
        uint n = 2 * maxFds;
        if (fd >= n)
            n = fd + maxFds;
        short *tmp = new short[n];
        for (uint i = 0; i < maxFds; i++)
            tmp[i] = ansPoll[i];
        for (uint i = maxFds; i < n; i++)
            tmp[i] = 0;
        delete (ansPoll);
        maxFds = n;
        ansPoll = tmp;
    }
}

///////////////////////////////////////////////////////////
// Struct Connexion
///////////////////////////////////////////////////////////

/** put Connection in a coherent state
 */
Connexion::Connexion ()
{
    state = emptyC;
    parser = NULL;
}

/** Destructor : never used : we recycle !!!
 */
Connexion::~Connexion ()
{
    assert(false);
}

/** Recycle a connexion
 */
void Connexion::recycle ()
{
    delete parser;
    request.recycle();
}
