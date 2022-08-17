/*
     
                     ,----------------,              ,---------,
                ,-----------------------,          ,"        ,"|
              ,"                      ,"|        ,"        ,"  |
             +-----------------------+  |      ,"        ,"    |
             |  .-----------------.  |  |     +---------+      |
             |  | huhai@ubuntu:   |  |  |     | -==----'|      |
             |  |                 |  |  |     |         |      |
             |  |                 |  |  |/----|`---=    |      |
             |  |                 |  |  |   ,/|==== ooo |      ;
             |  |                 |  |  |  // |(((( [33]|    ,"
             |  `-----------------'  |," .;'| |((((     |  ,"
             +-----------------------+  ;;  | |         |,"
                /_)______________(_/  //'   | +---------+
           ___________________________/___  `,
          /  oooooooooooooooo  .o.  oooo /,   ,"-----------
         / ==ooooooooooooooo==.o.  ooo= //   ,`--{)B     ,"

 *  
 * @Date:  2021-09-09
 * @LastEditTime:  2021-09-09
 * @LastEditors:
 * @FilePath:
 * add HTimerCenter by huhai 2022-05-16
 * @Description:
 */
#ifndef HHUTILS_H
#define HHUTILS_H
#include <string>
#include <vector>
#include <regex>
#include <time.h>
#include <atomic>
#include <array>
#include <stdlib.h>
#include <string.h>
#include <dlfcn.h>
#include <errno.h>
#include <pthread.h>
#include <sys/types.h>
#include <dirent.h>
#include <stdarg.h>
#include <unistd.h>
#include <mutex>
#include <vector>
#include <sys/time.h>
#include <queue>
#include <thread>
#include <atomic>
#include <sys/stat.h>
#include <sys/sysinfo.h>
#include <sys/stat.h>
#include <list>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/file.h>
namespace hhutils
{

    class HTimer;
    typedef std::function<bool(HTimer *timer)> TimerHandler;
    enum class E_HTIMER_TYPE
    {
        ONCE,
        REPEAT_FOREVER,
        REPEAT_TIMES_IF_FAILED
    };

    class FileSystem
    {
    public:
        static std::string getFileName(const std::string &path)
        {            
            auto p = path.rfind("/");
            if(p==std::string::npos){
                return path;
            }
            return path.substr(p + 1);
        }
        static std::string getDir(const std::string &path)
        {
            auto p = path.rfind("/");
            if(p==std::string::npos){
                return path;
            }
            return path.substr(0, p);
        }

        class FileLock
        {
            std::string lockpath;
            int fd;

        public:
            FileLock(const std::string &path) : lockpath(path + ".lock")
            {
                fd = -1;
            }

            bool lock()
            {
                fd = open(lockpath.c_str(), O_CREAT | O_RDWR, 00777);
                if (fd < 0)
                {
                    printf(" set lock: open file %s error %s\n", lockpath.c_str(), strerror(errno));
                    return false;
                }
                // block process
                if (flock(fd, LOCK_EX) != 0)
                {
                    close(fd);
                    return false;
                }
                return true;
            }

            bool unlock()
            {
                if (fd < 0)
                {
                    return true;
                }
                flock(fd, LOCK_UN);
                close(fd);
                fd = -1;
                return true;
            }
        };

        /*
         * return: > 0 The size of the path with unit KB
         *         < 0 get failed.
         */
        static int getFileSize(const std::string &path)
        {
            struct stat statbuff;
            if (stat(path.c_str(), &statbuff) < 0)
            {
                return -1;
            }
            return statbuff.st_size / 1024;
        }
    };

    class FileLock
    {
        int fd;
        std::string file;
    public:
        FileLock(const std::string &path)
        {
            file = path+".lock";
        }
        bool lock()
        {
            if(fd < 0)
            {
                fd = open(file.c_str(), O_CREAT | O_RDWR, 00777);
                if (fd < 0)
                {
                    return false;
                }
            }
            if (flock(fd, LOCK_EX) != 0)
            {
                close(fd);
                fd = -1;
                return false;
            }
            return true;
        }

        void unlock()
        {
            if (fd < 0)
            {
                return;
            }
            flock(fd, LOCK_UN);
            close(fd);
            fd = -1;
        }
    };



    class HTimer
    {
    protected:
        time_t intervalMs;
        std::chrono::steady_clock::time_point triggerPoint;
        TimerHandler activeHandler;
        std::string timerName;
        E_HTIMER_TYPE type;
        uint32_t repeatTimes;
        uint32_t invokTimes;
        bool valid;

