/*
 *
 * Copyright (C) u-blox Melbourn Ltd
 * u-blox Melbourn Ltd, Melbourn, UK
 *
 * All rights reserved.
 *
 * This source file is the sole property of u-blox Melbourn Ltd.
 * Reproduction or utilisation of this source in whole or part is
 * forbidden without the written consent of u-blox Melbourn Ltd.
 * hamed.siasi@u-blox.com
 *
 */



#include "mbed.h"
#include "MDM.h"
#ifdef TARGET_UBLOX_C027
 #include "C027_api.h"
#endif
#include "MDMAPN.h"
        
#define PROFILE         "0"   //!< this is the psd profile used
#define MAX_SIZE        128   //!< max expected messages
// num sockets
#define NUMSOCKETS      (sizeof(_sockets)/sizeof(*_sockets))
//! test if it is a socket is ok to use
#define ISSOCKET(s)     (((s) >= 0) && ((s) < NUMSOCKETS) && (_sockets[s].handle != SOCKET_ERROR))
//! check for timeout
#define TIMEOUT(t, ms)  ((ms != TIMEOUT_BLOCKING) && (ms < t.read_ms())) 
// num HTTP profiles
#define NUMPROFILES      (sizeof(_httpProfiles)/sizeof(*_httpProfiles))
//! test if it is an HTTP profile is ok to use
#define ISPROFILE(p)     (((p) >= 0) && ((p) < NUMPROFILES) && (_httpProfiles[p].handle != HTTP_PROF_ERROR))
//! registration ok check helper
#define REG_OK(r)       ((r == REG_HOME) || (r == REG_ROAMING)) 
//! registration done check helper (no need to poll further)
#define REG_DONE(r)     ((r == REG_HOME) || (r == REG_ROAMING) || (r == REG_DENIED)) 
//! helper to make sure that lock unlock pair is always balaced 
#define LOCK()         { lock() 
//! helper to make sure that lock unlock pair is always balaced 
#define UNLOCK()       } unlock()

#ifdef MDM_DEBUG
 #if 1 // colored terminal output using ANSI escape sequences
  #define COL(c) "\033[" c
 #else
  #define COL(c) 
 #endif
 #define DEF COL("39m")
 #define BLA COL("30m")
 #define RED COL("31m")
 #define GRE COL("32m")
 #define YEL COL("33m")
 #define BLU COL("34m")
 #define MAG COL("35m")
 #define CYA COL("36m")
 #define WHY COL("37m")
 
void dumpAtCmd(const char* buf, int len)
{
    ::printf(" %3d \"", len);
    while (len --) {
        char ch = *buf++;
        if ((ch > 0x1F) && (ch != 0x7F)) { // is printable
            if      (ch == '%')  ::printf("%%");
            else if (ch == '"')  ::printf("\\\"");
            else if (ch == '\\') ::printf("\\\\");
            else putchar(ch);
        } else {
            if      (ch == '\a') ::printf("\\a"); // BEL (0x07)
            else if (ch == '\b') ::printf("\\b"); // Backspace (0x08)
            else if (ch == '\t') ::printf("\\t"); // Horizontal Tab (0x09)
            else if (ch == '\n') ::printf("\\n"); // Linefeed (0x0A)
            else if (ch == '\v') ::printf("\\v"); // Vertical Tab (0x0B)
            else if (ch == '\f') ::printf("\\f"); // Formfeed (0x0C)
            else if (ch == '\r') ::printf("\\r"); // Carriage Return (0x0D)
            else                 ::printf("\\x%02x", (unsigned char)ch);
        }
    }
    ::printf("\"\r\n");
}
 
void MDMParser::_debugPrint(int level, const char* color, const char* format, ...)
{
    if (_debugLevel >= level) 
    {
        va_list args;
        va_start (args, format);
        if (color) ::printf(color);
        ::vprintf(format, args);
        if (color) ::printf(DEF);
        va_end (args);
    }
}
   
 #define ERROR(...)     _debugPrint(0, RED, __VA_ARGS__)
 #define INFO(...)      _debugPrint(1, GRE, __VA_ARGS__)
 #define TRACE(...)     _debugPrint(2, DEF, __VA_ARGS__)
 #define TEST(...)      _debugPrint(3, CYA, __VA_ARGS__)
 
#else
 
 #define ERROR(...) (void)0 // no tracing
 #define TEST(...)  (void)0 // no tracing
 #define INFO(...)  (void)0 // no tracing
 #define TRACE(...) (void)0 // no tracing

#endif

MDMParser* MDMParser::inst;

MDMParser::MDMParser(void)
{
    inst = this;
    memset(&_dev, 0, sizeof(_dev));
    memset(&_net, 0, sizeof(_net));
    _net.lac = 0xFFFF;
    _net.ci = 0xFFFFFFFF;
    _ip        = NOIP;
    _init      = false;
    memset(_sockets, 0, sizeof(_sockets));
    for (int socket = 0; socket < NUMSOCKETS; socket ++)
        _sockets[socket].handle = SOCKET_ERROR;
    memset(_httpProfiles, 0, sizeof(_httpProfiles));
    for (int profile = 0; profile < NUMPROFILES; profile ++)
        _httpProfiles[profile].handle = HTTP_PROF_ERROR;
#ifdef MDM_DEBUG
    _debugLevel = 1;
    _debugTime.start();
#endif
}

int MDMParser::send(const char* buf, int len)
{
#ifdef MDM_DEBUG
    if (_debugLevel >= 3) {
        ::printf("%10.3f AT send    ", _debugTime.read_ms()*0.001);
        dumpAtCmd(buf,len);
    }
#endif
    return _send(buf, len);
}

int MDMParser::sendFormated(const char* format, ...) {
    char buf[MAX_SIZE + 64];
    va_list args;
    va_start(args, format);
    int len = vsnprintf(buf,sizeof(buf), format, args);
    va_end(args);
    return send(buf, len);
}