    public:
        virtual ~HTimer() {}
        HTimer(time_t timerMs)
        {
            intervalMs = timerMs;
            invokTimes = repeatTimes = 0;
            valid = true;
            type = E_HTIMER_TYPE::ONCE;
        }

        virtual bool onTask() { return true; }

        std::string name() const
        {
            return timerName;
        }

        void setType(E_HTIMER_TYPE t, uint32_t repeatsCnt = 0)
        {
            type = t;
            repeatTimes = repeatsCnt;
        }

        HTimer(time_t timerMs, TimerHandler handler, const std::string &aname = "")
        {
            static std::atomic_int timeCnt = {0};
            type = E_HTIMER_TYPE::ONCE;
            intervalMs = timerMs;
            valid = true;
            reset();
            if (aname.empty())
            {
                timerName = std::to_string(timeCnt++);
            }
            else
            {
                timerName = aname;
                timeCnt++;
            }
            activeHandler = handler;
        }

        time_t leftTimeMs()
        {
            if (expired())
            {
                return 0;
            }
            auto now = std::chrono::steady_clock::now();
            return std::chrono::duration_cast<std::chrono::milliseconds>(now - triggerPoint).count();
        }

        /**
         * @brief: active now.
         */
        void reset()
        {
            triggerPoint = std::chrono::steady_clock::now() + std::chrono::milliseconds(intervalMs);
        }

        bool expired()
        {
            if (type == E_HTIMER_TYPE::ONCE)
            {
                return std::chrono::steady_clock::now() > triggerPoint;
            }
            if (type == E_HTIMER_TYPE::REPEAT_TIMES_IF_FAILED)
            {
                return invokTimes >= repeatTimes;
            }
            return false;
        }

        bool isValid()
        {
            return valid;
        }

        bool update()
        {
            if (!valid)
            {
                return false;
            }
            if (std::chrono::steady_clock::now() >= triggerPoint)
            {
                bool ret = false;
                if (activeHandler)
                {
                    ret = activeHandler(this);
                }
                else
                {
                    ret = onTask();
                }
                invokTimes++;

                switch (type)
                {
                case E_HTIMER_TYPE::ONCE:
                    valid = false;
                    break;
                case E_HTIMER_TYPE::REPEAT_FOREVER:
                    reset();
                    return true;
                case E_HTIMER_TYPE::REPEAT_TIMES_IF_FAILED:
                    if (ret)
                    {
                        valid = false;
                        return true;
                    }
                    if (invokTimes >= repeatTimes)
                    {
                        valid = false;
                        return false;
                    }
                    return true;
                default:
                    valid = false;
                    return true;
                }
                return true;
            }
            return false;
        }
    };

    class HTask : public HTimer
    {
    public:
        virtual ~HTask() {}
        HTask(uint32_t aintervalMs) : HTimer(aintervalMs)
        {
        }
    };

    class System
    {
    public:

        static std::string getProcName()
        {
            char buf[1024] = { 0 };
            int n;
            n = readlink("/proc/self/exe" , buf , sizeof(buf));
            if( n > 0 && n < (int)sizeof(buf))
            {
                return buf;
            }
            return "";
        }

        static std::string doSystemCmd(const std::string cmd)
        {
            FILE *fp;
            char path[1035];
            bzero(path, sizeof(path));

            /* Open the command for reading. */
            fp = popen(cmd.c_str(), "r");
            if (fp == NULL)
            {
                printf("do cmd :%s\n", cmd.c_str());
                printf("cmd ret failed:%s", strerror(errno));
                return "";
            }

            std::string ret = "";
            /* Read the output a line at a time - output it. */
            while (fgets(path, sizeof(path) - 1, fp) != NULL)
            {
                ret += path;
            }

            //        printf("cmd ret:%s\n",ret.c_str());
            /* close */
            pclose(fp);
            return ret;
        }

        static int32_t getFileLastModifyTime(const std::string &path)
        {
            if (access(path.c_str(), F_OK) != 0)
            {
                printf("file %s not exist\n", path.c_str());
                return -1;
            }
            struct stat sb;
            if (stat(path.c_str(), &sb) < 0)
            {
                printf("err fstat:%d\n", errno);
                return -2;
            }
            return sb.st_mtim.tv_sec;
        }
    };