int MDMParser::waitFinalResp(_CALLBACKPTR cb  /* = NULL*/,
                             void* param      /* = NULL*/,
                             int timeout_ms   /*= 5000*/)
{
    char buf[MAX_SIZE + 64 /* add some more space for framing */]; 
    Timer timer;
    timer.start();
    do {
        int ret = getLine(buf, sizeof(buf));
#ifdef MDM_DEBUG
        if ((_debugLevel >= 3) && (ret != WAIT) && (ret != NOT_FOUND))
        {
            int len = LENGTH(ret);
            int type = TYPE(ret);
            const char* s = (type == TYPE_UNKNOWN)? YEL "UNK" DEF : 
                            (type == TYPE_TEXT)   ? MAG "TXT" DEF : 
                            (type == TYPE_OK   )  ? GRE "OK " DEF : 
                            (type == TYPE_ERROR)  ? RED "ERR" DEF : 
                            (type == TYPE_PLUS)   ? CYA " + " DEF : 
                            (type == TYPE_PROMPT) ? BLU " > " DEF : 
                                                        "..." ;
            ::printf("%10.3f AT read %s", _debugTime.read_ms()*0.001, s);
            dumpAtCmd(buf, len);
        }
#endif        
        if ((ret != WAIT) && (ret != NOT_FOUND))
        {
            int type = TYPE(ret);
            // handle unsolicited commands here
            if (type == TYPE_PLUS)
            {
                const char* cmd = buf+3;
                int a, b, c, d, r;
                char s[32];


                // +CNMI: <mem>,<index>
                if (sscanf(cmd, "CMTI: \"%*[^\"]\",%d", &a) == 1)
                {
                    TRACE("New SMS at index %d\r\n", a);
                     // Socket Specific Command ---------------------------------
                     // +UUSORD: <socket>,<length>
                }

                else if ((sscanf(cmd, "NSONMI: %d,%d", &a, &b) == 2))
                {
                    int socket = _findSocket(a);
                    TRACE("Socket %d: handle %d has %d bytes pending\r\n", socket, a, b);
                    if (socket != SOCKET_ERROR)
                    {
                    	_sockets[socket].pending = b;
                    }
                }

                else if ((sscanf(cmd, "UUSORF: %d,%d", &a, &b) == 2))
                {
                    int socket = _findSocket(a);
                    TRACE("Socket %d: handle %d has %d bytes pending\r\n", socket, a, b);
                    if (socket != SOCKET_ERROR)
                        _sockets[socket].pending = b;
                // +UUSOCL: <socket>
                }

                else if ((sscanf(cmd, "UUSOCL: %d", &a) == 1))
                {
                    int socket = _findSocket(a);
                    TRACE("Socket %d: handle %d closed by remote host\r\n", socket, a);
                    if ((socket != SOCKET_ERROR) && _sockets[socket].connected)
                        _sockets[socket].connected = false;                                    
                    // +UULOC: <date>,<time>,<lat>,<long>,<alt>,<uncertainty>,<speed>, <direction>,<vertical_acc>,<sensor_used>,<SV_used>,<antenna_status>, <jamming_status>
                }

                else if (sscanf(cmd, "UULOC: %d/%d/%d,%d:%d:%d.%*d,%f,%f,%d,%d,%d,%d,%d,%d,%d,%*d,%*d",\
                        &_loc[0].time.tm_mday, &_loc[0].time.tm_mon, &_loc[0].time.tm_year, &_loc[0].time.tm_hour, &_loc[0].time.tm_min, &_loc[0].time.tm_sec,\
                        &_loc[0].latitude, &_loc[0].longitude, &_loc[0].altitutude, &_loc[0].uncertainty, &_loc[0].speed, &_loc[0].direction, &_loc[0].verticalAcc, \
                        &b, &_loc[0].svUsed) == 15)
                {
                    TRACE("Parsed UULOC position at index 0\r\n");                                        
                    _loc[0].sensor = (b==0)? CELL_LAST : (b==1)? CELL_GNSS : (b==2)? CELL_LOCATE : (b==3)? CELL_HYBRID : CELL_LAST;
                    _loc[0].time.tm_mon -= 1;
                    _loc[0].time.tm_wday=0;
                    _loc[0].time.tm_yday=0;
                    _loc[0].validData = true;
                    _locExpPos=1;
                    _locRcvPos++;
                     // +UULOC: <sol>,<num>,<sensor_used>,<date>,<time>,<lat>,<long>,<alt>,<uncertainty>,<speed>, <direction>,<vertical_acc>,,<SV_used>,<antenna_status>, <jamming_status>
               }

                else if (sscanf(cmd, "UULOC: %d,%d,%d,%d/%d/%d,%d:%d:%d.%*d,%f,%f,%d,%d,%d,%d,%d,%d,%*d,%*d",\
                        &a,&_locExpPos,&b, \
                        &_loc[CELL_MAX_HYP-1].time.tm_mday, &_loc[CELL_MAX_HYP-1].time.tm_mon, &_loc[CELL_MAX_HYP-1].time.tm_year, &_loc[CELL_MAX_HYP-1].time.tm_hour, &_loc[CELL_MAX_HYP-1].time.tm_min, &_loc[CELL_MAX_HYP-1].time.tm_sec,\
                        &_loc[CELL_MAX_HYP-1].latitude, &_loc[CELL_MAX_HYP-1].longitude, &_loc[CELL_MAX_HYP-1].altitutude, &_loc[CELL_MAX_HYP-1].uncertainty, &_loc[CELL_MAX_HYP-1].speed, &_loc[CELL_MAX_HYP-1].direction, &_loc[CELL_MAX_HYP-1].verticalAcc, \
                        &_loc[CELL_MAX_HYP-1].svUsed) == 17)
                {
                    if (--a>=0){                         
                        TRACE("Parsed UULOC position at index %d\r\n",a);                    
                        memcpy(&_loc[a], &_loc[CELL_MAX_HYP-1], sizeof(*_loc)); 
                        _loc[a].sensor = (b==0)? CELL_LAST : (b==1)? CELL_GNSS : (b==2)? CELL_LOCATE : (b==3)? CELL_HYBRID : CELL_LAST;                                       
                        _loc[a].time.tm_mon -= 1;
                        _loc[a].time.tm_wday=0;
                        _loc[a].time.tm_yday=0;
                        _loc[a].validData = true;                    
                        _locRcvPos++;                                            
                    }
                    //+UULOC: <sol>,<num>,<sensor_used>,<date>,<time>,<lat>,<long>,<alt>,<lat50>,<long50>,<major50>,<minor50>,<orientation50>,<confidence50>[,<lat95>,<long95>,<major95>,<minor95>,<orientation95>,<confidence95>]
               }

                else if (sscanf(cmd, "UULOC: %d,%d,%d,%d/%d/%d,%d:%d:%d.%*d,%f,%f,%d,%*f,%*f,%d,%*d,%*d,%*d",\
                        &a,&_locExpPos,&b, \
                        &_loc[CELL_MAX_HYP-1].time.tm_mday, &_loc[CELL_MAX_HYP-1].time.tm_mon, &_loc[CELL_MAX_HYP-1].time.tm_year, &_loc[CELL_MAX_HYP-1].time.tm_hour, &_loc[CELL_MAX_HYP-1].time.tm_min, &_loc[CELL_MAX_HYP-1].time.tm_sec,\
                        &_loc[CELL_MAX_HYP-1].latitude, &_loc[CELL_MAX_HYP-1].longitude, &_loc[CELL_MAX_HYP-1].altitutude, &_loc[CELL_MAX_HYP-1].uncertainty) == 13)
                {
                    if (--a>=0){    
                        TRACE("Parsed UULOC position at index %d\r\n",a);
                        memcpy(&_loc[a], &_loc[CELL_MAX_HYP-1], sizeof(*_loc));                                        
                        _loc[a].sensor = (b==0)? CELL_LAST : (b==1)? CELL_GNSS : (b==2)? CELL_LOCATE : (b==3)? CELL_HYBRID : CELL_LAST;
                        _loc[a].time.tm_mon -= 1;
                        _loc[a].time.tm_wday=0;
                        _loc[a].time.tm_yday=0;
                        _loc[a].validData = true;                    
                        _locRcvPos++;                    
                    }                              
                     // +UHTTPCR: <profile_id>,<op_code>,<param_val>
                }

                else if ((sscanf(cmd, "UUHTTPCR: %d,%d,%d", &a, &b, &c) == 3))
                {
                    _httpProfiles[a].cmd = b;          //command
                    _httpProfiles[a].result = c;       //result
                    TRACE("%s for profile %d: result code is %d\r\n", getHTTPcmd(b), a, c);
                }





                if (_dev.dev == DEV_LISA_C2)
                {
                    // CDMA Specific -------------------------------------------
                    // +CREG: <n><SID>,<NID>,<stat>
                    if (sscanf(cmd, "CREG: %*d,%d,%d,%d",&a,&b,&c) == 3) {
                        // _net.sid = a;
                        // _net.nid = b;
                        if      (c == 0) _net.csd = REG_NONE;     // not registered, home network
                        else if (c == 1) _net.csd = REG_HOME;     // registered, home network
                        else if (c == 2) _net.csd = REG_NONE;     // not registered, but MT is currently searching a new operator to register to
                        else if (c == 3) _net.csd = REG_DENIED;   // registration denied
                        else if (c == 5) _net.csd = REG_ROAMING;  // registered, roaming
                        _net.psd = _net.csd; // fake PSD registration (CDMA is always registered)
                        _net.act = ACT_CDMA;
                        // +CSS: <mode>[,<format>,<oper>[,<AcT>]]
                    } else if (sscanf(cmd, "CSS %*c,%2s,%*d",s) == 1) {
                        //_net.reg = (strcmp("Z", s) == 0) ? REG_UNKNOWN : REG_HOME;
                    }
                }
                else
                {
                    // GSM/UMTS Specific -------------------------------------------
                    // +UUPSDD: <profile_id> 
                    if (sscanf(cmd, "UUPSDD: %d",&a) == 1) {
                        if (*PROFILE == a) _ip = NOIP;
                    } else {
                        // +CREG|CGREG: <n>,<stat>[,<lac>,<ci>[,AcT[,<rac>]]] // reply to AT+CREG|AT+CGREG
                        // +CREG|CGREG: <stat>[,<lac>,<ci>[,AcT[,<rac>]]]     // URC
                        b = 0xFFFF; c = 0xFFFFFFFF; d = -1;
                        r = sscanf(cmd, "%s %*d,%d,\"%X\",\"%X\",%d",s,&a,&b,&c,&d);
                        if (r <= 1)
                            r = sscanf(cmd, "%s %d,\"%X\",\"%X\",%d",s,&a,&b,&c,&d);
                        if (r >= 2) {
                            Reg *reg = !strcmp(s, "CREG:")  ? &_net.csd : 
                                       !strcmp(s, "CGREG:") ? &_net.psd : 
                                       !strcmp(s, "CEREG:") ? &_net.eps : NULL;
                            if (reg) {
                                // network status
                                if      (a == 0) *reg = REG_NONE;     // 0: not registered, home network
                                else if (a == 1) *reg = REG_HOME;     // 1: registered, home network
                                else if (a == 2) *reg = REG_NONE;     // 2: not registered, but MT is currently searching a new operator to register to
                                else if (a == 3) *reg = REG_DENIED;   // 3: registration denied
                                else if (a == 4) *reg = REG_UNKNOWN;  // 4: unknown
                                else if (a == 5) *reg = REG_ROAMING;  // 5: registered, roaming
                                else if (a == 6) *reg = REG_HOME;     // 6: registered, sms only, home
                                if ((r >= 3) && (b != 0xFFFF))      _net.lac = b; // location area code
                                if ((r >= 4) && (c != 0xFFFFFFFF))  _net.ci  = c; // cell ID                                                                                                
                                // access technology
                                if (r >= 5) {
                                    if      (d == 0) _net.act = ACT_GSM;      // 0: GSM
                                    else if (d == 1) _net.act = ACT_GSM;      // 1: GSM COMPACT
                                    else if (d == 2) _net.act = ACT_UTRAN;    // 2: UTRAN
                                    else if (d == 3) _net.act = ACT_EDGE;     // 3: GSM with EDGE availability
                                    else if (d == 4) _net.act = ACT_UTRAN;    // 4: UTRAN with HSDPA availability
                                    else if (d == 5) _net.act = ACT_UTRAN;    // 5: UTRAN with HSUPA availability
                                    else if (d == 6) _net.act = ACT_UTRAN;    // 6: UTRAN with HSDPA and HSUPA availability
                                    else if (d == 7) _net.act = ACT_LTE;      // 7: LTE
                                }
                            }
                        }
                    }
                }
            }// type != TYPE_PLUS
            if (cb) {
                int len = LENGTH(ret);
                int ret = cb(type, buf, len, param);
                if (WAIT != ret)
                    return ret; 
            }
            if (type == TYPE_OK)
                return RESP_OK;
            if (type == TYPE_ERROR)
                return RESP_ERROR;
            if (type == TYPE_ERROR_CME)
                return RESP_ERROR_CME;                
            if (type == TYPE_PROMPT)    
                return RESP_PROMPT;
        }
        // relax a bit
        wait_ms(10); 
    }
    while (!TIMEOUT(timer, timeout_ms));
    return WAIT;
}

int MDMParser::_cbString(int type, const char* buf, int len, char* str)
{
    if (str && (type == TYPE_UNKNOWN)) {
        if (sscanf(buf, "\r\n%s\r\n", str) == 1)
            /*nothing*/;
    }
    return WAIT;
}

int MDMParser::_cbInt(int type, const char* buf, int len, int* val)
{
    if (val && (type == TYPE_UNKNOWN)) {
        if (sscanf(buf, "\r\n%d\r\n", val) == 1)
            /*nothing*/;
    }
    return WAIT;
}


// --------------------------------------------------------------------------------------------------------------------------------------------------------------
// init register
// --------------------------------------------------------------------------------------------------------------------------------------------------------------


bool MDMParser::connect(
            const char* simpin, 
            const char* apn, const char* username, 
            const char* password, Auth auth,
            PinName pn)
{
    bool ok = init(simpin, NULL, pn);  
#ifdef MDM_DEBUG
    if (_debugLevel >= 1) dumpDevStatus(&_dev);
#endif
    if (!ok)
        return false;
    ok = registerNet();
#ifdef MDM_DEBUG
    if (_debugLevel >= 1) dumpNetStatus(&_net);
#endif
    if (!ok)
        return false;
    IP ip = join(apn,username,password,auth);
#ifdef MDM_DEBUG
    if (_debugLevel >= 1) dumpIp(ip);
#endif
    if (ip == NOIP)
        return false;
    return true;
}

bool MDMParser::init(const char* simpin, DevStatus* status, PinName pn)
{
    int i = 10;
    LOCK();
    memset(&_dev, 0, sizeof(_dev));
    if (pn != NC) {
        INFO("Modem::wakeup ...\r\n");
        wait_ms(3000);

        DigitalOut pin(pn, 1);
        while (i--)
        {
            // SARA-U2/LISA-U2 50..80us
            pin = 0; ::wait_us(50);
            pin = 1; ::wait_ms(10); 
            
            // SARA-G35 >5ms, LISA-C2 > 150ms, LEON-G2 >5ms
            pin = 0; ::wait_ms(150);
            pin = 1; ::wait_ms(100);
            
            // purge any messages 
            purge();
            
            // check interface
            sendFormated("AT\r\n");
            int r = waitFinalResp(NULL,NULL,1000);
            if(RESP_OK == r) break;
        }
        if (i < 0) {
            ERROR("No Reply from Modem\r\n");
            goto failure;
        }
        waitFinalResp(NULL, NULL, 500);
    }
    _init = true;
    

    INFO("Modem::init\r\n");
    sendFormated("AT\r\n");
    waitFinalResp();
    wait_ms(1000);

    sendFormated("AT\r\n");
    waitFinalResp();
    wait_ms(1000);

    sendFormated("AT+NBAND?\r\n");
    waitFinalResp();
    wait_ms(2000);

    sendFormated("AT+CMEE=1\r\n");
    waitFinalResp();
    wait_ms(2000);

    INFO("Modem::Manufacturing revision \r\n");
    sendFormated("AT+CGMR\r\n");
    waitFinalResp();

    INFO("Modem::Radio functionality \r\n");
    sendFormated("AT+CFUN?\r\n");
    waitFinalResp();
    wait_ms(10000);

    INFO("Modem::RegisterF \r\n");
    sendFormated("AT+COPS=1,2,\"23591\"\r\n");     // NEWBURY
    //sendFormated("AT+COPS=1,2,\"46001\"\r\n");   // NEUL
    waitFinalResp();
    wait_ms(42000);

    INFO("Modem::Register status \r\n");
    sendFormated("AT+CEREG?\r\n");
    waitFinalResp();
    wait_ms(1000);


    bool ok = false;
    INFO("Modem:: address \r\n");
    sendFormated("AT+CGPADDR\r\n");
    waitFinalResp(_cbCGPAddr, &ok);
    if (!ok)
    {
    	INFO("Modem:: IP address ERROR !!!\r\n");
    	goto failure;
    }

    if (status)
        memcpy(status, &_dev, sizeof(DevStatus));
    UNLOCK();
    return true; 

failure:
    unlock();
    return false; 
}

bool MDMParser::powerOff(void)
{
    bool ok = false;
    if (_init) {
        LOCK();
        INFO("Modem::powerOff\r\n");
        sendFormated("AT+CPWROFF\r\n");
        if (RESP_OK == waitFinalResp(NULL,NULL,120*1000)) {
            _init = false;
            ok = true;
        }
        UNLOCK();
    }
    return ok;
}

int MDMParser::_cbATI(int type, const char* buf, int len, Dev* dev)
{
    if ((type == TYPE_UNKNOWN) && dev) {
        if      (strstr(buf, "SARA-G35"))   *dev = DEV_SARA_G35;
        else if (strstr(buf, "LISA-U200-03S")) *dev = DEV_LISA_U2_03S;
        else if (strstr(buf, "LISA-U2"))    *dev = DEV_LISA_U2;        
        else if (strstr(buf, "LISA-C2"))    *dev = DEV_LISA_C2;
        else if (strstr(buf, "SARA-U2"))    *dev = DEV_SARA_U2;
        else if (strstr(buf, "LEON-G2"))    *dev = DEV_LEON_G2;
        else if (strstr(buf, "TOBY-L2"))    *dev = DEV_TOBY_L2;
        else if (strstr(buf, "MPCI-L2"))    *dev = DEV_MPCI_L2;
    }
    return WAIT;
}

int MDMParser::_cbCPIN(int type, const char* buf, int len, Sim* sim)
{
    if (sim) {
        if (type == TYPE_PLUS){
            char s[16];
            if (sscanf(buf, "\r\n+CPIN: %[^\r]\r\n", s) >= 1)
                *sim = (0 == strcmp("READY", s)) ? SIM_READY : SIM_PIN;
        } else if (type == TYPE_ERROR) {
            if (strstr(buf, "+CME ERROR: SIM not inserted"))
                *sim = SIM_MISSING;
        }
    }
    return WAIT;
}

int MDMParser::_cbCCID(int type, const char* buf, int len, char* ccid)
{
    if ((type == TYPE_PLUS) && ccid){
        if (sscanf(buf, "\r\n+CCID: %[^\r]\r\n", ccid) == 1)
            /*TRACE("Got CCID: %s\r\n", ccid)*/;
    }
    return WAIT;
}

bool MDMParser::registerNet(NetStatus* status /*= NULL*/, int timeout_ms /*= 180000*/) 
{
    Timer timer;
    timer.start();
    INFO("Modem::register\r\n");
    while (!checkNetStatus(status) && !TIMEOUT(timer, timeout_ms))
        wait_ms(1000);
    if (_net.csd == REG_DENIED) ERROR("CSD Registration Denied\r\n");
    if (_net.psd == REG_DENIED) ERROR("PSD Registration Denied\r\n");
    if (_net.eps == REG_DENIED) ERROR("EPS Registration Denied\r\n");
    return REG_OK(_net.csd) || REG_OK(_net.psd) || REG_OK(_net.eps);
}

bool MDMParser::checkNetStatus(NetStatus* status /*= NULL*/)
{
  return true;
//	bool ok = false;
//	    LOCK();
//	    memset(&_net, 0, sizeof(_net));
//	    _net.lac = 0xFFFF;
//	    _net.ci = 0xFFFFFFFF;
//	    // check registration
//	    sendFormated("AT+CREG?\r\n");
//	    waitFinalResp();     // don't fail as service could be not subscribed
//	    if (_dev.dev != DEV_LISA_C2) {
//	        // check PSD registration
//	        sendFormated("AT+CGREG?\r\n");
//	        waitFinalResp(); // don't fail as service could be not subscribed
//	        if ((_dev.dev == DEV_TOBY_L2) ||  (_dev.dev == DEV_MPCI_L2)) {
//	            // check EPS network registration
//	            sendFormated("AT+CEREG?\r\n");
//	            waitFinalResp(); // don't fail as service could be not subscribed
//	        }
//	    }
//	    if (REG_OK(_net.csd) || REG_OK(_net.psd) || REG_OK(_net.eps))
//	    {
//	        // check modem specific status messages
//	        if (_dev.dev == DEV_LISA_C2) {
//	            sendFormated("AT+CSS?\r\n");
//	            if (RESP_OK != waitFinalResp())
//	                goto failure;
//	            while (1) {
//	                // get the Telephone number
//	                sendFormated("AT$MDN?\r\n");
//	                if (RESP_OK != waitFinalResp(_cbString, _net.num))
//	                    goto failure;
//	                // check if we have a Mobile Directory Number
//	                if (*_net.num && (memcmp(_net.num, "000000", 6) != 0))
//	                    break;
//
//	                INFO("Device not yet activated\r\n");
//	                INFO("Make sure you have a valid contract with the network operator for this device.\r\n");
//	                // Check if the the version contains a V for Verizon
//	                // Verizon: E0.V.xx.00.xxR,
//	                // Sprint E0.S.xx.00.xxR
//	                if (_dev.ver[3] == 'V') {
//	                    int i;
//	                    INFO("Start device over-the-air activation (this can take a few minutes)\r\n");
//	                    sendFormated("AT+CDV=*22899\r\n");
//	                    i = 1;
//	                    if ((RESP_OK != waitFinalResp(_cbUACTIND, &i, 120*1000)) || (i == 1)) {
//	                        ERROR("Device over-the-air activation failed\r\n");
//	                        goto failure;
//	                    }
//	                    INFO("Device over-the-air activation successful\r\n");
//
//	                    INFO("Start PRL over-the-air update (this can take a few minutes)\r\n");
//	                    sendFormated("AT+CDV=*22891\r\n");
//	                    i = 1;
//	                    if ((RESP_OK != waitFinalResp(_cbUACTIND, &i, 120*1000)) || (i == 1)) {
//	                        ERROR("PRL over-the-air update failed\r\n");
//	                        goto failure;
//	                    }
//	                    INFO("PRL over-the-air update successful\r\n");
//
//	                } else {
//	                    // Sprint or Aeris
//	                    INFO("Wait for OMA-DM over-the-air activation (this can take a few minutes)\r\n");
//	                    wait_ms(120*1000);
//	                }
//	            }
//	            // get the the Network access identifier string
//	            char nai[64];
//	            sendFormated("AT$QCMIPNAI?\r\n");
//	            if (RESP_OK != waitFinalResp(_cbString, nai))
//	                goto failure;
//	        } else {
//	            sendFormated("AT+COPS?\r\n");
//	            if (RESP_OK != waitFinalResp(_cbCOPS, &_net))
//	                goto failure;
//	            // get the MSISDNs related to this subscriber
//	            sendFormated("AT+CNUM\r\n");
//	            if (RESP_OK != waitFinalResp(_cbCNUM, _net.num))
//	                goto failure;
//	        }
//	        // get the signal strength indication
//	        sendFormated("AT+CSQ\r\n");
//	        if (RESP_OK != waitFinalResp(_cbCSQ, &_net))
//	            goto failure;
//	    }
//	    if (status) {
//	        memcpy(status, &_net, sizeof(NetStatus));
//	    }
//	    ok = REG_DONE(_net.csd) &&
//	        (REG_DONE(_net.psd) || REG_DONE(_net.eps));
//	    UNLOCK();
//	    return ok;
//
//	failure:
//	    unlock();
//	    return false;
}

int MDMParser::_cbCOPS(int type, const char* buf, int len, NetStatus* status)
{
    if ((type == TYPE_PLUS) && status){
        int act = 99;
        int mode = 99;
        // +COPS: <mode>[,<format>,<oper>[,<AcT>]]
       if (sscanf(buf, "\r\n+COPS: %d,%*d,\"%[^\"]\",%d",&mode,status->opr,&act) >= 1) {
            if      (act == 0) status->act = ACT_GSM;      // 0: GSM, 
            else if (act == 2) status->act = ACT_UTRAN;    // 2: UTRAN
            else if (act == 7) status->act = ACT_LTE;    // 2: UTRAN
            if (mode == 0)  status->regStatus = COPS_AUTOMATIC_REG;
            else if (mode == 1) status->regStatus = COPS_MANUAL_REG;
            else if (mode == 2) status->regStatus = COPS_DISABLED_REG;
        }
    }
    return WAIT;
}

int MDMParser::_cbCNUM(int type, const char* buf, int len, char* num)
{
    if ((type == TYPE_PLUS) && num){
        int a;
        if ((sscanf(buf, "\r\n+CNUM: \"My Number\",\"%31[^\"]\",%d", num, &a) == 2) && 
            ((a == 129) || (a == 145))) {
        }
    }
    return WAIT;
}
                    
int MDMParser::_cbCSQ(int type, const char* buf, int len, NetStatus* status)
{
    if ((type == TYPE_PLUS) && status){
        int a,b;
        char _ber[] = { 49, 43, 37, 25, 19, 13, 7, 0 }; // see 3GPP TS 45.008 [20] subclause 8.2.4
        // +CSQ: <rssi>,<qual>
        if (sscanf(buf, "\r\n+CSQ: %d,%d",&a,&b) == 2) {
            if (a != 99) status->rssi = -113 + 2*a;  // 0: -113 1: -111 ... 30: -53 dBm with 2 dBm steps, 31: >-51 dBm
            if ((b != 99) && (b < sizeof(_ber))) status->ber = _ber[b];  // 
        }
    }
    return WAIT;
}

int MDMParser::_cbUACTIND(int type, const char* buf, int len, int* i)
{
    if ((type == TYPE_PLUS) && i){
        int a;
        if (sscanf(buf, "\r\n+UACTIND: %d", &a) == 1) {
            *i = a;
        }
    }
    return WAIT;
}

// Neul L3 activity status
// Check if we have an IP address.
int MDMParser::_cbCGPAddr(int type, const char* buf, int len, bool* connected)
{
    int n;
    int status;
    int dummy, w, x, y, z;

    if (connected)
    {
        if (type == TYPE_PLUS)
        {
            // +CGPADDR: dummy,www.xxx.yyy.zzz
            if (sscanf(buf, "\r\n+CGPADDR:%d,%d.%d.%d.%d\r\n", &dummy, &w, &x, &y, &z) == 5)
            {
                *connected = true;
            }
            else
            {
                if (sscanf(buf, "\r\n+CGPADDR:%d\r\n", &dummy) == 1)
                {
                	*connected = false;
                }
            }
        }
    }
    return WAIT;
}


// -------------------------------------------------------------------------------------------------------------------------------------------------------------
// internet connection 
// -------------------------------------------------------------------------------------------------------------------------------------------------------------


bool MDMParser::_activateProfile(const char* apn, const char* username, const char* password, Auth auth)
{
    // Set up the APN
    if (*apn) {
        sendFormated("AT+UPSD=" PROFILE ",1,\"%s\"\r\n", apn);
        if (RESP_OK != waitFinalResp())
            return false;
    }
    if (*username) {
        sendFormated("AT+UPSD=" PROFILE ",2,\"%s\"\r\n", username);
        if (RESP_OK != waitFinalResp())
            return false;
    }
    if (*password) {
        sendFormated("AT+UPSD=" PROFILE ",3,\"%s\"\r\n", password);
        if (RESP_OK != waitFinalResp())
            return false;
    }
    // Set up the dynamic IP address assignment.
    sendFormated("AT+UPSD=" PROFILE ",7,\"0.0.0.0\"\r\n");
    if (RESP_OK != waitFinalResp())
        return false;
    // try different Authentication Protocols
    // 0 = none 
    // 1 = PAP (Password Authentication Protocol)
    // 2 = CHAP (Challenge Handshake Authentication Protocol)
    for (int i = AUTH_NONE; i <= AUTH_CHAP; i ++) {
        if ((auth == AUTH_DETECT) || (auth == i)) {
            // Set up the Authentication Protocol
            sendFormated("AT+UPSD=" PROFILE ",6,%d\r\n",i);
            if (RESP_OK != waitFinalResp())
                return false;
            // Activate the profile and make connection
            sendFormated("AT+UPSDA=" PROFILE ",3\r\n");
            if (RESP_OK == waitFinalResp(NULL,NULL,150*1000))
                return true;
        }
    }
    return false;
}