    class HTimerCenter
    {
        std::list<std::shared_ptr<HTimer>> timers;
        std::mutex timersLock;

    public:
        void update()
        {
            std::lock_guard<std::mutex> lg(timersLock);
            for (auto &t : timers)
            {
                if (t->isValid())
                {
                    t->update();
                }
                else
                {
                    timers.remove(t);
                }
            }
        }

        bool add(
            const TimerHandler &handler,
            time_t intervalMs,
            bool repeat = false, const std::string &name = "")
        {
            std::lock_guard<std::mutex> lg(timersLock);
            auto t = std::make_shared<HTimer>(intervalMs, handler, name);
            if (repeat)
            {
                t->setType(E_HTIMER_TYPE::REPEAT_FOREVER);
            }
            timers.push_back(t);
            return true;
        }
    };

    class HAsyncLoopThread
    {
        volatile std::atomic<bool> threadSwitch;
        volatile bool isThreadRunning;
        std::string name_;
        std::function<void()> task_;
        std::mutex lock;

        void setName(const std::string name = "")
        {
            static std::atomic<int> cnt;
            name_ = name;
            if (name_.empty())
            {
                name_ = "HAsyncLoopThread_" + std::to_string(cnt.fetch_add(1));
            }
        }

    public:
        HAsyncLoopThread()
        {
            isThreadRunning = false;
            threadSwitch = false;
            setName();
        }

        /**
         * @brief HAsyncLoopTask
         *  u should not block in task.
         * @param task
         */
        HAsyncLoopThread(const std::function<void()> &task, const std::string &name = "")
        {
            start(task, name);
        }

        bool isRunning()
        {
            return isThreadRunning;
        }

        void start(const std::function<void()> &task, const std::string &name = "")
        {
            task_ = task;
            setName(name);
            {
                std::lock_guard<std::mutex> lg(lock);
                if (threadSwitch)
                {
                    return;
                }
                threadSwitch = true;
                isThreadRunning = false;
            }
            std::thread([&]()
                        {
            isThreadRunning = true;
            while(threadSwitch){
                task_();
            }
            isThreadRunning = false; })
                .detach();
        }

        void stop()
        {
            {
                std::lock_guard<std::mutex> lg(lock);
                threadSwitch = false;
            }
            while (isThreadRunning)
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
        }

        virtual ~HAsyncLoopThread()
        {
            threadSwitch = false;
            while (isThreadRunning)
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
        }
    };

    /*
     * while no task is running, The class creat a thread to run it.
     */
    class HTinyThread
    {
        std::mutex lock;
        bool threadOverFlag;
        std::chrono::steady_clock::time_point threadStartTime;
        bool busy()
        {
            if (threadOverFlag)
            {
                if (threadStartTime - std::chrono::steady_clock::now() < std::chrono::seconds(10))
                {
                    return true;
                }
            }
            return false;
        }
        void run(const std::function<void()> &t)
        {
            std::thread([&]()
                        {
            t();
            threadOverFlag = false; })
                .detach();
        }

    public:
        HTinyThread()
        {
            threadOverFlag = false;
            threadStartTime = std::chrono::steady_clock::now();
        }

        bool runIfIdle(const std::function<void()> &t)
        {
            std::lock_guard<std::mutex> lg(lock);
            if (busy())
            {
                return false;
            }
            run(t);
            return true;
        }
    };

    template <class T, size_t maxSize = 10>
    class HSafeQueue
    {
        std::mutex lock;
        std::queue<T> queue_;

    public:
        void push(const T &v)
        {
            std::lock_guard<std::mutex> lg(lock);
            if (queue_.size() >= 10)
            {
                queue_.pop();
            }
            queue_.push(v);
        }

        bool empty()
        {
            std::lock_guard<std::mutex> lg(lock);
            return queue_.empty();
        }

        T &front()
        {
            std::lock_guard<std::mutex> lg(lock);
            return queue_.front();
        }

        void removeFirst()
        {
            std::lock_guard<std::mutex> lg(lock);
            if (!queue_.empty())
            {
                return queue_.pop();
            }
        }

        bool pop(T &v)
        {
            std::lock_guard<std::mutex> lg(lock);
            if (queue_.empty())
            {
                return false;
            }
            v = queue_.front();
            queue_.pop();
            return true;
        }

        size_t size()
        {
            std::lock_guard<std::mutex> lg(lock);
            return queue_.size();
        }
    };