bool MDMParser::_activateProfileReuseExternal(void)
{
    int cid = -1;
    sendFormated("AT+CGDCONT?\r\n");
    if (RESP_OK != waitFinalResp(_cbCGDCONT, &cid))
        return false;
    if (cid == -1)
        return false;
    // we found a context that provides us a valid IP so lets reuse it for the internal IP stack
    sendFormated("AT+UPSD=" PROFILE ",100,%d\r\n", cid);
    if (RESP_OK != waitFinalResp())
        return false;
    // Activate the profile and make connection
    sendFormated("AT+UPSDA=" PROFILE ",3\r\n");
    return (RESP_OK == waitFinalResp(NULL,NULL,150*1000));
}

bool MDMParser::_activateProfileByCid(int cid, const char* apn, const char* username, const char* password, Auth auth)
{
    sendFormated("AT+CGDCONT=%d,\"IP\",\"%s\"\r\n", cid, apn);
    if (RESP_OK != waitFinalResp())
        return false;
    sendFormated("AT+UAUTHREQ=%d,%d,\"%s\",\"%s\"\r\n", cid, auth, username, password);
    if (RESP_OK != waitFinalResp())
        return false;
    sendFormated("AT+UPSD=" PROFILE ",100,%d\r\n", cid);
    if (RESP_OK != waitFinalResp())
        return false;
    // Activate the profile and make connection
    sendFormated("AT+UPSDA=" PROFILE ",3\r\n");
    return (RESP_OK == waitFinalResp(NULL,NULL,150*1000));
}
 
int MDMParser::_cbCGDCONT(int type, const char* buf, int len, int* cid)
{
    // accept with and without leading \r\n in +CGDCONT:
    if ((type == TYPE_PLUS) && (buf[0] == '\r') && (buf[1] == '\n') && (len >= 2))
        buf += 2, len -= 2, type = TYPE_UNKNOWN;
    if (type == TYPE_UNKNOWN) {
        int a,b,c,d,t;
        //+CGDCONT: <cid>,"IP","<apn name>","<ip adr>",0,0,0,0,0,0
        if (sscanf(buf, "+CGDCONT: %d,\"IP\",\"%*[^\"]\",\"" IPSTR "\",%*d,%*d,%*d,%*d,%*d,%*d", &t, &a,&b,&c,&d) == 5) {
            if (IPADR(a,b,c,d) != NOIP) 
                *cid = t;
        }
    }
    return WAIT;
}

MDMParser::IP MDMParser::join(const char* apn /*= NULL*/, const char* username /*= NULL*/, 
                              const char* password /*= NULL*/, Auth auth /*= AUTH_DETECT*/)
{
    LOCK();
    INFO("Modem::join\r\n");
    _ip = NOIP;
    if (_dev.dev == DEV_LISA_C2) {
        // make a dumy dns lookup (which will fail, so ignore the result) 
        sendFormated("AT+UDNSRN=0,\"u-blox.com\"\r\n");
        waitFinalResp(); 
        // This fake lookup will enable the IP connection and we 
        // should have an IP after this, so we check it
        
        //Get local IP address
        sendFormated("AT+CMIP?\r\n");
        if (RESP_OK != waitFinalResp(_cbCMIP, &_ip))
            goto failure;
    
    } else { 
        // check gprs attach status 
        sendFormated("AT+CGATT=1\r\n");
        if (RESP_OK != waitFinalResp(NULL,NULL,3*60*1000)) 
            goto failure;
        // Check the profile
        int a = 0;
        bool force = true;
        sendFormated("AT+UPSND=" PROFILE ",8\r\n");
        if (RESP_OK != waitFinalResp(_cbUPSND, &a))
            goto failure;
        if (a == 1 && force) {
            // disconnect the profile already if it is connected 
            sendFormated("AT+UPSDA=" PROFILE ",4\r\n");
            if (RESP_OK != waitFinalResp(NULL,NULL,40*1000))
                goto failure;
            a = 0;
        }
        if (a == 0) {
            bool ok = false;
            // try to lookup the apn settings from our local database by mccmnc
            const char* config = NULL;
            if (!apn && !username && !password)
                config = apnconfig(_dev.imsi);
            do {
                if (config) {
                    apn      = _APN_GET(config);
                    username = _APN_GET(config);
                    password = _APN_GET(config);
                }
                // convert pointer to empty strings
                apn      = apn      ? apn      : "";
                username = username ? username : "";
                password = password ? password : "";
                auth = (*username && *password) ? auth : AUTH_NONE;
                TRACE("Testing APN Settings(\"%s\",\"%s\",\"%s\",%d)\r\n", apn, username, password, auth);
                if ((_dev.dev != DEV_TOBY_L2) && (_dev.dev != DEV_MPCI_L2))
                    ok = _activateProfile(apn, username, password, auth);
                else {
                    ok = _activateProfileReuseExternal();
                    if (ok) 
                        TRACE("Reusing External Context\r\n");
                    else
                        ok = _activateProfileByCid(1, apn, username, password, auth);
                }
            } while (!ok && config && *config); // maybe use next setting ? 
            if (!ok) {
                ERROR("Your modem APN/password/username may be wrong\r\n");
                goto failure;
            }
        }
        //Get local IP address
        sendFormated("AT+UPSND=" PROFILE ",0\r\n");
        if (RESP_OK != waitFinalResp(_cbUPSND, &_ip))
            goto failure;
    }
    UNLOCK();
    return _ip;
failure: 
    unlock();
    return NOIP;
}

int MDMParser::_cbUDOPN(int type, const char* buf, int len, char* mccmnc)
{
    if ((type == TYPE_PLUS) && mccmnc) {
        if (sscanf(buf, "\r\n+UDOPN: 0,\"%[^\"]\"", mccmnc) == 1)
            ;
    }
    return WAIT;
}

int MDMParser::_cbCMIP(int type, const char* buf, int len, IP* ip)
{
    if ((type == TYPE_UNKNOWN) && ip) {
        int a,b,c,d;
        if (sscanf(buf, "\r\n" IPSTR, &a,&b,&c,&d) == 4)
            *ip = IPADR(a,b,c,d);
    }
    return WAIT;
}
        
int MDMParser::_cbUPSND(int type, const char* buf, int len, int* act)
{
    if ((type == TYPE_PLUS) && act) {
        if (sscanf(buf, "\r\n+UPSND: %*d,%*d,%d", act) == 1)
            /*nothing*/;
    }
    return WAIT;
}

int MDMParser::_cbUPSND(int type, const char* buf, int len, IP* ip)
{
    if ((type == TYPE_PLUS) && ip) {
        int a,b,c,d;
        // +UPSND=<profile_id>,<param_tag>[,<dynamic_param_val>]
        if (sscanf(buf, "\r\n+UPSND: " PROFILE ",0,\"" IPSTR "\"", &a,&b,&c,&d) == 4)
            *ip = IPADR(a,b,c,d);
    }
    return WAIT;
}

int MDMParser::_cbUDNSRN(int type, const char* buf, int len, IP* ip)
{
    if ((type == TYPE_PLUS) && ip) {
        int a,b,c,d;
        if (sscanf(buf, "\r\n+UDNSRN: \"" IPSTR "\"", &a,&b,&c,&d) == 4)
            *ip = IPADR(a,b,c,d);
    }
    return WAIT;
}

bool MDMParser::disconnect(void)
{
    bool ok = false;
    LOCK();
    INFO("Modem::disconnect\r\n");
    if (_ip != NOIP) {
        if (_dev.dev == DEV_LISA_C2) {
            // There something to do here
            _ip = NOIP;
            ok = true;
        } else { 
            sendFormated("AT+UPSDA=" PROFILE ",4\r\n");
            if (RESP_OK != waitFinalResp()) {
                _ip = NOIP;
                ok = true;
            }
        }
    }
    UNLOCK();
    return ok;
}

MDMParser::IP MDMParser::gethostbyname(const char* host)
{
    IP ip = NOIP; 
    int a,b,c,d;
    if (sscanf(host, IPSTR, &a,&b,&c,&d) == 4)
        ip = IPADR(a,b,c,d);
    else {
        LOCK();
        sendFormated("AT+UDNSRN=0,\"%s\"\r\n", host);
        if (RESP_OK != waitFinalResp(_cbUDNSRN, &ip))
            ip = NOIP;
        UNLOCK();
    }
    return ip;
}



// -------------------------------------------------------------------------------------------------------------------------------------------------------------
// sockets
// -------------------------------------------------------------------------------------------------------------------------------------------------------------


int MDMParser::_cbUSOCR(int type, const char* buf, int len, int* handle)
{
	if (sscanf(buf,"\r\n %d", handle) == 1){
	}else{
		*handle = 0;
	}
    return WAIT;
}

int MDMParser::socketSocket(IpProtocol ipproto, int port)
{
	TRACE("socketSocket\r\n");
    int socket=0;
    wait_ms(5000);
    LOCK();

    sendFormated("AT+NSOCR=DGRAM,17,%d\r\n", port);

    int handle = SOCKET_ERROR;
    if ((RESP_OK == waitFinalResp(_cbUSOCR, &handle)) && (handle != SOCKET_ERROR))
    {
    	TRACE("Socket %d: handle %d was created\r\n", socket, handle);
    	_sockets[socket].handle     = handle;
    	_sockets[socket].timeout_ms = TIMEOUT_BLOCKING;
    	_sockets[socket].connected  = false;
    	_sockets[socket].pending    = 0;
    }
    else
    {
    	 TRACE("socketSocket ERROR !!! \r\n");
    	 socket = SOCKET_ERROR;
     }
    UNLOCK();
    return socket;
}

bool MDMParser::socketConnect(int socket, const char * host, int port)
{
    IP ip = gethostbyname(host);
    if (ip == NOIP)
        return false;
    // connect to socket
    bool ok = false; 
    LOCK();
    if (ISSOCKET(socket) && (!_sockets[socket].connected)) {
        TRACE("socketConnect(%d,%s,%d)\r\n", socket,host,port);


        //CONNECT SOCKET
        //sendFormated("AT+USOCO=%d,\"" IPSTR "\",%d\r\n", _sockets[socket].handle, IPNUM(ip), port);
        sendFormated("AT+NSOCO=%d,\"" IPSTR "\",%d\r\n", _sockets[socket].handle, IPNUM(ip), port);


        if (RESP_OK == waitFinalResp())
            ok = _sockets[socket].connected = true;
    }
    UNLOCK();
    return ok;
}

bool MDMParser::socketIsConnected(int socket)
{
    bool ok = false;
    LOCK();
    ok = ISSOCKET(socket) && _sockets[socket].connected;
    TRACE("socketIsConnected(%d) %s\r\n", socket, ok?"yes":"no");
    UNLOCK();
    return ok;
}

bool MDMParser::socketSetBlocking(int socket, int timeout_ms)
{
    bool ok = false;
    LOCK();
    TRACE("socketSetBlocking(%d,%d)\r\n", socket,timeout_ms);
    if (ISSOCKET(socket)) {
        _sockets[socket].timeout_ms = timeout_ms;
        ok = true;
    }
    UNLOCK();
    return ok;
}

bool  MDMParser::socketClose(int socket)
{
    bool ok = false;
    LOCK();
    if (ISSOCKET(socket) && _sockets[socket].connected) {
        TRACE("socketClose(%d)\r\n", socket);


        //CLOSE SOCKET
        //sendFormated("AT+USOCL=%d\r\n", _sockets[socket].handle);
        sendFormated("AT+NSOCL=%d\r\n", _sockets[socket].handle);


        if (RESP_OK == waitFinalResp()) {
            _sockets[socket].connected = false;
            ok = true;
        }
    }
    UNLOCK();
    return ok;
}

bool  MDMParser::socketFree(int socket)
{
    // make sure it is closed
    socketClose(socket);
    bool ok = true;
    LOCK();
    if (ISSOCKET(socket)) {
        TRACE("socketFree(%d)\r\n",  socket);
        _sockets[socket].handle     = SOCKET_ERROR;
        _sockets[socket].timeout_ms = TIMEOUT_BLOCKING;
        _sockets[socket].connected  = false;
        _sockets[socket].pending    = 0;
        ok = true;
    }
    UNLOCK();
    return ok;
}

#define USO_MAX_WRITE 1024 //!< maximum number of bytes to write to socket

int MDMParser::socketSend(int socket, const char * buf, int len)
{
    TRACE("socketSend(%d,,%d)\r\n", socket,len);
    int cnt = len;
    while (cnt > 0) {
        int blk = USO_MAX_WRITE;
        if (cnt < blk) 
            blk = cnt;
        bool ok = false;
        LOCK();
        if (ISSOCKET(socket)) {
            sendFormated("AT+USOWR=%d,%d\r\n",_sockets[socket].handle,blk);
            if (RESP_PROMPT == waitFinalResp()) {
                wait_ms(50);
                //send(buf, blk);
                if (RESP_OK == waitFinalResp()) {
                	wait_ms(50);
                	waitFinalResp();
                	ok = true;
                }
            }
        }
        UNLOCK();
        if (!ok) 
            return SOCKET_ERROR;
        buf += blk;
        cnt -= blk;
    }
    return (len - cnt);
}

void MDMParser::tohex(unsigned char * in, size_t insz, char * pout, size_t outsz)
{
    unsigned char * pin = in;
    const char * hex = "0123456789ABCDEF";

    if (outsz >= (insz * 2) + 1) {
		for(; pin < in+insz; pin++){
			*pout = hex[(*pin>>4) & 0xF];
			pout++;
			*pout = hex[ *pin     & 0xF];
			pout++;
		}
    }
	*pout = 0;
}

void MDMParser::hextostr(unsigned char * in, size_t insz, char * output, size_t outsz)
{
    const char* const hex = "0123456789ABCDEF";
    size_t len = 2*insz;
    int k=0;

    for (size_t i = 0; i < len; i += 2)
    {
        char a,b;
        a = in[i];
        const char* p = std::lower_bound(hex, hex + 16, a);
        //if (*p != a) return NULL;
        b = in[i + 1];
        const char* q = std::lower_bound(hex, hex + 16, b);
        //if (*q != b) return NULL;
        output[k++] = ((p - hex) << 4) | (q - hex);
    }
    //output[k] = '\0';
}

int MDMParser::socketSendTo(int socket, IP ip, int port, const char * buf, int len)
{
    int cnt = len;
    while (cnt > 0)
    {
        int blk = USO_MAX_WRITE;
        if (cnt < blk)
        {
            blk = cnt;
        }
        bool ok = false;
        LOCK();
        TRACE("<---- (%d) \"%*s\"\r\n", len, len, buf);

        char *str = (char*)malloc(3*len);
        tohex( (unsigned char *)buf, (size_t)len,  (char *)str,  3*len);

        sendFormated("AT+NSOST=0,195.46.10.19,%d,%d,%s\r\n", port,blk,str);      // Newbury OpenLAB
        //sendFormated("AT+NSOST=0,120.16.45.6,%d,%d,%s\r\n", port,blk,str);       // Neul ecco server

        wait_ms(50);
        waitFinalResp();
        wait_ms(50);
        waitFinalResp();
        ok = true;

        UNLOCK();
        if (!ok){
            return SOCKET_ERROR;
        }
        buf += blk;
        cnt -= blk;
    }
    return (len - cnt);
}

int MDMParser::socketReadable(int socket)
{
    int pending = SOCKET_ERROR;
    LOCK();
    if (ISSOCKET(socket) && _sockets[socket].connected) {
        TRACE("socketReadable(%d)\r\n", socket);
        // allow to receive unsolicited commands 
        waitFinalResp(NULL, NULL, 0);
        if (_sockets[socket].connected)
           pending = _sockets[socket].pending; 
    }
    UNLOCK();
    return pending;
}

int MDMParser::_cbUSORD(int type, const char* buf, int len, char* out)
{
	//HOW MUCH DATA I HAVE TO READ
    if ((type == TYPE_PLUS) && out) {
        int sz, sk;
        if ((sscanf(buf, "\r\n+NSONMI:%d,%d,", &sk, &sz) == 2) &&
            (buf[len-sz-2] == '\"') && (buf[len-1] == '\"')) {
            memcpy(out, &buf[len-1-sz], sz);
        }
    }
    return WAIT;
}

int MDMParser::socketRecv(int socket, char* buf, int len)
{
    int cnt = 0;
    TRACE("socketRecv(%d,,%d)\r\n", socket, len);
#ifdef MDM_DEBUG
    memset(buf, '\0', len);
#endif
    Timer timer;
    timer.start();
    while (len) {
        int blk = MAX_SIZE; // still need space for headers and unsolicited  commands 
        if (len < blk) blk = len;
        bool ok = false;        
        LOCK();
        if (ISSOCKET(socket)) {
            if (_sockets[socket].connected) {
                if (_sockets[socket].pending < blk)
                    blk = _sockets[socket].pending;
                if (blk > 0) {
                    sendFormated("AT+USORD=%d,%d\r\n",_sockets[socket].handle, blk);
                    if (RESP_OK == waitFinalResp(_cbUSORD, buf)) {
                        _sockets[socket].pending -= blk;
                        len -= blk;
                        cnt += blk;
                        buf += blk;
                        ok = true;
                    }
                } else if (!TIMEOUT(timer, _sockets[socket].timeout_ms)) {
                    ok = (WAIT == waitFinalResp(NULL,NULL,0)); // wait for URCs
                } else {
                    len = 0;
                    ok = true;
                }
            } else {
                len = 0;
                ok = true;
            }
        }
        UNLOCK();
        if (!ok) {
            TRACE("socketRecv: ERROR\r\n");
            return SOCKET_ERROR;
        }
    }
    TRACE("socketRecv: %d \"%*s\"\r\n", cnt, cnt, buf-cnt);
    return cnt;
}

int MDMParser::_cbUSORF(int type, const char* buf, int len, USORFparam* param)
{
	int sz, sk, p, a,b,c,d;
	int r = sscanf(buf, "\r\n%d,%d.%d.%d.%d,%d,%d,", &sk,&a,&b,&c,&d,&p,&sz);
	if (r == 7)
	{
		char *hexString  = (char*)malloc(2*sz);
		char *charString = (char*)malloc(sz);

		memcpy(hexString, &buf[len-4-(2*sz)] ,2*sz);
		hextostr (  (unsigned char *)hexString, 2*sz, charString, sz);

		memcpy(param->buf, charString, sz);

		free(hexString);
		free(charString);

		param->ip   = IPADR(a,b,c,d);
		param->port = p;
	}
    return WAIT;
}

int MDMParser::socketRecvFrom(int socket, IP* ip, int* port, char* buf, int len)
{
    int cnt = 0;
#ifdef MDM_DEBUG
    memset(buf, '\0', len);
#endif
    Timer timer;
    timer.start();
    while (len) //1152
    {
    	int blk = MAX_SIZE; //128
    	if (len < blk){
    		blk = len;
    	}
    	bool ok = false;
    	LOCK();
    	if (_sockets[socket].pending < blk){
    		blk = _sockets[socket].pending;
    	}
    	if (blk > 0){
    		sendFormated("AT+NSORF=0,%d\r\n", blk);
    		USORFparam param;
    		param.buf = buf;
    		if (RESP_OK == waitFinalResp(_cbUSORF, &param)){
    			_sockets[socket].pending -= blk;
    			*ip = param.ip;
    			*port = param.port;
    			len -= blk;
    			cnt += blk;
    			buf += blk;
    			len = 0;
    			ok = true;
    		}
    	}else if (!TIMEOUT(timer, _sockets[socket].timeout_ms)){
    		ok = (WAIT == waitFinalResp(NULL,NULL,0)); // wait for URCs
    	}else{
    		len = 0;
    		ok = true;
    	}
    	UNLOCK();
        if (!ok){
            TRACE("socketRecv: ERROR\r\n");
            return SOCKET_ERROR;
        }
    }
    timer.stop();
    timer.reset();

    TRACE("socketRecv: %d \"%*s\"\r\n", cnt, cnt, buf-cnt);
    return cnt;
}



int MDMParser::_findSocket(int handle) {
    for (int socket = 0; socket < NUMSOCKETS; socket ++) {
        if (_sockets[socket].handle == handle)
            return socket;
    }
    return SOCKET_ERROR;
}


// --------------------------------------------------------------------------------------------------------------------------------------------------------------
// HTTP
// --------------------------------------------------------------------------------------------------------------------------------------------------------------


int MDMParser::httpFindProfile()
{
    int profile = HTTP_PROF_ERROR;  //default value
    LOCK();
    // find a free HTTP profile 
    profile = _findProfile();
    TRACE("httpFindProfile: profile is %d\r\n", profile);
    if (profile != HTTP_PROF_ERROR) {
        _httpProfiles[profile].handle     = 1;
        _httpProfiles[profile].timeout_ms = TIMEOUT_BLOCKING;
        _httpProfiles[profile].pending    = false;
        _httpProfiles[profile].cmd        = -1;
        _httpProfiles[profile].result     = -1;
    }
    UNLOCK();
    return profile;
}

int MDMParser::_findProfile(int handle) {
    for (int profile = 0; profile < NUMPROFILES; profile++) {
        if (_httpProfiles[profile].handle == handle)
            return profile;
    }
    return HTTP_PROF_ERROR;
}

bool MDMParser::httpSetBlocking(int profile, int timeout_ms)
{
    bool ok = false;
    LOCK();
    TRACE("httpSetBlocking(%d,%d)\r\n", profile, timeout_ms);
    if (ISPROFILE(profile)) {
        _httpProfiles[profile].timeout_ms = timeout_ms;
        ok = true;
    }
    UNLOCK();
    return ok;
}

bool MDMParser::httpSetProfileForCmdMng(int profile)
{
    bool ok = false;
    LOCK();
    TRACE("httpSetProfileForCmdMng(%d)\r\n", profile);
    if (ISPROFILE(profile)) {
        _httpProfiles[profile].pending = true;
        _httpProfiles[profile].result = -1;
        ok = true;
    }
    UNLOCK();
    return ok;
}