    using _uu_edian = union
    {
        char c[4];
        unsigned long mylong;
    };
    static _uu_edian __endian_test = {{'l', '?', '?', 'b'}};
#ifndef SMALL_ENDIANNESS
#define SMALL_ENDIANNESS ((char)__endian_test.mylong)
#endif
#ifndef BIG_ENDIANNESS
#define BIG_ENDIANNESS (((__endian_test.mylong & 0x0ff) == 'b'))
#endif

    class hutils
    {
    public:
        class BytesOrder
        {

            static void reverseCopy(void *dst, const void *src, size_t size)
            {
                size_t dstId = 0;
                size_t srcId = size - 1;
                for (; dstId < size; dstId++, srcId--)
                {
                    ((uint8_t *)dst)[dstId] = ((uint8_t *)src)[srcId];
                }
            }

        public:
            // 链路层传输顺序为低位在前，高位在后；低字节在前，高字节在后。
            template <class T>
            static size_t serialize(const T &v, std::vector<uint8_t> &outStream)
            {

                std::vector<uint8_t> buff(sizeof(v), 0);
                if (SMALL_ENDIANNESS)
                {
                    memcpy(&buff[0], &v, sizeof(v));
                }
                else
                {
                    reverseCopy(&buff[0], &v, sizeof(v));
                }
                outStream.insert(outStream.end(), buff.begin(), buff.end());
                return sizeof(v);
            }

            template <class T>
            static size_t unserialize(const std::vector<uint8_t> &inStream, T &out)
            {
                int typeSize = (int)sizeof(out);
                if (inStream.size() < sizeof(out))
                {
                    //            printf("%lu <%lu\n", inStream.size(), sizeof(out));
                    throw std::runtime_error("unserialize error");
                }

                if (typeSize == 1)
                {
                    out = inStream[0];
                    return 1;
                }

                out = inStream[typeSize - 1];
                for (int i = typeSize - 2; i >= 0; i--)
                {
                    out *= 256;
                    out += inStream[i];
                }

                return sizeof(out);
            }

            static size_t unserialize3Byte(const uint8_t in[3], uint32_t &out)
            {
                if (SMALL_ENDIANNESS)
                {
                    memcpy(&out, &in, 3);
                }
                else
                {
                    out = ((in[2] << (8 * 2)) | (in[1] << 8) | (in[0]));
                }
                return 3;
            }

            template <class T>
            static T unserialize(T in)
            {
                if (SMALL_ENDIANNESS)
                {
                    return in;
                }
                else
                {
                    T out;
                    reverseCopy(&out, &in, sizeof(out));
                    return out;
                }
            }
        };

        /**
         * @brief getFileLastChangeTime
         * @param path
         * @return milliseconds of the file's last change time
         */
        static int64_t getFileLastChangeTime(const std::string &file)
        {
            struct stat sb;
            if (stat(file.c_str(), &sb) < 0)
            {
                return -1;
            }
            return sb.st_mtim.tv_sec * 1000 + sb.st_mtim.tv_nsec / 1000;
        }

        static int64_t UtcStr2HTime64(const char *pszUtcStr)
        {
            struct tm stm;
            int32_t iY, iM, iD, iH, iMin, iS, iMS, iEW;
            int32_t iVarNum = 0, iVarNeed = 7;
            if (strchr(pszUtcStr, 'Z') || strchr(pszUtcStr, 'z'))
            {
                iVarNum = sscanf(pszUtcStr, "%d-%d-%dT%d:%d:%d.%d", &iY, &iM, &iD, &iH, &iMin, &iS, &iMS);
            }
            else
            {
                iVarNeed = 8;
                iVarNum = sscanf(pszUtcStr, "%d-%d-%dT%d:%d:%d.%d+%d", &iY, &iM, &iD, &iH, &iMin, &iS, &iMS, &iEW);
            }

            if (iVarNum != iVarNeed)
            {
                return -1LL;
            }

            stm.tm_year = iY - 1900;
            stm.tm_mon = iM - 1;
            stm.tm_mday = iD;
            stm.tm_hour = iH;
            stm.tm_min = iMin;
            stm.tm_sec = iS;
            time_t tMk = mktime(&stm);
            return tMk * 1000LL + iMS;
        }