bool MDMParser::httpFreeProfile(int profile)
{
    bool ok = true;
    LOCK();
    if (ISPROFILE(profile)) {
        TRACE("httpFreeProfile(%d)\r\n", profile);
        _httpProfiles[profile].handle     = HTTP_PROF_ERROR;
        _httpProfiles[profile].timeout_ms = TIMEOUT_BLOCKING;
        _httpProfiles[profile].pending    = false;
        _httpProfiles[profile].cmd        = -1;
        _httpProfiles[profile].result     = -1;
        ok = true;
    }
    UNLOCK();
    return ok;
}

bool MDMParser::httpResetProfile(int httpProfile)
{
    bool ok = false;
    
    LOCK();
    TRACE("httpResetProfile(%d)\r\n", httpProfile);
    sendFormated("AT+UHTTP=%d\r\n", httpProfile);
    if (RESP_OK == waitFinalResp())
        ok = true;
    UNLOCK();
    
    return ok;
}

bool MDMParser::httpSetPar(int httpProfile, HttpOpCode httpOpCode, const char * httpInPar)
{
    bool ok = false;
    IP ip = NOIP;
    int httpInParNum = 0;
    
    LOCK();
    TRACE("httpSetPar(%d,%d,\"%s\")\r\n", httpProfile, httpOpCode, httpInPar);
    switch(httpOpCode){
        case HTTP_IP_ADDRESS:   //0
            ip = gethostbyname(httpInPar);
            if (ip == NOIP)
                return false;
            
            sendFormated("AT+UHTTP=%d,%d,\"" IPSTR "\"\r\n", httpProfile, httpOpCode, IPNUM(ip));
            if (RESP_OK == waitFinalResp())
                ok = true;
            break;
            
        case HTTP_SERVER_NAME:  //1
        case HTTP_USER_NAME:    //2
        case HTTP_PASSWORD:     //3
            sendFormated("AT+UHTTP=%d,%d,\"%s\"\r\n", httpProfile, httpOpCode, httpInPar);
            if (RESP_OK == waitFinalResp())
                ok = true;
            break;
        
        case HTTP_AUTH_TYPE:    //4    
        case HTTP_SERVER_PORT:  //5
            httpInParNum = atoi(httpInPar);
            sendFormated("AT+UHTTP=%d,%d,%d\r\n", httpProfile, httpOpCode, httpInParNum); 
            if (RESP_OK == waitFinalResp())
                ok = true;
            break;
            
        case HTTP_SECURE:       //6
            if(_dev.dev != DEV_LISA_C2)
            {
                httpInParNum = atoi(httpInPar);
                sendFormated("AT+UHTTP=%d,%d,%d\r\n", httpProfile, httpOpCode, httpInParNum); 
                if (RESP_OK == waitFinalResp())
                    ok = true;
            } else {
                TRACE("httpSetPar: HTTP secure option not supported by module\r\n");
                ok = false;
            }
            break;
            
        default:
            TRACE("httpSetPar: unknown httpOpCode %s\r\n", httpOpCode);
            ok = false; 
            break;   
    }
    UNLOCK();
    return ok;
}

bool MDMParser::httpCommand(int httpProfile, HttpCmd httpCmdCode, const char* httpPath, const char* httpOut, \
                            const char* httpIn, int httpContentType, const char* httpCustomPar, char* buf, int len)
{   
    bool ok = false;
#ifdef MDM_DEBUG
    memset(buf, '\0', len);
#endif
    LOCK();
    TRACE("%s\r\n", getHTTPcmd(httpCmdCode));
    switch (httpCmdCode) 
    {   
        case HTTP_HEAD:
            sendFormated("AT+UHTTPC=%d,%d,\"%s\",\"%s\"\r\n", httpProfile, HTTP_HEAD, httpPath, httpOut);
            break;
            
        case HTTP_GET:
            sendFormated("AT+UHTTPC=%d,%d,\"%s\",\"%s\"\r\n", httpProfile, HTTP_GET, httpPath, httpOut);
            break;
            
        case HTTP_DELETE:
            sendFormated("AT+UHTTPC=%d,%d,\"%s\",\"%s\"\r\n", httpProfile, HTTP_DELETE, httpPath, httpOut);
            break;
            
        case HTTP_PUT:
            //in this case the parameter httpIn is a filename
            sendFormated("AT+UHTTPC=%d,%d,\"%s\",\"%s\",\"%s\"\r\n", httpProfile, HTTP_PUT, httpPath, httpOut, httpIn);
            break;
            
        case HTTP_POST_FILE:
            //in this case the parameter httpIn is a filename
            if(_dev.dev != DEV_LISA_C2)
            {
                if(httpContentType != 6)
                    sendFormated("AT+UHTTPC=%d,%d,\"%s\",\"%s\",\"%s\",%d\r\n", \
                                  httpProfile, HTTP_POST_FILE, httpPath, httpOut, httpIn, httpContentType);
                else
                    sendFormated("AT+UHTTPC=%d,%d,\"%s\",\"%s\",\"%s\",%d,%d\r\n", \
                                  httpProfile, HTTP_POST_FILE, httpPath, httpOut, httpIn, httpContentType, httpCustomPar);
            }
            else{
                if((httpContentType != 5) && (httpContentType != 6) && (httpCustomPar == NULL))
                {
                    //parameters values consistent with the AT commands specs of LISA-C200
                    //(in particular httpCustomPar has to be not defined)
                    sendFormated("AT+UHTTPC=%d,%d,\"%s\",\"%s\",\"%s\",%d\r\n", \
                                  httpProfile, HTTP_POST_FILE, httpPath, httpOut, httpIn, httpContentType);
                } else {
                    TRACE("httpCommand: command not supported by module");
                    return ok;  //error
                }
            }
            break;
            
        case HTTP_POST_DATA:
            //in this case the parameter httpIn is a string containing data
            if(_dev.dev != DEV_LISA_C2)
            {
                if(httpContentType != 6)
                    sendFormated("AT+UHTTPC=%d,%d,\"%s\",\"%s\",\"%s\",%d\r\n", \
                                  httpProfile, HTTP_POST_DATA, httpPath, httpOut, httpIn, httpContentType);
                else
                    sendFormated("AT+UHTTPC=%d,%d,\"%s\",\"%s\",\"%s\",%d,%d\r\n", \
                                  httpProfile, HTTP_POST_DATA, httpPath, httpOut, httpIn, httpContentType, httpCustomPar);
            } else {
                if((httpContentType != 5) && (httpContentType != 6) && (httpCustomPar == NULL))
                {
                    //parameters values consistent with the AT commands specs of LISA-C200
                    //(in particular httpCustomPar has to be not defined)
                    sendFormated("AT+UHTTPC=%d,%d,\"%s\",\"%s\",\"%s\",%d\r\n", \
                                  httpProfile, HTTP_POST_DATA, httpPath, httpOut, httpIn, httpContentType);
                } else {
                    TRACE("httpCommand: command not supported by module");
                    return ok;  //error
                }
            }    
            break;
            
        default:
            TRACE("HTTP command not recognized\r\n");
            return ok;  //error
    }
    
    if (RESP_OK == waitFinalResp())
    {
        Timer timer;
        timer.start();
        httpSetProfileForCmdMng(httpProfile);
        while (_httpProfiles[httpProfile].pending)  //waiting for unsolicited
        {     
            ok = false;  //reset variable  
            if(_httpProfiles[httpProfile].result != -1)
            {    
                //received unsolicited: starting its analysis 
                _httpProfiles[httpProfile].pending = false;   
                if(_httpProfiles[httpProfile].result == 1)
                {
                    //HTTP command successfully executed
                    if(_dev.dev != DEV_LISA_C2)
                    {
                        TRACE("httpCommand: reading files with a dimension " \
                              "also greater than MAX_SIZE bytes\r\n");
                        if(readFileNew(httpOut,buf,len) >=0 )
                            ok = true;
                    } else {
                        TRACE("httpCommand: reading files with a dimension " \
                              "less than MAX_SIZE bytes, otherwise error\r\n");
                        if(readFile(httpOut,buf,len) >=0 )
                            ok = true;
                    }
                } else {
                    //HTTP command not successfully executed
                    ok = false;
                }
            } else if (!TIMEOUT(timer, _httpProfiles[httpProfile].timeout_ms)) {
                ok = (WAIT == waitFinalResp(NULL,NULL,0)); // wait for URCs
            } else  {
                //not received unsolicited and expired timer
                TRACE("httpCommand: not received unsolicited and expired timer\r\n");
                ok = false;
            }
            if (!ok) {
                TRACE("%s: ERROR\r\n", getHTTPcmd(httpCmdCode));
                _httpProfiles[httpProfile].pending = false;  //no more while loops
            }
        }
    }
    UNLOCK();
    return ok;
}

const char* MDMParser::getHTTPcmd(int httpCmdCode) 
{
    switch (httpCmdCode) 
    {
        case HTTP_HEAD:
            return "HTTP HEAD command";
        case HTTP_GET:
            return "HTTP GET command";
        case HTTP_DELETE:
            return "HTTP DELETE command";
        case HTTP_PUT:
            return "HTTP PUT command";
        case HTTP_POST_FILE:
            return "HTTP POST file command";
        case HTTP_POST_DATA:
            return "HTTP POST data command";
        default:
            return "HTTP command not recognized";
   }
}

// ------------------------------------------------------------------------------------------------------------------------------------------------------------
// SMS
// ------------------------------------------------------------------------------------------------------------------------------------------------------------

int MDMParser::_cbCMGL(int type, const char* buf, int len, CMGLparam* param)
{ 
    if ((type == TYPE_PLUS) && param && param->num) {
        // +CMGL: <ix>,...
        int ix;
        if (sscanf(buf, "\r\n+CMGL: %d,", &ix) == 1)
        {
            *param->ix++ = ix;
            param->num--;
        }
    }
    return WAIT;
}

int MDMParser::smsList(const char* stat /*= "ALL"*/, int* ix /*=NULL*/, int num /*= 0*/) {
    int ret = -1;
    LOCK();
    sendFormated("AT+CMGL=\"%s\"\r\n", stat);
    CMGLparam param;
    param.ix = ix;
    param.num = num;
    if (RESP_OK == waitFinalResp(_cbCMGL, &param))
        ret = num - param.num;
    UNLOCK();
    return ret;
}

bool MDMParser::smsSend(const char* num, const char* buf)
{
    bool ok = false;
    LOCK();
    sendFormated("AT+CMGS=\"%s\"\r\n",num);
    if (RESP_PROMPT == waitFinalResp(NULL,NULL,150*1000)) {
        send(buf, strlen(buf));
        const char ctrlZ = 0x1A;
        send(&ctrlZ, sizeof(ctrlZ));
        ok = (RESP_OK == waitFinalResp());
    }
    UNLOCK();
    return ok;
}

bool MDMParser::smsDelete(int ix)
{
    bool ok = false;
    LOCK();
    sendFormated("AT+CMGD=%d\r\n",ix);
    ok = (RESP_OK == waitFinalResp());
    UNLOCK();
    return ok;
}

int MDMParser::_cbCMGR(int type, const char* buf, int len, CMGRparam* param)
{
    if (param) {
        if (type == TYPE_PLUS) {
            if (sscanf(buf, "\r\n+CMGR: \"%*[^\"]\",\"%[^\"]", param->num) == 1) {
            }
        } else if ((type == TYPE_UNKNOWN) && (buf[len-2] == '\r') && (buf[len-1] == '\n')) {
            memcpy(param->buf, buf, len-2);
            param->buf[len-2] = '\0';
        }
    }
    return WAIT;
}

bool MDMParser::smsRead(int ix, char* num, char* buf, int len)
{
    bool ok = false;
    LOCK();
    CMGRparam param;
    param.num = num;
    param.buf = buf;
    sendFormated("AT+CMGR=%d\r\n",ix);
    ok = (RESP_OK == waitFinalResp(_cbCMGR, &param));
    UNLOCK();
    return ok;
}
   