        /**
         * @brief strToAddress
         * @param str
         * @param address
         * @return
         */
        static bool strToAddress(const std::string &str, std::array<uint8_t, 6> &address)
        {

            std::vector<uint8_t> addr;
            hutils::hexToBin(str, addr);
            if (addr.size() < 6)
            {
                return false;
            }
            address = {addr[addr.size() - 6],
                       addr[addr.size() - 5],
                       addr[addr.size() - 4],
                       addr[addr.size() - 3],
                       addr[addr.size() - 2],
                       addr[addr.size() - 1]};
            return true;
        }

        static const std::string genToken()
        {
            static std::atomic<uint32_t> s_uiSEQ;
            char pszDisToken[10] = {0};
            sprintf(pszDisToken, "%08x", s_uiSEQ.fetch_add(1));
            return pszDisToken;
        }

        template <class T>
        static const T genNumToken()
        {
            static std::atomic<T> s_uiSEQ;
            return s_uiSEQ;
        }

        /**
         * @brief timeFromMidnight
         * The seconds from midnight zero to now today.
         * @return
         */
        static time_t timeFromMidnight()
        {
            time_t t = time(NULL);
            struct tm now;
            localtime_r(&t, &now);
            now.tm_hour = 0;
            now.tm_min = 0;
            now.tm_sec = 0;
            return mktime(&now);
        }

        static uint64_t getSystemRunTime()
        {
            auto t = std::chrono::steady_clock::now();
            std::chrono::milliseconds ms =
                std::chrono::duration_cast<std::chrono::milliseconds>(t.time_since_epoch());
            return ms.count();
        }

        static std::string GetTimeUtcStr()
        {
            struct tm ts;
            struct timeval tv;
            struct timezone tz;
            static char pszTimeUtc[40];
            memset(pszTimeUtc, 0, sizeof(pszTimeUtc));
            gettimeofday(&tv, &tz);
            tv.tv_sec -= tz.tz_minuteswest;
            localtime_r(&tv.tv_sec, &ts);
            auto us = (int)(tv.tv_usec / 1000);
            sprintf(pszTimeUtc, "%04d-%02d-%02dT%02d:%02d:%02d.%03d",
                    ts.tm_year + 1900, ts.tm_mon + 1, ts.tm_mday,
                    ts.tm_hour, ts.tm_min, ts.tm_sec, us);
            return pszTimeUtc;
        }

        /**
         * @brief GetTimeUtcStrByStamp
         * @param t seconds
         * @return return "2020-03-01 01:01:01"
         */
        static std::string getTimeStrByTimestamp(time_t t)
        {
            static char pszTimeUtc[40];
            struct tm ts;
            localtime_r(&t, &ts);
            sprintf(pszTimeUtc, "%04d-%02d-%02d %02d:%02d:%02d",
                    ts.tm_year + 1900, ts.tm_mon + 1, ts.tm_mday,
                    ts.tm_hour, ts.tm_min, ts.tm_sec);
            return pszTimeUtc;
        }

        /**
         * @brief getTimestampByTimeStr
         * @param timestr  "2020-03-01 01:01:01"
         * @return timestamp
         */
        static time_t getTimestampByTimeStr(const std::string &timestr)
        {
            struct tm ts;
            auto times = hutils::split(timestr, " ");
            if (times.size() < 2)
            {
                return -1;
            }
            auto dates = hutils::split(times[0], "-");
            auto timess = hutils::split(times[1], ":");
            if (dates.size() < 3)
            {
                return -2;
            }
            if (timess.size() < 3)
            {
                return false;
            }

            ts.tm_year = std::stoul(dates[0]) - 1900;
            ts.tm_mon = std::stoul(dates[1]) - 1;
            ts.tm_mday = std::stoul(dates[2]);

            ts.tm_hour = std::stoul(timess[0]);
            ts.tm_min = std::stoul(timess[1]);
            ts.tm_sec = std::stoul(timess[2]);
            ts.tm_isdst = 0;
            return mktime(&ts);
        }

        static std::string tolower(const std::string &s)
        {
            std::string ls = s;
            std::transform(ls.begin(), ls.end(), ls.begin(), ::tolower);
            return ls;
        }

        static std::string toHexString(const void *in, size_t size)
        {
            std::string s;
            char buff[10];
            for (size_t i = 0; i < size; i++)
            {
                snprintf(buff, sizeof(buff), "%02X", ((const char *)in)[i] & 0x0ff);
                s += buff;
            }
            return s;
        }

        static std::string toHexString(uint32_t v)
        {
            char buff[10];
            snprintf(buff, sizeof(buff), "%08X", v);
            return buff;
        }