// -------------------------------------------------------------------------------------------------------------------------------------------------------
  
int MDMParser::_cbCUSD(int type, const char* buf, int len, char* resp)
{
    if ((type == TYPE_PLUS) && resp) {
        // +USD: \"%*[^\"]\",\"%[^\"]\",,\"%*[^\"]\",%d,%d,%d,%d,\"*[^\"]\",%d,%d"..);
        if (sscanf(buf, "\r\n+CUSD: %*d,\"%[^\"]\",%*d", resp) == 1) {
            /*nothing*/            
        }
    }
    return WAIT;
}  

bool MDMParser::ussdCommand(const char* cmd, char* buf)
{
    bool ok = false;
    LOCK();
    *buf = '\0';
    if (_dev.dev != DEV_LISA_C2) {
        sendFormated("AT+CUSD=1,\"%s\"\r\n",cmd);
        ok = (RESP_OK == waitFinalResp(_cbCUSD, buf));
    }
    UNLOCK();
    return ok;
}

// -----------------------------------------------------------------------------------------------------------------------------------------------------------
   
int MDMParser::_cbUDELFILE(int type, const char* buf, int len, void*)
{
    if ((type == TYPE_ERROR) && strstr(buf, "+CME ERROR: FILE NOT FOUND")) 
        return RESP_OK; // file does not exist, so all ok...
    return WAIT;
}  

bool MDMParser::delFile(const char* filename)
{
    bool ok = false;
    LOCK();
    sendFormated("AT+UDELFILE=\"%s\"\r\n", filename);
    ok = (RESP_OK == waitFinalResp(_cbUDELFILE));
    UNLOCK();
    return ok;
}

int MDMParser::writeFile(const char* filename, const char* buf, int len)
{
    bool ok = false;
    LOCK();
    sendFormated("AT+UDWNFILE=\"%s\",%d\r\n", filename, len);
    if (RESP_PROMPT == waitFinalResp()) {
        send(buf, len);
        ok = (RESP_OK == waitFinalResp());
    }
    UNLOCK();
    return ok ? len : -1;
}

int MDMParser::readFile(const char* filename, char* buf, int len)
{
    URDFILEparam param;
    param.filename = filename;
    param.buf = buf; 
    param.sz = len; 
    param.len = 0;
    LOCK();
    sendFormated("AT+URDFILE=\"%s\"\r\n", filename, len);
    if (RESP_OK != waitFinalResp(_cbURDFILE, &param))
        param.len = -1;
    UNLOCK();
    return param.len;
}

int MDMParser::_cbURDFILE(int type, const char* buf, int len, URDFILEparam* param)
{
    if ((type == TYPE_PLUS) && param && param->filename && param->buf) {
        char filename[48];
        int sz;
        if ((sscanf(buf, "\r\n+URDFILE: \"%[^\"]\",%d,", filename, &sz) == 2) && 
            (0 == strcmp(param->filename, filename)) &&
            (buf[len-sz-2] == '\"') && (buf[len-1] == '\"')) {
            param->len = (sz < param->sz) ? sz : param->sz;
            memcpy(param->buf, &buf[len-1-sz], param->len);
        }
    }
    return WAIT;
}

//The following function is useful for reading files with a dimension greater than MAX_SIZE bytes
int MDMParser::readFileNew(const char* filename, char* buf, int len)
{   
    int countBytes = -1;  //counter for file reading (default value)
    
    if(_dev.dev != DEV_LISA_C2)
    {
        //retrieve information about the file, in particular its size
        int filesize = infoFile(filename);
        TRACE("readFileNew: filename is %s; filesize is %d\r\n", filename, filesize);
        
        if (len < filesize)
            TRACE("readFileNew: WARNING. Buffer dimension is %d bytes," \
                  "while file size is %d bytes\r\n", len, filesize);
        
        if (filesize > 0)
        {
#ifdef MDM_DEBUG
            memset(buf, '\0', len);
#endif
            int offset = 0;              //start reading from 0
            int blockSize = MAX_SIZE;    //still need space for headers and unsolicited commands
            int bytesToRead = filesize;  //bytes to read 
            
            while (bytesToRead)
            {    
                bool ok = false;
                
                if (bytesToRead < blockSize)
                    blockSize = bytesToRead;
                
                LOCK();
                if (blockSize > 0) {
                            
                    sendFormated("AT+URDBLOCK=\"%s\",%d,%d\r\n", filename, offset, blockSize);
                    
                    if (RESP_OK == waitFinalResp(_cbURDBLOCK, buf)) {
                        bytesToRead -= blockSize;
                        offset += blockSize;
                        buf += blockSize;
                        ok = true;
                    } else {
                        //error condition
                        countBytes = -1;
                        ok = false;
                    }
                }
                UNLOCK();
                
                if (!ok) {
                    TRACE("readFileNew: ERROR\r\n");
                    return countBytes;  //in this case countBytes is -1
                }
            }
            
            countBytes = offset;  //total read bytes
            return countBytes;
        }
    } else {
        TRACE("httpCommand: command not supported by module"); 
    }
    return countBytes;  //it could be 0 or -1 (possible error)    
}

int MDMParser::_cbURDBLOCK(int type, const char* buf, int len, char* out)
{   
    char fileNameRes[48]; 
    int sizeRes;
    
    if ((type == TYPE_PLUS) && out) {
        if ((sscanf(buf, "\r\n+URDBLOCK: \"%[^\"]\",%d,", fileNameRes, &sizeRes) == 2) &&
            (buf[len-sizeRes-2] == '\"') && (buf[len-1] == '\"')) {
            memcpy(out, &buf[len-1-sizeRes], sizeRes);
        }
    }
    
    return WAIT;
}

int MDMParser::infoFile(const char* filename)
{
    int infoFile = 0;  //default value
    
    LOCK();
    sendFormated("AT+ULSTFILE=2,\"%s\"\r\n", filename);
    if (RESP_OK != waitFinalResp(_cbULSTFILE, &infoFile))
        infoFile = -1;  //error condition    
    UNLOCK();
    
    return infoFile;
}

int MDMParser::_cbULSTFILE(int type, const char* buf, int len, int* infoFile)
{ 
    if (infoFile) {
        if (type == TYPE_PLUS) {
            if (sscanf(buf, "\r\n+ULSTFILE: %d\r\n", infoFile) == 1) {
            }
        }
    }
    return WAIT;
}

// ---------------------------------------------------------------------------------------------------------------------------------------------------------


int MDMParser::cellLocSrvTcp(const char* token, const char* server_1, const char* server_2, int days/* = 14*/, \
        int period/* = 4*/, int resolution/* = 1*/)
{
    bool ok = false;
    LOCK();
    if (_dev.dev == DEV_LISA_U2_03S || _dev.dev == DEV_SARA_U2 ){
        sendFormated("AT+UGSRV=\"%s\",\"%s\",\"%s\"\r\n", server_1, server_2, token, days, period, resolution);
        ok = (RESP_OK == waitFinalResp());
    } else
        ERROR("Command not supported\r\n"); 
    UNLOCK();
    return ok;
}

int MDMParser::cellLocSrvUdp(const char* server_1 /*= "cell-live1.services.u-blox.com"*/, int port /*= 46434*/, \
        int latency/* = 1000*/, int mode/* = 0*/)
{
    bool ok = false;
    LOCK();
    if (_dev.dev != DEV_TOBY_L2){
        sendFormated("AT+UGAOP=\"%s\",%d,%d,%d\r\n", server_1, port, latency, mode);
        ok = (RESP_OK == waitFinalResp());
    } else
        ERROR("Command not supported\r\n"); 
    UNLOCK();
    return ok;
}

int MDMParser::cellLocUnsol(int mode)
{
    bool ok = false;
    LOCK();
    if (_dev.dev == DEV_LISA_U2_03S){
        sendFormated("AT+ULOCIND=%d\r\n", mode);
        ok = (RESP_OK == waitFinalResp());
    } else
        ERROR("Command not supported\r\n"); 
    UNLOCK();
    return ok;
}

int MDMParser::cellLocConfig(int scanMode)
{
    bool ok = false;
    LOCK();
    if (_dev.dev != DEV_TOBY_L2){
        sendFormated("AT+ULOCCELL=%d\r\n", scanMode);
        ok = (RESP_OK == waitFinalResp());    
    }else
        ERROR("Command not supported\r\n"); 
    UNLOCK();
    return ok;
}

int MDMParser::cellLocRequest(CellSensType sensor, int timeout, int accuracy, CellRespType type/* =1*/, int hypotesis/* =1*/)
{
    bool ok = false;
    
    if (hypotesis > 1 && type != CELL_MULTIHYP){
        ERROR("Num hypotesis is not set accordelying to CellRespType\r\n");
        return false;
        }        
    if (hypotesis > CELL_MAX_HYP){
        ERROR("Number of hypotesis is too big\r\n");
        return false;  
    }        
    LOCK();      
    _locRcvPos=0;
    _locExpPos=0;
    for (int i=0; i < hypotesis; i++)
        _loc[i].validData = false;          
    if (_dev.dev == DEV_LISA_U2_03S){
        sendFormated("AT+ULOC=2,%d,%d,%d,%d,%d\r\n", sensor, type, timeout, accuracy, hypotesis);        
        ok = (RESP_OK == waitFinalResp());
    } else if (_dev.dev != DEV_TOBY_L2){
        sendFormated("AT+ULOC=2,%d,1,%d,%d\r\n",  sensor, timeout, accuracy);
        ok = (RESP_OK == waitFinalResp());
    } else
        ERROR("Command not supported\r\n");  
    UNLOCK();
    return ok;
}  
int MDMParser::cellLocGetRes()
{
    return _locRcvPos;
}
int MDMParser::cellLocGetExpRes()
{
    int res=0;
    waitFinalResp(NULL,NULL,0);
    LOCK();
    if (_locRcvPos>0)
        res = _locExpPos;
    UNLOCK();
    return res;
}

int MDMParser::cellLocGetData(CellLocData *data, int index/*=0*/){
    
    if (!_loc[index].validData)
          return false;
    LOCK(); 
    memcpy(data, &_loc[index], sizeof(*_loc));
    UNLOCK();
    return true;
}

// --------------------------------------------------------------------------------------------------------------------------------------------------------------
bool MDMParser::setDebug(int level) 
{
#ifdef MDM_DEBUG
    _debugLevel = (level < -1) ? -1 : 
                  (level >  3) ?  3 : 
                                 level;
#endif
    return _debugLevel == level;
}

void MDMParser::dumpDevStatus(MDMParser::DevStatus* status, 
            _DPRINT dprint, void* param) 
{
    dprint(param, "Modem::devStatus\r\n");
    const char* txtDev[] = { "Unknown",  "SARA-G35", "LISA-U2", "LISA-U2-03S", "LISA-C2", 
                             "SARA-U2",  "LEON-G2",  "TOBY-L2", "MPCI-L2" };
    if (status->dev < sizeof(txtDev)/sizeof(*txtDev) && (status->dev != DEV_UNKNOWN))
        dprint(param, "  Device:       %s\r\n", txtDev[status->dev]);
    const char* txtLpm[] = { "Disabled", "Enabled", "Active" };
    if (status->lpm < sizeof(txtLpm)/sizeof(*txtLpm))
        dprint(param, "  Power Save:   %s\r\n", txtLpm[status->lpm]);
    const char* txtSim[] = { "Unknown", "Missing", "Pin", "Ready" };
    if (status->sim < sizeof(txtSim)/sizeof(*txtSim) && (status->sim != SIM_UNKNOWN))
        dprint(param, "  SIM:          %s\r\n", txtSim[status->sim]);
    if (*status->ccid)  
        dprint(param, "  CCID:         %s\r\n", status->ccid);
    if (*status->imei) 
        dprint(param, "  IMEI:         %s\r\n", status->imei);
    if (*status->imsi)  
        dprint(param, "  IMSI:         %s\r\n", status->imsi);
    if (*status->meid) 
        dprint(param, "  MEID:         %s\r\n", status->meid); // LISA-C
    if (*status->manu) 
        dprint(param, "  Manufacturer: %s\r\n", status->manu);
    if (*status->model)  
        dprint(param, "  Model:        %s\r\n", status->model);
    if (*status->ver)  
        dprint(param, "  Version:      %s\r\n", status->ver);
}