        static std::string toHexString(uint8_t v)
        {
            char buff[10];
            snprintf(buff, sizeof(buff), "%02X", v & 0x0ff);
            return buff;
        }

        static std::string toHexString(const std::vector<uint8_t> &data)
        {
            return toHexString(data.data(), data.size());
        }

        template <size_t size>
        static std::string toASCIIString(const std::array<uint8_t, size> &data)
        {
            std::vector<uint8_t> d;
            d.assign(data.begin(), data.end());
            d.push_back(0);
            return (const char *)d.data();
        }

        template <size_t size>
        static std::string toHexString(const std::array<uint8_t, size> &data)
        {
            return toHexString({data.begin(), data.end()});
        }

        bool static hexToBin(const std::string &hexStr, std::vector<uint8_t> &hexVec)
        {
            hexVec.clear();
            auto purehex = hexStr;
            purehex = hutils::replace(purehex, " ","");
            try
            {
                for (size_t i = 0; i < purehex.length(); i += 2)
                {
                    std::string hex = "0x" + purehex.substr(i, 2);
                    auto bin = std::stoul(hex, 0, 16);

                    //                LOGDBG("utils","s(%s)->bin(%02X)", hex.c_str(), bin&0xff);

                    if (bin < 0)
                    {
                        return false;
                    }
                    hexVec.push_back(bin);
                }
            }
            catch (const std::exception &err)
            {
                //            LOGERR("utils","error (%s) while convert hex str:%s\n", err.what(), hexStr.c_str());
                return false;
            }
            return true;
        }

        static std::vector<std::string> split(const std::string &input, const std::string &regex)
        {
            // passing -1 as the submatch index parameter performs splitting
            std::regex re(regex);
            std::sregex_token_iterator
                first{input.begin(), input.end(), re, -1},
                last;
            return {first, last};
        }

        static const std::string rtrim(const std::string &s, const char &ch = ' ')
        {
            std::string::const_iterator iter = find_if(s.rbegin(), s.rend(),
                                                       [&](const char c)
                                                       {
                                                           if (c == ch)
                                                           {
                                                               return false;
                                                           }
                                                           return true;
                                                       })
                                                   .base();
            return std::string(s.begin(), iter);
        }

        static const std::string ltrim(const std::string &s, const char &ch = ' ')
        {
            std::string::const_iterator iter = find_if(s.begin(), s.end(), [&](const char c)
                                                       {
            if(c == ch){
                return false;
            }
            return true; });
            return std::string(iter, s.end());
        }

        static const std::string trim(const std::string &s, const char &ch = ' ')
        {
            auto rs = rtrim(s, ch);
            return ltrim(rs, ch);
        }

        /*************************************************
         *  @name:  replace
         *  @brief:   replace oldStr in str with newStr
         *  @param str: original string
         *  @param oldS: old sub string in original string
         *  @param newS: new sub string
         *  @return:
         *  @date: 2019/08/23
         *  @author : huhai
         *  @note:
         *************************************************/
        static std::string &replace(std::string &oriS, const std::string &oldS, const std::string &newS)
        {
            std::string::size_type pos = 0;
            std::string::size_type srclen = oldS.size();
            std::string::size_type dstlen = newS.size();
            while ((pos = oriS.find(oldS, pos)) != std::string::npos)
            {
                oriS.replace(pos, srclen, newS);
                pos += dstlen;
            }
            return oriS;
        }

        /*************************************************
         *  @name:replaceChar
         *  @brief:
         *  @param :
         *  @return:
         *  @date: 2019/08/23
         *  @author : huhai
         *  @note:
         *************************************************/
        static std::string &replaceChar(std::string &oriString, char oriCh, char dstCh)
        {
            for (size_t i = 0; i < oriString.length(); i++)
            {
                if (oriString[i] == oriCh)
                {
                    oriString[i] = dstCh;
                }
            }
            return oriString;
        }

        /*************************************************
         *  @name:  format
         *  @brief:
         *  @param :
         *  @return:
         *  @date: 2019/08/23
         *  @author : huhai
         *  @note:
         *************************************************/
        static std::string format(const char *fmt, ...)
        {
            const int T_BUF_SIZE = 1024;
            char *buf = new char[T_BUF_SIZE];
            memset(buf, 0, T_BUF_SIZE);
            va_list vArgList;
            va_start(vArgList, fmt);
            vsnprintf(buf, T_BUF_SIZE, fmt, vArgList);
            va_end(vArgList);
            std::string str(buf);
            delete[] buf;
            return str;
        }