void MDMParser::dumpNetStatus(MDMParser::NetStatus *status,
            _DPRINT dprint, void* param)
{
    dprint(param, "Modem::netStatus\r\n");
    const char* txtReg[] = { "Unknown", "Denied", "None", "Home", "Roaming" };
    if (status->csd < sizeof(txtReg)/sizeof(*txtReg) && (status->csd != REG_UNKNOWN))
        dprint(param, "  CSD Registration:   %s\r\n", txtReg[status->csd]);
    if (status->psd < sizeof(txtReg)/sizeof(*txtReg) && (status->psd != REG_UNKNOWN))
        dprint(param, "  PSD Registration:   %s\r\n", txtReg[status->psd]);
    if (status->eps < sizeof(txtReg)/sizeof(*txtReg) && (status->eps != REG_UNKNOWN))
        dprint(param, "  EPS Registration:   %s\r\n", txtReg[status->eps]);
    const char* txtAct[] = { "Unknown", "GSM", "Edge", "3G", "CDMA", "LTE" };
    if (status->act < sizeof(txtAct)/sizeof(*txtAct) && (status->act != ACT_UNKNOWN))
        dprint(param, "  Access Technology:  %s\r\n", txtAct[status->act]);
    if (status->rssi) 
        dprint(param, "  Signal Strength:    %d dBm\r\n", status->rssi);
    if (status->ber) 
        dprint(param, "  Bit Error Rate:     %d\r\n", status->ber);
    if (*status->opr)  
        dprint(param, "  Operator:           %s\r\n", status->opr);
    if (status->lac != 0xFFFF)  
        dprint(param, "  Location Area Code: %04X\r\n", status->lac);
    if (status->ci != 0xFFFFFFFF)  
        dprint(param, "  Cell ID:            %08X\r\n", status->ci);
    if (*status->num)  
        dprint(param, "  Phone Number:       %s\r\n", status->num);
}

void MDMParser::dumpIp(MDMParser::IP ip,
            _DPRINT dprint, void* param) 
{
    if (ip != NOIP)
        dprint(param, "Modem:IP " IPSTR "\r\n", IPNUM(ip));
}
    
// ---------------------------------------------------------------------------------------------------------------------------------------------------------------
int MDMParser::_parseMatch(Pipe<char>* pipe, int len, const char* sta, const char* end)
{
    int o = 0;
    if (sta) {
        while (*sta) {
            if (++o > len)                  return WAIT;
            char ch = pipe->next();
            if (*sta++ != ch)               return NOT_FOUND;
        }
    }
    if (!end)                               return o; // no termination
    // at least any char
    if (++o > len)                      return WAIT;
    pipe->next();
    // check the end     
    int x = 0;
    while (end[x]) {
        if (++o > len)                      return WAIT;
        char ch = pipe->next();
        x = (end[x] == ch) ? x + 1 : 
            (end[0] == ch) ? 1 : 
                            0;
    }
    return o;
}

int MDMParser::_parseFormated(Pipe<char>* pipe, int len, const char* fmt)
{
    int o = 0;
    int num = 0;
    if (fmt) {
        while (*fmt) {
            if (++o > len)                  return WAIT;
            char ch = pipe->next();
            if (*fmt == '%') {
                fmt++;
                if (*fmt == 'd') { // numeric
                    fmt ++;
                    num = 0;
                    while (ch >= '0' && ch <= '9') {
                        num = num * 10 + (ch - '0'); 
                        if (++o > len)      return WAIT;
                        ch = pipe->next();
                    }
                }   
                else if (*fmt == 'c') { // char buffer (takes last numeric as length)
                    fmt ++;
                    while (num --) {
                        if (++o > len)      return WAIT;
                        ch = pipe->next();
                    }   
                }
                else if (*fmt == 's') {
                    fmt ++;
                    if (ch != '\"')         return NOT_FOUND;
                    do {
                        if (++o > len)      return WAIT;
                        ch = pipe->next();
                    } while (ch != '\"');
                    if (++o > len)          return WAIT;
                    ch = pipe->next();
                }
            }
            if (*fmt++ != ch)               return NOT_FOUND;
        }
    }
    return o; 
}

int MDMParser::_getLine(Pipe<char>* pipe, char* buf, int len)
{
    int unkn = 0;
    int sz = pipe->size();
    int fr = pipe->free();
    if (len > sz)
        len = sz;
    while (len > 0)
    {
        static struct { 
              const char* fmt;                              int type; 
        } lutF[] = {
            { "\r\n+USORD: %d,%d,\"%c\"",                   TYPE_PLUS       },
			{ "\r\n+NSONMI: %d,%d,\"%c\"",                  TYPE_PLUS       },
            { "\r\n+USORF: %d,\"" IPSTR "\",%d,%d,\"%c\"",  TYPE_PLUS       },
			{ "\r\n+NSORF: %d,\"" IPSTR "\",%d,%d,\"%c\"",  TYPE_PLUS       },
            { "\r\n+URDFILE: %s,%d,\"%c\"",                 TYPE_PLUS       },
            { "\r\n+URDBLOCK: %s,%d,\"%c\"",                TYPE_PLUS       },
        };
        static struct { 
              const char* sta;          const char* end;    int type; 
        } lut[] = {
            { "\r\nOK\r\n",             NULL,               TYPE_OK         },
            { "\r\nERROR\r\n",          NULL,               TYPE_ERROR      },
            { "\r\n+CME ERROR:",        "\r\n",             TYPE_ERROR_CME      }, 
            { "\r\n+CMS ERROR:",        "\r\n",             TYPE_ERROR      },
            { "\r\nRING\r\n",           NULL,               TYPE_RING       },
            { "\r\nCONNECT\r\n",        NULL,               TYPE_CONNECT    },
            { "\r\nNO CARRIER\r\n",     NULL,               TYPE_NOCARRIER  },
            { "\r\nNO DIALTONE\r\n",    NULL,               TYPE_NODIALTONE },
            { "\r\nBUSY\r\n",           NULL,               TYPE_BUSY       },
            { "\r\nNO ANSWER\r\n",      NULL,               TYPE_NOANSWER   },
            { "\r\n+",                  "\r\n",             TYPE_PLUS       },
            { "\r\n@",                  NULL,               TYPE_PROMPT     }, // Sockets
            { "\r\n>",                  NULL,               TYPE_PROMPT     }, // SMS
            { "\n>",                    NULL,               TYPE_PROMPT     }, // File
        };
        for (int i = 0; i < sizeof(lutF)/sizeof(*lutF); i ++) {
            pipe->set(unkn);
            int ln = _parseFormated(pipe, len, lutF[i].fmt);
            if (ln == WAIT && fr)                       
                return WAIT;
            if ((ln != NOT_FOUND) && (unkn > 0))  
                return TYPE_UNKNOWN | pipe->get(buf, unkn);
            if (ln > 0)
                return lutF[i].type  | pipe->get(buf, ln);
        }
        for (int i = 0; i < sizeof(lut)/sizeof(*lut); i ++) {
            pipe->set(unkn);
            int ln = _parseMatch(pipe, len, lut[i].sta, lut[i].end);
            if (ln == WAIT && fr)                       
                return WAIT;
            if ((ln != NOT_FOUND) && (unkn > 0))  
                return TYPE_UNKNOWN | pipe->get(buf, unkn);
            if (ln > 0)
                return lut[i].type | pipe->get(buf, ln);
        }
        // UNKNOWN
        unkn ++;
        len--;
    }
    return WAIT;
}

// ----------------------------------------------------------------
// Serial Implementation 
// ----------------------------------------------------------------

/*! Helper Dev Null Device 
    Small helper class used to shut off stderr/stdout. Sometimes stdin/stdout
    is shared with the serial port of the modem. Having printfs inbetween the 
    AT commands you cause a failure of the modem.
*/
class DevNull : public Stream {
public: 
    DevNull() : Stream(_name+1) { }             //!< Constructor
    void claim(const char* mode, FILE* file) 
        { freopen(_name, mode, file); }         //!< claim a stream
protected:
    virtual int _getc()         { return EOF; } //!< Nothing
    virtual int _putc(int c)    { return c; }   //!< Discard
    static const char* _name;                   //!< File name
};
const char* DevNull::_name = "/null";  //!< the null device name
static      DevNull null;              //!< the null device

MDMSerial::MDMSerial(PinName tx /*= MDMTXD*/, PinName rx /*= MDMRXD*/, 
            int baudrate /*= MDMBAUD*/,
#if DEVICE_SERIAL_FC
            PinName rts /*= MDMRTS*/, PinName cts /*= MDMCTS*/, 
#endif
            int rxSize /*= 256*/, int txSize /*= 128*/) : 
            SerialPipe(tx, rx, rxSize, txSize) 
{
    if (rx == USBRX) 
        null.claim("r", stdin);
    if (tx == USBTX) {
        null.claim("w", stdout);
        null.claim("w", stderr);
#ifdef MDM_DEBUG
        _debugLevel = -1;
#endif
    }
#ifdef TARGET_UBLOX_C027
    _onboard = (tx == MDMTXD) && (rx == MDMRXD);
    if (_onboard)
       c027_mdm_powerOn(false);
#endif
    baud(baudrate);
#if DEVICE_SERIAL_FC
    if ((rts != NC) || (cts != NC))
    {
        Flow flow = (cts == NC) ? RTS :
                    (rts == NC) ? CTS : RTSCTS ;
        set_flow_control(flow, rts, cts);
        if (cts != NC) _dev.lpm = LPM_ENABLED;
    }
#endif
}

MDMSerial::~MDMSerial(void)
{
    powerOff();
#ifdef TARGET_UBLOX_C027
    if (_onboard)
        c027_mdm_powerOff();
#endif
}

int MDMSerial::_send(const void* buf, int len)   
{ 
    return put((const char*)buf, len, true/*=blocking*/);
}

int MDMSerial::getLine(char* buffer, int length)
{
    return _getLine(&_pipeRx, buffer, length);
}

// ----------------------------------------------------------------
// USB Implementation 
// ----------------------------------------------------------------

#ifdef HAVE_MDMUSB
MDMUsb::MDMUsb(void)                             
{ 
#ifdef MDM_DEBUG
    _debugLevel = 1;
#endif    
#ifdef TARGET_UBLOX_C027
    _onboard = true;
    c027_mdm_powerOn(true);
#endif
}

MDMUsb::~MDMUsb(void)
{
    powerOff();
#ifdef TARGET_UBLOX_C027
    if (_onboard)
        c027_mdm_powerOff();
#endif
}

int MDMUsb::_send(const void* buf, int len)      { return 0; }

int MDMUsb::getLine(char* buffer, int length)    { return NOT_FOUND; }

#endif