        /*************************************************
         *  @name:  toInt
         *  @brief: convert the number from string type to integer type.
         *  @param :
         *  @return:
         *  @date: 2019/08/23
         *  @author : huhai
         *  @note:
         *************************************************/
        static int toInt(std::string &str)
        {
            return typeConvert<int, std::string>(str);
        }

        /*************************************************
         *  @name:
         *  @brief:
         *  @param :
         *  @return:
         *  @date: 2019/08/28
         *  @author : huhai
         *  @note:
         *************************************************/
        static bool isDigit(char c)
        {
            if (c >= '0' && c <= '9')
            {
                return true;
            }
            return false;
        }

        /*************************************************
         *  @name:
         *  @brief:
         *  @param :
         *  @return:
         *  @date: 2019/08/28
         *  @author : huhai
         *  @note:
         *************************************************/
        static bool isDigits(std::string &s)
        {
            for (size_t i = 0; i < s.length(); i++)
            {
                if (!isDigit(s[i]))
                {
                    return false;
                }
            }
            return true;
        }

        /*************************************************
         *  @name:  toString
         *  @brief:   convert numbers to string format.
         *  @param :
         *  @return:
         *  @date: 2019/08/23
         *  @author : huhai
         *  @note:
         *************************************************/
        template <class T>
        static std::string toString(const T &v)
        {
            return typeConvert<std::string, T>(v);
        }

        /*************************************************
             *  @name:  typeConvert
             *  @brief:   convert from one type to another.
                           such as int to string , or string to int.
             *  @param :
             *  @return:
             *  @date: 2019/08/23
             *  @author : huhai
             *  @note:
             *************************************************/
        template <class out_type, class in_value>
        static out_type typeConvert(const in_value &t)
        {
            using namespace std;
            stringstream stream;
            stream << t;
            out_type result;
            stream >> result;
            return result;
        }
        /*************************************************
         *  @name:  isValidDate
         *  @brief:   check if time is valid. such as 4-31 should return false.
         *  @param :  time
         *  @return:
         *  @date: 2019/08/28
         *  @author : huhai
         *  @note:
         *************************************************/
        static bool isValidTime(const struct tm &time)
        {
            struct tm tt;
            memcpy(&tt, &time, sizeof(tt));
            time_t ret = mktime(&tt);
            if (ret < 0)
                return false;

            struct tm tm_old;
            localtime_r(&ret, &tm_old);
            if (tm_old.tm_year != time.tm_year ||
                tm_old.tm_mon != time.tm_mon ||
                tm_old.tm_mday != time.tm_mday ||
                tm_old.tm_hour != time.tm_hour ||
                tm_old.tm_min != time.tm_min ||
                tm_old.tm_sec != time.tm_sec)
            {
                return false;
            }
            return true;
        }

        static std::string tm2string(const struct tm &time)
        {
            return format("%04d-%02d-%02d %02d:%02d:%02d", time.tm_year, time.tm_mon + 1, time.tm_mday,
                          time.tm_hour, time.tm_min, time.tm_sec);
        }

        /*************************************************
         *  @name:  strHHmmssToTm
         *  @brief:  Convert the string format time like "HH:mm:ss" to tm format.
         *  @param timeStr[in]: string format time. HH is in range [0,23].
         *  @param outTime[out]:the tm format to store the result.
         *  @return: returns true if successful, false if fails.
         *  @date: 2019/08/23
         *  @author : huhai
         *  @note:
         *************************************************/
        static bool strHHmmssToTm(std::string HHmmss, tm &outTime)
        {
            using namespace std;
            vector<string> vec = split(HHmmss.c_str(), ":");
            if (vec.size() < 3)
            {
                return false;
            }

            outTime.tm_hour = toInt(vec[0]);
            if (outTime.tm_hour > 23 || outTime.tm_hour < 0)
            {
                return false;
            }
            outTime.tm_min = toInt(vec[1]);
            if (outTime.tm_min > 59 || outTime.tm_min < 0)
            {
                return false;
            }
            outTime.tm_sec = toInt(vec[2]);
            if (outTime.tm_sec > 59 || outTime.tm_sec < 0)
            {
                return false;
            }
            return true;
        }
    };

}
#endif // HHUTILS_H
